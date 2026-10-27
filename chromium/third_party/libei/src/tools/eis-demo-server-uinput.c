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

#include <libevdev/libevdev-uinput.h>

#include "util-color.h"
#include "util-mem.h"
#include "util-strings.h"

#include "eis-demo-server.h"

DEFINE_TRIVIAL_CLEANUP_FUNC(struct libevdev *, libevdev_free);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct libevdev_uinput *, libevdev_uinput_destroy);

#define _cleanup_libevdev_ _cleanup_(libevdev_freep)
#define _cleanup_libevdev_uinput_ _cleanup_(libevdev_uinput_destroyp)
DEFINE_UNREF_CLEANUP_FUNC(eis_seat);
DEFINE_UNREF_CLEANUP_FUNC(eis_device);

static inline void
_printf_(1, 2)
colorprint(const char *format, ...)
{
	static uint64_t color = 0;
	run_only_once {
		color = rgb(255, 255, 255) | rgb_bg(20, 70, 0);
	}

	cprintf(color, "EIS uinput server:");
	printf(" ");
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

struct uinput_context {
	struct libevdev_uinput *ptr;
	struct libevdev_uinput *kbd;
};

static int
create_mouse(struct eis_demo_server *server, struct eis_seat *seat,
		struct eis_device **device_return)
{
	struct eis_client *client = eis_seat_get_client(seat);
	_cleanup_free_ char *devicename = xaprintf("%s pointer", eis_client_get_name(client));
	_unref_(eis_device) *device = eis_seat_new_device(seat);

	eis_device_configure_name(device, devicename);
	eis_device_configure_capability(device, EIS_DEVICE_CAP_POINTER);
	eis_device_configure_capability(device, EIS_DEVICE_CAP_BUTTON);
	eis_device_configure_capability(device, EIS_DEVICE_CAP_SCROLL);

	_cleanup_libevdev_ struct libevdev *dev = libevdev_new();
	libevdev_set_name(dev, devicename);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_SIDE, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_EXTRA, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_FORWARD, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_BACK, NULL);

	_cleanup_libevdev_uinput_ struct libevdev_uinput *uinput = NULL;
	int err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput);
	if (err == 0) {
		colorprint("Pointer device is %s\n", libevdev_uinput_get_devnode(uinput));
		eis_device_set_user_data(device, steal(&uinput));
		*device_return = steal(&device);
	}
	return err;
}

static int
create_keyboard(struct eis_demo_server *server, struct eis_seat *seat,
		struct eis_device **device_return)
{
	struct eis_client *client = eis_seat_get_client(seat);
	_cleanup_free_ char *devicename = xaprintf("%s keyboard", eis_client_get_name(client));
	_unref_(eis_device) *device = eis_seat_new_device(seat);

	eis_device_configure_name(device, devicename);
	eis_device_configure_capability(device, EIS_DEVICE_CAP_KEYBOARD);

	_cleanup_libevdev_ struct libevdev *dev = libevdev_new();
	libevdev_set_name(dev, devicename);
	for (unsigned code = 0; code <= KEY_MICMUTE; code++)
		libevdev_enable_event_code(dev, EV_KEY, code, NULL);

	_cleanup_libevdev_uinput_ struct libevdev_uinput *uinput = NULL;
	int err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput);
	if (err == 0) {
		colorprint("Keyboard device is %s\n", libevdev_uinput_get_devnode(uinput));
		eis_device_set_user_data(device, steal(&uinput));
		*device_return = steal(&device);
	}
	return err;
}

static int
eis_demo_server_uinput_handle_event(struct eis_demo_server *server,
				    struct eis_event *e)
{
	switch(eis_event_get_type(e)) {
	case EIS_EVENT_CLIENT_CONNECT:
		{
		struct eis_client *client = eis_event_get_client(e);
		eis_client_connect(client);
		colorprint("new client: %s, accepting. creating new seat 'default'\n", eis_client_get_name(client));

		_unref_(eis_seat) *seat = eis_client_new_seat(client, "default");
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_KEYBOARD);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_TOUCH);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_BUTTON);
		eis_seat_configure_capability(seat, EIS_DEVICE_CAP_SCROLL);
		eis_seat_add(seat);
		break;
		}
	case EIS_EVENT_CLIENT_DISCONNECT:
		{
		struct eis_client *client = eis_event_get_client(e);
		colorprint("client %s disconnected\n", eis_client_get_name(client));
		eis_client_disconnect(client);
		break;
		}
	case EIS_EVENT_SEAT_BIND:
		{
		struct eis_seat *seat = eis_event_get_seat(e);

		/* FIXME: does not handle device removal */

		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER)) {
			struct eis_device *device = NULL;
			int rc = create_mouse(server, seat, &device);
			if (rc != 0) {
				colorprint("Failed to create device: %s\n", strerror(-rc));
				return rc;
			}
			eis_device_add(device);
			eis_device_resume(device);
		}
		if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_KEYBOARD)) {
			struct eis_device *device = NULL;
			int rc = create_keyboard(server, seat, &device);
			if (rc != 0) {
				colorprint("Failed to create device: %s\n", strerror(-rc));
				return rc;
			}
			eis_device_add(device);
			eis_device_resume(device);
		}
		/* Note: our device has a dangling ref here. It's a
		 * demo-server so we can just ignore that */
		}
		break;
	case EIS_EVENT_DEVICE_CLOSED:
		{
		struct eis_device *device = eis_event_get_device(e);
		colorprint("device closed\n");
		struct libevdev_uinput *uinput = eis_device_get_user_data(device);
		if (uinput)
			libevdev_uinput_destroy(uinput);
		eis_device_remove(device);
		eis_device_unref(device); /* because we know we have a dangling ref */
		break;
		}
	case EIS_EVENT_POINTER_MOTION:
		{
		/* Note: we drop subpixel here */
		struct eis_device *device = eis_event_get_device(e);
		int x = eis_event_pointer_get_dx(e),
		    y = eis_event_pointer_get_dy(e);
		colorprint("REL_X %d, REL_Y %d\n", x, y);
		struct libevdev_uinput *uinput = eis_device_get_user_data(device);
		if (uinput) {
			libevdev_uinput_write_event(uinput, EV_REL, REL_X, x);
			libevdev_uinput_write_event(uinput, EV_REL, REL_Y, y);
			libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
		}
		}
		break;
	case EIS_EVENT_BUTTON_BUTTON:
		{
		struct eis_device *device = eis_event_get_device(e);
		uint32_t button = eis_event_button_get_button(e);
		bool state = eis_event_button_get_is_press(e);
		colorprint("%s %s\n",
			   libevdev_event_code_get_name(EV_KEY, button),
			   state ? "press" : "release");
		struct libevdev_uinput *uinput = eis_device_get_user_data(device);
		if (uinput) {
			libevdev_uinput_write_event(uinput, EV_KEY, button, state ? 1 : 0);
			libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
		}
		}
		break;
	case EIS_EVENT_KEYBOARD_KEY:
		{
		struct eis_device *device = eis_event_get_device(e);
		uint32_t key = eis_event_keyboard_get_key(e);
		bool state = eis_event_keyboard_get_key_is_press(e);
		colorprint("%s %s\n",
			   libevdev_event_code_get_name(EV_KEY, key),
			   state ? "press" : "release");
		struct libevdev_uinput *uinput = eis_device_get_user_data(device);
		if (uinput) {
			libevdev_uinput_write_event(uinput, EV_KEY, key, state ? 1 : 0);
			libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
		}
		}
		break;
	default:
		abort();
	}
	return 0;
}

int
eis_demo_server_setup_uinput_handler(struct eis_demo_server *server)
{
	struct uinput_context *ctx = xalloc(sizeof *ctx);
	server->handler.data = ctx;
	server->handler.handle_event = eis_demo_server_uinput_handle_event;

	return 0;
}
