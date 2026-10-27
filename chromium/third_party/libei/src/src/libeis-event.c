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

#include <stdarg.h>

#include "util-bits.h"
#include "util-object.h"
#include "util-macros.h"

#include "libeis-private.h"

static void
eis_event_destroy(struct eis_event *event)
{
	bool handled = false;

	switch (event->type) {
	case EIS_EVENT_CLIENT_CONNECT:
	case EIS_EVENT_CLIENT_DISCONNECT:
	case EIS_EVENT_SEAT_BIND:
	case EIS_EVENT_DEVICE_CLOSED:
	case EIS_EVENT_DEVICE_START_EMULATING:
	case EIS_EVENT_DEVICE_STOP_EMULATING:
	case EIS_EVENT_BUTTON_BUTTON:
	case EIS_EVENT_POINTER_MOTION:
	case EIS_EVENT_POINTER_MOTION_ABSOLUTE:
	case EIS_EVENT_SCROLL_DELTA:
	case EIS_EVENT_SCROLL_STOP:
	case EIS_EVENT_SCROLL_CANCEL:
	case EIS_EVENT_SCROLL_DISCRETE:
	case EIS_EVENT_KEYBOARD_KEY:
	case EIS_EVENT_TOUCH_DOWN:
	case EIS_EVENT_TOUCH_MOTION:
	case EIS_EVENT_TOUCH_UP:
	case EIS_EVENT_FRAME:
		handled = true;
		break;
	case EIS_EVENT_PONG:
		eis_ping_unref(event->pong.ping);
		handled = true;
		break;
	case EIS_EVENT_SYNC:
		eis_callback_unref(event->sync.callback);
		handled = true;
		break;
	}

	if (!handled)
		abort(); /* not yet implemented */

	event->device = eis_device_unref(event->device);
	event->seat = eis_seat_unref(event->seat);
	event->client = eis_client_unref(event->client);
}

static
OBJECT_IMPLEMENT_CREATE(eis_event);

struct eis_event *
eis_event_new_for_client(struct eis_client *client)
{
	struct eis *eis = eis_client_get_context(client);

	struct eis_event *e = eis_event_create(&eis->object);
	e->client = eis_client_ref(client);

	return e;
}

struct eis_event *
eis_event_new_for_seat(struct eis_seat *seat)
{
	struct eis_client *client = eis_seat_get_client(seat);
	struct eis *eis = eis_client_get_context(client);

	struct eis_event *e = eis_event_create(&eis->object);
	e->client = eis_client_ref(client);
	e->seat = eis_seat_ref(seat);

	return e;
}

struct eis_event *
eis_event_new_for_device(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);
	struct eis *eis = eis_client_get_context(client);

	struct eis_event *e = eis_event_create(&eis->object);
	e->client = eis_client_ref(client);
	e->seat = eis_seat_ref(eis_device_get_seat(device));
	e->device = eis_device_ref(device);

	return e;
}

/* this one is not public */
OBJECT_IMPLEMENT_REF(eis_event);

_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_event);
_public_
OBJECT_IMPLEMENT_GETTER(eis_event, type, enum eis_event_type);
_public_
OBJECT_IMPLEMENT_GETTER(eis_event, client, struct eis_client*);
_public_
OBJECT_IMPLEMENT_GETTER(eis_event, seat, struct eis_seat*);
_public_
OBJECT_IMPLEMENT_GETTER(eis_event, device, struct eis_device*);

static
OBJECT_IMPLEMENT_PARENT(eis_event, eis);

struct eis *
eis_event_get_context(struct eis_event *event)
{
	return eis_event_parent(event);
}

static inline bool
check_event_type(struct eis_event *event,
		 const char *function_name,
		 ...)
{
	bool rc = false;
	va_list args;
	unsigned int type_permitted;
	enum eis_event_type type = eis_event_get_type(event);

	va_start(args, function_name);
	type_permitted = va_arg(args, unsigned int);

	while (type_permitted != (unsigned int)-1) {
		if (type_permitted == type) {
			rc = true;
			break;
		}
		type_permitted = va_arg(args, unsigned int);
	}

	va_end(args);

	if (!rc)
		log_bug_client(eis_event_get_context(event),
			       "Invalid event type %s (%u) passed to %s()",
			       eis_event_type_to_string(type),
			       type, function_name);

	return rc;
}

#define require_event_type(event_, retval_, ...)	\
	if (!check_event_type(event_, __func__, __VA_ARGS__, -1)) \
		return retval_; \

_public_ uint64_t
eis_event_get_time(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_STOP,
			   EIS_EVENT_SCROLL_CANCEL,
			   EIS_EVENT_SCROLL_DISCRETE,
			   EIS_EVENT_KEYBOARD_KEY,
			   EIS_EVENT_TOUCH_DOWN,
			   EIS_EVENT_TOUCH_UP,
			   EIS_EVENT_TOUCH_MOTION,
			   EIS_EVENT_FRAME);

	return event->timestamp;
}

_public_ struct eis_ping *
eis_event_pong_get_ping(struct eis_event *event)
{
	require_event_type(event, NULL, EIS_EVENT_PONG);

	return event->pong.ping;
}

_public_ bool
eis_event_seat_has_capability(struct eis_event *event, enum eis_device_capability cap)
{
	require_event_type(event, false, EIS_EVENT_SEAT_BIND);

	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
	case EIS_DEVICE_CAP_KEYBOARD:
	case EIS_DEVICE_CAP_TOUCH:
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		return mask_all(event->bind.capabilities, cap);
	}
	return false;
}

_public_ uint32_t
eis_event_emulating_get_sequence(struct eis_event *event)
{
	require_event_type(event, 0, EIS_EVENT_DEVICE_START_EMULATING);

	return event->start_emulating.sequence;
}

_public_ double
eis_event_pointer_get_dx(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.dx;
}

_public_ double
eis_event_pointer_get_dy(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.dy;
}

_public_ double
eis_event_pointer_get_absolute_x(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.absx;
}

_public_ double
eis_event_pointer_get_absolute_y(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.absy;
}

_public_ uint32_t
eis_event_button_get_button(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.button;
}

_public_ bool
eis_event_button_get_is_press(struct eis_event *event)
{
	require_event_type(event, false,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);

	return event->pointer.button_is_press;
}

_public_ double
eis_event_scroll_get_dx(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);
	return event->pointer.sx;
}

_public_ double
eis_event_scroll_get_dy(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);
	return event->pointer.sy;
}

_public_ int32_t
eis_event_scroll_get_discrete_dx(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);
	return event->pointer.sdx;
}

_public_ int32_t
eis_event_scroll_get_discrete_dy(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_POINTER_MOTION,
			   EIS_EVENT_POINTER_MOTION_ABSOLUTE,
			   EIS_EVENT_BUTTON_BUTTON,
			   EIS_EVENT_SCROLL_DELTA,
			   EIS_EVENT_SCROLL_DISCRETE);
	return event->pointer.sdy;
}

_public_ bool
eis_event_scroll_get_stop_x(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_SCROLL_STOP,
			   EIS_EVENT_SCROLL_CANCEL);
	return event->pointer.stop_x;
}

_public_ bool
eis_event_scroll_get_stop_y(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_SCROLL_STOP,
			   EIS_EVENT_SCROLL_CANCEL);
	return event->pointer.stop_y;
}

_public_ uint32_t
eis_event_keyboard_get_key(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_KEYBOARD_KEY);

	return event->keyboard.key;
}

_public_ bool
eis_event_keyboard_get_key_is_press(struct eis_event *event)
{
	require_event_type(event, false,
			   EIS_EVENT_KEYBOARD_KEY);

	return event->keyboard.key_is_press;
}

_public_ uint32_t
eis_event_touch_get_id(struct eis_event *event)
{
	require_event_type(event, 0,
			   EIS_EVENT_TOUCH_DOWN,
			   EIS_EVENT_TOUCH_UP,
			   EIS_EVENT_TOUCH_MOTION);

	return event->touch.touchid;
}

_public_ double
eis_event_touch_get_x(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_TOUCH_DOWN,
			   EIS_EVENT_TOUCH_MOTION);

	return event->touch.x;
}

_public_ double
eis_event_touch_get_y(struct eis_event *event)
{
	require_event_type(event, 0.0,
			   EIS_EVENT_TOUCH_DOWN,
			   EIS_EVENT_TOUCH_MOTION);

	return event->touch.y;
}

_public_ bool
eis_event_touch_get_is_cancel(struct eis_event *event)
{
	require_event_type(event, false, EIS_EVENT_TOUCH_UP);

	return event->touch.is_cancel;
}
