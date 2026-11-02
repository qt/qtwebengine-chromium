/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MEDIATEK

// clang-format off
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#if !defined(ANDROID) || (ANDROID_API_LEVEL >= 31 && defined(HAS_DMABUF_SYSTEM_HEAP))
#include "external/dma-heap.h"
#endif
#include "external/mediatek_drm.h"
// clang-format on

#include "drv_helpers.h"
#include "drv_priv.h"
#include "util.h"

#define TILE_TYPE_LINEAR 0

// clang-format off
#if defined(MTK_MT8183) || \
    defined(MTK_MT8186)
// clang-format on
#define SUPPORT_YUV422
#endif

// All platforms except MT8173 should SUPPORT_FP16_AND_10BIT_ABGR
// clang-format off
#if defined(MTK_MT8183) || \
    defined(MTK_MT8186) || \
    defined(MTK_MT8188G) || \
    defined(MTK_MT8189) || \
    defined(MTK_MT8192) || \
    defined(MTK_MT8195) || \
    defined(MTK_MT8196)
// clang-format on
#define SUPPORT_FP16_AND_10BIT_ABGR
#else
#define DONT_USE_64_ALIGNMENT_FOR_VIDEO_BUFFERS
#endif

// Devices newer than MT8186 support AR30 overlays and 10-bit video.
// clang-format off
#if !defined(MTK_MT8173) && \
    !defined(MTK_MT8183) && \
    !defined(MTK_MT8186) && \
    !defined(MTK_MT8192)
// clang-format on
#define SUPPORT_P010
#define SUPPORT_AR30_OVERLAYS
#endif

// For Mali Sigurd based GPUs, the texture unit reads outside the specified texture dimensions.
// Therefore, certain formats require extra memory padding to its allocated surface to prevent the
// hardware from reading outside an allocation. For YVU420, we need additional padding for the last
// chroma plane.
#if defined(MTK_MT8186)
#define USE_EXTRA_PADDING_FOR_YVU420
#endif

#if defined(MTK_MT8196)
#define SUPPORT_PROTECTED_DMA_HEAP
#endif

struct mediatek_private_drv_data {
	int dma_heap_fd;

	bool init_subdrv_once;
	int subdrv_fd;
	struct driver *subdrv;
};

struct mediatek_private_map_data {
	void *cached_addr;
	void *gem_addr;
	int prime_fd;
};

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

// clang-format off
static const uint32_t texture_source_formats[] = {
#ifdef SUPPORT_YUV422
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUYV,
#endif
#ifdef SUPPORT_P010
	DRM_FORMAT_P010,
#endif
#ifdef SUPPORT_FP16_AND_10BIT_ABGR
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_ABGR16161616F,
#endif
	DRM_FORMAT_NV12,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YVU420_ANDROID
};

static const uint32_t video_yuv_formats[] = {
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV12,
#ifdef SUPPORT_P010
	DRM_FORMAT_P010,
#endif
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YVU420_ANDROID
};

// In addition to all scanout we should also support R8 and non YUV texture formats.
static const uint32_t gpu_data_buffer_formats[] = {
	DRM_FORMAT_R8,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_ABGR16161616F
};
// clang-format on

static bool is_video_yuv_format(uint32_t format)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(video_yuv_formats); ++i) {
		if (format == video_yuv_formats[i])
			return true;
	}
	return false;
}

#define PROTECTED_DMA_HEAP_PATH "/dev/dma_heap/restricted_mtk_cma"

static int mediatek_init(struct driver *drv)
{
	struct format_metadata metadata;
	struct mediatek_private_drv_data *priv;
	uint64_t protected = 0;

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		drv_loge("Failed calloc private data, errno=%d\n", -errno);
		return -errno;
	}

#ifdef SUPPORT_PROTECTED_DMA_HEAP
	protected = BO_USE_PROTECTED;
	priv->dma_heap_fd = open(PROTECTED_DMA_HEAP_PATH, O_RDONLY | O_CLOEXEC);
#if defined(ANDROID)
	// On ChromeOS, `protected` is always set to BO_USE_PROTECTED. There is
	// an issue with the sandbox that prevents opening the DMA heap CMA
	// file for some processes. For processes that need to actually allocate
	// buffers, the open() call succeeds.
	// TODO(b:444480658) figure out why open() fails in Chrome sometimes.
	// On Android this unsets `protected` when there is no DMA heap support
	// or if the file does not exist.
#if (ANDROID_API_LEVEL >= 31 && defined(HAS_DMABUF_SYSTEM_HEAP))
	if (priv->dma_heap_fd < 0) {
		if (errno == EACCES) {
			// The heap is there but we cannot access it.
			// Still report that protected buffers are theoretically available.
			// There will be a log message about sepolicy violation.
		} else {
			drv_logi("Failed opening secure CMA heap with error %s.\n",
				 strerror(errno));
			protected = 0;
		}
	}
#else
	protected = 0;
#endif // (ANDROID_API_LEVEL >= 31 && defined(HAS_DMABUF_SYSTEM_HEAP))
#endif // defined(ANDROID)
#else
	priv->dma_heap_fd = -1;
#endif // SUPPORT_PROTECTED_DMA_HEAP

	priv->init_subdrv_once = true;
	priv->subdrv_fd = -1;

	drv->priv = priv;

	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK | BO_USE_SCANOUT | protected);

	drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
			     &LINEAR_METADATA, BO_USE_TEXTURE_MASK | protected);

	drv_add_combination(drv, DRM_FORMAT_R8, &LINEAR_METADATA,
			    BO_USE_SW_MASK | BO_USE_LINEAR | protected);

#ifdef SUPPORT_AR30_OVERLAYS
	drv_add_combination(drv, DRM_FORMAT_ARGB2101010, &LINEAR_METADATA,
			    BO_USE_TEXTURE | BO_USE_SCANOUT | protected | BO_USE_LINEAR);
#endif

	/* YUYV format for video overlay and camera subsystem. */
	drv_add_combination(drv, DRM_FORMAT_YUYV, &LINEAR_METADATA,
			    BO_USE_HW_VIDEO_DECODER | BO_USE_SCANOUT | BO_USE_LINEAR |
				BO_USE_TEXTURE | protected);

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	/* Support BO_USE_HW_VIDEO_DECODER for protected content minigbm allocations. */
	metadata.tiling = TILE_TYPE_LINEAR;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_LINEAR;
	drv_modify_combination(drv, DRM_FORMAT_YVU420, &metadata,
			       BO_USE_HW_VIDEO_DECODER | protected);
#ifdef MTK_MT8173
	/*
	 * b/292507490: The MT8173 decoder can output YUV420 only. Some CTS tests feed the
	 * decoded buffer to the hardware encoder and the tests allocate the buffer with
	 * DRM_FORMAT_FLEX_YCbCr_420_888 with the mask of BO_USE_HW_VIDEO_ENCODER |
	 * BO_USE_HW_VIDEO_DECODER. Therefore, we have to allocate YUV420 in the case.
	 */
	drv_modify_combination(drv, DRM_FORMAT_YVU420, &metadata, BO_USE_HW_VIDEO_ENCODER);
#endif
	drv_modify_combination(drv, DRM_FORMAT_YVU420_ANDROID, &metadata,
			       BO_USE_HW_VIDEO_DECODER | protected);
#ifndef MTK_MT8173
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata,
			       BO_USE_HW_VIDEO_DECODER | protected);
#endif
	drv_modify_combination(drv, DRM_FORMAT_P010, &metadata,
			       BO_USE_HW_VIDEO_DECODER | protected);

	/*
	 * R8 format is used for Android's HAL_PIXEL_FORMAT_BLOB for input/output from
	 * hardware decoder/encoder.
	 */
	drv_modify_combination(drv, DRM_FORMAT_R8, &metadata,
			       BO_USE_HW_VIDEO_DECODER | BO_USE_HW_VIDEO_ENCODER |
				   BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE |
				   BO_USE_GPU_DATA_BUFFER | BO_USE_SENSOR_DIRECT_DATA);

	/* NV12 format for encoding and display. */
#ifndef MTK_MT8173
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata,
			       BO_USE_SCANOUT | BO_USE_HW_VIDEO_ENCODER | BO_USE_CAMERA_READ |
				   BO_USE_CAMERA_WRITE | BO_USE_SW_MASK);
#else
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata,
			       BO_USE_SCANOUT | BO_USE_HW_VIDEO_ENCODER | BO_USE_CAMERA_READ |
				   BO_USE_CAMERA_WRITE);
#endif

	/*
	 * Android also frequently requests YV12 formats for some camera implementations
	 * (including the external provider implementation).
	 */
	drv_modify_combination(drv, DRM_FORMAT_YVU420_ANDROID, &metadata, BO_USE_CAMERA_WRITE);

#ifdef MTK_MT8183
	/* Only for MT8183 Camera subsystem */
	drv_modify_combination(drv, DRM_FORMAT_NV21, &metadata,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);
	drv_modify_combination(drv, DRM_FORMAT_YUYV, &metadata,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);
	drv_modify_combination(drv, DRM_FORMAT_YVU420, &metadata,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);
	/* Private formats for private reprocessing in camera */
	drv_add_combination(drv, DRM_FORMAT_MTISP_SXYZW10, &metadata,
			    BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_SW_MASK);
#endif

	for (unsigned i = 0; i < ARRAY_SIZE(render_target_formats); i++) {
		drv_modify_combination(drv, render_target_formats[i], &metadata,
				       BO_USE_GPU_DATA_BUFFER);
	}
	for (unsigned i = 0; i < ARRAY_SIZE(gpu_data_buffer_formats); i++) {
		drv_modify_combination(drv, gpu_data_buffer_formats[i], &metadata,
				       BO_USE_GPU_DATA_BUFFER);
	}

	return drv_modify_linear_combinations(drv);
}

static void mediatek_close(struct driver *drv)
{
	struct mediatek_private_drv_data *priv = (struct mediatek_private_drv_data *)drv->priv;

	if (priv->dma_heap_fd >= 0)
		close(priv->dma_heap_fd);

	if (priv->subdrv) {
		drv_destroy(priv->subdrv);
		close(priv->subdrv_fd);
	}

	free(priv);
	drv->priv = NULL;
}

static bool bo_forward_to_subdrv(struct bo *bo)
{
	struct mediatek_private_drv_data *priv = bo->drv->priv;

	/* drm_mediatek allocates from 32-bit dma address space. When there is
	 * no known HW usage, we can allocate from the sub-driver to
	 * circumvent the limit.
	 */
	if (bo->meta.use_flags & BO_USE_HW_MASK)
		return false;

	/* Limit the forward to blobs for now. */
	const bool is_format_blob =
	    bo->meta.height == 1 && bo->meta.format == DRM_FORMAT_R8 && bo->meta.num_planes == 1;
	if (!is_format_blob)
		return false;

	if (priv->init_subdrv_once) {
		const char subdrv_path[] = "/dev/dma_heap/system";

		priv->init_subdrv_once = false;

		priv->subdrv_fd = open(subdrv_path, O_RDONLY | O_CLOEXEC);
		if (priv->subdrv_fd >= 0) {
			priv->subdrv = drv_create(priv->subdrv_fd, &backend_dma_heap);
			if (!priv->subdrv) {
				drv_logi("failed to create sub-driver\n");
				close(priv->subdrv_fd);
				priv->subdrv_fd = -1;
			}
		} else {
			drv_logi("failed to open %s\n", subdrv_path);
		}
	}
	if (!priv->subdrv)
		return false;

	bo->drv = priv->subdrv;
	return true;
}

static int mediatek_bo_create_with_modifiers(struct bo *bo, uint32_t width, uint32_t height,
					     uint32_t format, const uint64_t *modifiers,
					     uint32_t count)
{
	int ret;
	size_t plane;
	uint32_t stride;
	struct drm_mtk_gem_create gem_create = { 0 };

	const bool is_camera_write = bo->meta.use_flags & BO_USE_CAMERA_WRITE;
	const bool is_hw_video_encoder = bo->meta.use_flags & BO_USE_HW_VIDEO_ENCODER;
	const bool is_linear = bo->meta.use_flags & BO_USE_LINEAR;
	const bool is_protected = bo->meta.use_flags & BO_USE_PROTECTED;
	const bool is_scanout = bo->meta.use_flags & BO_USE_SCANOUT;
	const bool is_cursor = bo->meta.use_flags & BO_USE_CURSOR;
	/*
	 * We identify the ChromeOS Camera App buffers via these two USE flags. Those buffers need
	 * the same alignment as the video hardware encoding.
	 */
	const bool is_camera_preview = is_scanout && is_camera_write;
#ifdef MTK_MT8173
	const bool is_mt8173_video_decoder = bo->meta.use_flags & BO_USE_HW_VIDEO_DECODER;
#else
	const bool is_mt8173_video_decoder = false;
#endif
	/*
	 * Android sends blobs for encoding in the shape of a single-row pixel buffer. Use R8 +
	 * single row as a proxy for Android HAL_PIXEL_FORMAT_BLOB until a drm equivalent is
	 * defined.
	 */
	const bool is_format_blob = format == DRM_FORMAT_R8 && height == 1;

	if (!drv_has_modifier(modifiers, count, DRM_FORMAT_MOD_LINEAR)) {
		errno = EINVAL;
		drv_loge("no usable modifier found\n");
		return -EINVAL;
	}

	if (bo_forward_to_subdrv(bo)) {
		ret = bo->drv->backend->bo_compute_metadata(bo, width, height, format,
							    bo->meta.use_flags, modifiers, count);
		if (!ret)
			ret = bo->drv->backend->bo_create_from_metadata(bo);

		return ret;
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
	if (is_cursor) {
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
	 * Since the ARM L1 cache line size is 64 bytes, align to that as a
	 * performance optimization, except for video buffers on certain platforms,
	 * these should only be accessed from the GPU and VCODEC subsystems (maybe
	 * also MDP), so it's better to align to macroblocks.
	 */
	stride = drv_stride_from_format(format, width, 0);
#ifdef DONT_USE_64_ALIGNMENT_FOR_VIDEO_BUFFERS
	const uint32_t alignment = is_video_yuv_format(format) ? 16 : 64;
	stride = ALIGN(stride, alignment);
#else
	stride = ALIGN(stride, 64);
#endif

	/*
	 * The mediatek video decoder requires to align width and height by 64. But this is
	 * the requirement for mediatek tiled format (e.g. MT21 and MM21). The buffers are
	 * not allocated by minigbm. So we don't have to care about it. The tiled buffer is
	 * converted to NV12 or YV12, which is allocated by minigbm. V4L2 MDP doesn't
	 * require any special alignment for them.
	 * On the other hand, the mediatek video encoder requires a padding on each plane.
	 * When both video decoder and encoder use flag is masked (in some CTS test), we
	 * align with the encoder alignment.
	 * However, V4L2VideoDecodeAccelerator used on MT8173 fails handling the buffer with
	 * padding, although V4L2VideoDecoder used on MT8183 and later can do. We workaround
	 * this problem to allocate a buffer without padding on MT8173. This works because
	 * MT8173 decoder's output NV12 is converted to YV12 buffer that is allocated with
	 * video encoder usage mask only and thus have padding in Android.
	 * See go/mediatek-video-buffer-alignment-note for detail.
	 */
	if ((is_hw_video_encoder && !is_mt8173_video_decoder && !is_format_blob) ||
	    is_camera_preview) {
		uint32_t aligned_height = ALIGN(height, 32);
		uint32_t padding[DRV_MAX_PLANES] = { 0 };

		for (plane = 0; plane < bo->meta.num_planes; ++plane) {
			uint32_t plane_stride = drv_stride_from_format(format, stride, plane);
			padding[plane] = plane_stride *
					 (32 / drv_vertical_subsampling_from_format(format, plane));
		}

		drv_bo_from_format_and_padding(bo, stride, 1, aligned_height, format, padding);
	} else {
#ifdef USE_EXTRA_PADDING_FOR_YVU420
		/*
		 * Apply extra padding for YV12 if the height does not meet round up requirement and
		 * the image is to be sampled by gpu.
		 */
		static const uint32_t required_round_up = 4;
		const uint32_t height_mod = height % required_round_up;
		const bool is_texture = bo->meta.use_flags & BO_USE_TEXTURE;
		/*
		 * YVU420 and YVU420_ANDROID treatments have been aligned in mediatek backend. Check
		 * both since gbm frontend still maps linear YVU420 to YVU420_ANDROID for other hw
		 * backends.
		 */
		const bool is_format_yv12 =
		    format == DRM_FORMAT_YVU420 || format == DRM_FORMAT_YVU420_ANDROID;
#endif
#ifdef SUPPORT_YUV422
		/*
		 * JPEG Encoder Accelerator requires 16x16 alignment. We want the buffer
		 * from camera can be put in JEA directly so align the height to 16
		 * bytes.
		 */
		if (format == DRM_FORMAT_NV12)
			height = ALIGN(height, 16);
#endif
		drv_bo_from_format(bo, stride, 1, height, format);

#ifdef USE_EXTRA_PADDING_FOR_YVU420
		if (is_format_yv12 && is_texture && height_mod) {
			const uint32_t height_padding = required_round_up - height_mod;
			const uint32_t y_padding =
			    drv_size_from_format(format, bo->meta.strides[0], height_padding, 0);
			const uint32_t u_padding =
			    drv_size_from_format(format, bo->meta.strides[2], height_padding, 2);
			const uint32_t vu_size = drv_bo_get_plane_size(bo, 2) * 2;

			bo->meta.total_size += u_padding;

			/*
			 * Since we are not aligning Y, we must make sure that its padding fits
			 * inside the rest of the space allocated for the V/U planes.
			 */
			if (y_padding > vu_size) {
				/* Align with mali workaround to pad all 3 planes. */
				bo->meta.total_size += y_padding + u_padding;
			}
		}
#endif
	}

	/* For protected data buffer needs to allocate from DMA_HEAP directly */
	if (is_protected) {
#if !defined(ANDROID) || (ANDROID_API_LEVEL >= 31 && defined(HAS_DMABUF_SYSTEM_HEAP))
		int ret;
		struct mediatek_private_drv_data *priv =
		    (struct mediatek_private_drv_data *)bo->drv->priv;
		struct dma_heap_allocation_data heap_data = {
			.len = bo->meta.total_size,
			.fd_flags = O_RDWR | O_CLOEXEC,
		};

		/*
		 * Android Desktop hardware video decoders stack currently could not allocate the
		 * MT2T and P010 buffers accordingly. As a short term workaround, a secure P010
		 * buffer is allocated always and handle the P010 buffer as MT2T in the video stack.
		 * TODO(b/495560746): Remove this workaround and allocate P010 and MT2T per buffer
		 * usage.
		 */
#if !defined(ANDROID)
		if (format == DRM_FORMAT_P010) {
			/*
			 * Adjust the size so we don't waste tons of space. This was allocated
			 * with 16 bpp, but we only need 10 bpp. We can safely divide by 8 because
			 * we are aligned at a multiple higher than that.
			 */
			bo->meta.strides[0] = bo->meta.strides[0] * 10 / 16;
			bo->meta.strides[1] = bo->meta.strides[1] * 10 / 16;
			bo->meta.sizes[0] = bo->meta.sizes[0] * 10 / 16;
			bo->meta.sizes[1] = bo->meta.sizes[1] * 10 / 16;
			bo->meta.offsets[1] = bo->meta.sizes[0];
			bo->meta.total_size = bo->meta.total_size * 10 / 16;
			heap_data.len = bo->meta.total_size;
		}
#endif // !defined(Android)

		if (priv->dma_heap_fd < 0) {
			priv->dma_heap_fd = open(PROTECTED_DMA_HEAP_PATH, O_RDONLY | O_CLOEXEC);
			if (priv->dma_heap_fd < 0) {
				drv_loge("Failed opening secure CMA heap with error %s.\n",
					 strerror(errno));
				return -errno;
			}
		}

		ret = ioctl(priv->dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
		if (ret < 0) {
			drv_loge("Failed allocating CMA buffer ret=%d\n", ret);
			return ret;
		}

		/* Create GEM handle for secure CMA and close FD here */
		ret = drmPrimeFDToHandle(bo->drv->fd, heap_data.fd, &bo->handle.u32);
		close(heap_data.fd);
		if (ret) {
			drv_loge("Failed drmPrimeFDToHandle(fd:%d) ret=%d\n", heap_data.fd, ret);
			return ret;
		}
#else
		drv_loge("Protected allocation not supported\n");
		return -1;
#endif
		return 0;
	}

	/*
	 * For linear scanout buffers, the read/write pattern is usually linear i.e. each address is
	 * accessed sequentially, and there are fewer chances that an address will be repeatedly
	 * accessed.
	 * This behavior leads to less TLB dependency and cache misses i.e. no need to translate the
	 * same virtual address to a physical address multiple times.
	 *
	 * With that premise, it's safe to allow the DMA framework to fulfill such allocation
	 * requests with non-continuous smaller chunks of memory (e.g., 4KiB single pages) which
	 * are generally easier to allocate compared to large continuous chunks of memory, improving
	 * memory allocation efficiency and reduce the risk of allocation failures, especially when
	 * available memory budget is low or on memory-constrained devices.
	 */
	if (is_linear && is_scanout)
		gem_create.flags |= DRM_MTK_GEM_CREATE_FLAG_ALLOC_SINGLE_PAGES;

	gem_create.size = bo->meta.total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_CREATE, &gem_create);
	if (ret) {
		drv_loge("DRM_IOCTL_MTK_GEM_CREATE failed (size=%" PRIu64 ")\n", gem_create.size);
		return -errno;
	}

	bo->handle.u32 = gem_create.handle;

	return 0;
}

static int mediatek_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			      uint64_t use_flags)
{
	uint64_t modifiers[] = { DRM_FORMAT_MOD_LINEAR };
	return mediatek_bo_create_with_modifiers(bo, width, height, format, modifiers,
						 ARRAY_SIZE(modifiers));
}

static int mediatek_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	if (bo_forward_to_subdrv(bo))
		return bo->drv->backend->bo_import(bo, data);

	return drv_prime_bo_import(bo, data);
}

static void *mediatek_bo_map(struct bo *bo, struct vma *vma, uint32_t map_flags)
{
	int ret, prime_fd;
	struct drm_mtk_gem_map_off gem_map = { 0 };
	struct mediatek_private_map_data *priv;
	void *addr = NULL;

	gem_map.handle = bo->handle.u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_MAP_OFFSET, &gem_map);
	if (ret) {
		drv_loge("DRM_IOCTL_MTK_GEM_MAP_OFFSET failed\n");
		return MAP_FAILED;
	}

	prime_fd = drv_bo_get_plane_fd(bo, 0);
	if (prime_fd < 0) {
		drv_loge("Failed to get a prime fd\n");
		return MAP_FAILED;
	}

	addr = mmap(0, bo->meta.total_size, drv_get_prot(map_flags), MAP_SHARED, bo->drv->fd,
		    gem_map.offset);
	if (addr == MAP_FAILED)
		goto out_close_prime_fd;

	vma->length = bo->meta.total_size;

	priv = calloc(1, sizeof(*priv));
	if (!priv)
		goto out_unmap_addr;

	if (bo->meta.use_flags & BO_USE_RENDERSCRIPT) {
		priv->cached_addr = calloc(1, bo->meta.total_size);
		if (!priv->cached_addr)
			goto out_free_priv;

		priv->gem_addr = addr;
		addr = priv->cached_addr;
	}

	priv->prime_fd = prime_fd;
	vma->priv = priv;

	return addr;

out_free_priv:
	free(priv);
out_unmap_addr:
	munmap(addr, bo->meta.total_size);
out_close_prime_fd:
	close(prime_fd);
	return MAP_FAILED;
}

static int mediatek_bo_unmap(struct bo *bo, struct vma *vma)
{
	if (vma->priv) {
		struct mediatek_private_map_data *priv = vma->priv;

		if (priv->cached_addr) {
			vma->addr = priv->gem_addr;
			free(priv->cached_addr);
		}

		close(priv->prime_fd);
		free(priv);
		vma->priv = NULL;
	}

	return munmap(vma->addr, vma->length);
}

static int mediatek_bo_invalidate(struct bo *bo, struct mapping *mapping)
{
	struct mediatek_private_map_data *priv = mapping->vma->priv;

	if (priv) {
		struct pollfd fds = {
			.fd = priv->prime_fd,
		};

		if (mapping->vma->map_flags & BO_MAP_WRITE)
			fds.events |= POLLOUT;

		if (mapping->vma->map_flags & BO_MAP_READ)
			fds.events |= POLLIN;

		poll(&fds, 1, -1);
		if (fds.revents != fds.events)
			drv_loge("poll prime_fd failed\n");

		if (priv->cached_addr)
			memcpy(priv->cached_addr, priv->gem_addr, bo->meta.total_size);
	}

	return 0;
}

static int mediatek_bo_flush(struct bo *bo, struct mapping *mapping)
{
	struct mediatek_private_map_data *priv = mapping->vma->priv;
	if (priv && priv->cached_addr && (mapping->vma->map_flags & BO_MAP_WRITE))
		memcpy(priv->gem_addr, priv->cached_addr, bo->meta.total_size);

	return 0;
}

static void mediatek_resolve_format_and_use_flags(struct driver *drv, uint32_t format,
						  uint64_t use_flags, uint32_t *out_format,
						  uint64_t *out_use_flags)
{
	*out_format = format;
	*out_use_flags = use_flags;
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
#ifdef MTK_MT8183
		/* Only MT8183 Camera subsystem offers private reprocessing
		 * capability. CAMERA_READ indicates the buffer is intended for
		 * reprocessing and hence given the private format for MTK. */
		if (use_flags & BO_USE_CAMERA_READ) {
			*out_format = DRM_FORMAT_MTISP_SXYZW10;
			break;
		}
#endif
		if (use_flags & BO_USE_CAMERA_WRITE) {
			*out_format = DRM_FORMAT_NV12;
			break;
		}

		/* HACK: See b/28671744 */
		*out_format = DRM_FORMAT_XBGR8888;
		*out_use_flags &= ~BO_USE_HW_VIDEO_ENCODER;
		break;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
#ifndef MTK_MT8173
		*out_format = DRM_FORMAT_NV12;
		break;
#else
		/*
		 * b/292507490: The MT8173 decoder can output YUV420 only. Some CTS tests feed the
		 * decoded buffer to the hardware encoder and the tests allocate the buffer with
		 * DRM_FORMAT_FLEX_YCbCr_420_888 with the mask of BO_USE_HW_VIDEO_ENCODER |
		 * BO_USE_HW_VIDEO_DECODER. Therefore, we have to allocate YUV420 in the case.
		 */
		if ((use_flags &
		     (BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_HW_VIDEO_ENCODER)) &&
		    !(use_flags & BO_USE_HW_VIDEO_DECODER)) {
			*out_format = DRM_FORMAT_NV12;
			break;
		}
		/* HACK: See b/139714614 */
		*out_format = DRM_FORMAT_YVU420;
		*out_use_flags &= ~BO_USE_SCANOUT;
		break;
#endif
	default:
		break;
	}
	/* Mediatek doesn't support YUV overlays */
	if (is_video_yuv_format(format))
		*out_use_flags &= ~BO_USE_SCANOUT;
}

const struct backend backend_mediatek = {
	.name = "mediatek",
	.init = mediatek_init,
	.close = mediatek_close,
	.bo_create = mediatek_bo_create,
	.bo_create_with_modifiers = mediatek_bo_create_with_modifiers,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = mediatek_bo_import,
	.bo_export = drv_prime_bo_export,
	.bo_map = mediatek_bo_map,
	.bo_unmap = mediatek_bo_unmap,
	.bo_invalidate = mediatek_bo_invalidate,
	.bo_flush = mediatek_bo_flush,
	.resolve_format_and_use_flags = mediatek_resolve_format_and_use_flags,
};

#endif
