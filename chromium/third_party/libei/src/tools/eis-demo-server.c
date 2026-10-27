/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Red Hat, Inc.
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

/* A simple tool that provides an EIS implementation that prints incoming
 * events.
 *
 * This tool is useful for testing libei event flow, to make sure the
 * connection works and events arrive at the EIS implementation.
 *
 * The server has hardcoded regions and creates fixed devices, it's really
 * just for basic purposes.
 *
 * Usually, you'd want to:
 * - run the eis-demo-server (or some other EIS implementation)
 * - export LIBEI_SOCKET=eis-0, or whatever value was given
 * - run the libei client
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <inttypes.h>

#if HAVE_LIBXKBCOMMON
#include <xkbcommon/xkbcommon.h>
#endif

#include "src/util-color.h"
#include "src/util-mem.h"
#include "src/util-memfile.h"
#include "src/util-strings.h"
#include "src/util-time.h"

#include "eis-demo-server.h"

static bool stop = false;

static void sighandler(int signal) {
	stop = true;
}

static
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_demo_client);

static void
log_handler(struct eis *eis, enum eis_log_priority priority,
	    const char *message, struct eis_log_context *ctx)
{
	struct lut {
		const char *color;
		const char *prefix;
	} lut[] = {
		{ .color = ansi_colorcode[RED],		.prefix = "<undefined>", }, /* debug starts at 10 */
		{ .color = ansi_colorcode[HIGHLIGHT],	.prefix = "DEBUG", },
		{ .color = ansi_colorcode[GREEN],	.prefix = "INFO", },
		{ .color = ansi_colorcode[BLUE],	.prefix = "WARN", },
		{ .color = ansi_colorcode[RED],		.prefix = "ERROR", },
	};
	static time_t last_time = 0;
	const char *reset_code = ansi_colorcode[RESET];

	run_only_once {
		if (!isatty(STDOUT_FILENO)) {
			struct lut *l;
			ARRAY_FOR_EACH(lut, l)
				l->color = "";
			reset_code = "";
		}
	}

	time_t now = time(NULL);
	char timestamp[64];

	if (last_time != now) {
		struct tm *tm = localtime(&now);
		strftime(timestamp, sizeof(timestamp), "%T", tm);
	} else {
		xsnprintf(timestamp, sizeof(timestamp), "...");
	}

	size_t idx = priority/10;
	assert(idx < ARRAY_LENGTH(lut));
	fprintf(stdout, " EIS: %8s | %s%4s%s | %s\n", timestamp,
		lut[idx].color, lut[idx].prefix, reset_code, message);

	last_time = now;
}

static void
eis_demo_client_destroy(struct eis_demo_client *democlient)
{
	list_remove(&democlient->link);
	eis_client_unref(democlient->client);
	eis_device_unref(democlient->ptr);
	eis_device_unref(democlient->abs);
	eis_device_unref(democlient->kbd);
	eis_device_unref(democlient->touchscreen);
}

static
OBJECT_IMPLEMENT_CREATE(eis_demo_client);

static struct eis_demo_client *
eis_demo_client_new(struct eis_demo_server *server, struct eis_client *client)
{
	struct eis_demo_client *c = eis_demo_client_create(NULL);
	c->client = eis_client_ref(client);
	list_append(&server->clients, &c->link);
	return c;
}

static struct eis_demo_client *
eis_demo_client_find(struct eis_demo_server *server, struct eis_client *client)
{
	struct eis_demo_client *c;
	list_for_each(c, &server->clients, link) {
		if (c->client == client)
			return c;
	}
	return NULL;
}

DEFINE_UNREF_CLEANUP_FUNC(eis);
DEFINE_UNREF_CLEANUP_FUNC(eis_event);
DEFINE_UNREF_CLEANUP_FUNC(eis_keymap);
DEFINE_UNREF_CLEANUP_FUNC(eis_seat);
DEFINE_UNREF_CLEANUP_FUNC(eis_region);

static void unlink_free(char **path) {
	if (*path) {
		unlink(*path);
		free(*path);
	}
}
#define _cleanup_unlink_free_ _cleanup_(unlink_free)

static inline void
_printf_(1, 2)
colorprint(const char *format, ...)
{
	static uint64_t color = 0;
	run_only_once {
		color = rgb(1, 1, 1) | rgb_bg(255, 127, 0);
	}

	const char prefix[] = "EIS socket server: ";
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s", prefix);

	va_list args;
	va_start(args, format);
	vsnprintf(buf + strlen(prefix), sizeof(buf) - strlen(prefix), format, args);
	va_end(args);
	cprintf(color, "%s", buf);
}

#if HAVE_MEMFD_CREATE && HAVE_LIBXKBCOMMON
DEFINE_UNREF_CLEANUP_FUNC(xkb_context);
DEFINE_UNREF_CLEANUP_FUNC(xkb_keymap);
DEFINE_UNREF_CLEANUP_FUNC(xkb_state);
#endif

static void
setup_keymap(struct eis_demo_server *server, struct eis_device *device)
{
#if HAVE_MEMFD_CREATE && HAVE_LIBXKBCOMMON
	colorprint("Using server layout: %s\n", server->layout);
	_unref_(xkb_context) *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!ctx)
		return;

	struct xkb_rule_names names = {
		.rules = "evdev",
		.model = "pc105",
		.layout = server->layout,
	};

	_unref_(xkb_keymap) *keymap = xkb_keymap_new_from_names(ctx, &names, 0);
	if (!keymap)
		return;

	const char *str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	size_t len = strlen(str) - 1;

	struct memfile *f = memfile_new(str, len);
	if (!f)
		return;

	_unref_(eis_keymap) *k = eis_device_new_keymap(device,
						EIS_KEYMAP_TYPE_XKB, memfile_get_fd(f),
						memfile_get_size(f));
	eis_keymap_add(k);
	memfile_unref(f);

	_unref_(xkb_state) *state = xkb_state_new(keymap);
	if (!state)
		return;

	server->ctx = steal(&ctx);
	server->keymap = steal(&keymap);
	server->state = steal(&state);
#endif
}

static void
handle_key(struct eis_demo_server *server, uint32_t keycode, bool is_press)
{
	char keysym_name[64] = {0};

#if HAVE_LIBXKBCOMMON
	if (server->state) {
		uint32_t xkbkc = keycode + 8;
		xkb_state_update_key(server->state, xkbkc, is_press ? XKB_KEY_DOWN : XKB_KEY_UP);
		xkb_state_key_get_utf8(server->state, xkbkc, keysym_name, sizeof(keysym_name));
	}
#endif
	colorprint("key %u (%s) [%s]\n",
		   keycode, is_press ? "press" : "release",
		   keysym_name);
}

static struct eis_device *
add_device(struct eis_demo_server *server, struct eis_client *client,
	   struct eis_seat *seat, enum eis_device_capability cap)
{
	static uint32_t sequence;

	struct eis_device *device = NULL;
	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
		{
		struct eis_device *ptr = eis_seat_new_device(seat);
		eis_device_configure_name(ptr, "test pointer");
		eis_device_configure_capability(ptr, EIS_DEVICE_CAP_POINTER);
		eis_device_configure_capability(ptr, EIS_DEVICE_CAP_BUTTON);
		eis_device_configure_capability(ptr, EIS_DEVICE_CAP_SCROLL);
		_unref_(eis_region) *rel_region = eis_device_new_region(ptr);
		eis_region_set_mapping_id(rel_region, "demo region");
		eis_region_set_size(rel_region, 1920, 1080);
		eis_region_set_offset(rel_region, 0, 0);
		eis_region_add(rel_region);
		colorprint("Creating pointer device %s for %s\n", eis_device_get_name(ptr),
			   eis_client_get_name(client));
		eis_device_add(ptr);
		eis_device_resume(ptr);
		if (!eis_client_is_sender(client))
			eis_device_start_emulating(ptr, ++sequence);
		device = steal(&ptr);
		break;
		}
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
		{
		struct eis_device *abs = eis_seat_new_device(seat);
		eis_device_configure_name(abs, "test abs pointer");
		eis_device_configure_capability(abs, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
		eis_device_configure_capability(abs, EIS_DEVICE_CAP_BUTTON);
		eis_device_configure_capability(abs, EIS_DEVICE_CAP_SCROLL);
		_unref_(eis_region) *region = eis_device_new_region(abs);
		eis_region_set_mapping_id(region, "demo region");
		eis_region_set_size(region, 1920, 1080);
		eis_region_set_offset(region, 0, 0);
		eis_region_add(region);
		colorprint("Creating abs pointer device %s for %s\n", eis_device_get_name(abs),
			   eis_client_get_name(client));
		eis_device_add(abs);
		eis_device_resume(abs);
		if (!eis_client_is_sender(client))
			eis_device_start_emulating(abs, ++sequence);
		device = steal(&abs);
		break;
		}
	case EIS_DEVICE_CAP_KEYBOARD:
		{
		struct eis_device *kbd = eis_seat_new_device(seat);
		eis_device_configure_name(kbd, "test keyboard");
		eis_device_configure_capability(kbd, EIS_DEVICE_CAP_KEYBOARD);
		if (server->layout)
			setup_keymap(server, kbd);
		colorprint("Creating keyboard device %s for %s\n", eis_device_get_name(kbd),
			   eis_client_get_name(client));
		eis_device_add(kbd);
		eis_device_resume(kbd);
		if (!eis_client_is_sender(client))
			eis_device_start_emulating(kbd, ++sequence);
		device = steal(&kbd);
		break;
		}
	case EIS_DEVICE_CAP_TOUCH:
		{
		struct eis_device *touchscreen = eis_seat_new_device(seat);
		eis_device_configure_name(touchscreen, "test touchscreen");
		eis_device_configure_capability(touchscreen, EIS_DEVICE_CAP_TOUCH);
		colorprint("Creating touchscreen device %s for %s\n", eis_device_get_name(touchscreen),
			   eis_client_get_name(client));
		eis_device_add(touchscreen);
		eis_device_resume(touchscreen);
		if (!eis_client_is_sender(client))
			eis_device_start_emulating(touchscreen, ++sequence);
		device = steal(&touchscreen);
		break;
		}
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		/* Mixed in with pointer/abs - good enough for a demo server */
		break;
	}

	return device;
}



/**
 * The simplest event handler. Connect any client and any device and just
 * printf the events as the come in. This is an incomplete implementation,
 * it just does the basics for pointers and keyboards atm.
 */
static int
eis_demo_server_printf_handle_event(struct eis_demo_server *server,
				    struct eis_event *e)
{
	switch(eis_event_get_type(e)) {
	case EIS_EVENT_CLIENT_CONNECT:
		{
		struct eis_client *client = eis_event_get_client(e);
		bool is_sender = eis_client_is_sender(client);
		colorprint("new %s client: %s\n",
			   is_sender ? "sender" : "receiver",
			   eis_client_get_name(client));

		eis_demo_client_new(server, client);
		if (!is_sender)
			server->nreceiver_clients++;

		/* insert sophisticated authentication here */
		eis_client_connect(client);
		colorprint("accepting client, creating new seat 'default'\n");
		_unref_(eis_seat) *seat = eis_client_new_seat(client, "default");
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_KEYBOARD);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_TOUCH);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_BUTTON);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_SCROLL);
		eis_seat_add(seat);
		/* Note: we don't have a ref to this seat ourselves anywhere */
		break;
		}
	case EIS_EVENT_CLIENT_DISCONNECT:
		{
		struct eis_client *client = eis_event_get_client(e);
		bool is_sender = eis_client_is_sender(client);

		if (!is_sender)
			server->nreceiver_clients--;

		colorprint("client %s disconnected\n", eis_client_get_name(client));
		eis_client_disconnect(client);
		eis_demo_client_unref(eis_demo_client_find(server, client));
		break;
		}
	case EIS_EVENT_SEAT_BIND:
		{
		struct eis_client *client = eis_event_get_client(e);
		struct eis_demo_client *democlient = eis_demo_client_find(server, client);
		assert(democlient);

		struct eis_seat *seat = eis_event_get_seat(e);

		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER)) {
			if (!democlient->ptr)
				democlient->ptr = add_device(server, client, seat, EIS_DEVICE_CAP_POINTER);
		} else {
			if (democlient->ptr) {
				eis_device_remove(democlient->ptr);
				democlient->ptr = eis_device_unref(democlient->ptr);
			}
		}

		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER_ABSOLUTE)) {
			if (!democlient->abs)
				democlient->abs = add_device(server, client, seat, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
		} else {
			if (democlient->abs) {
				eis_device_remove(democlient->abs);
				democlient->abs = eis_device_unref(democlient->abs);
			}
		}

		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_KEYBOARD)) {
			if (!democlient->kbd)
				democlient->kbd = add_device(server, client, seat, EIS_DEVICE_CAP_KEYBOARD);
		} else {
			if (democlient->kbd) {
				eis_device_remove(democlient->kbd);
				democlient->kbd = eis_device_unref(democlient->kbd);
			}
		}

		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_TOUCH)) {
			if (!democlient->touchscreen)
				democlient->touchscreen = add_device(server, client, seat, EIS_DEVICE_CAP_TOUCH);
		} else {
			if (democlient->touchscreen) {
				eis_device_remove(democlient->touchscreen);
				democlient->touchscreen = eis_device_unref(democlient->touchscreen);
			}
		}

		/* Special "Feature", if all caps are unbound remove the seat.
		 * This is a demo server after all, so let's demo this. */
		if (!eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER) &&
		    !eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER_ABSOLUTE) &&
		    !eis_event_seat_has_capability(e, EIS_DEVICE_CAP_KEYBOARD) &&
		    !eis_event_seat_has_capability(e, EIS_DEVICE_CAP_TOUCH))
			eis_seat_remove(seat);

		break;
		}
	case EIS_EVENT_DEVICE_CLOSED:
		{
			struct eis_client *client = eis_event_get_client(e);
			struct eis_demo_client *democlient = eis_demo_client_find(server, client);
			assert(democlient);

			struct eis_device *device = eis_event_get_device(e);
			eis_device_remove(device);

			if (democlient->ptr == device)
				democlient->ptr = NULL;

			if (democlient->abs == device)
				democlient->abs = NULL;

			if (democlient->kbd == device)
				democlient->kbd = NULL;

			if (democlient->touchscreen == device)
				democlient->touchscreen = NULL;

			eis_device_unref(device);
		}
		break;
	case EIS_EVENT_DEVICE_START_EMULATING:
		{
		struct eis_device *device = eis_event_get_device(e);
		colorprint("Device %s is ready to send events\n", eis_device_get_name(device));
		}
		break;
	case EIS_EVENT_DEVICE_STOP_EMULATING:
		{
		struct eis_device *device = eis_event_get_device(e);
		colorprint("Device %s will no longer send events\n", eis_device_get_name(device));
		}
		break;
	case EIS_EVENT_POINTER_MOTION:
		{
		colorprint("motion by %.2f/%.2f\n",
		       eis_event_pointer_get_dx(e),
		       eis_event_pointer_get_dy(e));
		}
		break;
	case EIS_EVENT_POINTER_MOTION_ABSOLUTE:
		{
		colorprint("absmotion to %.2f/%.2f\n",
		       eis_event_pointer_get_absolute_x(e),
		       eis_event_pointer_get_absolute_y(e));
		}
		break;
	case EIS_EVENT_BUTTON_BUTTON:
		{
		colorprint("button %u (%s)\n",
		       eis_event_button_get_button(e),
		       eis_event_button_get_is_press(e) ? "press" : "release");
		}
		break;
	case EIS_EVENT_SCROLL_DELTA:
		{
		colorprint("scroll %.2f/%.2f\n",
			eis_event_scroll_get_dx(e),
			eis_event_scroll_get_dy(e));
		}
		break;
	case EIS_EVENT_SCROLL_DISCRETE:
		{
		colorprint("scroll discrete %d/%d\n",
			eis_event_scroll_get_discrete_dx(e),
			eis_event_scroll_get_discrete_dy(e));
		}
		break;
	case EIS_EVENT_KEYBOARD_KEY:
		{
		handle_key(server,
			   eis_event_keyboard_get_key(e),
			   eis_event_keyboard_get_key_is_press(e));
		}
		break;
	case EIS_EVENT_TOUCH_DOWN:
	case EIS_EVENT_TOUCH_MOTION:
		{
		colorprint("touch %s %u %.2f/%.2f\n",
			   eis_event_get_type(e) == EIS_EVENT_TOUCH_DOWN ? "down" : "motion",
			   eis_event_touch_get_id(e),
			   eis_event_touch_get_x(e),
			   eis_event_touch_get_y(e));
		}
		break;
	case EIS_EVENT_TOUCH_UP:
		{
		colorprint("touch up %u\n", eis_event_touch_get_id(e));
		}
		break;
	case EIS_EVENT_FRAME:
		{
		colorprint("frame timestamp: %" PRIu64 "\n",
			   eis_event_get_time(e));
		}
		break;
	case EIS_EVENT_PONG:
		{
		colorprint("pong\n");
		}
		break;
	case EIS_EVENT_SYNC:
		{
		colorprint("sync\n");
		}
		break;
	default:
		/* This is a demo server and we abort to make it easy to debug
		 * a missing implementation of some new event.
		 *
		 * Do not do this in a real EIS implementation, unknown
		 * events must be passed to eis_event_unref() and otherwise
		 * ignored.
		 */
		abort();
	}
	return 0;
}

static void
usage(FILE *fp, const char *argv0)
{
	fprintf(fp,
		"Usage: %s [--verbose] [--uinput] [--socketpath=/path/to/socket] [--interval=1000]\n"
		"\n"
		"Start an EIS demo server. The server accepts all client connections\n"
		"and devices and prints any events from the client to stdout.\n"
		"\n"
		"Options:\n"
		" --socketpath	Use the given socket path. Default: $XDG_RUNTIME_DIR/eis-0\n"
		" --layout	Use the given XKB layout (requires libxkbcommon). Default: none\n"
		" --uinput	Set up each device as uinput device (this requires root)\n"
		" --interval    Interval in milliseconds between polling\n"
		" --verbose	Enable debugging output\n"
		"",
		argv0);
}

int main(int argc, char **argv)
{
	bool verbose = false;
	bool uinput = false;
	unsigned int interval = 1000;
	const char *layout = NULL;

	_cleanup_unlink_free_ char *socketpath = NULL;
	const char *xdg = getenv("XDG_RUNTIME_DIR");
	if (xdg)
		socketpath = xaprintf("%s/eis-0", xdg);

	while (true) {
		enum {
			OPT_VERBOSE,
			OPT_LAYOUT,
			OPT_SOCKETPATH,
			OPT_UINPUT,
			OPT_INTERVAL,
		};
		static struct option long_opts[] = {
			{"socketpath",	required_argument, 0, OPT_SOCKETPATH},
			{"layout",	required_argument, 0, OPT_LAYOUT},
			{"uinput",	no_argument, 0, OPT_UINPUT},
			{"verbose",	no_argument, 0, OPT_VERBOSE},
			{"interval",    required_argument, 0, OPT_INTERVAL},
			{"help",	no_argument, 0, 'h'},
			{NULL},
		};

		int optind = 0;
		int c = getopt_long(argc, argv, "h", long_opts, &optind);
		if (c == -1)
			break;

		switch(c) {
			case 'h':
				usage(stdout, argv[0]);
				return EXIT_SUCCESS;
			case OPT_SOCKETPATH:
				free(socketpath);
				socketpath = xstrdup(optarg);
				break;
			case OPT_LAYOUT:
				layout = optarg;
				break;
			case OPT_UINPUT:
				uinput = true;
				break;
			case OPT_VERBOSE:
				verbose = true;
				break;
			case OPT_INTERVAL:
				interval = atoi(optarg);
				break;
			default:
				usage(stderr, argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (socketpath == NULL) {
		fprintf(stderr, "No socketpath given and $XDG_RUNTIME_DIR is not set\n");
		return EXIT_FAILURE;
	}


	struct eis_demo_server server = {
		.layout = layout,
		.handler.handle_event = eis_demo_server_printf_handle_event,
	};

	list_init(&server.clients);

	if (uinput) {
		int rc = -ENOTSUP;
#if HAVE_LIBEVDEV
		rc = eis_demo_server_setup_uinput_handler(&server);
#endif
		if (rc != 0) {
			fprintf(stderr, "Failed to set up uinput handler: %s\n", strerror(-rc));
			return EXIT_FAILURE;
		}
	}

	_unref_(eis) *eis = eis_new(NULL);
	assert(eis);

	if (verbose) {
		eis_log_set_priority(eis, EIS_LOG_PRIORITY_DEBUG);
		eis_log_set_handler(eis, log_handler);
	}

	signal(SIGINT, sighandler);

	int rc = eis_setup_backend_socket(eis, socketpath);
	if (rc != 0) {
		fprintf(stderr, "init failed: %s\n", strerror(errno));
		return 1;
	}

	colorprint("waiting on %s\n", socketpath);

	struct pollfd fds = {
		.fd = eis_get_fd(eis),
		.events = POLLIN,
		.revents = 0,
	};

	int nevents;
	while (!stop && (nevents = poll(&fds, 1, interval)) > -1) {
		if (nevents == 0 && server.nreceiver_clients == 0)
			continue;

		uint64_t now = eis_now(eis);
		colorprint("now: %" PRIu64 "\n", now);

		eis_dispatch(eis);

		while (true) {
			_unref_(eis_event) *e = eis_get_event(eis);
			if (!e)
				break;

			int rc = server.handler.handle_event(&server, e);
			if (rc != 0)
				break;
		}

		struct eis_demo_client *democlient;
		const int interval = ms2us(12); /* events are 12ms apart */

		list_for_each(democlient, &server.clients, link) {
			if (eis_client_is_sender(democlient->client))
				continue;

			struct eis_device *ptr = democlient->ptr;
			struct eis_device *kbd = democlient->kbd;
			struct eis_device *abs = democlient->abs;
			struct eis_device *touchscreen = democlient->touchscreen;
			if (ptr) {
				colorprint("sending motion event\n");
				eis_device_pointer_motion(ptr, -1, 1);
				/* BTN_LEFT */
				colorprint("sending button event\n");
				eis_device_button_button(ptr, BTN_LEFT, true);
				eis_device_frame(ptr, now);
				now += interval;
				eis_device_button_button(ptr, BTN_LEFT, false);
				eis_device_frame(ptr, now);
				now += interval;
				colorprint("sending scroll events\n");
				eis_device_scroll_delta(ptr, 1, 1);
				eis_device_frame(ptr, now);
				now += interval;
				eis_device_scroll_discrete(ptr, 120, 120);
				eis_device_frame(ptr, now);
				now += interval;
			}

			if (kbd) {
				static int key = 0;
				colorprint("sending key event\n");
				eis_device_keyboard_key(kbd, KEY_Q + key, true); /* KEY_Q */
				eis_device_frame(kbd, now);
				now += interval;
				eis_device_keyboard_key(kbd, KEY_Q + key, false); /* KEY_Q */
				eis_device_frame(kbd, now);
				now += interval;
				key = (key + 1) % 6;
			}

			if (abs) {
				static int x, y;
				colorprint("sending abs event\n");
				eis_device_pointer_motion_absolute(abs, 150 + ++x, 150 - ++y);
				eis_device_frame(abs, now);
				now += interval;
			}

			if (touchscreen) {
				static int x, y;
				static int counter = 0;
				struct eis_touch *touch = democlient->touch;

				switch (counter++ % 5) {
					case 0:
						touch = eis_device_touch_new(touchscreen);
						if (touch) { /* NULL if client was disconnected internally already */
							colorprint("sending touch down event\n");
							eis_touch_down(touch, 100 + ++x, 200 - ++y);
							eis_device_frame(touchscreen, now);
							democlient->touch = touch;
						}
						break;
					case 4:
						if (touch) {
							colorprint("sending touch down event\n");
							eis_touch_up(touch);
							eis_device_frame(touchscreen, now);
							democlient->touch = eis_touch_unref(touch);
						}
						break;
					default:
						if (touch) {
							eis_touch_motion(touch, 100 + ++x, 200 - ++y);
							eis_device_frame(touchscreen, now);
						}
						break;
				}

			}
		}
	}

	struct eis_demo_client *democlient;
	list_for_each_safe(democlient, &server.clients, link) {
		eis_demo_client_unref(democlient);
	}

	return 0;
}
