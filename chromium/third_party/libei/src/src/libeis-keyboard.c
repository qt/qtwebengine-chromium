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
 * FROM, OUT OF OR IN keyboard WITH THE SOFTWARE OR THE USE OR OTHER
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
eis_keyboard_destroy(struct eis_keyboard *keyboard)
{
	struct eis_client * client = eis_keyboard_get_client(keyboard);
	eis_client_unregister_object(client, &keyboard->proto_object);
}

OBJECT_IMPLEMENT_REF(eis_keyboard);
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_keyboard);
OBJECT_IMPLEMENT_GETTER_AS_REF(eis_keyboard, proto_object, const struct brei_object *);

static
OBJECT_IMPLEMENT_CREATE(eis_keyboard);
static
OBJECT_IMPLEMENT_PARENT(eis_keyboard, eis_device);

uint32_t
eis_keyboard_get_version(struct eis_keyboard *keyboard)
{
	return keyboard->proto_object.version;
}

object_id_t
eis_keyboard_get_id(struct eis_keyboard *keyboard)
{
	return keyboard->proto_object.id;
}

struct eis_device *
eis_keyboard_get_device(struct eis_keyboard *keyboard)
{
	return eis_keyboard_parent(keyboard);
}

struct eis_client*
eis_keyboard_get_client(struct eis_keyboard *keyboard)
{
	return eis_device_get_client(eis_keyboard_get_device(keyboard));
}

struct eis*
eis_keyboard_get_context(struct eis_keyboard *keyboard)
{
	struct eis_client *client = eis_keyboard_get_client(keyboard);
	return eis_client_get_context(client);
}

const struct eis_keyboard_interface *
eis_keyboard_get_interface(struct eis_keyboard *keyboard) {
	return eis_device_get_keyboard_interface(eis_keyboard_get_device(keyboard));
}

struct eis_keyboard *
eis_keyboard_new(struct eis_device *device)
{
	struct eis_keyboard *keyboard = eis_keyboard_create(&device->object);
	struct eis_client *client = eis_device_get_client(device);

	keyboard->proto_object.id = eis_client_get_new_id(client);
	keyboard->proto_object.implementation = keyboard;
	keyboard->proto_object.interface = &eis_keyboard_proto_interface;
	keyboard->proto_object.version = client->interface_versions.ei_keyboard;
	list_init(&keyboard->proto_object.link);

	eis_client_register_object(client, &keyboard->proto_object);

	return keyboard; /* ref owned by caller */
}
