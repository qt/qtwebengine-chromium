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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "util-bits.h"
#include "util-io.h"
#include "util-macros.h"
#include "util-mem.h"
#include "util-sources.h"
#include "util-strings.h"
#include "util-structs.h"
#include "util-tristate.h"
#include "util-time.h"
#include "util-version.h"

#include "libeis-private.h"
#include "brei-shared.h"
#include "eis-proto.h"

DEFINE_TRISTATE(started, finished, connected);
DEFINE_UNREF_CLEANUP_FUNC(brei_result);

static void
client_drop_seats(struct eis_client *client);

static void
eis_client_destroy(struct eis_client *client)
{
	client_drop_seats(client);
	eis_handshake_unref(client->setup);
	eis_connection_unref(client->connection);
	free(client->name);
	brei_context_unref(client->brei);
	source_remove(client->source);
	source_unref(client->source);
	list_remove(&client->link);
}

static
OBJECT_IMPLEMENT_CREATE(eis_client);
static
OBJECT_IMPLEMENT_PARENT(eis_client, eis);

_public_
OBJECT_IMPLEMENT_REF(eis_client);

_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_client);

_public_
OBJECT_IMPLEMENT_GETTER(eis_client, name, const char*);
_public_
OBJECT_IMPLEMENT_SETTER(eis_client, user_data, void*);
_public_
OBJECT_IMPLEMENT_GETTER(eis_client, user_data, void*);

uint32_t
eis_client_get_next_serial(struct eis_client *client)
{
	return ++client->serial;
}

void
eis_client_update_client_serial(struct eis_client *client, uint32_t serial)
{
	client->last_client_serial = serial;
}

_public_ struct eis*
eis_client_get_context(struct eis_client *client)
{
	return eis_client_parent(client);
}

const struct brei_object *
eis_client_get_proto_object(struct eis_client *client)
{
	return &client->connection->proto_object;
}

object_id_t
eis_client_get_new_id(struct eis_client *client)
{
	static const uint64_t mask = -1 >> 8;
	static const uint64_t offset = 0xff00000000000000;
	return offset | (client->next_object_id++ & mask);
}

bool
eis_client_update_client_object_id(struct eis_client *client, object_id_t id)
{
	if (id <= client->last_client_object_id)
		return false;

	client->last_client_object_id = id;
	return true;
}

void
eis_client_register_object(struct eis_client *client, struct brei_object *object)
{
	struct eis *eis = eis_client_get_context(client);
	log_debug(eis, "registering %s v%u object %#" PRIx64 "", object->interface->name, object->version, object->id);
	list_append(&client->proto_objects, &object->link);
}

void
eis_client_unregister_object(struct eis_client *client, struct brei_object *object)
{
	struct eis *eis = eis_client_get_context(client);
	log_debug(eis, "deregistering %s v%u object %#" PRIx64 "", object->interface->name, object->version, object->id);
	list_remove(&object->link);
}

struct eis_client *
eis_client_get_client(struct eis_client *client)
{
	return client;
}

_public_ bool
eis_client_is_sender(struct eis_client *client)
{
	return client->is_sender;
}

int
eis_client_send_message(struct eis_client *client, const struct brei_object *object,
			uint32_t opcode, const char *signature, size_t nargs, ...)
{
	struct eis *eis = eis_client_get_context(client);

	log_debug(eis, "sending: object %#" PRIx64 " (%s@v%u:%s(%u)) signature '%s'",
	   object->id,
	   object->interface->name,
	   object->interface->version,
	   object->interface->events[opcode].name,
	   opcode,
	   signature);

	va_list args;
	va_start(args, nargs);
	_unref_(brei_result) *result = brei_marshal_message(client->brei,
						            object->id,
							    opcode, signature,
							    nargs, args);
	va_end(args);

	if (brei_result_get_reason(result) != 0) {
		log_warn(eis, "failed to marshal message: %s",
			 brei_result_get_explanation(result));
		return -EBADMSG;
	}

	_cleanup_iobuf_ struct iobuf *buf = brei_result_get_data(result);
	assert(buf);
	int fd = source_get_fd(client->source);
	int rc = iobuf_send(buf, fd);
	return rc < 0 ? rc : 0;
}

static int
client_send_seat_added(struct eis_client *client, struct eis_seat *seat)
{
	/* Client didn't announce ei_seat */
	if (client->interface_versions.ei_seat == 0)
		return 0;

	return eis_connection_event_seat(client->connection, eis_seat_get_id(seat),
					 eis_seat_get_version(seat));
}

_public_ void
eis_client_connect(struct eis_client *client)
{
	switch(client->state) {
	case EIS_CLIENT_STATE_DISCONNECTED:
		return;
	case EIS_CLIENT_STATE_CONNECTING:
		break;
	default:
		log_bug_client(eis_client_get_context(client),
			       "%s: client already connected", __func__);
		return;
	}
	client->state = EIS_CLIENT_STATE_CONNECTED;
}

static void
client_drop_seats(struct eis_client *client)
{
	struct eis_seat *s;
	list_for_each_safe(s, &client->seats, link) {
		eis_seat_drop(s);
	}
}

static void
client_disconnect(struct eis_client *client,
		  enum eis_connection_disconnect_reason reason,
		  const char *explanation)
{
	switch(client->state) {
	case EIS_CLIENT_STATE_DISCONNECTED:
		/* Client already disconnected? don't bother sending an
		 * event */
		return;
	case EIS_CLIENT_STATE_REQUESTED_DISCONNECT:
		/* Drop the seats again (see client_msg_disconnect) because
		 * the server may have added seats between the client requesting the
		 * disconnect and EIS actually processing that event
		 */
		client_drop_seats(client);
		/* DISCONNECTED event is already in the EIS queue */
		/* Must not send disconnected to the client */
		client->connection = eis_connection_unref(client->connection);
		client->state = EIS_CLIENT_STATE_DISCONNECTED;
		source_remove(client->source);
		break;
	case EIS_CLIENT_STATE_CONNECTING:
	case EIS_CLIENT_STATE_CONNECTED:
		client_drop_seats(client);
		eis_connection_remove_pending_callbacks(client->connection);
		eis_queue_disconnect_event(client);
		eis_connection_event_disconnected(client->connection,
						  client->last_client_serial,
						  reason, explanation);
		client->connection = eis_connection_unref(client->connection);
		client->state = EIS_CLIENT_STATE_DISCONNECTED;
		source_remove(client->source);
		break;
	case EIS_CLIENT_STATE_NEW:
		client->state = EIS_CLIENT_STATE_DISCONNECTED;
		source_remove(client->source);
		break;
	}

	eis_client_unref(client);
}

_public_ void
eis_client_disconnect(struct eis_client *client)
{
	client_disconnect(client, EIS_CONNECTION_DISCONNECT_REASON_DISCONNECTED, NULL);
}

void
eis_client_disconnect_with_reason(struct eis_client *client,
				  enum eis_connection_disconnect_reason reason,
				  const char *explanation)
{
	client_disconnect(client, reason, explanation);
}

void
eis_client_setup_done(struct eis_client *client, const char *name, bool is_sender,
		      const struct eis_client_interface_versions *versions)
{
	client->setup = NULL;	/* connection object cleans itself up */
	client->name = xstrdup(name);
	client->is_sender = is_sender;
	client->interface_versions = *versions;
	client->state = EIS_CLIENT_STATE_CONNECTING;
	/* We don't queue the connect event yet because we send an extra ping/pong
	 * out, just to make sure that part of the protocol works.
	 * See pong() in libeis-client.c
	 */
}

#define DISCONNECT_IF_INVALID_ID(client_, id_) do { \
	if (!brei_is_client_id(id_) || !eis_client_update_client_object_id(client, id_)) { \
		struct eis *_ctx = eis_client_get_context(client_); \
		log_bug_client(_ctx, "Invalid object id %#" PRIx64 ". Disconnecting client", id_); \
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL, "Invalid object id %#" PRIx64 ".", new_id); \
	} \
} while(0)

static struct brei_result *
client_msg_disconnect(struct eis_connection *connection)
{
	struct eis_client * client = eis_connection_get_client(connection);

	/* We need to drop the seats because that unrolls the EIS device state
	 * so that EIS gets the correct DEVICE_REMOVED sequence, etc.
	 * Then queue the DISCONNECTED event and wait for EIS to call
	 * eis_client_disconnect()
	 */
	client_drop_seats(client);
	eis_queue_disconnect_event(client);
	client->state = EIS_CLIENT_STATE_REQUESTED_DISCONNECT;
	return NULL;
}

static struct brei_result *
client_msg_sync(struct eis_connection *connection, object_id_t new_id, uint32_t version)
{
	struct eis_client *client = eis_connection_get_client(connection);

	DISCONNECT_IF_INVALID_ID(client, new_id);
	DISCONNECT_IF_INVALID_VERSION(client, ei_connection, new_id, version);

	eis_queue_sync_event(client, new_id, version);

	return 0;
}

static const struct eis_connection_interface intf_state_new = {
	.sync = client_msg_sync,
	.disconnect = client_msg_disconnect,
};

/* Client is waiting for us, shouldn't send anything except disconnect */
static const struct eis_connection_interface intf_state_connecting = {
	.sync = client_msg_sync,
	.disconnect = client_msg_disconnect,
};

static const struct eis_connection_interface intf_state_connected = {
	.sync = client_msg_sync,
	.disconnect = client_msg_disconnect,
};

static const struct eis_connection_interface *interfaces[] = {
	[EIS_CLIENT_STATE_NEW] = &intf_state_new,
	[EIS_CLIENT_STATE_CONNECTING] = &intf_state_connecting,
	[EIS_CLIENT_STATE_CONNECTED] = &intf_state_connected,
	[EIS_CLIENT_STATE_REQUESTED_DISCONNECT] = NULL,
	[EIS_CLIENT_STATE_DISCONNECTED] = NULL,
};

const struct eis_connection_interface *
eis_client_get_interface(struct eis_client *client)
{
	assert(client->state < ARRAY_LENGTH(interfaces));
	return interfaces[client->state];
}

static int
lookup_object(object_id_t object_id, struct brei_object **object, void *userdata)
{
	struct eis_client *client = userdata;

	struct brei_object *obj;
	list_for_each(obj, &client->proto_objects, link) {
		if (obj->id == object_id) {
			*object = obj;
			return 0;
		}
	}

	log_debug(eis_client_get_context(client), "Failed to find object %#" PRIx64 "", object_id);
	if (client->connection)
		eis_connection_event_invalid_object(client->connection,
						    client->last_client_serial,
						    object_id);

	return -ENOENT;
}

static void
client_dispatch(struct source *source, void *userdata)
{
	_unref_(eis_client) *client = eis_client_ref(userdata);
	enum eis_client_state old_state = client->state;

	_unref_(brei_result) *result = brei_dispatch(client->brei, source_get_fd(source),
						     lookup_object, client);
	if (result) {
		if (old_state != EIS_CLIENT_STATE_REQUESTED_DISCONNECT ||
			brei_result_get_reason(result) != BREI_CONNECTION_DISCONNECT_REASON_TRANSPORT)
			log_warn(eis_client_get_context(client), "Client error: %s",
				 brei_result_get_explanation(result));

		brei_drain_fd(source_get_fd(source));
		eis_client_disconnect_with_reason(client,
						  brei_result_get_reason(result),
						  brei_result_get_explanation(result));
	}

	static const char *client_states[] = {
		"NEW",
		"CONNECTING",
		"CONNECTED",
		"REQUESTED_DISCONNECT",
		"DISCONNECTED",
	};
	if (old_state != client->state) {
		assert(old_state < ARRAY_LENGTH(client_states));
		assert(client->state < ARRAY_LENGTH(client_states));
		log_debug(eis_client_get_context(client), "Client dispatch: %s -> %s",
			  client_states[old_state],
			  client_states[client->state]);
	}

}

struct eis_client *
eis_client_new(struct eis *eis, int fd)
{
	static uint32_t client_id;
	struct eis_client *client = eis_client_create(&eis->object);

	client->brei = brei_context_new(client);
	brei_context_set_log_context(client->brei, eis);
	brei_context_set_log_func(client->brei, (brei_logfunc_t)eis_log_msg_va);

	client->is_sender = true;
	client->id = ++client_id;
	list_init(&client->seats);
	list_init(&client->seats_pending);
	list_init(&client->proto_objects);

	client->interface_versions = (struct eis_client_interface_versions){
		.ei_connection = VERSION_V(1),
		.ei_handshake = VERSION_V(1),
		.ei_callback = VERSION_V(1),
		.ei_pingpong = VERSION_V(1),
		.ei_seat = VERSION_V(1),
		.ei_device = VERSION_V(2),
		.ei_pointer = VERSION_V(1),
		.ei_pointer_absolute = VERSION_V(1),
		.ei_scroll = VERSION_V(1),
		.ei_button = VERSION_V(1),
		.ei_keyboard = VERSION_V(1),
		.ei_touchscreen = VERSION_V(2),
	};
	struct source *s = source_new(fd, client_dispatch, client);
	int rc = sink_add_source(eis->sink, s);

	if (rc != 0) {
		source_unref(s);
		return NULL;
	}

	client->source = source_ref(s);
	client->state = EIS_CLIENT_STATE_NEW;

	eis_add_client(eis, eis_client_ref(client));

	source_unref(s);

	client->setup = eis_handshake_new(client, &client->interface_versions);

	return client;
}

void
eis_client_add_seat(struct eis_client *client, struct eis_seat *seat)
{
	/* remove from the pending list first */
	list_remove(&seat->link);
	/* We own this seat now */
	eis_seat_ref(seat);
	list_append(&client->seats, &seat->link);
	client_send_seat_added(client, seat);
}
