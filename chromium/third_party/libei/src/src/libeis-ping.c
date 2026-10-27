/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2024 Red Hat, Inc.
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

#include <stdio.h>

#include "util-mem.h"
#include "util-macros.h"
#include "util-object.h"
#include "util-list.h"
#include "brei-shared.h"

#include "libeis-private.h"
#include "libeis-connection.h"

struct eis_ping {
	struct object object;
	uint64_t id;
	void *user_data;

	struct eis_client *client;
	bool is_pending;
	bool is_done;
};

static void
eis_ping_destroy(struct eis_ping *ping)
{
	if (!ping->is_pending)
		eis_client_unref(ping->client);
}

static
OBJECT_IMPLEMENT_CREATE(eis_ping);

_public_
OBJECT_IMPLEMENT_REF(eis_ping);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_ping);
_public_
OBJECT_IMPLEMENT_GETTER(eis_ping, id, uint64_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_ping, user_data, void*);
_public_
OBJECT_IMPLEMENT_SETTER(eis_ping, user_data, void*);
static
OBJECT_IMPLEMENT_GETTER(eis_ping, client, struct eis_client*);

_public_ struct eis_ping *
eis_client_new_ping(struct eis_client *client)
{
	static uint64_t id = 0;

	struct eis_ping *ping = eis_ping_create(NULL);
	ping->id = ++id;
	/* Ref our context while it's pending (i.e. only the caller has the ref).
	 * Once it's pending we no longer need the ref.
	 */
	ping->client = eis_client_ref(client);
	ping->is_pending = false;
	ping->is_done = false;

	return ping;
}

static void
on_pong(struct eis_connection_ping_callback *callback)
{
	struct eis_ping *ping = eis_connection_ping_callback_get_user_data(callback);
	ping->is_done = true;

	struct eis_client *client = eis_connection_ping_callback_get_client(callback);
	eis_queue_pong_event(client, ping);
	/* eis_ping ref is removed in on_destroy */
}

static void
on_destroy(struct eis_connection_ping_callback *callback)
{
	/* This is only called if we never receisved a pong */
	_unref_(eis_ping) *ping = eis_connection_ping_callback_get_user_data(callback);

	/* We never got a pong because we got disconnected. Queue a fake pong event */
	if (!ping->is_done) {
		struct eis_client *client = eis_connection_ping_callback_get_client(callback);
		eis_queue_pong_event(client, ping);
	}
}

_public_ void
eis_ping(struct eis_ping *ping)
{
	struct eis_client *client = eis_ping_get_client(ping);

	eis_client_unref(client);
	ping->client = client;
	ping->is_pending = true;

	_unref_(eis_connection_ping_callback) *cb = eis_connection_ping_callback_new(client->connection,
										     on_pong,
										     on_destroy,
										     eis_ping_ref(ping));
	eis_connection_ping(client->connection, cb);
}
