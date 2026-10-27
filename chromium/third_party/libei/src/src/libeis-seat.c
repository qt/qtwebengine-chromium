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

#include "util-macros.h"
#include "util-bits.h"
#include "util-strings.h"
#include "libeis-private.h"
#include "eis-proto.h"

static void
eis_seat_destroy(struct eis_seat *seat)
{
	struct eis_device *d;

	/* We expect those to have been removed already*/
	list_for_each(d, &seat->devices, link) {
		assert(!"device list not empty");
	}
	free(seat->name);
}

_public_
OBJECT_IMPLEMENT_REF(eis_seat);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_seat);
static
OBJECT_IMPLEMENT_CREATE(eis_seat);
static
OBJECT_IMPLEMENT_PARENT(eis_seat, eis_client);
_public_
OBJECT_IMPLEMENT_GETTER(eis_seat, user_data, void *);
_public_
OBJECT_IMPLEMENT_SETTER(eis_seat, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_seat, name, const char *);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_seat, proto_object, const struct brei_object *);

object_id_t
eis_seat_get_id(struct eis_seat *seat) {
	return seat->proto_object.id;
}

uint32_t
eis_seat_get_version(struct eis_seat *seat) {
	return seat->proto_object.version;
}

_public_ struct eis_client *
eis_seat_get_client(struct eis_seat *seat)
{
	return eis_seat_parent(seat);
}

static struct brei_result *
client_msg_release(struct eis_seat *seat)
{
	/* There is no public API in libei to remove a seat, and there's no
	 * public API in libeis to know the client has released the seat. it's
	 * too niche to care about. So here we simply pretend it's bound to 0
	 * and remove it, that should do the trick.
	 */
	eis_seat_drop(seat);
	return NULL;
}

static struct brei_result *
client_msg_bind(struct eis_seat *seat, uint64_t caps)
{
	uint32_t capabilities = 0;

	if (caps & ~seat->capabilities.proto_mask)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_VALUE,
				       "Invalid capabilities %#" PRIx64, caps);

	/* Convert from protocol capabilities to our C API capabilities */
	if (caps & bit(EIS_POINTER_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_POINTER;
	if (caps & bit(EIS_POINTER_ABSOLUTE_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_POINTER_ABSOLUTE;
	if (caps & bit(EIS_KEYBOARD_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_KEYBOARD;
	if (caps & bit(EIS_TOUCHSCREEN_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_TOUCH;
	if (caps & bit(EIS_BUTTON_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_BUTTON;
	if (caps & bit(EIS_SCROLL_INTERFACE_INDEX))
		capabilities |= EIS_DEVICE_CAP_SCROLL;

	eis_seat_bind(seat, capabilities);

	return NULL;
}

static const struct eis_seat_interface interface = {
	.release = client_msg_release,
	.bind = client_msg_bind,
};

const struct eis_seat_interface *
eis_seat_get_interface(struct eis_seat *seat)
{
	return &interface;
}

_public_ struct eis *
eis_seat_get_context(struct eis_seat *seat)
{
	return eis_client_get_context(eis_seat_get_client(seat));
}

_public_ struct eis_seat *
eis_client_new_seat(struct eis_client *client, const char *name)
{
	struct eis_seat *seat = eis_seat_create(&client->object);

	seat->proto_object.id = eis_client_get_new_id(client);
	seat->proto_object.implementation = seat;
	seat->proto_object.interface = &eis_seat_proto_interface;
	seat->proto_object.version = client->interface_versions.ei_seat;
	list_init(&seat->proto_object.link);

	seat->state = EIS_SEAT_STATE_PENDING;
	seat->name = xstrdup(name);
	list_init(&seat->devices);
	/* seat is owned by caller until it's added */
	list_append(&client->seats_pending, &seat->link);

	return seat;
}

_public_ void
eis_seat_add(struct eis_seat *seat)
{
	struct eis_client *client = eis_seat_get_client(seat);

	switch (seat->state) {
	case EIS_SEAT_STATE_PENDING:
		break;
	case EIS_SEAT_STATE_ADDED:
	case EIS_SEAT_STATE_BOUND:
	case EIS_SEAT_STATE_REMOVED:
	case EIS_SEAT_STATE_REMOVED_INTERNALLY:
	case EIS_SEAT_STATE_DEAD:
		log_bug_client(eis_client_get_context(client),
			       "%s: seat already added/removed/dead", __func__);
		return;
	}

	seat->state = EIS_SEAT_STATE_ADDED;
	eis_client_register_object(client, &seat->proto_object);
	eis_client_add_seat(client, seat);
	eis_seat_event_name(seat, seat->name);

	if (seat->capabilities.c_mask & EIS_DEVICE_CAP_POINTER &&
	    client->interface_versions.ei_pointer > 0) {
		uint64_t mask = bit(EIS_POINTER_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_POINTER_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);
	}

	if (seat->capabilities.c_mask & EIS_DEVICE_CAP_POINTER_ABSOLUTE &&
	    client->interface_versions.ei_pointer_absolute > 0) {
		uint64_t mask = bit(EIS_POINTER_ABSOLUTE_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_POINTER_ABSOLUTE_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);
	}

	if (seat->capabilities.c_mask & (EIS_DEVICE_CAP_POINTER|EIS_DEVICE_CAP_POINTER_ABSOLUTE) &&
	    (client->interface_versions.ei_pointer > 0 || client->interface_versions.ei_pointer_absolute > 0)) {
		uint64_t mask = bit(EIS_SCROLL_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_SCROLL_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);

		mask = bit(EIS_BUTTON_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_BUTTON_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);
	}

	if (seat->capabilities.c_mask & EIS_DEVICE_CAP_KEYBOARD &&
	    client->interface_versions.ei_keyboard > 0) {
		uint64_t mask = bit(EIS_KEYBOARD_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_KEYBOARD_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);
	}

	if (seat->capabilities.c_mask & EIS_DEVICE_CAP_TOUCH &&
	    client->interface_versions.ei_touchscreen > 0) {
		uint64_t mask = bit(EIS_TOUCHSCREEN_INTERFACE_INDEX);
		eis_seat_event_capability(seat, mask,
					  EIS_TOUCHSCREEN_INTERFACE_NAME);
		mask_add(seat->capabilities.proto_mask, mask);
	}

	eis_seat_event_done(seat);
}

void
eis_seat_bind(struct eis_seat *seat, uint32_t caps)
{
	struct eis_client *client = eis_seat_get_client(seat);

	switch (seat->state) {
	case EIS_SEAT_STATE_ADDED:
	case EIS_SEAT_STATE_BOUND:
		break;
	case EIS_SEAT_STATE_PENDING:
	case EIS_SEAT_STATE_REMOVED:
	case EIS_SEAT_STATE_REMOVED_INTERNALLY:
	case EIS_SEAT_STATE_DEAD:
		log_bug_client(eis_client_get_context(client),
			       "%s: seat cannot be bound", __func__);
		return;
	}

	caps &= seat->capabilities.c_mask;
	seat->state = EIS_SEAT_STATE_BOUND;

	uint32_t old_caps = seat->capabilities.bound;
	seat->capabilities.bound = caps;
	if (old_caps != caps)
		eis_queue_seat_bind_event(seat, caps);
}

void
eis_seat_drop(struct eis_seat *seat)
{
	if (seat->state == EIS_SEAT_STATE_BOUND)
		eis_seat_bind(seat, 0);

	struct eis_device *d;
	list_for_each_safe(d, &seat->devices, link) {
		eis_device_remove(d);
	}

	eis_seat_event_destroyed(seat, eis_client_get_next_serial(eis_seat_get_client(seat)));
	seat->state = EIS_SEAT_STATE_REMOVED;

	list_remove(&seat->link);
	seat->state = EIS_SEAT_STATE_REMOVED_INTERNALLY;

	struct eis_client *client = eis_seat_get_client(seat);
	eis_client_unregister_object(client, &seat->proto_object);

	eis_seat_unref(seat);
}

_public_ void
eis_seat_remove(struct eis_seat *seat)
{
	struct eis_client *client = eis_seat_get_client(seat);
	_unref_(eis_seat) *s = eis_seat_ref(seat);

	switch (seat->state) {
	case EIS_SEAT_STATE_PENDING:
	case EIS_SEAT_STATE_ADDED:
	case EIS_SEAT_STATE_BOUND:
		eis_seat_drop(s);
		s->state = EIS_SEAT_STATE_REMOVED;
		break;
	case EIS_SEAT_STATE_REMOVED_INTERNALLY:
		s->state = EIS_SEAT_STATE_REMOVED;
		break;
	case EIS_SEAT_STATE_REMOVED:
	case EIS_SEAT_STATE_DEAD:
		log_bug_client(eis_client_get_context(client),
			       "%s: seat already removed", __func__);
		return;
	}

}

_public_ void
eis_seat_configure_capability(struct eis_seat *seat,
			      enum eis_device_capability cap)
{
	if (seat->state != EIS_SEAT_STATE_PENDING)
		return;

	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
	case EIS_DEVICE_CAP_KEYBOARD:
	case EIS_DEVICE_CAP_TOUCH:
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		mask_add(seat->capabilities.c_mask, cap);
		break;
	}
}

_public_ bool
eis_seat_has_capability(struct eis_seat *seat,
			enum eis_device_capability cap)
{
	switch (cap) {
	case EIS_DEVICE_CAP_POINTER:
	case EIS_DEVICE_CAP_POINTER_ABSOLUTE:
	case EIS_DEVICE_CAP_KEYBOARD:
	case EIS_DEVICE_CAP_TOUCH:
	case EIS_DEVICE_CAP_BUTTON:
	case EIS_DEVICE_CAP_SCROLL:
		return mask_all(seat->capabilities.c_mask, cap);
	}
	return false;
}
