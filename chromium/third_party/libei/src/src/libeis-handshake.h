
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

#include "util-object.h"
#include "brei-shared.h"
#include "libeis-client.h"

struct eis;
struct eis_client;

/* This is a protocol-only object, not exposed in the API */
struct eis_handshake {
	struct object object;
	struct brei_object proto_object;

	uint32_t version;
	char *name;
	bool is_sender;

	struct eis_client_interface_versions client_versions;
	struct eis_client_interface_versions server_versions;
};

OBJECT_DECLARE_GETTER(eis_handshake, context, struct eis *);
OBJECT_DECLARE_GETTER(eis_handshake, client, struct eis_client *);
OBJECT_DECLARE_GETTER(eis_handshake, id, object_id_t);
OBJECT_DECLARE_GETTER(eis_handshake, version, uint32_t);
OBJECT_DECLARE_GETTER(eis_handshake, proto_object, const struct brei_object *);
OBJECT_DECLARE_GETTER(eis_handshake, interface, const struct eis_handshake_interface *);
OBJECT_DECLARE_REF(eis_handshake);
OBJECT_DECLARE_UNREF(eis_handshake);

struct eis_handshake *
eis_handshake_new(struct eis_client *client,
			 const struct eis_client_interface_versions *versions);
