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

/* A simple tool that provides a libei client that sends a fixed set of
 * events every second.
 *
 * This tool is useful for testing EIS implementations, to make sure we can
 * a connection, we receive devices and that we can send events.
 *
 * Usually, you'd want to:
 * - run the eis-demo-server (or some other EIS implementation)
 * - export LIBEI_SOCKET=eis-0, or whatever value was given
 * - run the ei-demo-client
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#if HAVE_LIBEVDEV
#include <libevdev/libevdev.h>
#else
#define libevdev_event_code_get_name(...) ""
#endif

#include "libei.h"

#include "src/util-macros.h"
#include "src/util-mem.h"
#include "src/util-io.h"

DEFINE_UNREF_CLEANUP_FUNC(ei);
DEFINE_UNREF_CLEANUP_FUNC(ei_event);

#define truefalse(v_) (v_) ? "true" : "false"

static void
usage(FILE *fp, const char *argv0)
{
	fprintf(fp,
		"Usage: %s [--verbose] [--socketfd=<int>]\n"
		"\n"
		"Start an EI client and print communication from the EIS implementation in YAML format.\n"
		"The client will bind to all available capabilities on all seats.\n"
		"\n"
		"By default, this client connects to $LIBEI_SOCKET if set or $XDG_RUNTIME_DIR/eis-0\n"
		"\n"
		"Options:\n"
		" --socketfd    Use the given fd as socket to the EIS implementation\n"
		" --verbose	Enable debugging output\n"
		" --receiver    Enable receiver mode\n"
		" --sender      Enable sender mode (default)\n"
		"",
		argv0);
}

static void
print_header(void)
{
	printf("ei:\n");
	printf("  version: %s\n", EI_VERSION);
}

static void
print_event_header(struct ei_event *event)
{
	run_only_once {
		printf("events:\n");
	}
	printf("- type: %s\n", ei_event_type_to_string(ei_event_get_type(event)));
}

static void
print_seat_event(struct ei_event *event)
{
	struct ei_seat *seat = ei_event_get_seat(event);
	char *caps[6] = {NULL};
	size_t idx = 0;

	if (ei_seat_has_capability(seat, EI_DEVICE_CAP_POINTER))
		caps[idx++] = "pointer";
	if (ei_seat_has_capability(seat, EI_DEVICE_CAP_POINTER_ABSOLUTE))
		caps[idx++] = "pointer-absolute";
	if (ei_seat_has_capability(seat, EI_DEVICE_CAP_KEYBOARD))
		caps[idx++] = "keyboard";
	if (ei_seat_has_capability(seat, EI_DEVICE_CAP_TOUCH))
		caps[idx++] = "touch";

	_cleanup_free_ char *capabilities = strv_join(caps, ", ");

	printf("  seat: %s\n", ei_seat_get_name(seat));
	printf("  capabilities: [%s]\n", capabilities);
}

static void
print_device(struct ei_event *event)
{
	struct ei_device *device = ei_event_get_device(event);
	struct ei_seat *seat = ei_event_get_seat(event);

	printf("  device: %s\n", ei_device_get_name(device));
	printf("  seat: %s\n", ei_seat_get_name(seat));
}

static void
print_device_details(struct ei_event *event)
{
	struct ei_device *device = ei_event_get_device(event);

	print_device(event);

	char *caps[6] = {NULL};
	size_t idx = 0;

	if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER))
		caps[idx++] = "pointer";
	if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE))
		caps[idx++] = "pointer-absolute";
	if (ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD))
		caps[idx++] = "keyboard";
	if (ei_device_has_capability(device, EI_DEVICE_CAP_TOUCH))
		caps[idx++] = "touch";
	_cleanup_free_ char *capabilities = strv_join(caps, ", ");

	printf("  type: %s\n", ei_device_get_type(device) == EI_DEVICE_TYPE_VIRTUAL ? "virtual" : "physical");
	printf("  capabilities: [%s]\n", capabilities);

	idx = 0;
	struct ei_region *region;
	while ((region = ei_device_get_region(device, idx++))) {
	       if (idx == 1)
			printf("  regions:\n");

		uint32_t w = ei_region_get_width(region);
		uint32_t h = ei_region_get_height(region);
		uint32_t x = ei_region_get_x(region);
		uint32_t y = ei_region_get_y(region);
		double scale = ei_region_get_physical_scale(region);

		const char *mapping_id = ei_region_get_mapping_id(region);
		printf("    - { x: %u, y: %u, w: %u, h: %u, scale: %.2f, mapping_id: '%s' }\n",
		       x, y, w, h, scale,
		       mapping_id ? mapping_id : "<none>");
	}

	struct ei_keymap *keymap = ei_device_keyboard_get_keymap(device);
	if (keymap) {
		switch (ei_keymap_get_type(keymap)) {
		case EI_KEYMAP_TYPE_XKB:
			printf("  keymap: xkb\n");
			break;
		}
	}
}

static void
print_motion_event(struct ei_event *event)
{
	print_device(event);

	double dx = ei_event_pointer_get_dx(event);
	double dy = ei_event_pointer_get_dy(event);

	printf("  motion: [%f, %f]\n", dx, dy);
}

static void
print_abs_event(struct ei_event *event)
{
	print_device(event);

	double x = ei_event_pointer_get_absolute_x(event);
	double y = ei_event_pointer_get_absolute_y(event);

	printf("  position: [%f, %f]\n", x, y);
}

static void
print_button_event(struct ei_event *event)
{
	print_device(event);

	uint32_t button = ei_event_button_get_button(event);
	bool press = ei_event_button_get_is_press(event);

	printf("  button: %u # %s\n", button, libevdev_event_code_get_name(EV_KEY, button));
	printf("  press: %s\n", press ? "true" : "false");
}

static void
print_key_event(struct ei_event *event)
{
	print_device(event);

	uint32_t key = ei_event_keyboard_get_key(event);
	bool press = ei_event_keyboard_get_key_is_press(event);

	printf("  key: %u # %s\n", key, libevdev_event_code_get_name(EV_KEY, key));
	printf("  press: %s\n", press ? "true" : "false");
}

static void
print_touch_event(struct ei_event *event)
{
	print_device(event);

	uint32_t touchid = ei_event_touch_get_id(event);
	printf("   touchid: %u\n", touchid);

	if (ei_event_get_type(event) != EI_EVENT_TOUCH_UP) {
		double x = ei_event_touch_get_x(event);
		double y = ei_event_touch_get_y(event);

		printf("   position: [%f, %f]\n", x, y);
	}
}

static void
print_scroll_event(struct ei_event *event)
{
	print_device(event);

	double x = ei_event_scroll_get_dx(event);
	double y = ei_event_scroll_get_dy(event);

	printf("  scroll: [%f, %f]\n", x, y);
}

static void
print_scroll_discrete_event(struct ei_event *event)
{
	print_device(event);

	int32_t x = ei_event_scroll_get_discrete_dx(event);
	int32_t y = ei_event_scroll_get_discrete_dy(event);

	printf("  scroll: [%d, %d]\n", x, y);
}

static void
print_scroll_stop_event(struct ei_event *event)
{
	print_device(event);

	bool x = ei_event_scroll_get_stop_x(event);
	bool y = ei_event_scroll_get_stop_y(event);

	printf("  scroll: [%s, %s]\n", truefalse(x), truefalse(y));
}

static void
print_modifiers_event(struct ei_event *event)
{
	print_device(event);

	printf("  group: %u\n", ei_event_keyboard_get_xkb_group(event));
	printf("  depressed: \"0x%x\"\n", ei_event_keyboard_get_xkb_mods_depressed(event));
	printf("  latched: \"0x%x\"\n", ei_event_keyboard_get_xkb_mods_latched(event));
	printf("  locked: \"0x%x\"\n", ei_event_keyboard_get_xkb_mods_locked(event));
}

static void
print_pong_event(struct ei_event *event)
{
	struct ei_ping *ping = ei_event_pong_get_ping(event);

	printf("  id: %#" PRIx64 "\n", ei_ping_get_id(ping));
}

int main(int argc, char **argv)
{
	enum {
		MODE_RECEIVER,
		MODE_SENDER,
	} mode = MODE_SENDER;
	bool verbose = false;
	_cleanup_close_ int socketfd = -1;

	while (1) {
		enum {
			OPT_SOCKETFD,
			OPT_VERBOSE,
			OPT_RECEIVER,
			OPT_SENDER,
		};
		static struct option long_opts[] = {
			{"socketfd",	required_argument, 0, OPT_SOCKETFD},
			{"verbose",	no_argument, 0, OPT_VERBOSE},
			{"receiver",	no_argument, 0, OPT_RECEIVER},
			{"sender",	no_argument, 0, OPT_SENDER},
			{"help",	no_argument, 0, 'h'},
			{.name = NULL},
		};

		int optind = 0;
		int c = getopt_long(argc, argv, "h", long_opts, &optind);
		if (c == -1)
			break;

		switch(c) {
		case 'h':
			usage(stdout, argv[0]);
			return EXIT_SUCCESS;
		case OPT_VERBOSE:
			verbose = true;
			break;
		case OPT_SOCKETFD:
			if (!xatoi(optarg, &socketfd)) {
				fprintf(stderr, "Invalid socketfd: %s", optarg);
				return 2;
			}
			break;
		case OPT_SENDER:
			mode = MODE_SENDER;
			break;
		case OPT_RECEIVER:
			mode = MODE_RECEIVER;
			break;
		default:
			usage(stderr, argv[0]);
			return EXIT_FAILURE;
		}
	}

	_unref_(ei) *ei = NULL;
	if (mode == MODE_RECEIVER)
		ei = ei_new_receiver(NULL);
	else
		ei = ei_new_sender(NULL);

	if (verbose)
		ei_log_set_priority(ei, EI_LOG_PRIORITY_DEBUG);

	ei_configure_name(ei, "ei-debug-events");

	int rc = -EINVAL;
	if (socketfd == -1) {
		const char SOCKETNAME[] = "eis-0";
		rc = ei_setup_backend_socket(ei, getenv("LIBEI_SOCKET") ? NULL : SOCKETNAME);
	} else {
		rc = ei_setup_backend_fd(ei, socketfd);
	}

	if (rc != 0) {
		fprintf(stderr, "Failed to setup backend: %s\n", strerror(errno));
		return 1;
	}

	print_header();

	struct pollfd fds = {
		.fd = ei_get_fd(ei),
		.events = POLLIN,
		.revents = 0,
	};

	while (poll(&fds, 1, 2000) > -1) {
		ei_dispatch(ei);

		while (true) {
			_unref_(ei_event) *e = ei_get_event(ei);
			if (!e)
				break;

			struct ei_seat *seat = ei_event_get_seat(e);

			print_event_header(e);

			switch(ei_event_get_type(e)) {
			case EI_EVENT_CONNECT:
				break;
			case EI_EVENT_DISCONNECT:
				goto finished;
			case EI_EVENT_SEAT_ADDED:
				ei_seat_bind_capabilities(seat, EI_DEVICE_CAP_POINTER,
							  EI_DEVICE_CAP_POINTER_ABSOLUTE,
							  EI_DEVICE_CAP_KEYBOARD,
							  EI_DEVICE_CAP_TOUCH,
							  EI_DEVICE_CAP_BUTTON,
							  EI_DEVICE_CAP_SCROLL,
							  NULL);
				_fallthrough_;
			case EI_EVENT_SEAT_REMOVED:
				print_seat_event(e);
				break;
			case EI_EVENT_DEVICE_ADDED:
				print_device_details(e);
				break;
			case EI_EVENT_DEVICE_REMOVED:
			case EI_EVENT_DEVICE_RESUMED:
			case EI_EVENT_DEVICE_PAUSED:
			case EI_EVENT_DEVICE_START_EMULATING:
			case EI_EVENT_DEVICE_STOP_EMULATING:
			case EI_EVENT_FRAME:
				print_device(e);
				break;
			case EI_EVENT_POINTER_MOTION:
				print_motion_event(e);
				break;
			case EI_EVENT_POINTER_MOTION_ABSOLUTE:
				print_abs_event(e);
				break;
			case EI_EVENT_BUTTON_BUTTON:
				print_button_event(e);
				break;
			case EI_EVENT_KEYBOARD_KEY:
				print_key_event(e);
				break;
			case EI_EVENT_TOUCH_DOWN:
			case EI_EVENT_TOUCH_MOTION:
			case EI_EVENT_TOUCH_UP:
				print_touch_event(e);
				break;
			case EI_EVENT_SCROLL_DELTA:
				print_scroll_event(e);
				break;
			case EI_EVENT_SCROLL_DISCRETE:
				print_scroll_discrete_event(e);
				break;
			case EI_EVENT_SCROLL_STOP:
			case EI_EVENT_SCROLL_CANCEL:
				print_scroll_stop_event(e);
				break;
			case EI_EVENT_KEYBOARD_MODIFIERS:
				print_modifiers_event(e);
				break;
			case EI_EVENT_PONG:
				print_pong_event(e);
				break;
			case EI_EVENT_SYNC:
				break;
			}
		}
	}

finished:

	return 0;
}
