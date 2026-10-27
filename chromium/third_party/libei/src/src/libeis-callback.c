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
 * FROM, OUT OF OR IN callback WITH THE SOFTWARE OR THE USE OR OTHER
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
eis_callback_destroy(struct eis_callback *callback)
{
	struct eis_client * client = eis_callback_get_client(callback);
	eis_client_unregister_object(client, &callback->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_callback);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_callback);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_callback, proto_object, const struct brei_object *);
OBJECT_IMPLEMENT_GETTER(eis_callback, user_data, void *);
OBJECT_IMPLEMENT_SETTER(eis_callback, user_data, void *);

static
OBJECT_IMPLEMENT_CREATE(eis_callback);
static
OBJECT_IMPLEMENT_PARENT(eis_callback, eis_client);

struct eis_client*
eis_callback_get_client(struct eis_callback *callback)
{
	return eis_callback_parent(callback);
}

struct eis*
eis_callback_get_context(struct eis_callback *callback)
{
	struct eis_client *client = eis_callback_parent(callback);
	return eis_client_get_context(client);
}

object_id_t
eis_callback_get_id(struct eis_callback *callback)
{
	return callback->proto_object.id;
}

uint32_t
eis_callback_get_version(struct eis_callback *callback)
{
	return callback->proto_object.version;
}

static const struct eis_callback_interface interface = {
};

const struct eis_callback_interface *
eis_callback_get_interface(struct eis_callback *callback) {
	return &interface;
}

struct eis_callback *
eis_callback_new(struct eis_client *client, uint32_t new_id, uint32_t version)
{
	struct eis_callback *callback = eis_callback_create(&client->object);

	callback->proto_object.id = new_id;
	callback->proto_object.implementation = callback;
	callback->proto_object.interface = &eis_callback_proto_interface;
	callback->proto_object.version = version;
	list_init(&callback->proto_object.link);

	eis_client_register_object(client, &callback->proto_object);

	return callback; /* ref owned by caller */
}
