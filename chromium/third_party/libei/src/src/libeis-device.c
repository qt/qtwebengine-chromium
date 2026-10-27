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

#include "util-macros.h"
#include "util-bits.h"
#include "util-io.h"
#include "util-time.h"

#include "libeis-private.h"
#include "eis-proto.h"

static_assert((int)EIS_DEVICE_TYPE_VIRTUAL == EIS_DEVICE_DEVICE_TYPE_VIRTUAL, "ABI mismatch");
static_assert((int)EIS_DEVICE_TYPE_PHYSICAL == EIS_DEVICE_DEVICE_TYPE_PHYSICAL, "ABI mismatch");

_public_
OBJECT_IMPLEMENT_REF(eis_keymap);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_keymap);
_public_
OBJECT_IMPLEMENT_GETTER(eis_keymap, type, enum eis_keymap_type);
_public_
OBJECT_IMPLEMENT_GETTER(eis_keymap, fd, int);
_public_
OBJECT_IMPLEMENT_GETTER(eis_keymap, size, size_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_keymap, device, struct eis_device *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_keymap, user_data, void *);
_public_
OBJECT_IMPLEMENT_SETTER(eis_keymap, user_data, void *);

static void
eis_keymap_destroy(struct eis_keymap *keymap)
{
	if (!keymap->assigned)
		eis_device_unref(keymap->device);
	xclose(keymap->fd);
}

static
OBJECT_IMPLEMENT_CREATE(eis_keymap);

_public_ struct eis_keymap *
eis_device_new_keymap(struct eis_device *device,
	       enum eis_keymap_type type, int fd, size_t size)
{
	switch (type) {
	case EIS_KEYMAP_TYPE_XKB:
		break;
	default:
		return NULL;
	}

	if (fd < 0 || size == 0)
		return NULL;

	int newfd = xdup(fd);
	if (newfd < 0)
		return NULL;

	struct eis_keymap *keymap = eis_keymap_create(NULL);
	keymap->device = eis_device_ref(device);
	keymap->fd = newfd;
	keymap->type = type;
	keymap->size = size;

	return keymap;
}

_public_ struct eis *
eis_device_get_context(struct eis_device *device)
{
	return eis_client_get_context(eis_device_get_client(device));
}

_public_ void
eis_keymap_add(struct eis_keymap *keymap)
{
	struct eis_device *device = eis_keymap_get_device(keymap);

	if (device->state != EIS_DEVICE_STATE_NEW) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device already (dis)connected", __func__);
		return;
	}

	if (device->keymap) {
		log_bug_client(eis_device_get_context(device),
			       "%s: only one keymap can only be assigned and only once", __func__);
		return;
	}

	/* New keymap holds ref to the device, for assigned keymap the device
	 * holds the ref to the keymap instead */
	device->keymap = eis_keymap_ref(keymap);
	keymap->assigned = true;
	eis_device_unref(keymap->device);
}

_public_ struct eis_keymap *
eis_device_keyboard_get_keymap(struct eis_device *device)
{
	return device->keymap;
}

static void
eis_device_destroy(struct eis_device *device)
{
	struct eis_region *r;
	struct eis_event *event;

	list_for_each_safe(r, &device->regions, link)
		eis_region_unref(r);

	/* regions_new does not own a ref */

	eis_keymap_unref(device->keymap);

	list_for_each_safe(event, &device->pending_event_queue, link) {
		list_remove(&event->link);
		eis_event_unref(event);
	}

	eis_pointer_unref(device->pointer);
	eis_touchscreen_unref(device->touchscreen);
	eis_keyboard_unref(device->keyboard);

	free(device->name);
}

_public_
OBJECT_IMPLEMENT_REF(eis_device);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_device);
static
OBJECT_IMPLEMENT_CREATE(eis_device);
static
OBJECT_IMPLEMENT_PARENT(eis_device, eis_seat);
_public_
OBJECT_IMPLEMENT_GETTER(eis_device, user_data, void *);
_public_
OBJECT_IMPLEMENT_SETTER(eis_device, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_device, name, const char *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_device, type, enum eis_device_type);
_public_
OBJECT_IMPLEMENT_GETTER(eis_device, width, uint32_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_device, height, uint32_t);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_device, proto_object, const struct brei_object *);

object_id_t
eis_device_get_id(struct eis_device *device)
{
	return device->proto_object.id;
}

_public_ struct eis_seat *
eis_device_get_seat(struct eis_device *device)
{
	return eis_device_parent(device);
}

_public_ struct eis_region *
eis_device_get_region(struct eis_device *device, size_t index)
{
	return list_nth_entry(struct eis_region, &device->regions, link, index);
}

_public_ struct eis_region *
eis_device_get_region_at(struct eis_device *device, double x, double y)
{
	struct eis_region *r;

	list_for_each(r, &device->regions, link) {
		if (eis_region_contains(r, x, y))
			return r;
	}
	return NULL;
}

_public_ struct eis_client *
eis_device_get_client(struct eis_device *device)
{
	return eis_seat_get_client(eis_device_get_seat(device));
}

static inline bool
eis_device_in_region(struct eis_device *device, double x, double y)
{
	struct eis_region *r;

	if (list_empty(&device->regions))
		return true;

	list_for_each(r, &device->regions, link) {
		if (eis_region_contains(r, x, y))
			return true;
	}

	return false;
}

static struct brei_result *
client_msg_release(struct eis_device *device)
{
	eis_device_closed_by_client(device);
	return NULL;
}

#define DISCONNECT_IF_RECEIVER_CONTEXT(device_) do { \
	struct eis_client *client_ = eis_device_get_client(device_); \
	if (!eis_client_is_sender(client_)) { \
		struct eis *_ctx = eis_client_get_context(client_); \
		log_bug_client(_ctx, "Invalid event from receiver ei context. Disconnecting client"); \
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_MODE, "Invalid event from receiver ei context"); \
	} \
} while(0)


static struct brei_result *
client_msg_start_emulating(struct eis_device *device, uint32_t serial, uint32_t sequence)
{
	struct brei_result *result = NULL;

	eis_client_update_client_serial(eis_device_get_client(device), serial);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	switch (device->state) {
	case EIS_DEVICE_STATE_DEAD:
	case EIS_DEVICE_STATE_CLOSED_BY_CLIENT:
	case EIS_DEVICE_STATE_NEW:
	case EIS_DEVICE_STATE_EMULATING:
		result = brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
					 "Invalid device state %u for a start_emulating event", device->state);
		break;
	case EIS_DEVICE_STATE_RESUMED:
		eis_queue_device_start_emulating_event(device, sequence);
		device->state = EIS_DEVICE_STATE_EMULATING;
		break;
	case EIS_DEVICE_STATE_PAUSED:
		/* race condition, that's fine */
		break;
	}

	return result;
}

static struct brei_result *
client_msg_stop_emulating(struct eis_device *device, uint32_t serial)
{
	struct brei_result *result = NULL;

	eis_client_update_client_serial(eis_device_get_client(device), serial);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	switch (device->state) {
	case EIS_DEVICE_STATE_DEAD:
	case EIS_DEVICE_STATE_CLOSED_BY_CLIENT:
	case EIS_DEVICE_STATE_NEW:
	case EIS_DEVICE_STATE_RESUMED:
		result = brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
					 "Invalid device state %u for a stop_emulating event", device->state);
		break;
	case EIS_DEVICE_STATE_EMULATING:
		eis_queue_device_stop_emulating_event(device);
		device->state = EIS_DEVICE_STATE_RESUMED;
		break;
	case EIS_DEVICE_STATE_PAUSED:
		/* race condition, that's fine */
		break;
	}

	return result;
}


static struct brei_result *
maybe_error_on_device_state(struct eis_device *device, const char *event_type)
{
	switch (device->state) {
	case EIS_DEVICE_STATE_RESUMED:
		/* could be a race condition, but it's unlikely unless the
		 * EIS implementation pauses and resumes immediately without
		 * giving the client a chance to catch up. So let's
		 * treat this as error until we see real issues.
		 */
		break;
	case EIS_DEVICE_STATE_PAUSED:
                /* we paused the device but the client sent us an event
                 * - most likely a race condition, so let's ignore it */
                return NULL;
	case EIS_DEVICE_STATE_EMULATING:
		return NULL;
	case EIS_DEVICE_STATE_NEW:
	case EIS_DEVICE_STATE_CLOSED_BY_CLIENT:
	case EIS_DEVICE_STATE_DEAD:
		break;
	}

	return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
			       "Invalid device state %u for a %s event", device->state, event_type);
}

static struct brei_result *
client_msg_frame(struct eis_device *device, uint32_t serial, uint64_t time)
{
	eis_client_update_client_serial(eis_device_get_client(device), serial);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_frame_event(device, time);
		return NULL;
	}

	return maybe_error_on_device_state(device, "frame");
}


static const struct eis_device_interface interface = {
	.release = client_msg_release,
	.start_emulating = client_msg_start_emulating,
	.stop_emulating = client_msg_stop_emulating,
	.frame = client_msg_frame,
};

const struct eis_device_interface *
eis_device_get_interface(struct eis_device *device)
{
	return &interface;
}

static struct brei_result *
client_msg_pointer_rel(struct eis_pointer *pointer, float x, float y)
{
	struct eis_device *device = eis_pointer_get_device(pointer);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Pointer rel event for non-pointer device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_pointer_rel_event(device, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer rel");
}

static struct brei_result *
client_msg_pointer_abs(struct eis_pointer_absolute *pointer, float x, float y)
{
	struct eis_device *device = eis_pointer_absolute_get_device(pointer);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER_ABSOLUTE)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Pointer abs event for non-pointer device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		if (eis_device_in_region(device, x, y))
			eis_queue_pointer_abs_event(device, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer abs");
}

static struct brei_result *
client_msg_button(struct eis_button *button, uint32_t btn, uint32_t state)
{
	struct eis_device *device = eis_button_get_device(button);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_BUTTON)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Button event for non-button device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_pointer_button_event(device, btn, !!state);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer button");
}

static struct brei_result *
client_msg_scroll(struct eis_scroll *scroll, float x, float y)
{
	struct eis_device *device = eis_scroll_get_device(scroll);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Scroll event for non-scroll device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_pointer_scroll_event(device, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer scroll");
}

static struct brei_result *
client_msg_scroll_discrete(struct eis_scroll *scroll, int32_t x, int32_t y)
{
	struct eis_device *device = eis_scroll_get_device(scroll);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Scroll discrete event for non-scroll device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_pointer_scroll_discrete_event(device, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer scroll discrete");
}

static struct brei_result *
client_msg_scroll_stop(struct eis_scroll *scroll,
		       uint32_t x, uint32_t y, uint32_t is_cancel)
{
	struct eis_device *device = eis_scroll_get_device(scroll);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Scroll stop event for non-scroll device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		if (is_cancel)
			eis_queue_pointer_scroll_cancel_event(device, !!x, !!y);
		else
			eis_queue_pointer_scroll_stop_event(device, !!x, !!y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "pointer scroll stop");
}

static struct brei_result *
client_msg_pointer_release(struct eis_pointer *pointer)
{
	struct eis_device *device = eis_pointer_get_device(pointer);
	eis_pointer_event_destroyed(device->pointer,
				    eis_client_get_next_serial(eis_device_get_client(device)));
	eis_pointer_unref(steal(&device->pointer));
	return 0;
}

static struct brei_result *
client_msg_pointer_absolute_release(struct eis_pointer_absolute *pointer)
{
	struct eis_device *device = eis_pointer_absolute_get_device(pointer);
	eis_pointer_absolute_event_destroyed(device->pointer_absolute,
					     eis_client_get_next_serial(eis_device_get_client(device)));
	eis_pointer_absolute_unref(steal(&device->pointer_absolute));
	return 0;
}

static struct brei_result *
client_msg_scroll_release(struct eis_scroll *scroll)
{
	struct eis_device *device = eis_scroll_get_device(scroll);
	eis_scroll_event_destroyed(device->scroll,
				   eis_client_get_next_serial(eis_device_get_client(device)));
	eis_scroll_unref(steal(&device->scroll));
	return 0;
}

static struct brei_result *
client_msg_button_release(struct eis_button *button)
{
	struct eis_device *device = eis_button_get_device(button);
	eis_button_event_destroyed(device->button,
				   eis_client_get_next_serial(eis_device_get_client(device)));
	eis_button_unref(steal(&device->button));
	return 0;
}

static const struct eis_pointer_interface pointer_interface = {
	.release = client_msg_pointer_release,
	.motion_relative = client_msg_pointer_rel,
};

static const struct eis_pointer_absolute_interface pointer_absolute_interface = {
	.release = client_msg_pointer_absolute_release,
	.motion_absolute = client_msg_pointer_abs,
};

static const struct eis_scroll_interface scroll_interface = {
	.release = client_msg_scroll_release,
	.scroll = client_msg_scroll,
	.scroll_discrete = client_msg_scroll_discrete,
	.scroll_stop = client_msg_scroll_stop,
};

static const struct eis_button_interface button_interface = {
	.release = client_msg_button_release,
	.button = client_msg_button,
};

const struct eis_pointer_interface *
eis_device_get_pointer_interface(struct eis_device *device)
{
	return &pointer_interface;
}

const struct eis_pointer_absolute_interface *
eis_device_get_pointer_absolute_interface(struct eis_device *device)
{
	return &pointer_absolute_interface;
}

const struct eis_scroll_interface *
eis_device_get_scroll_interface(struct eis_device *device)
{
	return &scroll_interface;
}

const struct eis_button_interface *
eis_device_get_button_interface(struct eis_device *device)
{
	return &button_interface;
}

static struct brei_result *
client_msg_keyboard_key(struct eis_keyboard *keyboard, uint32_t key, uint32_t state)
{
	struct eis_device *device = eis_keyboard_get_device(keyboard);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_KEYBOARD)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Key event for non-keyboard device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_keyboard_key_event(device, key, !!state);
		return NULL;
	}

	return maybe_error_on_device_state(device, "key");
}

static struct brei_result *
client_msg_keyboard_release(struct eis_keyboard *keyboard)
{
	struct eis_device *device = eis_keyboard_get_device(keyboard);
	eis_keyboard_event_destroyed(device->keyboard,
				    eis_client_get_next_serial(eis_device_get_client(device)));
	eis_keyboard_unref(steal(&device->keyboard));
	return 0;
}

static const struct eis_keyboard_interface keyboard_interface = {
	.release = client_msg_keyboard_release,
	.key = client_msg_keyboard_key,
};

const struct eis_keyboard_interface *
eis_device_get_keyboard_interface(struct eis_device *device)
{
	return &keyboard_interface;
}

static struct brei_result *
client_msg_touch_down(struct eis_touchscreen *touchscreen,
		      uint32_t touchid, float x, float y)
{
	struct eis_device *device = eis_touchscreen_get_device(touchscreen);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_TOUCH)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Touch down event for non-touch device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_touch_down_event(device, touchid, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "touch down");
}

static struct brei_result *
client_msg_touch_motion(struct eis_touchscreen *touchscreen,
			uint32_t touchid, float x, float y)
{
	struct eis_device *device = eis_touchscreen_get_device(touchscreen);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_TOUCH)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Touch motion event for non-touch device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_touch_motion_event(device, touchid, x, y);
		return NULL;
	}

	return maybe_error_on_device_state(device, "touch motion");
}

static struct brei_result *
client_msg_touch_up(struct eis_touchscreen *touchscreen, uint32_t touchid)
{
	struct eis_device *device = eis_touchscreen_get_device(touchscreen);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_TOUCH)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Touch up event for non-touch device");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_touch_up_event(device, touchid);
		return NULL;
	}

	return maybe_error_on_device_state(device, "touch up");
}

static struct brei_result *
client_msg_touch_cancel(struct eis_touchscreen *touchscreen, uint32_t touchid)
{
	struct eis_device *device = eis_touchscreen_get_device(touchscreen);

	DISCONNECT_IF_RECEIVER_CONTEXT(device);

	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_TOUCH)) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Touch cancel event for non-touch device");
	}

	struct eis_client *client = eis_device_get_client(device);
	if (client->interface_versions.ei_touchscreen < EIS_TOUCHSCREEN_EVENT_CANCEL_SINCE_VERSION) {
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Touch cancel event for touchscreen version v1");
	}

	if (device->state == EIS_DEVICE_STATE_EMULATING) {
		eis_queue_touch_cancel_event(device, touchid);
		return NULL;
	}

	return maybe_error_on_device_state(device, "touch cancel");
}

static struct brei_result *
client_msg_touchscreen_release(struct eis_touchscreen *touchscreen)
{
	struct eis_device *device = eis_touchscreen_get_device(touchscreen);
	eis_touchscreen_event_destroyed(device->touchscreen,
					eis_client_get_next_serial(eis_device_get_client(device)));
	eis_touchscreen_unref(steal(&device->touchscreen));
	return NULL;
}

static const struct eis_touchscreen_interface touchscreen_interface = {
	.release = client_msg_touchscreen_release,
	.down = client_msg_touch_down,
	.motion = client_msg_touch_motion,
	.up = client_msg_touch_up,
	.cancel = client_msg_touch_cancel,
};

const struct eis_touchscreen_interface *
eis_device_get_touchscreen_interface(struct eis_device *device)
{
	return &touchscreen_interface;
}

_public_ struct eis_device *
eis_seat_new_device(struct eis_seat *seat)
{
	struct eis_device *device = eis_device_create(&seat->object);
	struct eis_client *client = eis_seat_get_client(seat);

	device->proto_object.id = eis_client_get_new_id(client);
	device->proto_object.implementation = device;
	device->proto_object.interface = &eis_device_proto_interface;
	device->proto_object.version = client->interface_versions.ei_device;
	assert(device->proto_object.version != 0);
	list_init(&device->proto_object.link);

	device->name = xstrdup("unnamed device");
	device->capabilities = 0;
	device->state = EIS_DEVICE_STATE_NEW;
	device->type = EIS_DEVICE_TYPE_VIRTUAL;
	list_init(&device->regions);
	list_init(&device->regions_new);
	list_init(&device->pending_event_queue);

	list_append(&seat->devices, &device->link);

	return eis_device_ref(device);
}

_public_ void
eis_device_configure_type(struct eis_device *device, enum eis_device_type type)
{
	if (device->state != EIS_DEVICE_STATE_NEW)
		return;

	switch (type) {
		case EIS_DEVICE_TYPE_VIRTUAL:
		case EIS_DEVICE_TYPE_PHYSICAL:
			break;
		default:
			log_bug_client(eis_device_get_context(device), "Invalid device type %u", type);
			return;
	}

	device->type = type;
}

_public_ void
eis_device_configure_name(struct eis_device *device, const char *name)
{
	if (device->state != EIS_DEVICE_STATE_NEW)
		return;

	free(device->name);
	device->name = xstrdup(name);
}

_public_ void
eis_device_configure_capability(struct eis_device *device, enum eis_device_capability cap)
{
	if (device->state != EIS_DEVICE_STATE_NEW)
		return;

	if (!eis_seat_has_capability(eis_device_get_seat(device), cap))
		return;

	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
	case EIS_DEVICE_CAP_KEYBOARD:
	case EIS_DEVICE_CAP_TOUCH:
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		mask_add(device->capabilities, cap);
		break;
	}
}

_public_ void
eis_device_configure_size(struct eis_device *device, uint32_t width, uint32_t height)
{
	if (device->type != EIS_DEVICE_TYPE_PHYSICAL) {
		log_bug_client(eis_device_get_context(device), "Device type physical required for size");
		return;
	}

	if (width > 2000 || height > 2000)
		log_warn(eis_device_get_context(device), "Suspicious device size: %ux%umm", width, height);

	device->width = width;
	device->height = height;
}

_public_ void
eis_device_add(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);
	struct eis_seat *seat = eis_device_get_seat(device);

	if (device->state != EIS_DEVICE_STATE_NEW) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device already (dis)connected", __func__);
		return;
	}

	if (!device->capabilities) {
		log_bug_client(eis_device_get_context(device),
			       "%s: adding device without capabilities", __func__);
	}

	device->state = EIS_DEVICE_STATE_PAUSED;
	eis_client_register_object(client, &device->proto_object);
	eis_seat_event_device(seat, device->proto_object.id, device->proto_object.version);
	int rc = eis_device_event_name(device, device->name);
	if (rc < 0)
		goto out;

	rc = eis_device_event_device_type(device, device->type);
	if (rc < 0)
		goto out;

	if (device->type == EIS_DEVICE_TYPE_PHYSICAL) {
		rc = eis_device_event_dimensions(device, device->width, device->height);
		if (rc < 0)
			goto out;
	}
	if (device->type == EIS_DEVICE_TYPE_VIRTUAL) {
		struct eis_region *r;
		list_for_each(r, &device->regions, link) {
			if (r->mapping_id) {
				if (client->interface_versions.ei_device >= EIS_DEVICE_EVENT_REGION_MAPPING_ID_SINCE_VERSION) {
					rc = eis_device_event_region_mapping_id(device, r->mapping_id);
					if (rc < 0)
						goto out;
				} else {
					/* If our client doesn't support mapping_id, drop it */
					free(r->mapping_id);
					r->mapping_id = NULL;
				}
			}
			rc = eis_device_event_region(device, r->x, r->y, r->width, r->height, r->physical_scale);
			if (rc < 0)
				goto out;
		}
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER)) {
		device->pointer = eis_pointer_new(device);
		rc = eis_device_event_interface(device, eis_pointer_get_id(device->pointer),
						EIS_POINTER_INTERFACE_NAME,
						eis_pointer_get_version(device->pointer));
		if (rc < 0)
			goto out;
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER_ABSOLUTE)) {
		device->pointer_absolute = eis_pointer_absolute_new(device);
		rc = eis_device_event_interface(device, eis_pointer_absolute_get_id(device->pointer_absolute),
						EIS_POINTER_ABSOLUTE_INTERFACE_NAME,
						eis_pointer_absolute_get_version(device->pointer_absolute));
		if (rc < 0)
			goto out;
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		device->scroll = eis_scroll_new(device);
		rc = eis_device_event_interface(device, eis_scroll_get_id(device->scroll),
						EIS_SCROLL_INTERFACE_NAME,
						eis_scroll_get_version(device->scroll));
		if (rc < 0)
			goto out;
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_BUTTON)) {
		device->button = eis_button_new(device);
		rc = eis_device_event_interface(device, eis_button_get_id(device->button),
						EIS_BUTTON_INTERFACE_NAME,
						eis_button_get_version(device->button));
		if (rc < 0)
			goto out;
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_KEYBOARD)) {
		device->keyboard = eis_keyboard_new(device);
		rc = eis_device_event_interface(device, eis_keyboard_get_id(device->keyboard),
						EIS_KEYBOARD_INTERFACE_NAME,
						eis_keyboard_get_version(device->keyboard));
		if (rc < 0)
			goto out;

		if (device->keymap)
			rc = eis_keyboard_event_keymap(device->keyboard, device->keymap->type,
						       device->keymap->size, device->keymap->fd);
		if (rc < 0)
			goto out;
	}
	if (eis_device_has_capability(device, EIS_DEVICE_CAP_TOUCH)) {
		device->touchscreen = eis_touchscreen_new(device);
		rc = eis_device_event_interface(device, eis_touchscreen_get_id(device->touchscreen),
						EIS_TOUCHSCREEN_INTERFACE_NAME,
						eis_touchscreen_get_version(device->touchscreen));
		if (rc < 0)
			goto out;
	}

	rc = eis_device_event_done(device);
out:
	if (rc < 0) {
		log_error(eis_client_get_context(client), "Failed to add device, disconnecting client");
		eis_client_disconnect(client);
	}
	return;
}

_public_ void
eis_device_remove(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state == EIS_DEVICE_STATE_DEAD)
		return;

	if (device->state == EIS_DEVICE_STATE_EMULATING &&
	    !eis_client_is_sender(eis_device_get_client(device)))
		eis_device_stop_emulating(device);

	if (device->pointer) {
		eis_pointer_event_destroyed(device->pointer, eis_client_get_next_serial(client));
		eis_pointer_unref(steal(&device->pointer));
	}
	if (device->pointer_absolute) {
		eis_pointer_absolute_event_destroyed(device->pointer_absolute, eis_client_get_next_serial(client));
		eis_pointer_absolute_unref(steal(&device->pointer_absolute));
	}
	if (device->button) {
		eis_button_event_destroyed(device->button, eis_client_get_next_serial(client));
		eis_button_unref(steal(&device->button));
	}
	if (device->scroll) {
		eis_scroll_event_destroyed(device->scroll, eis_client_get_next_serial(client));
		eis_scroll_unref(steal(&device->scroll));
	}
	if (device->touchscreen) {
		eis_touchscreen_event_destroyed(device->touchscreen, eis_client_get_next_serial(client));
		eis_touchscreen_unref(steal(&device->touchscreen));
	}
	if (device->keyboard) {
		eis_keyboard_event_destroyed(device->keyboard, eis_client_get_next_serial(client));
		eis_keyboard_unref(steal(&device->keyboard));
	}

	if (device->state != EIS_DEVICE_STATE_NEW)
		eis_device_event_destroyed(device, eis_client_get_next_serial(client));

	struct eis_event *event;
	list_for_each_safe(event, &device->pending_event_queue, link) {
		list_remove(&event->link);
		eis_event_unref(event);
	}

	device->state = EIS_DEVICE_STATE_DEAD;
	eis_client_unregister_object(client, &device->proto_object);
	list_remove(&device->link);
	eis_device_unref(device);
}

_public_ bool
eis_device_has_capability(struct eis_device *device,
			  enum eis_device_capability cap)
{
	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
	case EIS_DEVICE_CAP_KEYBOARD:
	case EIS_DEVICE_CAP_TOUCH:
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		return mask_all(device->capabilities, cap);
	}
	return false;
}

static void
eis_device_frame_now(struct eis_device *device)
{
	uint64_t now = eis_now(eis_device_get_context(device));

	eis_device_frame(device, now);
}

static void
_flush_frame(struct eis_device *device, const char *func)
{
	if (device->send_frame_event) {
		log_bug_client(eis_device_get_context(device),
			       "%s: missing call to eis_device_frame()", func);
		eis_device_frame_now(device);
	}
}
#define eis_device_flush_frame(d_) _flush_frame(d_, __func__)

_public_ void
eis_device_start_emulating(struct eis_device *device, uint32_t sequence)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state != EIS_DEVICE_STATE_RESUMED)
		return;

	assert(!device->send_frame_event);

	device->state = EIS_DEVICE_STATE_EMULATING;

	eis_device_event_start_emulating(device, eis_client_get_next_serial(client), sequence);
}

_public_ void
eis_device_stop_emulating(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	eis_device_flush_frame(device);

	device->state = EIS_DEVICE_STATE_RESUMED;

	eis_device_event_stop_emulating(device, eis_client_get_next_serial(client));
}

_public_ void
eis_device_pointer_motion(struct eis_device *device,
			 double x, double y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a pointer", __func__);
		return;
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	device->send_frame_event = true;

	eis_pointer_event_motion_relative(device->pointer, x, y);
}

_public_ void
eis_device_pointer_motion_absolute(struct eis_device *device,
				  double x, double y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_POINTER_ABSOLUTE)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not an absolute pointer", __func__);
		return;
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	if (!eis_device_in_region(device, x, y))
		return;

	device->send_frame_event = true;

	eis_pointer_absolute_event_motion_absolute(device->pointer_absolute, x, y);
}

_public_ void
eis_device_button_button(struct eis_device *device,
			 uint32_t button, bool is_press)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_BUTTON)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a button device", __func__);
		return;
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	/* Ignore anything < BTN_MOUSE. Avoids the common error of sending
	 * numerical buttons instead of BTN_LEFT and friends. */
	if (button < 0x110) {
		log_bug_client(eis_device_get_context(device),
			       "%s: button code must be one of BTN_*", __func__);
		return;
	}

	device->send_frame_event = true;

	eis_button_event_button(device->button, button, is_press);
}

static inline void
eis_device_resume_scrolling(struct eis_device *device, double x, double y)
{
	if (x) {
		device->scroll_state.x_is_stopped = false;
		device->scroll_state.x_is_cancelled = false;
	}
	if (y) {
		device->scroll_state.y_is_stopped = false;
		device->scroll_state.y_is_cancelled = false;
	}
}

_public_ void
eis_device_scroll_delta(struct eis_device *device, double x, double y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a scroll device", __func__);
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	eis_device_resume_scrolling(device, x, y);

	device->send_frame_event = true;

	eis_scroll_event_scroll(device->scroll, x, y);
}

_public_ void
eis_device_scroll_stop(struct eis_device *device, bool x, bool y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a scroll device", __func__);
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	/* Filter out duplicate scroll stop requests */
	if (x && !device->scroll_state.x_is_stopped)
		device->scroll_state.x_is_stopped = true;
	else
		x = false;

	if (y && !device->scroll_state.y_is_stopped)
		device->scroll_state.y_is_stopped = true;
	else
		y = false;

	if (x || y) {
		device->send_frame_event = true;
		eis_scroll_event_scroll_stop(device->scroll, x, y, false);
	}
}

_public_ void
eis_device_scroll_cancel(struct eis_device *device, bool x, bool y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a scroll device", __func__);
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	/* Filter out duplicate scroll cancelled requests */
	if (x && !device->scroll_state.x_is_cancelled) {
		device->scroll_state.x_is_stopped = true;
		device->scroll_state.x_is_cancelled = true;
	} else {
		x = false;
	}

	if (y && !device->scroll_state.y_is_cancelled) {
		device->scroll_state.y_is_stopped = true;
		device->scroll_state.y_is_cancelled = true;
	} else {
		y = false;
	}

	if (x || y) {
		device->send_frame_event = true;
		eis_scroll_event_scroll_stop(device->scroll, x, y, true);
	}
}

_public_ void
eis_device_scroll_discrete(struct eis_device *device, int32_t x, int32_t y)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_SCROLL)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a scroll device", __func__);
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	if (abs(x) == 1 || abs(y) == 1) {
		log_bug_client(eis_device_get_context(device),
			       "%s: suspicious discrete event value 1, did you mean 120?", __func__);
	}

	eis_device_resume_scrolling(device, x, y);

	device->send_frame_event = true;

	eis_scroll_event_scroll_discrete(device->scroll, x, y);
}

_public_ void
eis_device_keyboard_key(struct eis_device *device,
		       uint32_t key, bool is_press)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_KEYBOARD)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a keyboard", __func__);
		return;
	}

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	device->send_frame_event = true;

	eis_keyboard_event_key(device->keyboard, key, is_press);
}

_public_
OBJECT_IMPLEMENT_REF(eis_touch);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_touch);
_public_
OBJECT_IMPLEMENT_GETTER(eis_touch, device, struct eis_device*);
_public_
OBJECT_IMPLEMENT_GETTER(eis_touch, user_data, void *);
_public_
OBJECT_IMPLEMENT_SETTER(eis_touch, user_data, void *);

static void
eis_touch_destroy(struct eis_touch *touch)
{
	if (touch->state == TOUCH_IS_DOWN)
		eis_touch_up(touch);
	/* Enforce a frame, otherwise we're just pending. If the client
	 * doesn't want this, it needs to eis_touch_up() */
	eis_device_frame_now(touch->device);
	eis_device_unref(touch->device);
}

static
OBJECT_IMPLEMENT_CREATE(eis_touch);

_public_ struct eis_touch *
eis_device_touch_new(struct eis_device *device)
{
	static uint32_t tracking_id = 0;

	/* Not using the device as parent object because we need a ref
	 * to it */
	struct eis_touch *touch = eis_touch_create(NULL);

	touch->device = eis_device_ref(device);
	touch->state = TOUCH_IS_NEW;
	touch->tracking_id = ++tracking_id;

	return touch;
}

_public_ void
eis_touch_down(struct eis_touch *touch, double x, double y)
{
	struct eis_device *device = eis_touch_get_device(touch);

	if (touch->state != TOUCH_IS_NEW) {
		log_bug_client(eis_device_get_context(device),
			       "%s: touch %u already down or up", __func__, touch->tracking_id);
		return;
	}

	if (!eis_device_in_region(device, x, y)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: touch %u has invalid x/y coordinates", __func__, touch->tracking_id);
		touch->state = TOUCH_IS_UP;
		return;
	}

	touch->state = TOUCH_IS_DOWN;
	device->send_frame_event = true;

	eis_touchscreen_event_down(device->touchscreen, touch->tracking_id, x, y);
}

_public_ void
eis_touch_motion(struct eis_touch *touch, double x, double y)
{
	if (touch->state != TOUCH_IS_DOWN)
		return;

	struct eis_device *device = eis_touch_get_device(touch);
	if (!eis_device_in_region(device, x, y)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: touch %u has invalid x/y coordinates", __func__, touch->tracking_id);
		eis_touch_up(touch);
		return;
	}

	device->send_frame_event = true;

	eis_touchscreen_event_motion(device->touchscreen, touch->tracking_id, x, y);
}

_public_ void
eis_touch_up(struct eis_touch *touch)
{
	struct eis_device *device = eis_touch_get_device(touch);

	if (touch->state != TOUCH_IS_DOWN) {
		log_bug_client(eis_device_get_context(device),
			       "%s: touch %u is not currently down", __func__, touch->tracking_id);
		return;
	}

	touch->state = TOUCH_IS_UP;
	device->send_frame_event = true;

	eis_touchscreen_event_up(device->touchscreen, touch->tracking_id);
}

_public_ void
eis_touch_cancel(struct eis_touch *touch)
{
	struct eis_device *device = eis_touch_get_device(touch);

	if (touch->state != TOUCH_IS_DOWN) {
		log_bug_client(eis_device_get_context(device),
			       "%s: touch %u is not currently down", __func__, touch->tracking_id);
		return;
	}

	touch->state = TOUCH_IS_UP;
	device->send_frame_event = true;

	struct eis_client *client = eis_device_get_client(device);
	if (client->interface_versions.ei_touchscreen >= EIS_TOUCHSCREEN_EVENT_CANCEL_SINCE_VERSION)
		eis_touchscreen_event_cancel(device->touchscreen, touch->tracking_id);
	else
		eis_touchscreen_event_up(device->touchscreen, touch->tracking_id);
}

_public_ void
eis_device_frame(struct eis_device *device, uint64_t time)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state != EIS_DEVICE_STATE_EMULATING)
		return;

	if (!device->send_frame_event)
		return;

	device->send_frame_event = false;

	eis_device_event_frame(device, eis_client_get_next_serial(client),
			       time);
}

void
eis_device_closed_by_client(struct eis_device *device)
{
	switch (device->state) {
	case EIS_DEVICE_STATE_DEAD:
	case EIS_DEVICE_STATE_CLOSED_BY_CLIENT:
		/* libei bug, ignore */
		break;
	case EIS_DEVICE_STATE_EMULATING:
		if (!eis_client_is_sender(eis_device_get_client(device)))
			eis_queue_device_stop_emulating_event(device);
		_fallthrough_;
	case EIS_DEVICE_STATE_NEW:
	case EIS_DEVICE_STATE_PAUSED:
	case EIS_DEVICE_STATE_RESUMED:
		eis_queue_device_closed_event(device);
		device->state = EIS_DEVICE_STATE_CLOSED_BY_CLIENT;
		break;
	}
}

_public_ void
eis_device_pause(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state != EIS_DEVICE_STATE_RESUMED)
		return;

	device->state = EIS_DEVICE_STATE_PAUSED;
	eis_device_event_paused(device, eis_client_get_next_serial(client));
}

_public_ void
eis_device_resume(struct eis_device *device)
{
	struct eis_client *client = eis_device_get_client(device);

	if (device->state != EIS_DEVICE_STATE_PAUSED)
		return;

	device->state = EIS_DEVICE_STATE_RESUMED;
	eis_device_event_resumed(device, eis_client_get_next_serial(client));
}

_public_ void
eis_device_keyboard_send_xkb_modifiers(struct eis_device *device, uint32_t depressed,
				       uint32_t latched, uint32_t locked, uint32_t group)
{
	if (!eis_device_has_capability(device, EIS_DEVICE_CAP_KEYBOARD)) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device is not a keyboard", __func__);
		return;
	}

	eis_keyboard_event_modifiers(device->keyboard,
				     eis_client_get_next_serial(eis_device_get_client(device)),
				     depressed, locked, latched, group);
}
