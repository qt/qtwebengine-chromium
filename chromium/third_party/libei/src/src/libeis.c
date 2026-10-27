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

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "util-macros.h"
#include "util-object.h"
#include "util-sources.h"
#include "util-strings.h"
#include "util-time.h"

#include "libeis.h"
#include "libeis-private.h"
#include "eis-proto.h"

_Static_assert(sizeof(enum eis_device_capability) == sizeof(int), "Invalid enum size");
_Static_assert(sizeof(enum eis_keymap_type) == sizeof(int), "Invalid enum size");
_Static_assert(sizeof(enum eis_event_type) == sizeof(int), "Invalid enum size");
_Static_assert(sizeof(enum eis_log_priority) == sizeof(int), "Invalid enum size");

DEFINE_UNREF_CLEANUP_FUNC(brei_result);
DEFINE_UNREF_CLEANUP_FUNC(eis_callback);

static void
eis_destroy(struct eis *eis)
{
	struct eis_client *c;

	list_for_each_safe(c, &eis->clients, link) {
		eis_client_disconnect(c);
	}

	struct eis_event *e;
	while ((e = eis_get_event(eis)) != NULL)
		eis_event_unref(e);

	if (eis->backend_interface.destroy)
		eis->backend_interface.destroy(eis, eis->backend);
	sink_unref(eis->sink);
}

static
OBJECT_IMPLEMENT_CREATE(eis);
_public_
OBJECT_IMPLEMENT_REF(eis);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis);
_public_
OBJECT_IMPLEMENT_SETTER(eis, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(eis, user_data, void *);

_public_ struct eis *
eis_new(void *user_data)
{
	_unref_(eis) *eis = eis_create(NULL);

	list_init(&eis->clients);
	list_init(&eis->event_queue);

	eis_log_set_handler(eis, NULL);
	eis_log_set_priority(eis, EIS_LOG_PRIORITY_INFO);
	eis->sink = sink_new();
	if (!eis->sink)
		return NULL;

	eis->user_data = user_data;

	return steal(&eis);
}

_public_ int
eis_get_fd(struct eis *eis)
{
	return sink_get_fd(eis->sink);
}

struct eis *
eis_get_context(struct eis *eis)
{
	return eis;
}

_public_ void
eis_dispatch(struct eis *eis)
{
	sink_dispatch(eis->sink);
}

_public_ const char *
eis_event_type_to_string(enum eis_event_type type)
{
	switch(type) {
	CASE_RETURN_STRING(EIS_EVENT_CLIENT_CONNECT);
	CASE_RETURN_STRING(EIS_EVENT_CLIENT_DISCONNECT);
	CASE_RETURN_STRING(EIS_EVENT_SEAT_BIND);
	CASE_RETURN_STRING(EIS_EVENT_DEVICE_CLOSED);
	CASE_RETURN_STRING(EIS_EVENT_PONG);
	CASE_RETURN_STRING(EIS_EVENT_SYNC);
	CASE_RETURN_STRING(EIS_EVENT_DEVICE_START_EMULATING);
	CASE_RETURN_STRING(EIS_EVENT_DEVICE_STOP_EMULATING);
	CASE_RETURN_STRING(EIS_EVENT_POINTER_MOTION);
	CASE_RETURN_STRING(EIS_EVENT_POINTER_MOTION_ABSOLUTE);
	CASE_RETURN_STRING(EIS_EVENT_BUTTON_BUTTON);
	CASE_RETURN_STRING(EIS_EVENT_SCROLL_DELTA);
	CASE_RETURN_STRING(EIS_EVENT_SCROLL_STOP);
	CASE_RETURN_STRING(EIS_EVENT_SCROLL_CANCEL);
	CASE_RETURN_STRING(EIS_EVENT_SCROLL_DISCRETE);
	CASE_RETURN_STRING(EIS_EVENT_KEYBOARD_KEY);
	CASE_RETURN_STRING(EIS_EVENT_TOUCH_DOWN);
	CASE_RETURN_STRING(EIS_EVENT_TOUCH_UP);
	CASE_RETURN_STRING(EIS_EVENT_TOUCH_MOTION);
	CASE_RETURN_STRING(EIS_EVENT_FRAME);
	}

	return NULL;
}

static void
update_event_timestamp(struct eis_event *event, uint64_t time)
{
	switch (event->type) {
	case EIS_EVENT_POINTER_MOTION:
	case EIS_EVENT_POINTER_MOTION_ABSOLUTE:
	case EIS_EVENT_BUTTON_BUTTON:
	case EIS_EVENT_SCROLL_DELTA:
	case EIS_EVENT_SCROLL_STOP:
	case EIS_EVENT_SCROLL_CANCEL:
	case EIS_EVENT_SCROLL_DISCRETE:
	case EIS_EVENT_KEYBOARD_KEY:
	case EIS_EVENT_TOUCH_DOWN:
	case EIS_EVENT_TOUCH_UP:
	case EIS_EVENT_TOUCH_MOTION:
		if (event->timestamp != 0) {
			log_bug(eis_event_get_context(event),
				"Unexpected timestamp for event of type: %s",
				eis_event_type_to_string(event->type));
			return;
		}
		event->timestamp = time;
		break;
	default:
		log_bug(eis_event_get_context(event),
			"Unexpected event %s in pending queue event",
			eis_event_type_to_string(event->type));
		return;
	}
}

static void
eis_queue_event(struct eis_event *event)
{
	struct eis *eis = eis_event_get_context(event);
	struct eis_device *device = eis_event_get_device(event);
	struct list *queue = &eis->event_queue;
	const char *prefix = "";

	switch (event->type) {
	case EIS_EVENT_POINTER_MOTION:
	case EIS_EVENT_POINTER_MOTION_ABSOLUTE:
	case EIS_EVENT_BUTTON_BUTTON:
	case EIS_EVENT_SCROLL_DELTA:
	case EIS_EVENT_SCROLL_STOP:
	case EIS_EVENT_SCROLL_CANCEL:
	case EIS_EVENT_SCROLL_DISCRETE:
	case EIS_EVENT_KEYBOARD_KEY:
	case EIS_EVENT_TOUCH_DOWN:
	case EIS_EVENT_TOUCH_UP:
	case EIS_EVENT_TOUCH_MOTION:
		prefix = "pending ";
		queue = &device->pending_event_queue;
		break;
	case EIS_EVENT_FRAME: {
		/* silently discard empty frames */
		if (list_empty(&device->pending_event_queue))
			return;

		struct eis_event *pending;
		list_for_each_safe(pending, &device->pending_event_queue, link) {
			update_event_timestamp(pending, event->timestamp);
			list_remove(&pending->link);
			list_append(&eis->event_queue, &pending->link);
		}

		break;
	}
	default:
		if (device && !list_empty(&device->pending_event_queue))
			eis_queue_frame_event(device, eis_now(eis));
		break;
	}

	log_debug(eis, "queuing %sevent type %s (%u)",
		  prefix,
		  eis_event_type_to_string(event->type), event->type);

	list_append(queue, &event->link);
}

void
eis_queue_connect_event(struct eis_client *client)
{
	struct eis_event *e = eis_event_new_for_client(client);
	e->type = EIS_EVENT_CLIENT_CONNECT;
	eis_queue_event(e);
}

void
eis_queue_disconnect_event(struct eis_client *client)
{
	struct eis_event *e = eis_event_new_for_client(client);
	e->type = EIS_EVENT_CLIENT_DISCONNECT;
	eis_queue_event(e);
}

void
eis_queue_seat_bind_event(struct eis_seat *seat, uint32_t capabilities)
{
	struct eis_event *e = eis_event_new_for_seat(seat);
	e->type = EIS_EVENT_SEAT_BIND;
	e->bind.capabilities = capabilities;
	eis_queue_event(e);
}

void
eis_queue_device_closed_event(struct eis_device *device)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_DEVICE_CLOSED;
	eis_queue_event(e);
}

void
eis_queue_sync_event(struct eis_client *client, object_id_t newid, uint32_t version)
{
	struct eis_event *e = eis_event_new_for_client(client);
	e->type = EIS_EVENT_SYNC;
	e->sync.callback = eis_callback_new(client, newid, version);
	eis_queue_event(e);
}

void
eis_queue_pong_event(struct eis_client *client, struct eis_ping *ping)
{
	struct eis_event *e = eis_event_new_for_client(client);
	e->type = EIS_EVENT_PONG;
	e->pong.ping = eis_ping_ref(ping);
	eis_queue_event(e);
}

void
eis_queue_frame_event(struct eis_device *device, uint64_t time)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_FRAME;
	e->timestamp = time;
	eis_queue_event(e);
}

void
eis_queue_device_start_emulating_event(struct eis_device *device, uint32_t sequence)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_DEVICE_START_EMULATING;
	eis_queue_event(e);
}

void
eis_queue_device_stop_emulating_event(struct eis_device *device)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_DEVICE_STOP_EMULATING;
	eis_queue_event(e);
}

void
eis_queue_pointer_rel_event(struct eis_device *device,
			    double dx, double dy)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_POINTER_MOTION;
	e->pointer.dx = dx;
	e->pointer.dy = dy;
	eis_queue_event(e);
}

void
eis_queue_pointer_abs_event(struct eis_device *device,
			    double x, double y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_POINTER_MOTION_ABSOLUTE;
	e->pointer.absx = x;
	e->pointer.absy = y;
	eis_queue_event(e);
}

void
eis_queue_pointer_button_event(struct eis_device *device, uint32_t button,
			       bool is_press)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_BUTTON_BUTTON;
	e->pointer.button = button;
	e->pointer.button_is_press = is_press;

	eis_queue_event(e);
}

void
eis_queue_pointer_scroll_event(struct eis_device *device,
			       double x, double y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_SCROLL_DELTA;
	e->pointer.sx = x;
	e->pointer.sy = y;
	eis_queue_event(e);
}

void
eis_queue_pointer_scroll_discrete_event(struct eis_device *device,
					int32_t x, int32_t y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_SCROLL_DISCRETE;
	e->pointer.sdx = x;
	e->pointer.sdy = y;
	eis_queue_event(e);
}

void
eis_queue_pointer_scroll_stop_event(struct eis_device *device, bool x, bool y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_SCROLL_STOP;
	e->pointer.stop_x = x;
	e->pointer.stop_y = y;
	eis_queue_event(e);
}

void
eis_queue_pointer_scroll_cancel_event(struct eis_device *device, bool x, bool y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_SCROLL_CANCEL;
	e->pointer.stop_x = x;
	e->pointer.stop_y = y;
	eis_queue_event(e);
}

void
eis_queue_keyboard_key_event(struct eis_device *device, uint32_t key,
			     bool is_press)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_KEYBOARD_KEY;
	e->keyboard.key = key;
	e->keyboard.key_is_press = is_press;
	eis_queue_event(e);
}

void
eis_queue_touch_down_event(struct eis_device *device, uint32_t touchid,
			   double x, double y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_TOUCH_DOWN;
	e->touch.touchid = touchid,
	e->touch.x = x;
	e->touch.y = y;
	eis_queue_event(e);
}

void
eis_queue_touch_motion_event(struct eis_device *device, uint32_t touchid,
			     double x, double y)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_TOUCH_MOTION;
	e->touch.touchid = touchid,
	e->touch.x = x;
	e->touch.y = y;
	eis_queue_event(e);
}

void
eis_queue_touch_up_event(struct eis_device *device, uint32_t touchid)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_TOUCH_UP;
	e->touch.touchid = touchid;
	e->touch.is_cancel = false;
	eis_queue_event(e);
}

void
eis_queue_touch_cancel_event(struct eis_device *device, uint32_t touchid)
{
	struct eis_event *e = eis_event_new_for_device(device);
	e->type = EIS_EVENT_TOUCH_UP;
	e->touch.touchid = touchid,
	e->touch.is_cancel = true;
	eis_queue_event(e);
}

_public_ struct eis_event*
eis_get_event(struct eis *eis)
{
	if (list_empty(&eis->event_queue))
		return NULL;

	struct eis_event *e = list_first_entry(&eis->event_queue, e, link);
	list_remove(&e->link);

	if (e->type == EIS_EVENT_SYNC) {
		_unref_(eis_callback) *callback = steal(&e->sync.callback);
		log_debug(eis_event_get_context(e) ,
			  "object %#" PRIx64 ": connection sync done",
			  eis_callback_get_id(callback));

		int rc = eis_callback_event_done(callback, 0);
		_unref_(brei_result) *result = brei_result_new_from_neg_errno(rc);
		if (result) {
			log_debug(eis_event_get_context(e), ".... result is %d\n", rc);
			struct eis_client *client = eis_event_get_client(e);
			eis_client_disconnect_with_reason(client,
							  brei_result_get_reason(result),
							  brei_result_get_explanation(result));
		}
	}

	return e;
}

_public_ struct eis_event *
eis_peek_event(struct eis *eis)
{
	if (list_empty(&eis->event_queue))
		return NULL;

	struct eis_event *e = list_first_entry(&eis->event_queue, e, link);
	return eis_event_ref(e);
}

void
eis_add_client(struct eis *eis, struct eis_client *client)
{
	list_append(&eis->clients, &client->link);
}

_public_ void
eis_clock_set_now_func(struct eis *eis, eis_clock_now_func func)
{
	eis->clock_now = func;
}

_public_ uint64_t
eis_now(struct eis *eis)
{
	uint64_t ts = 0;

	if (eis->clock_now)
		ts = eis->clock_now(eis);
	else {
		int rc = now(&ts);

		if (rc < 0) {
			/* We should probably disconnect here but the chances of this
			 * happening are so slim it's not worth worrying about. Plus,
			 * if this fails we're likely to be inside ei_device_frame()
			 * so we should flush a frame event before disconnecting and... */
			log_error(eis, "clock_gettime failed: %s", strerror(-rc));
		}
	}

	return ts;
}
