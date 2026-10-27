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
 * FROM, OUT OF OR IN touchscreen WITH THE SOFTWARE OR THE USE OR OTHER
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
eis_touchscreen_destroy(struct eis_touchscreen *touchscreen)
{
	struct eis_client * client = eis_touchscreen_get_client(touchscreen);
	eis_client_unregister_object(client, &touchscreen->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_touchscreen);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_touchscreen);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_touchscreen, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_touchscreen);
static
OBJECT_IMPLEMENT_PARENT(eis_touchscreen, eis_device);

uint32_t
eis_touchscreen_get_version(struct eis_touchscreen *touchscreen)
{
	return touchscreen->proto_object.version;
}

object_id_t
eis_touchscreen_get_id(struct eis_touchscreen *touchscreen)
{
	return touchscreen->proto_object.id;
}

struct eis_device *
eis_touchscreen_get_device(struct eis_touchscreen *touchscreen)
{
	return eis_touchscreen_parent(touchscreen);
}

struct eis_client*
eis_touchscreen_get_client(struct eis_touchscreen *touchscreen)
{
	return eis_device_get_client(eis_touchscreen_get_device(touchscreen));
}

struct eis*
eis_touchscreen_get_context(struct eis_touchscreen *touchscreen)
{
	struct eis_client *client = eis_touchscreen_get_client(touchscreen);
	return eis_client_get_context(client);
}

const struct eis_touchscreen_interface *
eis_touchscreen_get_interface(struct eis_touchscreen *touchscreen) {
	return eis_device_get_touchscreen_interface(eis_touchscreen_get_device(touchscreen));
}

struct eis_touchscreen *
eis_touchscreen_new(struct eis_device *device)
{
	struct eis_touchscreen *touchscreen = eis_touchscreen_create(&device->object);
	struct eis_client *client = eis_device_get_client(device);

	touchscreen->proto_object.id = eis_client_get_new_id(client);
	touchscreen->proto_object.implementation = touchscreen;
	touchscreen->proto_object.interface = &eis_touchscreen_proto_interface;
	touchscreen->proto_object.version = client->interface_versions.ei_touchscreen;
	list_init(&touchscreen->proto_object.link);

	eis_client_register_object(client, &touchscreen->proto_object);

	return touchscreen; /* ref owned by caller */
}
