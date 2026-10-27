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
eis_pingpong_destroy(struct eis_pingpong *pingpong)
{
	struct eis_client * client = eis_pingpong_get_client(pingpong);
	eis_client_unregister_object(client, &pingpong->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_pingpong);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_pingpong);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_pingpong, proto_object, const struct brei_object *);
OBJECT_IMPLEMENT_GETTER(eis_pingpong, user_data, void *);
OBJECT_IMPLEMENT_SETTER(eis_pingpong, user_data, void *);

static
OBJECT_IMPLEMENT_CREATE(eis_pingpong);
static
OBJECT_IMPLEMENT_PARENT(eis_pingpong, eis_client);

struct eis_client*
eis_pingpong_get_client(struct eis_pingpong *pingpong)
{
	return eis_pingpong_parent(pingpong);
}

struct eis*
eis_pingpong_get_context(struct eis_pingpong *pingpong)
{
	struct eis_client *client = eis_pingpong_parent(pingpong);
	return eis_client_get_context(client);
}

object_id_t
eis_pingpong_get_id(struct eis_pingpong *pingpong)
{
	return pingpong->proto_object.id;
}

uint32_t
eis_pingpong_get_version(struct eis_pingpong *pingpong)
{
	return pingpong->proto_object.version;
}

static struct brei_result *
client_msg_done(struct eis_pingpong *pingpong, uint64_t pingpong_data)
{
	pingpong->func(pingpong, pingpong->pingpong_data, pingpong_data);
	return NULL;
}

static const struct eis_pingpong_interface interface = {
	.done = client_msg_done,
};

const struct eis_pingpong_interface *
eis_pingpong_get_interface(struct eis_pingpong *pingpong) {
	return &interface;
}

struct eis_pingpong *
eis_pingpong_new(struct eis_client *client, eis_pingpong_func func, void *pingpong_data)
{
	struct eis_pingpong *pingpong = eis_pingpong_create(&client->object);

	pingpong->proto_object.id = eis_client_get_new_id(client);
	pingpong->proto_object.implementation = pingpong;
	pingpong->proto_object.interface = &eis_pingpong_proto_interface;
	pingpong->proto_object.version = VERSION_V(1); /* FIXME */
	pingpong->pingpong_data = pingpong_data;
	eis_client_register_object(client, &pingpong->proto_object);

	pingpong->func = func;
	list_init(&pingpong->link);

	return pingpong; /* ref owned by caller */
}
