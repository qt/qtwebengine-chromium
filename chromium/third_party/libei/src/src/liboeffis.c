/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#if HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif

#include "util-io.h"
#include "util-macros.h"
#include "util-object.h"
#include "util-sources.h"
#include "util-strings.h"
#include "util-time.h"
#include "util-version.h"

#include "liboeffis.h"

_Static_assert(sizeof(enum oeffis_event_type) == sizeof(int), "Invalid enum size");
_Static_assert(sizeof(enum oeffis_device) == sizeof(uint32_t), "Invalid enum size");

/* oeffis is simple enough that we don't really need debugging or a log
 * handler. If you want this on, just define it */
/* #define log_debug(...) fprintf(stderr, "DEBUG " __VA_ARGS__) */
#define log_debug(...) /* */

static void
portal_init(struct oeffis *oeffis, const char *busname);
static void
portal_start(struct oeffis *oeffis);

enum oeffis_state {
	OEFFIS_STATE_NEW,
	OEFFIS_STATE_CREATE_SESSION,
	OEFFIS_STATE_SESSION_CREATED,
	OEFFIS_STATE_STARTED,
	OEFFIS_STATE_CONNECTED_TO_EIS,
	OEFFIS_STATE_DISCONNECTED, /* used for closed as well since
				      internally it's the same thing */
};

struct oeffis {
	struct object object;
	void *user_data;
	struct sink *sink;

	enum oeffis_state state;
	uint32_t devices;

        /* We can have a maximum of 3 events (connected, optional closed,
         * disconnected) so we can have the event queue be a fixed
         * null-terminated array, have a pointer to the current element and
         * shift that on with every event */
        enum oeffis_event_type event_queue[4];
	/* Points to the next client-visible event in the event queue. only
	 * advanced by the client */
	enum oeffis_event_type *next_event;

	int eis_fd;

	/* NULL until OEFFIS_STATE_DISCONNECTED */
	char *error_message;

	/* internal epollfd tickler */
	struct source *epoll_tickler_source;
	int pipefd[2];

	/* sd-bus pieces */
	struct source *bus_source;
	sd_bus *bus;
	sd_bus_slot *slot_request_response; /* Re-used for Request.Response */
	sd_bus_slot *slot_session_closed;
	char *busname;
	char *session_path;
	char *sender_name;
};

static void
oeffis_destroy(struct oeffis *oeffis)
{
	free(oeffis->error_message);
	sink_unref(oeffis->sink);
	xclose(oeffis->eis_fd);
	xclose(oeffis->pipefd[0]);
	xclose(oeffis->pipefd[1]);

	free(oeffis->sender_name);
	free(oeffis->session_path);
	free(oeffis->busname);
	sd_bus_close(oeffis->bus);
	sd_bus_unref(oeffis->bus);
	sd_bus_slot_unref(oeffis->slot_request_response);
	sd_bus_slot_unref(oeffis->slot_session_closed);
}

static
OBJECT_IMPLEMENT_CREATE(oeffis);
_public_
OBJECT_IMPLEMENT_REF(oeffis);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(oeffis);
_public_
OBJECT_IMPLEMENT_SETTER(oeffis, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(oeffis, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(oeffis, error_message, const char *);

DEFINE_UNREF_CLEANUP_FUNC(source);

static void
tickle(struct oeffis *oeffis)
{
	xwrite(oeffis->pipefd[1], "kitzel", 6);
}

static void
_printf_(2, 3)
oeffis_disconnect(struct oeffis *oeffis, const char *fmt, ...)
{
	if (oeffis->state == OEFFIS_STATE_DISCONNECTED)
		return;

	va_list args;
	va_start(args, fmt);
	oeffis->state = OEFFIS_STATE_DISCONNECTED;
	oeffis->error_message = xvaprintf(fmt, args);
	va_end(args);

	*oeffis->next_event = OEFFIS_EVENT_DISCONNECTED;

	oeffis->eis_fd = xclose(oeffis->eis_fd);

	tickle(oeffis);

	/* FIXME: need to so more here? */
}

static void
tickled(struct source *source, void *data)
{
	/* Nothing to do here, just drain the data */
	char buf[64];
	xread(source_get_fd(source), buf, sizeof(buf));
}

_public_ struct oeffis *
oeffis_new(void *user_data)
{
	_unref_(oeffis) *oeffis = oeffis_create(NULL);

	oeffis->state = OEFFIS_STATE_NEW;
	oeffis->user_data = user_data;
	oeffis->next_event = oeffis->event_queue;
	oeffis->eis_fd = -1;
	oeffis->pipefd[0] = -1;
	oeffis->pipefd[1] = -1;

	oeffis->sink = sink_new();
	if (!oeffis->sink)
		return NULL;

        /* set up a pipe we can write to to force the epoll to wake up even when
         * nothing else happens */
	int rc = xpipe2(oeffis->pipefd, O_CLOEXEC|O_NONBLOCK);
	if (rc < 0)
		return NULL;

        _unref_(source) *s = source_new(oeffis->pipefd[0], tickled, NULL);
	sink_add_source(oeffis->sink, s);

	return steal(&oeffis);
}

_public_ int
oeffis_get_fd(struct oeffis *oeffis)
{
	return sink_get_fd(oeffis->sink);
}

_public_ int
oeffis_get_eis_fd(struct oeffis *oeffis)
{
	if (oeffis->state != OEFFIS_STATE_CONNECTED_TO_EIS) {
		errno = ENODEV;
		return -1;
	}

	return xdup(oeffis->eis_fd);
}

_public_ enum oeffis_event_type
oeffis_get_event(struct oeffis *oeffis)
{
	enum oeffis_event_type e = *oeffis->next_event;

	if (e != OEFFIS_EVENT_NONE)
		oeffis->next_event++;

	assert(oeffis->next_event < oeffis->event_queue + ARRAY_LENGTH(oeffis->event_queue));

	return e;
}

_public_ void
oeffis_create_session(struct oeffis *oeffis, uint32_t devices)
{
	oeffis_create_session_on_bus(oeffis, "org.freedesktop.portal.Desktop", devices);
}

_public_ void
oeffis_create_session_on_bus(struct oeffis *oeffis, const char *busname, uint32_t devices)
{
	if (oeffis->state != OEFFIS_STATE_NEW)
		return;

	oeffis->devices = devices;
	oeffis->state = OEFFIS_STATE_CREATE_SESSION;
	portal_init(oeffis, busname);
}

_public_ void
oeffis_dispatch(struct oeffis *oeffis)
{
	sink_dispatch(oeffis->sink);
}

static int
oeffis_set_eis_fd(struct oeffis *oeffis, int eisfd)
{
	if (oeffis->state != OEFFIS_STATE_STARTED)
		return -EALREADY;

	oeffis->state = OEFFIS_STATE_CONNECTED_TO_EIS;
	oeffis->eis_fd = eisfd;
	*oeffis->next_event = OEFFIS_EVENT_CONNECTED_TO_EIS;

	tickle(oeffis);

	return 0;
}

static void
oeffis_close(struct oeffis *oeffis)
{
	switch (oeffis->state) {
	case OEFFIS_STATE_NEW:
		oeffis_disconnect(oeffis, "Bug: Received Session.Close in state NEW.");
		break;
	case OEFFIS_STATE_CREATE_SESSION:
	case OEFFIS_STATE_SESSION_CREATED:
	case OEFFIS_STATE_CONNECTED_TO_EIS:
	case OEFFIS_STATE_STARTED:
		*oeffis->next_event = OEFFIS_EVENT_CLOSED;
		tickle(oeffis);
		oeffis->state = OEFFIS_STATE_DISCONNECTED;
		break;
	case OEFFIS_STATE_DISCONNECTED:
		break;
	}
}

/********************************************** DBus implementation **************************************************/

static char *
sender_name(sd_bus *bus)
{
	_cleanup_free_ char *sender = NULL;
	const char *name = NULL;

	if ((sd_bus_get_unique_name(bus, &name) != 0) || strlen(name) < 1)
		return NULL;

	name += 1; /* drop initial : */
	sender = xalloc(strlen(name));

	for (unsigned i = 0; name[i]; i++) {
		sender[i] = name[i] == '.' ? '_' : name[i];
	}

	return steal(&sender);
}
static char *
xdp_token(void)
{
	/* next for easier debugging, rand() so we don't ever conflict in
	 * real life situations */
	static uint32_t next = 0;
	return xaprintf("oeffis_%u_%d", next++, rand());
}

static char *
xdp_request_path(char *sender_name, char *token)
{
	return xaprintf("/org/freedesktop/portal/desktop/request/%s/%s", sender_name, token);
}

static char *
xdp_session_path(char *sender_name, char *token)
{
	return xaprintf("/org/freedesktop/portal/desktop/session/%s/%s", sender_name, token);
}


static int
connect_to_eis_returned(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
	int eisfd;
	struct oeffis *oeffis = userdata;
	int rc = sd_bus_message_get_errno(m);

	if (rc > 0) {
		oeffis_disconnect(oeffis, "Error calling ConnectToEIS: %s", strerror(rc));
		return rc;
	}

	rc = sd_bus_message_read(m, "h", &eisfd);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Unable to get fd from portal: %s", strerror(-rc));
		return -rc;
	}

	/* the fd is owned by the message */
	rc = xerrno(xdup(eisfd));
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to dup fd: %s", strerror(-rc));
		return -rc;

	} else {
		eisfd = rc;
		int flags = fcntl(eisfd, F_GETFL, 0);
		fcntl(eisfd, F_SETFL, flags | O_NONBLOCK);
	}

	log_debug("Got fd %d from portal", eisfd);

	rc = oeffis_set_eis_fd(oeffis, eisfd);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to set the fd: %s", strerror(-rc));
		return -rc;
	}

	return 0;
}

static void
portal_connect_to_eis(struct oeffis *oeffis)
{
	sd_bus *bus = oeffis->bus;

	int rc = 0;
	with_signals_blocked(SIGALRM) {
		rc = sd_bus_call_method_async(bus, NULL, oeffis->busname,
					"/org/freedesktop/portal/desktop",
					"org.freedesktop.portal.RemoteDesktop",
					"ConnectToEIS",
					&connect_to_eis_returned,
					oeffis,
					"oa{sv}",
					oeffis->session_path,
					0);
	}

	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to call ConnectToEIS: %s", strerror(-rc));
		return;
	}
}

static int
session_closed_received(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
	struct oeffis *oeffis = userdata;

	oeffis_close(oeffis);

	return 0;
}

static void
dbus_dispatch(struct source *source, void *data)
{
	struct oeffis *oeffis = data;
	sd_bus *bus = oeffis->bus;

	int rc;
	do {
		rc = sd_bus_process(bus, NULL);
	} while (rc > 0);

	if (rc < 0)
		oeffis_disconnect(oeffis, "dbus processing failed with %s", strerror(-rc));
}

static int
portal_setup_request(struct oeffis *oeffis, sd_bus_message_handler_t response_handler,
		     char **token_return, sd_bus_slot **slot_return)
{
	sd_bus *bus = oeffis->bus;
	_unref_(sd_bus_slot) *slot = NULL;
	_cleanup_free_ char *token = xdp_token();
	_cleanup_free_ char *handle = xdp_request_path(oeffis->sender_name, token);

	int rc = 0;
	with_signals_blocked(SIGALRM) {
		rc = sd_bus_match_signal(bus, &slot,
					 oeffis->busname,
					 handle,
					 "org.freedesktop.portal.Request",
					 "Response",
					 response_handler,
					 oeffis);
	}

	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to subscribe to Request.Response signal: %s", strerror(-rc));
		return rc;
	}

	*token_return = steal(&token);
	*slot_return = steal(&slot);

	return 0;
}

static int
portal_start_response_received(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
	struct oeffis *oeffis = userdata;

	/* We'll only get this signal once */
	oeffis->slot_request_response = sd_bus_slot_unref(oeffis->slot_request_response);

	unsigned int response;
	int rc = sd_bus_message_read(m, "u", &response);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to read response from signal: %s", strerror(-rc));
		return 0;
	}

	log_debug("Portal Start reponse is %u", response);
	if (response != 0) {
		oeffis_disconnect(oeffis, "Portal denied Start");
		return 0;
	}

	oeffis->state = OEFFIS_STATE_STARTED;

	/* Response includes the the device bitmask but we don't care about this here */

	/* Don't need a separate state here, ConnectToEIS is synchronous */
	portal_connect_to_eis(oeffis);

	return 0;
}

static void
portal_start(struct oeffis *oeffis)
{
	_cleanup_free_ char *token = NULL;
	_unref_(sd_bus_slot) *request_slot = NULL;

	int rc = portal_setup_request(oeffis, portal_start_response_received, &token, &request_slot);
	if (rc < 0)
		return;

	_cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
	_unref_(sd_bus_message) *response = NULL;
	sd_bus *bus = oeffis->bus;
	with_signals_blocked(SIGALRM) {
		rc = sd_bus_call_method(bus,
					oeffis->busname,
					"/org/freedesktop/portal/desktop",
					"org.freedesktop.portal.RemoteDesktop",
					"Start",
					&error,
					&response,
					"osa{sv}",
					oeffis->session_path,
					"", /* parent window */
					1,
					"handle_token", /* string key */
					"s", token /* variant string */
				       );
	}

	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to call method: %s", strerror(-rc));
		return;
	}

	const char *path = NULL;
	rc = sd_bus_message_read(response, "o", &path);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to parse Start reply: %s", strerror(-rc));
		return;
	}

	oeffis->slot_request_response = sd_bus_slot_ref(request_slot);
	return;
}

static int
portal_select_devices_response_received(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
	struct oeffis *oeffis = userdata;

	/* We'll only get this signal once */
	oeffis->slot_request_response = sd_bus_slot_unref(oeffis->slot_request_response);

	unsigned int response;
	int rc = sd_bus_message_read(m, "u", &response);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to read response from signal: %s", strerror(-rc));
		return 0;
	}

	log_debug("Portal SelectDevices reponse is %u", response);
	if (response != 0) {
		oeffis_disconnect(oeffis, "Portal denied SelectDevices");
		return 0;
	}

	/* Response includes the the device bitmask but we don't care about this here */
        portal_start(oeffis);

	return 0;
}

static void
portal_select_devices(struct oeffis *oeffis)
{
	sd_bus *bus = oeffis->bus;

	_cleanup_free_ char *token = NULL;
	_unref_(sd_bus_slot) *request_slot = NULL;
	int rc = portal_setup_request(oeffis, portal_select_devices_response_received, &token, &request_slot);
	if (rc < 0)
		return;

	_cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
	_unref_(sd_bus_message) *response = NULL;
	with_signals_blocked(SIGALRM) {
		rc = sd_bus_call_method(bus,
					oeffis->busname,
					"/org/freedesktop/portal/desktop",
					"org.freedesktop.portal.RemoteDesktop",
					"SelectDevices",
					&error,
					&response,
					"oa{sv}",
					oeffis->session_path,
					oeffis->devices == OEFFIS_DEVICE_ALL_DEVICES ? 1 : 2,
					"handle_token", /* string key */
					"s", token, /* variant string */
					"types", /* string key */
					"u", oeffis->devices
				       );
	}

	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to call method: %s", strerror(-rc));
		return;
	}

	const char *path = NULL;
	rc = sd_bus_message_read(response, "o", &path);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to parse Start reply: %s", strerror(-rc));
		return;
	}

	oeffis->slot_request_response = sd_bus_slot_ref(request_slot);
}

static int
portal_create_session_response_received(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
	struct oeffis *oeffis = userdata;

	/* We'll only get this signal once */
	oeffis->slot_request_response = sd_bus_slot_unref(oeffis->slot_request_response);

	unsigned int response;
	int rc = sd_bus_message_read(m, "u", &response);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to read response from signal: %s", strerror(-rc));
		return 0;
	}

	log_debug("Portal CreateSession reponse is %u", response);

	const char *session_handle = NULL;
	if (response == 0) {
		const char *key;
		rc = sd_bus_message_read(m, "a{sv}", 1, &key, "s", &session_handle);
		if (rc < 0) {
			oeffis_disconnect(oeffis, "Failed to read session handle from signal: %s", strerror(-rc));
			return 0;
		}

		if (!streq(key, "session_handle")) {
			oeffis_disconnect(oeffis, "Invalid or unhandled option: %s", key);
			return 0;
		}
	}

	if (response != 0) {
		oeffis_disconnect(oeffis, "Portal denied CreateSession");
		return 0;
	}

	oeffis->session_path = xstrdup(session_handle);
	oeffis->state = OEFFIS_STATE_SESSION_CREATED;

	portal_select_devices(oeffis);

	return 0;
}

static void
portal_init(struct oeffis *oeffis, const char *busname)
{
	_cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
	_unref_(sd_bus) *bus = NULL;
	_unref_(sd_bus_message) *response = NULL;
	const char *path = NULL;

	int rc = sd_bus_open_user(&bus);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to init dbus: %s", strerror(-rc));
		return;
	}

	oeffis->sender_name = sender_name(bus);
	if (!oeffis->sender_name) {
		oeffis_disconnect(oeffis, "Failed to parse sender name");
		return;
	}

	oeffis->bus = sd_bus_ref(bus);
	oeffis->busname = xstrdup(busname);

	uint32_t version;
	rc = sd_bus_get_property_trivial(bus, busname,
					 "/org/freedesktop/portal/desktop",
					 "org.freedesktop.portal.RemoteDesktop",
					 "version",
					 &error,
					 'u',
					 &version);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to get RemoteDesktop.version: %s", strerror(sd_bus_error_get_errno(&error)));
		return;
	} else if (version < VERSION_V(2)) {
		oeffis_disconnect(oeffis, "RemoteDesktop.version is %u, we need 2", version);
		return;
	}
	log_debug("RemoteDesktop.version is %u", version);

	_cleanup_free_ char *token = NULL;
	_unref_(sd_bus_slot) *request_slot = NULL;
	rc = portal_setup_request(oeffis, portal_create_session_response_received, &token, &request_slot);
	if (rc < 0)
		return;

	_unref_(sd_bus_slot) *session_slot = NULL;
	_cleanup_free_ char *session_token = xdp_token();
	_cleanup_free_ char *session_handle = xdp_session_path(oeffis->sender_name, session_token);
	rc = sd_bus_match_signal(bus, &session_slot,
				 busname,
				 session_handle,
				 "org.freedesktop.portal.Session",
				 "Closed",
				 session_closed_received,
				 oeffis);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to subscribe to Session.Closed signal: %s", strerror(-rc));
		return;
	}

	with_signals_blocked(SIGALRM) {
		rc = sd_bus_call_method(bus,
					busname,
					"/org/freedesktop/portal/desktop",
					"org.freedesktop.portal.RemoteDesktop",
					"CreateSession",
					&error,
					&response,
					"a{sv}", 2,
					"handle_token", /* string key */
					"s", token, /* variant string */
					"session_handle_token", /* string key */
					"s", session_token /* variant string */
				       );
	}

	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to call method: %s", strerror(-rc));
		return;
	}

	rc = sd_bus_message_read(response, "o", &path);
	if (rc < 0) {
		oeffis_disconnect(oeffis, "Failed to parse CreateSession reply: %s", strerror(-rc));
		return;
	}

	log_debug("Portal Response object is %s", path);

	_unref_(source) *s = source_new(sd_bus_get_fd(bus), dbus_dispatch, oeffis);
	source_never_close_fd(s); /* the bus object handles the fd */
	rc = sink_add_source(oeffis->sink, s);
	if (rc == 0) {
		oeffis->bus_source = source_ref(s);
		oeffis->slot_request_response = sd_bus_slot_ref(request_slot);
		oeffis->slot_session_closed = sd_bus_slot_ref(session_slot);
	}

	return;
}

#ifdef _enable_tests_
#include "util-munit.h"

MUNIT_TEST(test_init_unref)
{
	struct oeffis *oeffis = oeffis_new(NULL);

	munit_assert_int(oeffis->state, ==, OEFFIS_STATE_NEW);
	munit_assert_not_null(oeffis->sink);
	munit_assert_int(oeffis->eis_fd, ==, -1);

	struct oeffis *refd = oeffis_ref(oeffis);
	munit_assert_ptr_equal(oeffis, refd);
	munit_assert_int(oeffis->object.refcount, ==, 2);

	struct oeffis *unrefd = oeffis_unref(oeffis);
	munit_assert_null(unrefd);

	unrefd = oeffis_unref(oeffis);
	munit_assert_null(unrefd);

	return MUNIT_OK;
}

MUNIT_TEST(test_failed_connect)
{
	struct oeffis *oeffis = oeffis_new(NULL);
	enum oeffis_event_type event;

	oeffis_create_session_on_bus(oeffis, "foo.bar.Example", 0);
	while ((event = oeffis_get_event(oeffis)) == OEFFIS_EVENT_NONE)
		oeffis_dispatch(oeffis);
	munit_assert_int(event, ==, OEFFIS_EVENT_DISCONNECTED);
	munit_assert_int(oeffis->state, ==, OEFFIS_STATE_DISCONNECTED);
	munit_assert_not_null(oeffis->error_message);

	oeffis_unref(oeffis);

	return MUNIT_OK;
}
#endif
