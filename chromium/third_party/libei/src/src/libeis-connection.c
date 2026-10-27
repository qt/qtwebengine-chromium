/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Red Hat, Inc.
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
#include <stdbool.h>

#include "util-bits.h"
#include "util-macros.h"
#include "util-mem.h"
#include "util-io.h"
#include "util-strings.h"
#include "util-version.h"

#include "libeis-private.h"
#include "eis-proto.h"

static void
eis_connection_destroy(struct eis_connection *connection)
{
	struct eis_client *client = eis_connection_get_client(connection);
	eis_client_unregister_object(client, &connection->proto_object);

	/* Should be a noop */
	eis_connection_remove_pending_callbacks(connection);
}

OBJECT_IMPLEMENT_REF(eis_connection);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_connection);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_connection, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_connection);
static
OBJECT_IMPLEMENT_PARENT(eis_connection, eis_client);

uint32_t
eis_connection_get_version(struct eis_connection *connection)
{
	return connection->proto_object.version;
}

object_id_t
eis_connection_get_id(struct eis_connection *connection)
{
	return connection->proto_object.id;
}

struct eis_client*
eis_connection_get_client(struct eis_connection *connection)
{
	return eis_connection_parent(connection);
}

struct eis*
eis_connection_get_context(struct eis_connection *connection)
{
	struct eis_client *client = eis_connection_parent(connection);
	return eis_client_get_context(client);
}

const struct eis_connection_interface *
eis_connection_get_interface(struct eis_connection *connection) {
	struct eis_client *client = eis_connection_parent(connection);
	return eis_client_get_interface(client);
}

struct eis_connection *
eis_connection_new(struct eis_client *client)
{
	struct eis_connection *connection = eis_connection_create(&client->object);

	connection->proto_object.id = eis_client_get_new_id(client);
	connection->proto_object.implementation = connection;
	connection->proto_object.interface = &eis_connection_proto_interface;
	connection->proto_object.version = client->interface_versions.ei_connection;
	eis_client_register_object(client, &connection->proto_object);

	list_init(&connection->pending_pingpongs);


	return connection; /* ref owned by caller */
}

void
eis_connection_remove_pending_callbacks(struct eis_connection *connection)
{
	struct eis_callback *cb;
	list_for_each_safe(cb, &connection->pending_pingpongs, link) {
		list_remove(&cb->link);
		eis_connection_ping_callback_unref(cb->user_data);
		eis_callback_unref(cb);
	}
}

static void
eis_connection_ping_callback_destroy(struct eis_connection_ping_callback *callback)
{
	if (callback->destroy)
		callback->destroy(callback);
}

static
OBJECT_IMPLEMENT_CREATE(eis_connection_ping_callback);
OBJECT_IMPLEMENT_REF(eis_connection_ping_callback);
OBJECT_IMPLEMENT_UNREF(eis_connection_ping_callback);
OBJECT_IMPLEMENT_GETTER(eis_connection_ping_callback, user_data, void *);
static
OBJECT_IMPLEMENT_PARENT(eis_connection_ping_callback, eis_connection);

struct eis_connection *
eis_connection_ping_callback_get_connection(struct eis_connection_ping_callback *callback)
{
	return eis_connection_ping_callback_parent(callback);
}

struct eis_client *
eis_connection_ping_callback_get_client(struct eis_connection_ping_callback *callback)
{
	struct eis_connection *connection = eis_connection_ping_callback_get_connection(callback);
	return eis_connection_get_client(connection);
}

struct eis *
eis_connection_ping_callback_get_context(struct eis_connection_ping_callback *callback)
{
	struct eis_connection *connection = eis_connection_ping_callback_get_connection(callback);
	return eis_connection_get_context(connection);
}

struct eis_connection_ping_callback *
eis_connection_ping_callback_new(struct eis_connection *connection,
				 eis_connection_ping_callback_done_t done,
				 eis_connection_ping_callback_destroy_t destroy,
				 void *user_data)
{
	struct eis_connection_ping_callback *callback = eis_connection_ping_callback_create(&connection->object);
	callback->done = done;
	callback->destroy = destroy;
	callback->user_data = user_data;
	return callback;
}

static void
pingpong_callback(struct eis_pingpong *pingpong, void *pingpong_data, uint64_t proto_data)
{
	_unref_(eis_connection_ping_callback) *data = eis_pingpong_get_user_data(pingpong);
	if (data->done)
		data->done(data);

	/* remove from pending callbacks */
	list_remove(&pingpong->link);
	eis_pingpong_unref(pingpong);
}

void
eis_connection_ping(struct eis_connection *connection,
		    struct eis_connection_ping_callback *cb)
{
	struct eis_client *client = eis_connection_get_client(connection);

	/* This is double-wrapped because we only use this for debugging purposes for
	 * now. The actual callback calls pingpong_callback with our connection,
	 * then we extract the user_data on the object and call into the
	 * cb supplied to this function.
	 */
	struct eis_pingpong *pingpong = eis_pingpong_new(client, pingpong_callback, connection);
	eis_pingpong_set_user_data(pingpong, eis_connection_ping_callback_ref(cb));
	list_append(&connection->pending_pingpongs, &pingpong->link);

	eis_connection_event_ping(connection, eis_pingpong_get_id(pingpong),
				  eis_pingpong_get_version(pingpong));
}
