/*
 * Copyright 2024 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_XE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

#ifdef __ANDROID__
#include <android/binder_ibinder.h>
#include <cutils/android_filesystem_config.h>
#endif

#include "drv_helpers.h"
#include "drv_priv.h"

#include "external/xe_drm.h"
#include "intel_defines.h"
#include "util.h"

struct modifier_support_t {
	const uint64_t *order;
	uint32_t count;
};

struct xe_device {
	uint32_t graphics_version;
	int device_id;
	bool is_xelpd;
	/*TODO : cleanup is_mtl_or_newer to avoid adding variables for every new platforms */
	bool is_mtl_or_newer;
	int32_t has_hw_protection;
	bool has_local_mem;
	int revision;

	uint64_t gtt_size;
	/**
	 * Memory vm bind alignment and buffer size requirement
	 */

	unsigned mem_alignment;
	struct modifier_support_t modifier;
	int32_t num_fences_avail;
	bool has_mmap_offset;
};

static void xe_info_from_device_id(struct xe_device *xe)
{
	unsigned i;
	xe->graphics_version = 0;
	xe->is_xelpd = false;
	xe->is_mtl_or_newer = false;

	/* search lists from most-->least specific */
	for (i = 0; i < ARRAY_SIZE(adlp_ids); i++) {
		if (adlp_ids[i] == xe->device_id) {
			xe->is_xelpd = true;
			xe->graphics_version = 12;
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(rplp_ids); i++) {
		if (rplp_ids[i] == xe->device_id) {
			xe->is_xelpd = true;
			xe->graphics_version = 12;
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtl_ids); i++) {
		if (mtl_ids[i] == xe->device_id) {
			xe->graphics_version = 12;
			xe->is_mtl_or_newer = true;
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(lnl_ids); i++) {
		if (lnl_ids[i] == xe->device_id) {
			xe->graphics_version = 20;
			xe->is_mtl_or_newer = true;
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ptl_ids); i++) {
		if (ptl_ids[i] == xe->device_id) {
			xe->graphics_version = 30;
			xe->is_mtl_or_newer = true;
			return;
		}
	}

	/* Gen 12 */
	for (i = 0; i < ARRAY_SIZE(gen12_ids); i++) {
		if (gen12_ids[i] == xe->device_id) {
			xe->graphics_version = 12;
			return;
		}
	}
}

static void xe_get_modifier_order(struct xe_device *xe)
{
	if (xe->is_mtl_or_newer) {
		xe->modifier.order = xe_lpdp_modifier_order;
		xe->modifier.count = ARRAY_SIZE(xe_lpdp_modifier_order);
	} else if (xe->is_xelpd) {
		xe->modifier.order = gen12_modifier_order;
		xe->modifier.count = ARRAY_SIZE(gen12_modifier_order);
	} else {
		xe->modifier.order = xe_lpdp_modifier_order;
		xe->modifier.count = ARRAY_SIZE(xe_lpdp_modifier_order);
	}
}

static uint64_t unset_flags(uint64_t current_flags, uint64_t mask)
{
	uint64_t value = current_flags & ~mask;
	return value;
}

/* TODO(ryanneph): share implementation with i915_add_combinations */
static int xe_add_combinations(struct driver *drv)
{
	struct xe_device *xe = drv->priv;

	const uint64_t scanout_and_render = BO_USE_RENDER_MASK | BO_USE_SCANOUT;
	const uint64_t render = BO_USE_RENDER_MASK;
	const uint64_t texture_only = BO_USE_TEXTURE_MASK;
	// HW protected buffers also need to be scanned out.
	const uint64_t hw_protected =
	    xe->has_hw_protection ? (BO_USE_PROTECTED | BO_USE_SCANOUT) : 0;

	const uint64_t linear_mask = BO_USE_RENDERSCRIPT | BO_USE_LINEAR | BO_USE_SW_READ_OFTEN |
				     BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY |
				     BO_USE_SW_WRITE_RARELY;

	struct format_metadata metadata_linear = { .tiling = XE_TILING_NONE,
						   .priority = 1,
						   .modifier = DRM_FORMAT_MOD_LINEAR };

	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &metadata_linear, scanout_and_render);

	drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats), &metadata_linear,
			     render);

	drv_add_combinations(drv, texture_only_formats, ARRAY_SIZE(texture_only_formats),
			     &metadata_linear, texture_only);

	drv_modify_linear_combinations(drv);

	/* NV12 format for camera, display, decoding and encoding. */
	/* IPU3 camera ISP supports only NV12 output. */
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata_linear,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_SCANOUT |
				   BO_USE_HW_VIDEO_DECODER | BO_USE_HW_VIDEO_ENCODER |
				   hw_protected);

	/* P010 linear can be used for scanout too. */
	drv_modify_combination(drv, DRM_FORMAT_P010, &metadata_linear, BO_USE_SCANOUT);

	/*
	 * Android also frequently requests YV12 formats for some camera implementations
	 * (including the external provider implementation).
	 */
	drv_modify_combination(drv, DRM_FORMAT_YVU420_ANDROID, &metadata_linear,
			       BO_USE_CAMERA_WRITE);

	/* Rendering on non HW renderable external formats is supported on formats that
	 * can be resolved by VK_ANDROID_external_format_resolve.
	 * Currently: NV12, P010, linear YV12
	 */
	for (unsigned i = 0; i < ARRAY_SIZE(external_resolve_formats); i++) {
		drv_modify_combination(drv, external_resolve_formats[i], &metadata_linear, render);
	}

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &metadata_linear, BO_USE_SW_MASK);

	/*
	 * R8 format is used for Android's HAL_PIXEL_FORMAT_BLOB and is used for JPEG snapshots
	 * from camera and input/output from hardware decoder/encoder.
	 * BO_USE_RENDERING is required for camera BLOB buffers that are also requested with
	 * GPU_RENDER_TARGET usage (e.g. CtsVirtualDevicesCameraCtsTestCases).
	 */
	drv_modify_combination(drv, DRM_FORMAT_R8, &metadata_linear,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_HW_VIDEO_DECODER |
				   BO_USE_HW_VIDEO_ENCODER | BO_USE_GPU_DATA_BUFFER |
				   BO_USE_SENSOR_DIRECT_DATA | BO_USE_RENDERING);

	/* Android AIDL gralloc allows use of AHB-backed external memory for storage images. */
	for (unsigned i = 0; i < ARRAY_SIZE(image_storage_formats); i++) {
		drv_modify_combination(drv, image_storage_formats[i], &metadata_linear,
				       BO_USE_GPU_DATA_BUFFER);
	}

	const uint64_t render_not_linear = unset_flags(render, linear_mask);
	const uint64_t scanout_and_render_not_linear = render_not_linear | BO_USE_SCANOUT;
	struct format_metadata metadata_x_tiled = { .tiling = XE_TILING_X,
						    .priority = 2,
						    .modifier = I915_FORMAT_MOD_X_TILED };

	drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats), &metadata_x_tiled,
			     render_not_linear);
	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &metadata_x_tiled, scanout_and_render_not_linear);

	/* Rendering can be supported via VK_ANDROID_external_format_resolve */
	const uint64_t nv12_usage = BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER | BO_USE_SCANOUT |
				    hw_protected | render_not_linear;
	const uint64_t p010_usage =
	    BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER | hw_protected | BO_USE_HW_VIDEO_ENCODER |
	    (xe->graphics_version >= 11 ? BO_USE_SCANOUT : 0) | render_not_linear;

	if (xe->is_mtl_or_newer) {
		struct format_metadata metadata_4_tiled = { .tiling = XE_TILING_4,
							    .priority = 3,
							    .modifier = I915_FORMAT_MOD_4_TILED };

		drv_add_combination(drv, DRM_FORMAT_NV12, &metadata_4_tiled, nv12_usage);
		drv_add_combination(drv, DRM_FORMAT_P010, &metadata_4_tiled, p010_usage);
		drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats),
				     &metadata_4_tiled, render_not_linear);
		drv_add_combinations(drv, scanout_render_formats,
				     ARRAY_SIZE(scanout_render_formats), &metadata_4_tiled,
				     scanout_and_render_not_linear);
	} else {
		struct format_metadata metadata_y_tiled = { .tiling = XE_TILING_Y,
							    .priority = 3,
							    .modifier = I915_FORMAT_MOD_Y_TILED };

		drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats),
				     &metadata_y_tiled, render_not_linear);
		drv_add_combinations(drv, scanout_render_formats,
				     ARRAY_SIZE(scanout_render_formats), &metadata_y_tiled,
				     scanout_and_render_not_linear);
		drv_add_combination(drv, DRM_FORMAT_NV12, &metadata_y_tiled, nv12_usage);
		drv_add_combination(drv, DRM_FORMAT_P010, &metadata_y_tiled, p010_usage);
	}
	return 0;
}

static int xe_align_dimensions(struct bo *bo, uint32_t format, uint32_t tiling, uint32_t *stride,
			       uint32_t *aligned_height)
{
	uint32_t horizontal_alignment = 0;
	uint32_t vertical_alignment = 0;

	switch (tiling) {
	default:
	case XE_TILING_NONE:
		/*
		 * The Intel GPU doesn't need any alignment in linear mode,
		 * but libva requires the allocation stride to be aligned to
		 * 16 bytes and height to 4 rows. Further, we round up the
		 * horizontal alignment so that row start on a cache line (64
		 * bytes).
		 */
#ifdef LINEAR_ALIGN_256
		/*
		 * If we want to import these buffers to amdgpu they need to
		 * their match LINEAR_ALIGNED requirement of 256 byte alignment.
		 */
		horizontal_alignment = 256;
#else
		horizontal_alignment = 64;
#endif
		/*
		 * For hardware video encoding buffers, we want to align to the size of a
		 * macroblock, because otherwise we will end up encoding uninitialized data.
		 * This can result in substantial quality degradations, especially on lower
		 * resolution videos, because this uninitialized data may be high entropy.
		 * For R8 and height=1, we assume the surface will be used as a linear buffer blob
		 * (such as VkBuffer). The hardware allows vertical_alignment=1 only for non-tiled
		 * 1D surfaces, which covers the VkBuffer case. However, if the app uses the surface
		 * as a 2D image with height=1, then this code is buggy. For 2D images, the hardware
		 * requires a vertical_alignment >= 4, and underallocating with vertical_alignment=1
		 * will cause the GPU to read out-of-bounds.
		 *
		 * TODO: add a new DRM_FORMAT_BLOB format for this case, or further tighten up the
		 * constraints with GPU_DATA_BUFFER usage when the guest has migrated to use
		 * virtgpu_cross_domain backend which passes that flag through.
		 */
		if (bo->meta.use_flags & BO_USE_HW_VIDEO_ENCODER) {
			vertical_alignment = 8;
		} else if (format == DRM_FORMAT_R8 && *aligned_height == 1)
			vertical_alignment = 1;
		else
			vertical_alignment = 4;

		break;
	case XE_TILING_X:
		horizontal_alignment = 512;
		vertical_alignment = 8;
		break;

	case XE_TILING_Y:
	case XE_TILING_4:
		horizontal_alignment = 128;
		vertical_alignment = 32;
		break;
	}

	*aligned_height = ALIGN(*aligned_height, vertical_alignment);
	*stride = ALIGN(*stride, horizontal_alignment);

	return 0;
}

static void xe_query_pxp_support(struct driver *drv, struct xe_device *xe)
{
	struct drm_xe_query_pxp_status pxp_status = {0};
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_PXP_STATUS,
		.size = sizeof(struct drm_xe_query_pxp_status),
		.data = (uintptr_t)&pxp_status,
	};

	/* Ignore the transient status, the query will return an error if PXP
	 * is not supported or disabled due to a fatal error during init.
	 */
	if (drmIoctl(drv->fd, DRM_IOCTL_XE_DEVICE_QUERY, &query) == 0) {
		if (pxp_status.supported_session_types & 1u << DRM_XE_PXP_TYPE_HWDRM)
			xe->has_hw_protection = 1;
	} else {
		drv_logi("Xe PXP not supported or disabled.");
	}
}

static bool xe_query_config(struct driver *drv, struct xe_device *xe)
{
	struct drm_xe_device_query query = {
		.query = DRM_XE_DEVICE_QUERY_CONFIG,
	};
	if (drmIoctl(drv->fd, DRM_IOCTL_XE_DEVICE_QUERY, &query))
		return false;

	struct drm_xe_query_config *config = calloc(1, query.size);
	if (!config)
		return false;

	query.data = (uintptr_t)config;
	if (drmIoctl(drv->fd, DRM_IOCTL_XE_DEVICE_QUERY, &query))
		goto data_query_failed;

	if (config->info[DRM_XE_QUERY_CONFIG_FLAGS] & DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM)
		xe->has_local_mem = true;
	else
		xe->has_local_mem = false;

	xe->revision = (config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16) & 0xFFFF;
	xe->gtt_size = 1ull << config->info[DRM_XE_QUERY_CONFIG_VA_BITS];
	xe->mem_alignment = config->info[DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT];

	free(config);
	return true;

data_query_failed:
	free(config);
	return false;
}

static bool xe_device_probe(struct driver *drv, struct xe_device *xe)
{
	/* Retrieve the device info by querying KMD through IOCTL
	 */
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_CONFIG,
		.size = 0,
		.data = 0,
	};

	if (drmIoctl(drv->fd, DRM_IOCTL_XE_DEVICE_QUERY, &query))
		return false;

	struct drm_xe_query_config *config = calloc(1, query.size);
	if (!config)
		return false;

	query.data = (uintptr_t)config;
	if (drmIoctl(drv->fd, DRM_IOCTL_XE_DEVICE_QUERY, &query)) {
		free(config);
		return false;
	}

	xe->device_id =
	    ((config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] << 16) >> 16) & 0xFFFF;
	xe->revision = (config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16) & 0xFFFF;

	free(config);
	return true;
}

static int xe_init(struct driver *drv)
{
	struct xe_device *xe;

	xe = calloc(1, sizeof(*xe));
	if (!xe)
		return -ENOMEM;

	if (!xe_device_probe(drv, xe)) {
		drv_loge("Failed to query device id using DRM_IOCTL_XE_DEVICE_QUERY");
		return -EINVAL;
	}

	xe_query_config(drv, xe);

	xe_query_pxp_support(drv, xe);

	/* must call before xe->graphics_version is used anywhere else */
	xe_info_from_device_id(xe);

	xe_get_modifier_order(xe);

	if (xe->graphics_version < 12) {
		drv_loge("Xe driver is not supported on your platform: 0x%x\n", xe->device_id);
		return -errno;
	}

	drv->priv = xe;

	return xe_add_combinations(drv);
	return 0;
}

static uint32_t xe_gem_create(struct bo *bo, int drv_fd, uint64_t size, uint32_t placement,
			      uint32_t flags, uint16_t cpu_caching)
{
	struct xe_device *xe = bo->drv->priv;
	struct drm_xe_ext_set_property create_ext = {
		.base.name = DRM_XE_GEM_CREATE_EXTENSION_SET_PROPERTY,
		.property = DRM_XE_GEM_CREATE_SET_PROPERTY_PXP_TYPE,
		.value = DRM_XE_PXP_TYPE_HWDRM,
	};
	struct drm_xe_gem_create gem_create = {
		.vm_id = 0, /*If .vm_id == 0, it is exportable via PRIME fd */
		.size = size,
		.placement = placement,
		.flags = flags,
		.cpu_caching = cpu_caching,
	};

	if (xe->has_hw_protection && (bo->meta.use_flags & BO_USE_PROTECTED)) {
		gem_create.extensions = (uintptr_t)&create_ext;
	}

	int ret = drmIoctl(drv_fd, DRM_IOCTL_XE_GEM_CREATE, &gem_create);
	if (ret) {
		drv_loge("Xe IOCTL DRM_IOCTL_XE_GEM_CREATE, BO creation failed, aborting: %d\n ",
			 -errno);
		return -errno;
	}

	bo->handle.u32 = gem_create.handle;

	return ret;
}

/*
 * Returns true if the height of a buffer of the given format should be aligned
 * to the largest coded unit (LCU) assuming that it will be used for video. This
 * is based on gmmlib's GmmIsYUVFormatLCUAligned().
 */
static bool xe_format_needs_LCU_alignment(uint32_t format, size_t plane, const struct xe_device *xe)
{
	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P016:
		return (xe->graphics_version >= 12) && plane == 1;
	}
	return false;
}

static int xe_bo_from_format(struct bo *bo, uint32_t width, uint32_t height, uint32_t format)
{
	uint32_t offset;
	size_t plane;
	int ret, pagesize;
	struct xe_device *xe = bo->drv->priv;

	offset = 0;
	pagesize = getpagesize();

	for (plane = 0; plane < drv_num_planes_from_format(format); plane++) {
		uint32_t stride = drv_stride_from_format(format, width, plane);
		uint32_t plane_height = drv_height_from_format(format, height, plane);

		if (bo->meta.tiling != XE_TILING_NONE)
			assert(IS_ALIGNED(offset, pagesize));

		ret = xe_align_dimensions(bo, format, bo->meta.tiling, &stride, &plane_height);
		if (ret)
			return ret;

		if (xe_format_needs_LCU_alignment(format, plane, xe)) {
			/*
			 * Align the height of the V plane for certain formats to the
			 * largest coded unit (assuming that this BO may be used for video)
			 * to be consistent with gmmlib.
			 */
			plane_height = ALIGN(plane_height, 64);
		}

		bo->meta.strides[plane] = stride;
		bo->meta.sizes[plane] = stride * plane_height;
		bo->meta.offsets[plane] = offset;
		offset += bo->meta.sizes[plane];
	}

	bo->meta.total_size = ALIGN(offset, pagesize);

	return 0;
}

static size_t xe_num_planes_from_modifier(struct driver *drv, uint32_t format, uint64_t modifier)
{
	size_t num_planes = drv_num_planes_from_format(format);

	if (modifier == I915_FORMAT_MOD_Y_TILED_CCS ||
	    modifier == I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS) {
		assert(num_planes == 1);
		return 2;
	}

	return num_planes;
}

static int xe_bo_compute_metadata(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				  uint64_t use_flags, const uint64_t *modifiers, uint32_t count)
{
	int ret = 0;
	uint64_t modifier;
	struct xe_device *xe = bo->drv->priv;

	if (modifiers) {
		modifier =
		    drv_pick_modifier(modifiers, count, xe->modifier.order, xe->modifier.count);
	} else {
		struct combination *combo = drv_get_combination(bo->drv, format, use_flags);
		if (!combo)
			return -EINVAL;

		if ((xe->is_mtl_or_newer) &&
		    ((use_flags == (BO_USE_SCANOUT | BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER)) ||
		     (use_flags == (BO_USE_RENDERING | BO_USE_TEXTURE | BO_USE_SCANOUT)) ||
		     (use_flags == (BO_USE_TEXTURE | BO_USE_RENDERING)))) {
			modifier = (xe->graphics_version >= 20) ? I915_FORMAT_MOD_4_TILED_LNL_CCS
								: I915_FORMAT_MOD_4_TILED;
		} else {
			modifier = combo->metadata.modifier;
		}
	}
	/*
	 * For cursor buffer, add padding as needed to reach a known cursor-plane-supported
	 * buffer size, as reported by the cursor capability properties.
	 *
	 * If the requested dimensions exceed either of the reported capabilities, or if the
	 * capabilities couldn't be read, silently fallback by continuing without additional
	 * padding. The buffer can still be used normally, and be committed to non-cursor
	 * planes.
	 */
	if (use_flags & BO_USE_CURSOR) {
		uint64_t cursor_width = 0;
		uint64_t cursor_height = 0;
		bool err = drmGetCap(bo->drv->fd, DRM_CAP_CURSOR_WIDTH, &cursor_width) ||
			   drmGetCap(bo->drv->fd, DRM_CAP_CURSOR_HEIGHT, &cursor_height);

		if (!err && width <= cursor_width && height <= cursor_height) {
			width = cursor_width;
			height = cursor_height;
		}
	}

	/*
	 * Skip I915_FORMAT_MOD_Y_TILED_CCS modifier if compression is disabled
	 * Pick y tiled modifier if it has been passed in, otherwise use linear
	 */
	if (!bo->drv->compression && modifier == I915_FORMAT_MOD_Y_TILED_CCS) {
		uint32_t i;
		for (i = 0; modifiers && i < count; i++) {
			if (modifiers[i] == I915_FORMAT_MOD_Y_TILED)
				break;
		}
		if (i == count)
			modifier = DRM_FORMAT_MOD_LINEAR;
		else
			modifier = I915_FORMAT_MOD_Y_TILED;
	}

	switch (modifier) {
	case DRM_FORMAT_MOD_LINEAR:
		bo->meta.tiling = XE_TILING_NONE;
		break;
	case I915_FORMAT_MOD_X_TILED:
		bo->meta.tiling = XE_TILING_X;
		break;
	case I915_FORMAT_MOD_Y_TILED:
	case I915_FORMAT_MOD_Y_TILED_CCS:
	/* For now support only I915_TILING_Y as this works with all
	 * IPs(render/media/display)
	 */
	case I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS:
		bo->meta.tiling = XE_TILING_Y;
		break;
	case I915_FORMAT_MOD_4_TILED:
	case I915_FORMAT_MOD_4_TILED_LNL_CCS:
		bo->meta.tiling = XE_TILING_4;
		break;
	}

	bo->meta.format_modifier = modifier;

	if (format == DRM_FORMAT_YVU420_ANDROID) {
		/*
		 * We only need to be able to use this as a linear texture,
		 * which doesn't put any HW restrictions on how we lay it
		 * out. The Android format does require the stride to be a
		 * multiple of 16 and expects the Cr and Cb stride to be
		 * ALIGN(Y_stride / 2, 16), which we can make happen by
		 * aligning to 32 bytes here.
		 */
		uint32_t stride = ALIGN(width, 32);
		ret = drv_bo_from_format(bo, stride, 1, height, format);
		bo->meta.total_size = ALIGN(bo->meta.total_size, getpagesize());
	} else if (modifier == I915_FORMAT_MOD_Y_TILED_CCS) {
		/*
		 * For compressed surfaces, we need a color control surface
		 * (CCS). Color compression is only supported for Y tiled
		 * surfaces, and for each 32x16 tiles in the main surface we
		 * need a tile in the control surface.  Y tiles are 128 bytes
		 * wide and 32 lines tall and we use that to first compute the
		 * width and height in tiles of the main surface. stride and
		 * height are already multiples of 128 and 32, respectively:
		 */
		uint32_t stride = drv_stride_from_format(format, width, 0);
		uint32_t width_in_tiles = DIV_ROUND_UP(stride, 128);
		uint32_t height_in_tiles = DIV_ROUND_UP(height, 32);
		uint32_t size = width_in_tiles * height_in_tiles * 4096;
		uint32_t offset = 0;

		bo->meta.strides[0] = width_in_tiles * 128;
		bo->meta.sizes[0] = size;
		bo->meta.offsets[0] = offset;
		offset += size;

		/*
		 * Now, compute the width and height in tiles of the control
		 * surface by dividing and rounding up.
		 */
		uint32_t ccs_width_in_tiles = DIV_ROUND_UP(width_in_tiles, 32);
		uint32_t ccs_height_in_tiles = DIV_ROUND_UP(height_in_tiles, 16);
		uint32_t ccs_size = ccs_width_in_tiles * ccs_height_in_tiles * 4096;

		/*
		 * With stride and height aligned to y tiles, offset is
		 * already a multiple of 4096, which is the required alignment
		 * of the CCS.
		 */
		bo->meta.strides[1] = ccs_width_in_tiles * 128;
		bo->meta.sizes[1] = ccs_size;
		bo->meta.offsets[1] = offset;
		offset += ccs_size;

		bo->meta.num_planes = xe_num_planes_from_modifier(bo->drv, format, modifier);
		bo->meta.total_size = offset;
	} else if (modifier == I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS) {
		/*
		 * considering only 128 byte compression and one cache line of
		 * aux buffer(64B) contains compression status of 4-Y tiles.
		 * Which is 4 * (128B * 32L).
		 * line stride(bytes) is 4 * 128B
		 * and tile stride(lines) is 32L
		 */
		uint32_t stride = ALIGN(drv_stride_from_format(format, width, 0), 512);

		height = ALIGN(drv_height_from_format(format, height, 0), 32);

		if (xe->is_xelpd && (stride > 1)) {
			stride = 1 << (32 - __builtin_clz(stride - 1));
			height = ALIGN(drv_height_from_format(format, height, 0), 128);
		}

		bo->meta.strides[0] = stride;
		/* size calculation and alignment are 64KB aligned
		 * size as per spec
		 */
		bo->meta.sizes[0] = ALIGN(stride * height, 65536);
		bo->meta.offsets[0] = 0;

		/* Aux buffer is linear and page aligned. It is placed after
		 * other planes and aligned to main buffer stride.
		 */
		bo->meta.strides[1] = bo->meta.strides[0] / 8;
		/* Aligned to page size */
		bo->meta.sizes[1] = ALIGN(bo->meta.sizes[0] / 256, getpagesize());
		bo->meta.offsets[1] = bo->meta.sizes[0];
		/* Total number of planes & sizes */
		bo->meta.num_planes = xe_num_planes_from_modifier(bo->drv, format, modifier);
		bo->meta.total_size = bo->meta.sizes[0] + bo->meta.sizes[1];
	} else {
		ret = xe_bo_from_format(bo, width, height, format);
	}

	return ret;
}

static int xe_bo_create_from_metadata(struct bo *bo)
{
	int drv_fd = bo->drv->fd;
	uint32_t flags = 0;
	uint32_t cpu_caching;
	size_t bo_size = bo->meta.total_size;
	uint32_t gem_placement = 0;

	/* All compressed BOs must be WC cached as per spec */
	if (bo->meta.use_flags & BO_USE_SCANOUT) {
		flags |= DRM_XE_GEM_CREATE_FLAG_SCANOUT;
		cpu_caching = DRM_XE_GEM_CPU_CACHING_WC;
	} else if (bo->meta.format_modifier == I915_FORMAT_MOD_4_TILED_LNL_CCS) {
		cpu_caching = DRM_XE_GEM_CPU_CACHING_WC;
	} else {
		cpu_caching = DRM_XE_GEM_CPU_CACHING_WB;
	}

	/* FIXME: let's assume iGPU with SYSMEM is only supported */
	gem_placement |= BITFIELD_BIT(DRM_XE_MEM_REGION_CLASS_SYSMEM);

	/* Create and allocate BO in GPU memory */
	int ret = xe_gem_create(bo, drv_fd, bo_size, gem_placement, flags, cpu_caching);

	return ret;
}

static void xe_close(struct driver *drv)
{
	free(drv->priv);
	drv->priv = NULL;
}

static int xe_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	int ret;

	bo->meta.num_planes =
	    xe_num_planes_from_modifier(bo->drv, data->format, data->format_modifier);

	ret = drv_prime_bo_import(bo, data);
	if (ret)
		return ret;

	return 0;
}

static void *xe_bo_map(struct bo *bo, struct vma *vma, uint32_t map_flags)
{
	int ret;
	void *addr = MAP_FAILED;

	struct drm_xe_gem_mmap_offset gem_map = {
		.handle = bo->handle.u32,
	};

	/* Get the fake offset back */
	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_XE_GEM_MMAP_OFFSET, &gem_map);
	if (ret == 0) {
		addr = mmap(0, bo->meta.total_size, PROT_READ | PROT_WRITE, MAP_SHARED, bo->drv->fd,
			    gem_map.offset);
	}

	if (addr == MAP_FAILED) {
		drv_loge("Xe GEM mmap failed with error(%d)\n", errno);

		return addr;
	}

	vma->length = bo->meta.total_size;

	return addr;
}

#define XE_CACHELINE_SIZE 64
#define XE_CACHELINE_MASK (XE_CACHELINE_SIZE - 1)
static void xe_clflush(void *start, size_t size)
{
	/* copy of i915_clflush() */
	void *p = (void *)(((uintptr_t)start) & ~XE_CACHELINE_MASK);
	void *end = (void *)((uintptr_t)start + size);

	__builtin_ia32_mfence();
	while (p < end) {
#if defined(__CLFLUSHOPT__)
		__builtin_ia32_clflushopt(p);
#else
		__builtin_ia32_clflush(p);
#endif
		p = (void *)((uintptr_t)p + XE_CACHELINE_SIZE);
	}
	__builtin_ia32_mfence();
}

static int xe_bo_flush(struct bo *bo, struct mapping *mapping)
{
	if (bo->meta.tiling == XE_TILING_NONE) {
		xe_clflush(mapping->vma->addr, mapping->vma->length);
	}

	return 0;
}

#ifdef __ANDROID__
static bool xe_is_protected_usage_permitted(struct driver *drv, uint64_t use_flags)
{
	if ((use_flags & BO_USE_PROTECTED) &&
	    ((use_flags & BO_USE_RENDERING) ||
	     (use_flags & BO_USE_GPU_DATA_BUFFER))) {
		uid_t uid = AIBinder_getCallingUid();
		return uid == AID_SYSTEM;
	}
	return true;
}
#endif

const struct backend backend_xe = {
	.name = "xe",
	.init = xe_init,
	.close = xe_close,
	.bo_compute_metadata = xe_bo_compute_metadata,
	.bo_create_from_metadata = xe_bo_create_from_metadata,
	.bo_map = xe_bo_map,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_unmap = drv_bo_munmap,
	.num_planes_from_modifier = xe_num_planes_from_modifier,
	.bo_import = xe_bo_import,
	.bo_export = drv_prime_bo_export,
	.bo_flush = xe_bo_flush,
	.resolve_format_and_use_flags = drv_resolve_format_and_use_flags_helper,
#ifdef __ANDROID__
	.is_protected_usage_permitted = xe_is_protected_usage_permitted,
#endif
};

#endif
