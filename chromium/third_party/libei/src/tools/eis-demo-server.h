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

#include "config.h"

#include "libeis.h"
#include "util-list.h"
#include "util-object.h"

struct eis_demo_client {
	struct object object;
	struct list link;

	struct eis_client *client;
	struct eis_device *ptr;
	struct eis_device *kbd;
	struct eis_device *abs;
	struct eis_device *touchscreen;
	struct eis_touch *touch;
};

struct eis_demo_server {
	const char *layout;
#if HAVE_LIBXKBCOMMON
	struct xkb_context *ctx;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
#endif
	/* Event handler */
	struct {
		int (*handle_event)(struct eis_demo_server *server,
				    struct eis_event *e);
		void *data;
	} handler;

	struct list clients;
	unsigned int nreceiver_clients;
};

#if HAVE_LIBEVDEV
int
eis_demo_server_setup_uinput_handler(struct eis_demo_server *server);
#endif
