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
eis_handshake_destroy(struct eis_handshake *setup)
{
	struct eis_client * client = eis_handshake_get_client(setup);
	eis_client_unregister_object(client, &setup->proto_object);

	free(setup->name);
}

OBJECT_IMPLEMENT_REF(eis_handshake);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_handshake);
OBJECT_IMPLEMENT_GETTER(eis_handshake, version, uint32_t);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_handshake, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_handshake);
static
OBJECT_IMPLEMENT_PARENT(eis_handshake, eis_client);

struct eis_client*
eis_handshake_get_client(struct eis_handshake *setup)
{
	return eis_handshake_parent(setup);
}

struct eis*
eis_handshake_get_context(struct eis_handshake *setup)
{
	struct eis_client *client = eis_handshake_parent(setup);
	return eis_client_get_context(client);
}

object_id_t
eis_handshake_get_id(struct eis_handshake *setup)
{
	return setup->proto_object.id;
}

static void
on_pong(struct eis_connection_ping_callback *callback)
{
	struct eis_client *client = eis_connection_ping_callback_get_client(callback);
	eis_queue_connect_event(client);
}

static struct brei_result*
client_msg_handshake_version(struct eis_handshake *setup, uint32_t version)
{
	struct eis_client *client = eis_handshake_get_client(setup);
	struct eis *eis = eis_client_get_context(client);

	log_debug(eis, "client %#x supports handshake version %u", client->id, version);

	if (version == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_VALUE, "Invalid handshake version %u", version);

	if (setup->client_versions.ei_handshake != 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Duplicate handshake version");
	if (version > setup->server_versions.ei_handshake)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Invalid handshake version %ud", version);

	setup->client_versions.ei_handshake = min(setup->server_versions.ei_handshake, version);

	return 0;
}

static struct brei_result *
client_msg_finish(struct eis_handshake *setup)
{
	struct eis_client *client = eis_handshake_get_client(setup);

	/* Required interfaces - immediate disconnection if missing */
	if (setup->client_versions.ei_handshake == 0 ||
	    setup->client_versions.ei_connection == 0 ||
	    setup->client_versions.ei_callback == 0 ||
	    setup->client_versions.ei_pingpong == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Missing versions for required interfaces");

	/* ei_callback needs a client-created object, so we must tell the client
	 * about our version. But for convenience and to make sure this all works
	 * send all our versions down the wire */
#define SEND_INTERFACE_VERSION(upper_name_, lower_name_) \
	eis_handshake_event_interface_version(setup, upper_name_ ##_INTERFACE_NAME, \
					      setup->client_versions.lower_name_);

	SEND_INTERFACE_VERSION(EIS_CALLBACK, ei_callback);
	SEND_INTERFACE_VERSION(EIS_CONNECTION, ei_connection);
	SEND_INTERFACE_VERSION(EIS_PINGPONG, ei_pingpong);
	SEND_INTERFACE_VERSION(EIS_SEAT, ei_seat);
	SEND_INTERFACE_VERSION(EIS_DEVICE, ei_device);
	SEND_INTERFACE_VERSION(EIS_POINTER, ei_pointer);
	SEND_INTERFACE_VERSION(EIS_POINTER_ABSOLUTE, ei_pointer_absolute);
	SEND_INTERFACE_VERSION(EIS_BUTTON, ei_button);
	SEND_INTERFACE_VERSION(EIS_KEYBOARD, ei_keyboard);
	SEND_INTERFACE_VERSION(EIS_TOUCHSCREEN, ei_touchscreen);

#undef SEND_INTERFACE_VERSION

	eis_client_setup_done(client, setup->name, setup->is_sender, &setup->client_versions);

	client->connection = eis_connection_new(client);
	eis_handshake_event_connection(setup,
				       eis_client_get_next_serial(client),
				       eis_connection_get_id(client->connection),
				       eis_connection_get_version(client->connection));

	/* These aren't required but libei is pointless without them, so let's enforce them
	 * by establishing the connection and immediately sending the disconnect */
	if (setup->client_versions.ei_seat == 0 ||
	    setup->client_versions.ei_device == 0) {
		eis_client_disconnect(client);
	} else {
		_unref_(eis_connection_ping_callback) *cb = eis_connection_ping_callback_new(client->connection,
											     on_pong,
											     NULL,
											     NULL);
		/* Force a ping/pong. This isn't necessary but it doesn't hurt much here
		 * and it ensures that any client implementation doesn't have that part missing */
		eis_connection_ping(client->connection, cb);
	}

	client->setup = eis_handshake_unref(setup);

	return NULL;
}

static struct brei_result *
client_msg_name(struct eis_handshake *setup, const char *name)
{
	if (setup->client_versions.ei_handshake == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Missing handshake versions");

	if (setup->name)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL, "Duplicate client name");

	setup->name = xstrdup(name);

	return 0;
}

static struct brei_result *
client_msg_context_type(struct eis_handshake *setup, uint32_t type)
{
	if (setup->client_versions.ei_handshake == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Missing handshake versions");

	switch(type) {
	case EIS_HANDSHAKE_CONTEXT_TYPE_SENDER:
		setup->is_sender = true;
		return NULL;
	case EIS_HANDSHAKE_CONTEXT_TYPE_RECEIVER:
		setup->is_sender = false;
		return NULL;
	}

	return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_VALUE, "Invalid context type %u", type);
}

static struct brei_result *
client_msg_interface_version(struct eis_handshake *setup, const char *name, uint32_t version)
{
	if (setup->client_versions.ei_handshake == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "Missing handshake versions");

	if (streq(name, EIS_HANDSHAKE_INTERFACE_NAME))
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
				       "%s may not be used in interface_version", name);

	struct eis_client *client = eis_handshake_get_client(setup);
	struct eis *eis = eis_client_get_context(client);
	struct v {
		const char *name;
		uint32_t* client_version;
		uint32_t* server_version;
	} version_map[] = {
#define VERSION_ENTRY(name_) { \
	.name = #name_, \
	.client_version = &setup->client_versions.name_, \
	.server_version = &setup->server_versions.name_, \
}
		VERSION_ENTRY(ei_callback),
		VERSION_ENTRY(ei_pingpong),
		VERSION_ENTRY(ei_connection),
		/* ei_handshake is not handled here */
		VERSION_ENTRY(ei_seat),
		VERSION_ENTRY(ei_device),
		VERSION_ENTRY(ei_pointer),
		VERSION_ENTRY(ei_pointer_absolute),
		VERSION_ENTRY(ei_button),
		VERSION_ENTRY(ei_scroll),
		VERSION_ENTRY(ei_keyboard),
		VERSION_ENTRY(ei_touchscreen),
#undef VERSION_ENTRY
	};

	log_debug(eis, "client %#x supports %s version %u", client->id, name, version);

	if (version == 0)
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_VALUE, "Invalid %s version %u", name, version);

	struct v *v;
	ARRAY_FOR_EACH(version_map, v) {
		if (streq(v->name, name)) {
			/* Versions must not be set twice */
			if (*v->client_version != 0)
				return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL,
						       "Duplicate %s version", name);
			*v->client_version = min(*v->server_version, version);
			return 0;
		}
	}

	/* Unknown interfaces are ignored */

	return 0;
}

static const struct eis_handshake_interface interface = {
	.handshake_version = client_msg_handshake_version,
	.finish = client_msg_finish,
	.context_type = client_msg_context_type,
	.name = client_msg_name,
	.interface_version = client_msg_interface_version,
};

const struct eis_handshake_interface *
eis_handshake_get_interface(struct eis_handshake *setup) {
	return &interface;
}

struct eis_handshake *
eis_handshake_new(struct eis_client *client,
			 const struct eis_client_interface_versions *versions)
{
	struct eis_handshake *setup = eis_handshake_create(&client->object);

	setup->proto_object.id = 0;
	setup->proto_object.implementation = setup;
	setup->proto_object.interface = &eis_handshake_proto_interface;
	/* This object is always v1 until the client tells us otherwise */
	setup->proto_object.version = VERSION_V(1);
	list_init(&setup->proto_object.link);

	setup->version = VERSION_V(1); /* our ei-handshake version */
	setup->server_versions = *versions;

	eis_client_register_object(client, &setup->proto_object);
	eis_handshake_event_handshake_version(setup, versions->ei_handshake);

	return setup; /* ref owned by caller */
}
