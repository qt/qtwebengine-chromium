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

#include <stdint.h>
#include "util-object.h"
#include "util-list.h"
#include "brei-shared.h"

enum eis_seat_state {
	EIS_SEAT_STATE_PENDING,
	EIS_SEAT_STATE_ADDED,
	EIS_SEAT_STATE_BOUND,
	EIS_SEAT_STATE_REMOVED_INTERNALLY, /* Removed internally but eis_seat_remove() may be called */
	EIS_SEAT_STATE_REMOVED, /* Removed but still waiting for some devices to be removed */
	EIS_SEAT_STATE_DEAD, /* Removed from our list */
};

struct eis_seat {
	struct object object; /* parent is ei_client */

	struct brei_object proto_object;

	struct list link;
	void *user_data;

	enum eis_seat_state state;
	char *name;
	struct {
		uint32_t c_mask; /* this is the C API bitmask */
		uint64_t proto_mask; /* the protocol mask */

		uint32_t bound; /* C API bitmask */
	} capabilities;

	struct list devices;
};

OBJECT_DECLARE_GETTER(eis_seat, id, object_id_t);
OBJECT_DECLARE_GETTER(eis_seat, version, uint32_t);
OBJECT_DECLARE_GETTER(eis_seat, proto_object, const struct brei_object *);
OBJECT_DECLARE_GETTER(eis_seat, interface, const struct eis_seat_interface *);

void
eis_seat_bind(struct eis_seat *seat, uint32_t cap);

void
eis_seat_drop(struct eis_seat *seat);
