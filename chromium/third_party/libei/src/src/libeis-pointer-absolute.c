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
 * FROM, OUT OF OR IN pointer_absolute WITH THE SOFTWARE OR THE USE OR OTHER
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
eis_pointer_absolute_destroy(struct eis_pointer_absolute *pointer_absolute)
{
	struct eis_client * client = eis_pointer_absolute_get_client(pointer_absolute);
	eis_client_unregister_object(client, &pointer_absolute->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_pointer_absolute);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_pointer_absolute);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_pointer_absolute, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_pointer_absolute);
static
OBJECT_IMPLEMENT_PARENT(eis_pointer_absolute, eis_device);

uint32_t
eis_pointer_absolute_get_version(struct eis_pointer_absolute *pointer_absolute)
{
	return pointer_absolute->proto_object.version;
}

object_id_t
eis_pointer_absolute_get_id(struct eis_pointer_absolute *pointer_absolute)
{
	return pointer_absolute->proto_object.id;
}

struct eis_device *
eis_pointer_absolute_get_device(struct eis_pointer_absolute *pointer_absolute)
{
	return eis_pointer_absolute_parent(pointer_absolute);
}

struct eis_client*
eis_pointer_absolute_get_client(struct eis_pointer_absolute *pointer_absolute)
{
	return eis_device_get_client(eis_pointer_absolute_get_device(pointer_absolute));
}

struct eis*
eis_pointer_absolute_get_context(struct eis_pointer_absolute *pointer_absolute)
{
	struct eis_client *client = eis_pointer_absolute_get_client(pointer_absolute);
	return eis_client_get_context(client);
}

const struct eis_pointer_absolute_interface *
eis_pointer_absolute_get_interface(struct eis_pointer_absolute *pointer_absolute) {
	return eis_device_get_pointer_absolute_interface(
		eis_pointer_absolute_get_device(pointer_absolute));
}

struct eis_pointer_absolute *
eis_pointer_absolute_new(struct eis_device *device)
{
	struct eis_pointer_absolute *pointer_absolute = eis_pointer_absolute_create(&device->object);
	struct eis_client *client = eis_device_get_client(device);

	pointer_absolute->proto_object.id = eis_client_get_new_id(client);
	pointer_absolute->proto_object.implementation = pointer_absolute;
	pointer_absolute->proto_object.interface = &eis_pointer_absolute_proto_interface;
	pointer_absolute->proto_object.version = client->interface_versions.ei_pointer_absolute;
	list_init(&pointer_absolute->proto_object.link);

	eis_client_register_object(client, &pointer_absolute->proto_object);

	return pointer_absolute; /* ref owned by caller */
}
