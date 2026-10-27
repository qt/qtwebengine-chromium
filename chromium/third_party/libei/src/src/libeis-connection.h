
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

#include "util-mem.h"
#include "util-object.h"
#include "brei-shared.h"

struct eis;
struct eis_client;
struct eis_connection_ping_callback;

/* This is a protocol-only object, not exposed in the API */
struct eis_connection {
	struct object object;
	struct brei_object proto_object;

	struct list pending_pingpongs;
};

OBJECT_DECLARE_GETTER(eis_connection, context, struct eis *);
OBJECT_DECLARE_GETTER(eis_connection, id, object_id_t);
OBJECT_DECLARE_GETTER(eis_connection, version, uint32_t);
OBJECT_DECLARE_GETTER(eis_connection, client, struct eis_client *);
OBJECT_DECLARE_GETTER(eis_connection, proto_object, const struct brei_object *);
OBJECT_DECLARE_GETTER(eis_connection, interface, const struct eis_connection_interface *);
OBJECT_DECLARE_REF(eis_connection);
OBJECT_DECLARE_UNREF(eis_connection);

struct eis_connection *
eis_connection_new(struct eis_client *client);

void
eis_connection_remove_pending_callbacks(struct eis_connection *connection);

/**
 * Called when the ei_callback.done request is received after
 * an ei_connection_ping() event.
 */
typedef void (*eis_connection_ping_callback_t)(struct eis_connection *connection,
					       void *user_data);

/**
 * Called when the ei_callback.done event is received after
 * an eis_connection_ping() request.
 */
typedef void (*eis_connection_ping_callback_done_t)(struct eis_connection_ping_callback *callback);

/**
 * Called for each registered callback when the last reference to it is
 * destroyed. This should be used to clean up user_data, if need be.
 *
 * This function is always called, even if disconnected and the done() function is never called.
 */
typedef void (*eis_connection_ping_callback_destroy_t)(struct eis_connection_ping_callback *callback);

struct eis_connection_ping_callback {
	struct object object; /* parent is struct eis */
	eis_connection_ping_callback_done_t done;
	eis_connection_ping_callback_destroy_t destroy;
	void *user_data;
};

struct eis_connection_ping_callback *
eis_connection_ping_callback_new(struct eis_connection *connection,
				 eis_connection_ping_callback_done_t done,
				 eis_connection_ping_callback_destroy_t destroy,
				 void *user_data);

OBJECT_DECLARE_REF(eis_connection_ping_callback);
OBJECT_DECLARE_UNREF(eis_connection_ping_callback);
OBJECT_DECLARE_GETTER(eis_connection_ping_callback, connection, struct eis_connection *);
OBJECT_DECLARE_GETTER(eis_connection_ping_callback, client, struct eis_client *);
OBJECT_DECLARE_GETTER(eis_connection_ping_callback, context, struct eis *);
OBJECT_DECLARE_GETTER(eis_connection_ping_callback, user_data, void*);
DEFINE_UNREF_CLEANUP_FUNC(eis_connection_ping_callback);

void
eis_connection_ping(struct eis_connection *connection, struct eis_connection_ping_callback *callback);
