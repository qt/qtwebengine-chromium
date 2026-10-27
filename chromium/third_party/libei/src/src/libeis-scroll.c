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
 * FROM, OUT OF OR IN scroll WITH THE SOFTWARE OR THE USE OR OTHER
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
eis_scroll_destroy(struct eis_scroll *scroll)
{
	struct eis_client * client = eis_scroll_get_client(scroll);
	eis_client_unregister_object(client, &scroll->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_scroll);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_scroll);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_scroll, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_scroll);
static
OBJECT_IMPLEMENT_PARENT(eis_scroll, eis_device);

uint32_t
eis_scroll_get_version(struct eis_scroll *scroll)
{
	return scroll->proto_object.version;
}

object_id_t
eis_scroll_get_id(struct eis_scroll *scroll)
{
	return scroll->proto_object.id;
}

struct eis_device *
eis_scroll_get_device(struct eis_scroll *scroll)
{
	return eis_scroll_parent(scroll);
}

struct eis_client*
eis_scroll_get_client(struct eis_scroll *scroll)
{
	return eis_device_get_client(eis_scroll_get_device(scroll));
}

struct eis*
eis_scroll_get_context(struct eis_scroll *scroll)
{
	struct eis_client *client = eis_scroll_get_client(scroll);
	return eis_client_get_context(client);
}

const struct eis_scroll_interface *
eis_scroll_get_interface(struct eis_scroll *scroll) {
	return eis_device_get_scroll_interface(eis_scroll_get_device(scroll));
}

struct eis_scroll *
eis_scroll_new(struct eis_device *device)
{
	struct eis_scroll *scroll = eis_scroll_create(&device->object);
	struct eis_client *client = eis_device_get_client(device);

	scroll->proto_object.id = eis_client_get_new_id(client);
	scroll->proto_object.implementation = scroll;
	scroll->proto_object.interface = &eis_scroll_proto_interface;
	scroll->proto_object.version = client->interface_versions.ei_scroll;
	list_init(&scroll->proto_object.link);

	eis_client_register_object(client, &scroll->proto_object);

	return scroll; /* ref owned by caller */
}
