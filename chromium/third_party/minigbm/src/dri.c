/*
 * Copyright 2017 Advanced Micro Devices. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_AMDGPU

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

#include "dri.h"
#include "drv_helpers.h"
#include "drv_priv.h"
#include "util.h"

// Avoid transitively including a bunch of unnecessary headers.
#define GL_GLEXT_LEGACY
#include "GL/internal/dri_interface.h"
#undef GL_GLEXT_LEGACY

struct dri_driver {
	int fd;
	void *driver_handle;
	__DRIscreen *device;
	__DRIcontext *context; /* Needed for map/unmap operations. */
	const __DRIextension **extensions;
	const __DRIcoreExtension *core_extension;
	const __DRIdri2Extension *dri2_extension;
	const __DRIimageExtension *image_extension;
	const __DRI2flushExtension *flush_extension;
	const __DRIconfig **configs;
};

static const struct {
	uint32_t drm_format;
	int dri_image_format;
} drm_to_dri_image_formats[] = {
	{ DRM_FORMAT_R8, __DRI_IMAGE_FORMAT_R8 },
	{ DRM_FORMAT_GR88, __DRI_IMAGE_FORMAT_GR88 },
	{ DRM_FORMAT_RGB565, __DRI_IMAGE_FORMAT_RGB565 },
	{ DRM_FORMAT_XRGB8888, __DRI_IMAGE_FORMAT_XRGB8888 },
	{ DRM_FORMAT_ARGB8888, __DRI_IMAGE_FORMAT_ARGB8888 },
	{ DRM_FORMAT_XBGR8888, __DRI_IMAGE_FORMAT_XBGR8888 },
	{ DRM_FORMAT_ABGR8888, __DRI_IMAGE_FORMAT_ABGR8888 },
	{ DRM_FORMAT_XRGB2101010, __DRI_IMAGE_FORMAT_XRGB2101010 },
	{ DRM_FORMAT_XBGR2101010, __DRI_IMAGE_FORMAT_XBGR2101010 },
	{ DRM_FORMAT_ARGB2101010, __DRI_IMAGE_FORMAT_ARGB2101010 },
	{ DRM_FORMAT_ABGR2101010, __DRI_IMAGE_FORMAT_ABGR2101010 },
	{ DRM_FORMAT_ABGR16161616F, __DRI_IMAGE_FORMAT_ABGR16161616F },
};

static int drm_format_to_dri_format(uint32_t drm_format)
{
	uint32_t i;
	for (i = 0; i < ARRAY_SIZE(drm_to_dri_image_formats); i++) {
		if (drm_to_dri_image_formats[i].drm_format == drm_format)
			return drm_to_dri_image_formats[i].dri_image_format;
	}

	return 0;
}

static bool lookup_extension(const __DRIextension *const *extensions, const char *name,
			     int min_version, const __DRIextension **dst)
{
	while (*extensions) {
		if ((*extensions)->name && !strcmp((*extensions)->name, name) &&
		    (*extensions)->version >= min_version) {
			*dst = *extensions;
			return true;
		}

		extensions++;
	}

	return false;
}

/*
 * The DRI GEM namespace may be different from the minigbm's driver GEM namespace. We have
 * to import into minigbm.
 */
static int import_into_minigbm(struct dri_driver *dri, struct bo *bo)
{
	uint32_t handle = 0;
	int ret, modifier_upper, modifier_lower, num_planes, prime_fd;
	off_t dmabuf_size;
	__DRIimage *plane_image = NULL;

	if (dri->image_extension->queryImage(bo->priv, __DRI_IMAGE_ATTRIB_MODIFIER_UPPER,
					     &modifier_upper) &&
	    dri->image_extension->queryImage(bo->priv, __DRI_IMAGE_ATTRIB_MODIFIER_LOWER,
					     &modifier_lower))
		bo->meta.format_modifier =
		    ((uint64_t)modifier_upper << 32) | (uint32_t)modifier_lower;
	else
		bo->meta.format_modifier = DRM_FORMAT_MOD_INVALID;

	if (!dri->image_extension->queryImage(bo->priv, __DRI_IMAGE_ATTRIB_NUM_PLANES, &num_planes))
		return -errno;

	bo->meta.num_planes = num_planes;

	if (!dri->image_extension->queryImage(bo->priv, __DRI_IMAGE_ATTRIB_FD, &prime_fd))
		return -errno;

	dmabuf_size = lseek(prime_fd, 0, SEEK_END);
	if (dmabuf_size == (off_t)-1) {
		close(prime_fd);
		return -errno;
	}

	lseek(prime_fd, 0, SEEK_SET);

	ret = drmPrimeFDToHandle(bo->drv->fd, prime_fd, &handle);

	close(prime_fd);

	if (ret) {
		drv_loge("drmPrimeFDToHandle failed with %s\n", strerror(errno));
		return ret;
	}

	bo->handle.u32 = handle;
	for (int i = 0; i < num_planes; ++i) {
		int stride, offset;
		plane_image = dri->image_extension->fromPlanar(bo->priv, i, NULL);
		__DRIimage *image = plane_image ? plane_image : bo->priv;

		if (!dri->image_extension->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &stride) ||
		    !dri->image_extension->queryImage(image, __DRI_IMAGE_ATTRIB_OFFSET, &offset)) {
			ret = -errno;
			goto cleanup;
		}

		bo->meta.strides[i] = stride;
		bo->meta.offsets[i] = offset;

		if (plane_image)
			dri->image_extension->destroyImage(plane_image);

		if (i > 0)
			bo->meta.sizes[i - 1] = bo->meta.offsets[i] - bo->meta.offsets[i - 1];
	}

	bo->meta.sizes[num_planes - 1] = dmabuf_size - bo->meta.offsets[num_planes - 1];
	bo->meta.total_size = dmabuf_size;

	return 0;

cleanup:
	if (plane_image)
		dri->image_extension->destroyImage(plane_image);

	if (handle != 0)
		drv_gem_close(bo->drv, handle);

	return ret;
}

const __DRIuseInvalidateExtension use_invalidate = {
	.base = { __DRI_USE_INVALIDATE, 1 },
};

void *dri_dlopen(const char *dri_so_path)
{
	return dlopen(dri_so_path, RTLD_NOW | RTLD_GLOBAL);
}

void dri_dlclose(void *dri_so_handle)
{
	dlclose(dri_so_handle);
}

struct dri_driver *dri_init(struct driver *drv, const char *dri_so_path, const char *driver_suffix)
{
	char fname[128];
	const __DRIextension **(*get_extensions)();
	const __DRIextension *loader_extensions[] = { &use_invalidate.base, NULL };

	struct dri_driver *dri = calloc(1, sizeof(*dri));
	if (!dri)
		return NULL;

	char *node_name = drmGetRenderDeviceNameFromFd(drv_get_fd(drv));
	if (!node_name)
		goto free_dri;

	dri->fd = open(node_name, O_RDWR);
	free(node_name);
	if (dri->fd < 0)
		goto free_dri;

	dri->driver_handle = dri_dlopen(dri_so_path);
	if (!dri->driver_handle)
		goto close_dri_fd;

	snprintf(fname, sizeof(fname), __DRI_DRIVER_GET_EXTENSIONS "_%s", driver_suffix);
	get_extensions = dlsym(dri->driver_handle, fname);
	if (!get_extensions)
		goto free_handle;

	dri->extensions = get_extensions();
	if (!dri->extensions)
		goto free_handle;

	if (!lookup_extension(dri->extensions, __DRI_CORE, 2,
			      (const __DRIextension **)&dri->core_extension))
		goto free_handle;

	/* Version 4 for createNewScreen2 */
	if (!lookup_extension(dri->extensions, __DRI_DRI2, 4,
			      (const __DRIextension **)&dri->dri2_extension))
		goto free_handle;

	dri->device = dri->dri2_extension->createNewScreen2(0, dri->fd, loader_extensions,
							    dri->extensions, &dri->configs, NULL);
	if (!dri->device)
		goto free_handle;

	dri->context =
	    dri->dri2_extension->createNewContext(dri->device, *dri->configs, NULL, NULL);

	if (!dri->context)
		goto free_screen;

	if (!lookup_extension(dri->core_extension->getExtensions(dri->device), __DRI_IMAGE, 12,
			      (const __DRIextension **)&dri->image_extension))
		goto free_context;

	if (!lookup_extension(dri->core_extension->getExtensions(dri->device), __DRI2_FLUSH, 4,
			      (const __DRIextension **)&dri->flush_extension))
		goto free_context;

	return dri;

free_context:
	dri->core_extension->destroyContext(dri->context);
free_screen:
	dri->core_extension->destroyScreen(dri->device);
free_handle:
	dri_dlclose(dri->driver_handle);
	dri->driver_handle = NULL;
close_dri_fd:
	close(dri->fd);
free_dri:
	free(dri);
	return NULL;
}

void dri_close(struct dri_driver *dri)
{
	dri->core_extension->destroyContext(dri->context);
	dri->core_extension->destroyScreen(dri->device);
	dri_dlclose(dri->driver_handle);
	dri->driver_handle = NULL;
	close(dri->fd);
	free(dri);
}

int dri_bo_create(struct dri_driver *dri, struct bo *bo, uint32_t width, uint32_t height,
		  uint32_t format, uint64_t use_flags)
{
	unsigned int dri_use;
	int ret, dri_format;

	dri_format = drm_format_to_dri_format(format);

	/* Gallium drivers require shared to get the handle and stride. */
	dri_use = __DRI_IMAGE_USE_SHARE;
	if (use_flags & BO_USE_SCANOUT)
		dri_use |= __DRI_IMAGE_USE_SCANOUT;
	if (use_flags & BO_USE_CURSOR)
		dri_use |= __DRI_IMAGE_USE_CURSOR;
	if (use_flags & BO_USE_LINEAR)
		dri_use |= __DRI_IMAGE_USE_LINEAR;

	bo->priv = dri->image_extension->createImage(dri->device, width, height, dri_format,
						     dri_use, NULL);
	if (!bo->priv) {
		ret = -errno;
		return ret;
	}

	ret = import_into_minigbm(dri, bo);
	if (ret)
		goto free_image;

	return 0;

free_image:
	dri->image_extension->destroyImage(bo->priv);
	return ret;
}

int dri_bo_create_with_modifiers(struct dri_driver *dri, struct bo *bo, uint32_t width,
				 uint32_t height, uint32_t format, uint64_t use_flags,
				 const uint64_t *modifiers, uint32_t modifier_count)
{
	int ret, dri_format;

	if (!dri->image_extension->createImageWithModifiers)
		return -ENOENT;

	dri_format = drm_format_to_dri_format(format);

	bo->priv = dri->image_extension->createImageWithModifiers(
	    dri->device, width, height, dri_format, modifiers, modifier_count, NULL);
	if (!bo->priv) {
		ret = -errno;
		return ret;
	}

	ret = import_into_minigbm(dri, bo);
	if (ret)
		goto free_image;

	return 0;

free_image:
	dri->image_extension->destroyImage(bo->priv);
	return ret;
}

int dri_bo_import(struct dri_driver *dri, struct bo *bo, struct drv_import_fd_data *data)
{
	int ret;

	if (data->format_modifier != DRM_FORMAT_MOD_INVALID) {
		unsigned error;

		if (!dri->image_extension->createImageFromDmaBufs2)
			return -ENOSYS;

		// clang-format off
		bo->priv = dri->image_extension->createImageFromDmaBufs2(dri->device, data->width, data->height,
									 drv_get_standard_fourcc(data->format),
									 data->format_modifier,
									 data->fds,
									 bo->meta.num_planes,
									 (int *)data->strides,
									 (int *)data->offsets,
									 __DRI_YUV_COLOR_SPACE_UNDEFINED,
									 __DRI_YUV_RANGE_UNDEFINED,
									 __DRI_YUV_CHROMA_SITING_UNDEFINED,
									 __DRI_YUV_CHROMA_SITING_UNDEFINED,
									 &error, NULL);
		// clang-format on

		/* Could translate the DRI error, but the Mesa GBM also returns ENOSYS. */
		if (!bo->priv)
			return -ENOSYS;
	} else {
		// clang-format off
		bo->priv = dri->image_extension->createImageFromFds(dri->device, data->width, data->height,
								    drv_get_standard_fourcc(data->format), data->fds,
								    bo->meta.num_planes,
								    (int *)data->strides,
								    (int *)data->offsets, NULL);
		// clang-format on
		if (!bo->priv)
			return -errno;
	}

	ret = import_into_minigbm(dri, bo);
	if (ret) {
		dri->image_extension->destroyImage(bo->priv);
		return ret;
	}

	return 0;
}

int dri_bo_release(struct dri_driver *dri, struct bo *bo)
{
	assert(bo->priv);
	dri->image_extension->destroyImage(bo->priv);
	/* Not clearing bo->priv as we still use it to determine which destroy to call. */
	return 0;
}

int dri_bo_destroy(struct dri_driver *dri, struct bo *bo)
{
	assert(bo->priv);
	drv_gem_close(bo->drv, bo->handle.u32);
	bo->priv = NULL;
	return 0;
}

/*
 * Map an image plane.
 *
 * This relies on the underlying driver to do a decompressing and/or de-tiling
 * blit if necessary,
 *
 * This function itself is not thread-safe; we rely on the fact that the caller
 * locks a per-driver mutex.
 */
void *dri_bo_map(struct dri_driver *dri, struct bo *bo, struct vma *vma, size_t plane,
		 uint32_t map_flags)
{
	/* GBM flags and DRI flags are the same. */
	vma->addr = dri->image_extension->mapImage(dri->context, bo->priv, 0, 0, bo->meta.width,
						   bo->meta.height, map_flags,
						   (int *)&vma->map_strides[plane], &vma->priv);
	if (!vma->addr)
		return MAP_FAILED;

	return vma->addr;
}

int dri_bo_unmap(struct dri_driver *dri, struct bo *bo, struct vma *vma)
{
	assert(vma->priv);
	dri->image_extension->unmapImage(dri->context, bo->priv, vma->priv);

	/*
	 * From gbm_dri.c in Mesa:
	 *
	 * "Not all DRI drivers use direct maps. They may queue up DMA operations
	 *  on the mapping context. Since there is no explicit gbm flush mechanism,
	 *  we need to flush here."
	 */

	dri->flush_extension->flush_with_flags(dri->context, NULL, __DRI2_FLUSH_CONTEXT, 0);
	return 0;
}

size_t dri_num_planes_from_modifier(struct dri_driver *dri, uint32_t format, uint64_t modifier)
{
	uint64_t planes = 0;

	/* We do not do any modifier checks here. The create will fail later if the modifier is not
	 * supported.
	 */
	if (dri->image_extension->queryDmaBufFormatModifierAttribs &&
	    dri->image_extension->queryDmaBufFormatModifierAttribs(
		dri->device, format, modifier, __DRI_IMAGE_FORMAT_MODIFIER_ATTRIB_PLANE_COUNT,
		&planes))
		return planes;

	return drv_num_planes_from_format(format);
}

bool dri_query_modifiers(struct dri_driver *dri, uint32_t format, int max, uint64_t *modifiers,
			 int *count)
{
	if (!dri->image_extension->queryDmaBufModifiers)
		return false;

	return dri->image_extension->queryDmaBufModifiers(dri->device, format, max, modifiers, NULL,
							  count);
}

#endif
