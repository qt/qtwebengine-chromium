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

#include "libeis.h"
#include "brei-shared.h"

#include "util-object.h"
#include "util-list.h"
#include "eis-proto.h"

enum eis_client_state {
	EIS_CLIENT_STATE_NEW,		/* socket just handed over */
	EIS_CLIENT_STATE_CONNECTING,	/* client completed setup but hasn't been accepted yet */
	EIS_CLIENT_STATE_CONNECTED,	/* caller has done eis_client_connect */
	EIS_CLIENT_STATE_REQUESTED_DISCONNECT, /* caller has disconnected */
	EIS_CLIENT_STATE_DISCONNECTED,
};

struct eis_client_interface_versions {
	uint32_t ei_connection;
	uint32_t ei_handshake;
	uint32_t ei_callback;
	uint32_t ei_pingpong;
	uint32_t ei_seat;
	uint32_t ei_device;
	uint32_t ei_pointer;
	uint32_t ei_pointer_absolute;
	uint32_t ei_scroll;
	uint32_t ei_button;
	uint32_t ei_keyboard;
	uint32_t ei_touchscreen;
};

struct eis_client {
	struct object object;
	struct brei_context *brei;
	struct eis_connection *connection;

	struct list proto_objects; /* struct brei_objects list */
	object_id_t next_object_id;
	object_id_t last_client_object_id;

	uint32_t serial;
	uint32_t last_client_serial;

	struct eis_handshake *setup;

	struct eis_client_interface_versions interface_versions;

	void *user_data;
	struct list link;
	struct source *source;
	uint32_t version;
	uint32_t configure_version;
	uint32_t id;
	enum eis_client_state state;
	char *name;
	bool is_sender;

	struct list seats;
	struct list seats_pending;
};

OBJECT_DECLARE_GETTER(eis_client, client, struct eis_client *);
OBJECT_DECLARE_GETTER(eis_client, interface, const struct eis_connection_interface *);
OBJECT_DECLARE_GETTER(eis_client, proto_object, const struct brei_object *);

struct eis_client *
eis_client_new(struct eis *eis, int fd);

uint32_t
eis_client_get_next_serial(struct eis_client *client);

void
eis_client_update_client_serial(struct eis_client *client, uint32_t serial);

object_id_t
eis_client_get_new_id(struct eis_client *client);

bool
eis_client_update_client_object_id(struct eis_client *client, object_id_t id);

void
eis_client_register_object(struct eis_client *client, struct brei_object *object);

void
eis_client_unregister_object(struct eis_client *client, struct brei_object *object);

void
eis_add_client(struct eis *eis, struct eis_client *client);

void
eis_client_setup_done(struct eis_client *client, const char *name, bool is_sender,
		      const struct eis_client_interface_versions *versions);

int
eis_client_send_message(struct eis_client *client, const struct brei_object *obj,
			uint32_t opcode, const char *signature, size_t nargs, ...);

void
eis_client_add_seat(struct eis_client *client, struct eis_seat *seat);

void
eis_client_keyboard_modifiers(struct eis_client *client, struct eis_device *device,
			      uint32_t depressed, uint32_t latched, uint32_t locked,
			      uint32_t group);

void
eis_client_disconnect_with_reason(struct eis_client *client,
				  enum eis_connection_disconnect_reason reason,
				  const char *explanation);
