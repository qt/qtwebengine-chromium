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

#pragma once

#include "libeis.h"

#include "util-object.h"
#include "util-list.h"
#include "brei-shared.h"

enum eis_device_state {
	EIS_DEVICE_STATE_NEW,
	EIS_DEVICE_STATE_PAUSED,
	EIS_DEVICE_STATE_RESUMED,
	EIS_DEVICE_STATE_EMULATING,
	EIS_DEVICE_STATE_CLOSED_BY_CLIENT,
	EIS_DEVICE_STATE_DEAD,
};

struct eis_device {
	struct object object; /* parent is ei_seat, and we have a ref to it */
	struct list link;

	struct brei_object proto_object;

	struct eis_pointer *pointer;
	struct eis_pointer_absolute *pointer_absolute;
	struct eis_scroll *scroll;
	struct eis_button *button;
	struct eis_keyboard *keyboard;
	struct eis_touchscreen *touchscreen;

	char *name;
	enum eis_device_state state;
	uint32_t capabilities;
	void *user_data;
	enum eis_device_type type;

	uint32_t width, height;

	struct list regions;
	struct list regions_new; /* not yet added */

	struct eis_keymap *keymap;

	struct list pending_event_queue; /* incoming events waiting for a frame */

	bool send_frame_event;

	struct {
		bool x_is_stopped, y_is_stopped;
		bool x_is_cancelled, y_is_cancelled;
	} scroll_state;

};

struct eis_touch {
	struct object object;
	struct eis_device *device;
	void *user_data;
	uint32_t tracking_id;
	enum {
		TOUCH_IS_NEW,
		TOUCH_IS_DOWN,
		TOUCH_IS_UP,
	} state;

	double x, y;
};

struct eis_keymap {
	struct object object;
	struct eis_device *device;
	void *user_data;
	enum eis_keymap_type type;
	int fd;
	size_t size;
	bool assigned;
};

struct eis_xkb_modifiers {
	uint32_t depressed;
	uint32_t locked;
	uint32_t latched;
	uint32_t group;
};

OBJECT_DECLARE_GETTER(eis_device, id, object_id_t);
OBJECT_DECLARE_GETTER(eis_device, proto_object, const struct brei_object *);
OBJECT_DECLARE_GETTER(eis_device, interface, const struct eis_device_interface *);
OBJECT_DECLARE_GETTER(eis_device, pointer_interface, const struct eis_pointer_interface *);
OBJECT_DECLARE_GETTER(eis_device, pointer_absolute_interface, const struct eis_pointer_absolute_interface *);
OBJECT_DECLARE_GETTER(eis_device, scroll_interface, const struct eis_scroll_interface *);
OBJECT_DECLARE_GETTER(eis_device, button_interface, const struct eis_button_interface *);
OBJECT_DECLARE_GETTER(eis_device, keyboard_interface, const struct eis_keyboard_interface *);
OBJECT_DECLARE_GETTER(eis_device, touchscreen_interface, const struct eis_touchscreen_interface *);

void
eis_device_set_client_keymap(struct eis_device *device,
		      enum eis_keymap_type type,
		      int keymap_fd, size_t size);

void
eis_device_closed_by_client(struct eis_device *device);
