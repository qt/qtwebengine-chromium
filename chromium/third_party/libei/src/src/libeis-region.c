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

#include "util-strings.h"
#include "libeis-private.h"

static void
eis_region_destroy(struct eis_region *region)
{
	free(region->mapping_id);
	list_remove(&region->link);
	if (!region->added_to_device)
		eis_device_unref(region->device);
}

_public_
OBJECT_IMPLEMENT_REF(eis_region);
_public_
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_region);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, user_data, void *);
_public_
OBJECT_IMPLEMENT_SETTER(eis_region, user_data, void *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, x, uint32_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, y, uint32_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, width, uint32_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, height, uint32_t);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, physical_scale, double);
_public_
OBJECT_IMPLEMENT_GETTER(eis_region, mapping_id, const char *);

static
OBJECT_IMPLEMENT_CREATE(eis_region);

_public_ struct eis_region *
eis_device_new_region(struct eis_device *device)
{
	switch (device->type) {
	 case EIS_DEVICE_TYPE_VIRTUAL:
		 break;
	 case EIS_DEVICE_TYPE_PHYSICAL:
		 log_bug_client(eis_device_get_context(device), "Regions on physical devices are not supported");
		 return NULL;
	}

	struct eis_region *region = eis_region_create(NULL);

	region->device = eis_device_ref(device);
	region->physical_scale = 1.0;

	list_append(&device->regions_new, &region->link);

	/* initial refcount is owned by caller, so an unref in the caller will
	 * destroy the region immediately */
	return region;
}

_public_ void
eis_region_set_offset(struct eis_region *region, uint32_t x, uint32_t y)
{
	if (region->added_to_device)
		return;

	region->x = x;
	region->y = y;
}

_public_ void
eis_region_set_size(struct eis_region *region, uint32_t w, uint32_t h)
{
	if (region->added_to_device)
		return;

	region->width = w;
	region->height = h;
}

_public_ void
eis_region_set_physical_scale(struct eis_region *region, double scale)
{
	if (region->added_to_device)
		return;

	if (scale > 0.0)
		region->physical_scale = scale;
}

_public_ void
eis_region_set_mapping_id(struct eis_region *region, const char *mapping_id)
{
	if (region->added_to_device)
		return;

	if (mapping_id == NULL) {
		struct eis_device *device = region->device;
		log_bug_client(eis_device_get_context(device),
			       "%s: a region's mapping_id must not be NULL", __func__);
		return;
	}

	region->mapping_id = xstrdup(mapping_id);
}

_public_ void
eis_region_add(struct eis_region *region)
{
	struct eis_device *device = region->device;

	if (device->state != EIS_DEVICE_STATE_NEW) {
		log_bug_client(eis_device_get_context(device),
			       "%s: device already (dis)connected", __func__);
		return;
	}

	if (region->added_to_device)
		return;

	region->added_to_device = true;
	list_remove(&region->link);
	list_append(&device->regions, &region->link);
	/* The device now owns a ref to the region and the region no longer
	 * needs a device ref, it will be cleaned up when the device dies */
	eis_region_ref(region);
	eis_device_unref(region->device);
}

_public_ bool
eis_region_contains(struct eis_region *r, double x, double y)
{
	return (x >= r->x && x < r->x + r->width &&
		y >= r->y && y < r->y + r->height);
}

#ifdef _enable_tests_
#include "util-munit.h"
MUNIT_TEST(test_region_setters)
{
	struct eis_region r = {0};

	eis_region_set_size(&r, 1, 2);
	eis_region_set_offset(&r, 3, 4);
	eis_region_set_physical_scale(&r, 5.6);

	munit_assert_int(eis_region_get_width(&r), ==, 1);
	munit_assert_int(eis_region_get_height(&r), ==, 2);
	munit_assert_int(eis_region_get_x(&r), ==, 3);
	munit_assert_int(eis_region_get_y(&r), ==, 4);
	munit_assert_double(eis_region_get_physical_scale(&r), ==, 5.6);

	return MUNIT_OK;
}

MUNIT_TEST(test_region_contains)
{
	struct eis_region r = {0};

	eis_region_set_size(&r, 100, 200);
	eis_region_set_offset(&r, 300, 400);

	munit_assert_true(eis_region_contains(&r, 300, 400));
	munit_assert_true(eis_region_contains(&r, 399.9, 599.9));

	munit_assert_false(eis_region_contains(&r, 299.9, 400));
	munit_assert_false(eis_region_contains(&r, 300, 399.9));

	munit_assert_false(eis_region_contains(&r, 400.1, 400));
	munit_assert_false(eis_region_contains(&r, 400, 399.9));

	munit_assert_false(eis_region_contains(&r, 299.9, 599.9));
	munit_assert_false(eis_region_contains(&r, 300, 600.1));

	munit_assert_false(eis_region_contains(&r, 400, 599.9));
	munit_assert_false(eis_region_contains(&r, 399, 600));

	return MUNIT_OK;
}
#endif
