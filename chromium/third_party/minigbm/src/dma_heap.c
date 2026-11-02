/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "drv_helpers.h"
#include "drv_priv.h"
#include "external/dma-buf.h"
#include "external/dma-heap.h"
#include "util.h"

/* blob format only */
static const uint32_t dma_heap_blob_format = DRM_FORMAT_R8;

/* all blob use flags */
static const uint64_t dma_heap_blob_use_flags =
    BO_USE_LINEAR | BO_USE_SW_MASK | BO_USE_GPU_DATA_BUFFER | BO_USE_SENSOR_DIRECT_DATA;

static bool bo_is_supported(const struct bo *bo, const uint64_t *modifiers, uint32_t count)
{
	return bo->meta.height == 1 && bo->meta.format == dma_heap_blob_format &&
	       bo->meta.num_planes == 1 && !(bo->meta.use_flags & ~dma_heap_blob_use_flags) &&
	       (!count || drv_has_modifier(modifiers, count, DRM_FORMAT_MOD_LINEAR) ||
		drv_has_modifier(modifiers, count, DRM_FORMAT_MOD_INVALID));
}

static int dma_heap_bo_compute_metadata(struct bo *bo, uint32_t width, uint32_t height,
					uint32_t format, uint64_t use_flags,
					const uint64_t *modifiers, uint32_t count)
{
	if (!bo_is_supported(bo, modifiers, count))
		return -EINVAL;

	bo->meta.sizes[0] = bo->meta.width;
	bo->meta.total_size = bo->meta.width;

	return 0;
}

static int dma_heap_bo_create_from_metadata(struct bo *bo)
{
	struct dma_heap_allocation_data args = {
		.len = bo->meta.total_size,
		.fd_flags = O_RDWR | O_CLOEXEC,
	};
	int ret;

	ret = ioctl(bo->drv->fd, DMA_HEAP_IOCTL_ALLOC, &args);
	if (ret)
		return -errno;

	bo->handle.fd = args.fd;

	return 0;
}

static int dma_heap_bo_destroy(struct bo *bo)
{
	return close(bo->handle.fd);
}

static int dma_heap_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	int fd;

	if (!bo_is_supported(bo, &data->format_modifier, 1))
		return -EINVAL;

	if (data->fds[0] < 0 || data->offsets[0])
		return -EINVAL;

	fd = dup(data->fds[0]);
	if (fd < 0)
		return -errno;

	bo->handle.fd = fd;

	return 0;
}

static int dma_heap_bo_export(struct bo *bo, size_t plane)
{
	const int fd = dup(bo->handle.fd);
	return fd >= 0 ? fd : -errno;
}

static void *dma_heap_bo_map(struct bo *bo, struct vma *vma, uint32_t map_flags)
{
	vma->length = bo->meta.total_size;
	return mmap(NULL, vma->length, drv_get_prot(map_flags), MAP_SHARED, bo->handle.fd, 0);
}

static bool bo_wait(struct bo *bo, uint32_t map_flags)
{
	const int timeout = -1;
	struct pollfd pollfd = {
		.fd = bo->handle.fd,
		.events = map_flags & BO_MAP_WRITE ? POLLOUT : POLLIN,
	};

	while (true) {
		const int ret = poll(&pollfd, 1, timeout);
		if (ret > 0)
			return pollfd.revents & pollfd.events;
		else if (ret == 0 || !(errno == EINTR || errno == EAGAIN))
			return false;
	}
}

static int bo_sync(struct bo *bo, bool start, uint32_t map_flags)
{
	struct dma_buf_sync args = { 0 };

	args.flags |= start ? DMA_BUF_SYNC_START : DMA_BUF_SYNC_END;

	if (map_flags & BO_MAP_READ)
		args.flags |= DMA_BUF_SYNC_READ;
	if (map_flags & BO_MAP_WRITE)
		args.flags |= DMA_BUF_SYNC_WRITE;

	return ioctl(bo->handle.fd, DMA_BUF_IOCTL_SYNC, &args) ? -errno : 0;
}

static int dma_heap_bo_invalidate(struct bo *bo, struct mapping *mapping)
{
	/* (incorrectly) treat invalidate as start cpu access */
	if (!bo_wait(bo, mapping->vma->map_flags))
		return -EINVAL;
	return bo_sync(bo, true, mapping->vma->map_flags);
}

static int dma_heap_bo_flush(struct bo *bo, struct mapping *mapping)
{
	/* (incorrectly) treat flush as end cpu access */
	return bo_sync(bo, false, mapping->vma->map_flags);
}

static int dma_heap_init(struct driver *drv)
{
	drv_add_combinations(drv, &dma_heap_blob_format, 1, &LINEAR_METADATA,
			     dma_heap_blob_use_flags);

	return 0;
}

const struct backend backend_dma_heap = {
	.name = "dma_heap",
	.init = dma_heap_init,
	.bo_compute_metadata = dma_heap_bo_compute_metadata,
	.bo_create_from_metadata = dma_heap_bo_create_from_metadata,
	.bo_destroy = dma_heap_bo_destroy,
	.bo_import = dma_heap_bo_import,
	.bo_export = dma_heap_bo_export,
	.bo_map = dma_heap_bo_map,
	.bo_unmap = drv_bo_munmap,
	.bo_invalidate = dma_heap_bo_invalidate,
	.bo_flush = dma_heap_bo_flush,
	.resolve_format_and_use_flags = drv_resolve_format_and_use_flags_helper,
};
