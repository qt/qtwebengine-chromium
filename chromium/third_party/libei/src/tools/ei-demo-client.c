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
#include <poll.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <linux/input-event-codes.h>

#if HAVE_LIBXKBCOMMON
#include <xkbcommon/xkbcommon.h>
#endif

#include "libei.h"

#include "src/util-macros.h"
#include "src/util-mem.h"
#include "src/util-memmap.h"
#include "src/util-color.h"
#include "src/util-io.h"
#include "src/util-strings.h"
#include "src/util-time.h"

DEFINE_UNREF_CLEANUP_FUNC(ei);
DEFINE_UNREF_CLEANUP_FUNC(ei_device);
DEFINE_UNREF_CLEANUP_FUNC(ei_event);

static inline void
_printf_(1, 2)
colorprint(const char *format, ...)
{
	static uint64_t color = 0;
	run_only_once {
		color = rgb(1, 1, 1) | rgb_bg(230, 0, 230);
	}

	cprintf(color, "EI socket client: ");
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

#if HAVE_LIBXKBCOMMON
DEFINE_UNREF_CLEANUP_FUNC(xkb_context);
DEFINE_UNREF_CLEANUP_FUNC(xkb_keymap);
DEFINE_UNREF_CLEANUP_FUNC(xkb_state);
DEFINE_UNREF_CLEANUP_FUNC(memmap);
#endif

static void
setup_xkb_keymap(struct ei_keymap *keymap)
{
#if HAVE_LIBXKBCOMMON
	if (!keymap)
		return;

	_unref_(xkb_context) *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!ctx)
		return;

	_unref_(memmap) *memmap = memmap_new(ei_keymap_get_fd(keymap), ei_keymap_get_size(keymap));
	if (!memmap) {
		colorprint("Failed to mmap XKB keymap: %m\n");
		return;
	}

	/* workaround for libxkbcommon#307 (fixed in libxkbcommon 1.6.0) - strip the trailing null byte */
	size_t sz = memmap_get_size(memmap);
	char *data = memmap_get_data(memmap);
	while (sz > 0 && data[sz - 1] == '\0')
		--sz;

	_unref_(xkb_keymap) *xkbmap = xkb_keymap_new_from_buffer(ctx, memmap_get_data(memmap),
								 memmap_get_size(memmap),
								 XKB_KEYMAP_FORMAT_TEXT_V1,
								 XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!xkbmap)
		return;

	_unref_(xkb_state) *xkbstate = xkb_state_new(xkbmap);
	if (!xkbstate)
		return;

	char layout[6 * 7 + 1] = {0}; /* 6 keys, 7 bytes per key min */
	for (unsigned int evcode = KEY_Q; evcode <= KEY_Y; evcode++) {
		char utf8[7];
		xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkbstate, evcode + 8);
		xkb_keysym_to_utf8(keysym, utf8, sizeof(utf8));
		strcat(layout, utf8);
	}

	colorprint("XKB keymap: %s\n", layout);
#endif
}

static void
handle_keymap(struct ei_event *event)
{
	struct ei_device *device = ei_event_get_device(event);

	if (!ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD))
		return;

	struct ei_keymap *keymap = ei_device_keyboard_get_keymap(device);
	if (!keymap)
		return;

	enum ei_keymap_type type = ei_keymap_get_type(keymap);
	switch (type) {
		case EI_KEYMAP_TYPE_XKB:
			setup_xkb_keymap(keymap);
			break;
	}
}

static void
handle_regions(struct ei_device *device)
{
	uint32_t idx = 0;
	struct ei_region *r;

	while ((r = ei_device_get_region (device, idx++))) {
		int x, y, w, h;
		x = ei_region_get_x(r);
		y = ei_region_get_y(r);
		w = ei_region_get_width(r);
		h = ei_region_get_height(r);

		colorprint("%s has region %dx%d@%d,%d\n",
			   ei_device_get_name(device), w, h, x, y);
	}
}

static void
usage(FILE *fp, const char *argv0)
{
	fprintf(fp,
		"Usage: %s [--verbose] [--socket|--portal] [--busname=a.b.c.d] [--layout=us] [--interval=2000]\n"
		"\n"
		"Start an EI demo client. The client will connect to EIS\n"
		"with the chosen backend (default: socket) and emulate pointer\n"
		"and keyboard events in a loop.\n"
		"\n"
		"Options:\n"
		" --socket	Use the socket backend. The socket path is $LIBEI_SOCKET if set, \n"
		"		otherwise $XDG_RUNTIME_DIR/eis-0\n"
		" --socketfd    Use the given fd as socket to the EIS implementation\n"
		" --verbose	Enable debugging output\n"
		" --receiver	Create a receiver EIS context, receiving events instead of sending them\n"
		" --interval    Interval in milliseconds between polling\n"
		" --iterations  Limit the number of iterations and disconnect once reached\n"
		"",
		argv0);
}

int main(int argc, char **argv)
{
	enum {
		SOCKET,
		FD,
	} backend = SOCKET;
	_cleanup_close_ int socketfd = -1;
	bool verbose = false;
	bool receiver = false;
	unsigned int interval = 2000;
	uint32_t iterations = UINT32_MAX;
	_cleanup_free_ char *busname = xstrdup("org.freedesktop.portal.Desktop");

	while (1) {
		enum {
			OPT_BACKEND_SOCKET,
			OPT_VERBOSE,
			OPT_RECEIVER,
			OPT_INTERVAL,
			OPT_ITERATIONS,
			OPT_SOCKETFD,
		};
		static struct option long_opts[] = {
			{"socket",	no_argument, 0, OPT_BACKEND_SOCKET},
			{"socketfd",	required_argument, 0, OPT_SOCKETFD},
			{"verbose",	no_argument, 0, OPT_VERBOSE},
			{"receiver",	no_argument, 0, OPT_RECEIVER},
			{"interval",    required_argument, 0, OPT_INTERVAL},
			{"iterations",  required_argument, 0, OPT_ITERATIONS},
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
		case OPT_BACKEND_SOCKET:
			backend = SOCKET;
			break;
		case OPT_SOCKETFD:
			backend = FD;
			if (!xatoi(optarg, &socketfd)) {
				fprintf(stderr, "Invalid socketfd: %s", optarg);
				return 2;
			}
			break;
		case OPT_RECEIVER:
			receiver = true;
			break;
		case OPT_INTERVAL:
			interval = atoi(optarg);
			break;
		case OPT_ITERATIONS:
			iterations = atoi(optarg);
			break;
		default:
			usage(stderr, argv[0]);
			return EXIT_FAILURE;
		}
	}

	_unref_(ei) *ei = NULL;
	if (receiver)
		ei = ei_new_receiver(NULL);
	else
		ei = ei_new_sender(NULL);
	assert(ei);

	if (verbose)
		ei_log_set_priority(ei, EI_LOG_PRIORITY_DEBUG);

	ei_configure_name(ei, "ei-demo-client");

	int rc = -EINVAL;

	if (backend == SOCKET) {
		const char SOCKETNAME[] = "eis-0";
		colorprint("connecting to %s\n", SOCKETNAME);
		rc = ei_setup_backend_socket(ei, getenv("LIBEI_SOCKET") ? NULL : SOCKETNAME);
	} else if (backend == FD) {
		rc = ei_setup_backend_fd(ei, socketfd);
	}

	if (rc != 0) {
		fprintf(stderr, "init failed: %s\n", strerror(-rc));
		return 1;
	}

	struct pollfd fds = {
		.fd = ei_get_fd(ei),
		.events = POLLIN,
		.revents = 0,
	};
	_unref_(ei_device) *ptr = NULL;
	_unref_(ei_device) *kbd = NULL;
	_unref_(ei_device) *abs = NULL;
	_unref_(ei_device) *touch = NULL;

	bool stop = false;
	bool have_ptr = false;
	bool have_kbd = false;
	bool have_abs = false;
	bool have_touch = false;
	struct ei_seat *default_seat = NULL;

	uint32_t sequence = 0;
	uint32_t iteration = 0;

	while (!stop && poll(&fds, 1, interval) > -1) {
		++iteration;
		ei_dispatch(ei);

		while (!stop) {
			_unref_(ei_event) *e = ei_get_event(ei);
			if (!e)
				break;

			switch(ei_event_get_type(e)) {
			case EI_EVENT_CONNECT:
				colorprint("connected\n");
				break;
			case EI_EVENT_DISCONNECT:
				{
				colorprint("disconnected us\n");
				stop = true;
				break;
				}
			case EI_EVENT_SEAT_ADDED:
				{
				if (default_seat) {
					colorprint("ignoring other seats\n");
					break;
				}
				default_seat = ei_seat_ref(ei_event_get_seat(e));
				colorprint("seat added: %s\n", ei_seat_get_name(default_seat));
				ei_seat_bind_capabilities(default_seat, EI_DEVICE_CAP_POINTER,
							  EI_DEVICE_CAP_KEYBOARD,
							  EI_DEVICE_CAP_POINTER_ABSOLUTE,
							  EI_DEVICE_CAP_TOUCH,
							  EI_DEVICE_CAP_BUTTON,
							  EI_DEVICE_CAP_SCROLL, NULL);
				break;
				}
			case EI_EVENT_SEAT_REMOVED:
				/* Don't need to close the devices, libei will
				 * give us the right events */
				if (ei_event_get_seat(e) == default_seat)
					default_seat = ei_seat_unref(default_seat);
				break;
			case EI_EVENT_DEVICE_ADDED:
				{
				struct ei_device *device = ei_event_get_device(e);

				if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER)) {
					colorprint("New pointer device: %s\n", ei_device_get_name(device));
					ptr = ei_device_ref(device);
				}
				if (ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) {
					colorprint("New keyboard device: %s\n", ei_device_get_name(device));
					kbd = ei_device_ref(device);
					handle_keymap(e);
				}
				if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
					colorprint("New abs pointer device: %s\n", ei_device_get_name(device));
					abs = ei_device_ref(device);
					handle_regions(device);
				}
				if (ei_device_has_capability(device, EI_DEVICE_CAP_TOUCH)) {
					colorprint("New touch device: %s\n", ei_device_get_name(device));
					touch = ei_device_ref(device);
					handle_regions(device);
				}
				}
				break;
			case EI_EVENT_DEVICE_RESUMED:
				if (ei_event_get_device(e) == ptr) {
					if (!receiver)
						ei_device_start_emulating(ptr, ++sequence);
					colorprint("Pointer device was resumed\n");
					have_ptr = true;
				}
				if (ei_event_get_device(e) == kbd) {
					if (!receiver)
						ei_device_start_emulating(kbd, ++sequence);
					colorprint("Keyboard device was resumed\n");
					have_kbd = true;
				}
				if (ei_event_get_device(e) == abs) {
					if (!receiver)
						ei_device_start_emulating(abs, ++sequence);
					colorprint("Abs pointer device was resumed\n");
					have_abs = true;
				}
				if (ei_event_get_device(e) == touch) {
					if (!receiver)
						ei_device_start_emulating(touch, ++sequence);
					colorprint("Touch device was resumed\n");
					have_touch = true;
				}
				break;
			case EI_EVENT_DEVICE_PAUSED:
				if (ei_event_get_device(e) == ptr) {
					colorprint("Pointer device was paused\n");
					have_ptr = false;
				}
				if (ei_event_get_device(e) == kbd) {
					colorprint("Keyboard device was paused\n");
					have_kbd = false;
				}
				if (ei_event_get_device(e) == abs) {
					colorprint("Abs pointer device was paused\n");
					have_abs = false;
				}
				if (ei_event_get_device(e) == touch) {
					colorprint("Touch device was paused\n");
					have_touch = false;
				}
				break;
			case EI_EVENT_DEVICE_REMOVED:
				{
				colorprint("our device was removed\n");
				break;
				}
			case EI_EVENT_FRAME:
				break;
			case EI_EVENT_DEVICE_START_EMULATING:
				{
				struct ei_device *device = ei_event_get_device(e);
				colorprint("Device %s is ready to send events\n", ei_device_get_name(device));
				}
				break;
			case EI_EVENT_DEVICE_STOP_EMULATING:
				{
				struct ei_device *device = ei_event_get_device(e);
				colorprint("Device %s will no longer send events\n", ei_device_get_name(device));
				}
				break;
			case EI_EVENT_POINTER_MOTION:
				{
				colorprint("motion by %.2f/%.2f\n",
				ei_event_pointer_get_dx(e),
				ei_event_pointer_get_dy(e));
				}
				break;
			case EI_EVENT_POINTER_MOTION_ABSOLUTE:
				{
				colorprint("absmotion to %.2f/%.2f\n",
				ei_event_pointer_get_absolute_x(e),
				ei_event_pointer_get_absolute_y(e));
				}
				break;
			case EI_EVENT_BUTTON_BUTTON:
				{
				colorprint("button %u (%s)\n",
				ei_event_button_get_button(e),
				ei_event_button_get_is_press(e) ? "press" : "release");
				}
				break;
			case EI_EVENT_SCROLL_DELTA:
				{
				colorprint("scroll %.2f/%.2f\n",
					ei_event_scroll_get_dx(e),
					ei_event_scroll_get_dy(e));
				}
				break;
			case EI_EVENT_SCROLL_DISCRETE:
				{
				colorprint("scroll discrete %d/%d\n",
					ei_event_scroll_get_discrete_dx(e),
					ei_event_scroll_get_discrete_dy(e));
				}
				break;
			case EI_EVENT_KEYBOARD_KEY:
				{
				colorprint("key %u (%s)\n",
					   ei_event_keyboard_get_key(e),
					   ei_event_keyboard_get_key_is_press(e) ? "press" : "release");
				}
				break;
			case EI_EVENT_TOUCH_DOWN:
			case EI_EVENT_TOUCH_MOTION:
				{
				colorprint("touch %s %u %.2f/%.2f\n",
					   ei_event_get_type(e) == EI_EVENT_TOUCH_DOWN ? "down" : "motion",
					   ei_event_touch_get_id(e),
					   ei_event_touch_get_x(e),
					   ei_event_touch_get_y(e));
				}
				break;
			case EI_EVENT_TOUCH_UP:
				{
				colorprint("touch up %u\n", ei_event_touch_get_id(e));
				}
				break;
			default:
				abort();
			}
		}

		if (iteration >= iterations || stop)
			break;

		if (!receiver) {
			uint64_t now = ei_now(ei);
			uint64_t interval = ms2us(10); /* pretend events are 10ms apart */

			colorprint("now: %" PRIu64 "\n", now);
			if (have_ptr) {
				colorprint("sending motion event\n");
				ei_device_pointer_motion(ptr, -1, 1);
				/* BTN_LEFT */
				colorprint("sending button event\n");
				ei_device_button_button(ptr, BTN_LEFT, true);
				ei_device_frame(ptr, now);
				now += interval;
				ei_device_button_button(ptr, BTN_LEFT, false);
				ei_device_frame(ptr, now);
				now += interval;
				colorprint("sending scroll events\n");
				ei_device_scroll_delta(ptr, 1, 1);
				ei_device_frame(ptr, now);
				now += interval;
				ei_device_scroll_discrete(ptr, 120, 120);
				ei_device_frame(ptr, now);
				now += interval;
			}

			if (have_kbd) {
				static int key = 0;
				colorprint("sending key event\n");
				ei_device_keyboard_key(kbd, KEY_Q + key, true); /* KEY_Q */
				ei_device_frame(kbd, now);
				now += interval;
				ei_device_keyboard_key(kbd, KEY_Q + key, false); /* KEY_Q */
				ei_device_frame(kbd, now);
				now += interval;
				key = (key + 1) % 6;
			}

			if (have_abs) {
				static int x, y;
				colorprint("sending abs event\n");
				ei_device_pointer_motion_absolute(abs, 150 + ++x, 150 - ++y);
				ei_device_frame(abs, now);
				now += interval;
			}

			if (have_touch) {
				static int x, y;
				static int counter = 0;
				static struct ei_touch *t;

				switch (counter++ % 5) {
					case 0:
						colorprint("sending touch down event\n");
						t = ei_device_touch_new(touch);
						ei_touch_down(t, 100 + ++x, 200 - ++y);
						ei_device_frame(touch, now);
						break;
					case 4:
						colorprint("sending touch down event\n");
						ei_touch_up(t);
						ei_device_frame(touch, now);
						t = ei_touch_unref(t);
						break;
					default:
						ei_touch_motion(t, 100 + ++x, 200 - ++y);
						ei_device_frame(touch, now);
						break;
				}

			}
		}
	}

	colorprint("shutting down\n");
	if (ptr)
		ei_device_close(ptr);
	if (kbd)
		ei_device_close(kbd);
	if (abs)
		ei_device_close(abs);
	if (touch)
		ei_device_close(touch);
	if (default_seat) {
		ei_seat_bind_capabilities(default_seat, EI_DEVICE_CAP_POINTER,
							EI_DEVICE_CAP_KEYBOARD,
							EI_DEVICE_CAP_POINTER_ABSOLUTE,
							EI_DEVICE_CAP_TOUCH,
							EI_DEVICE_CAP_BUTTON,
							EI_DEVICE_CAP_SCROLL, NULL);
		ei_seat_unref(default_seat);
	}

	ei = ei_unref(ei);

	return 0;
}
