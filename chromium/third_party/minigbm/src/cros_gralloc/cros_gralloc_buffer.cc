/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc_buffer.h"

#include <assert.h>
#include <sys/mman.h>

#include <cutils/native_handle.h>

#include "cros_gralloc_buffer_metadata.h"

#ifndef HAS_NO_AIDL_METADATA
using aidl::android::hardware::graphics::common::BlendMode;
using aidl::android::hardware::graphics::common::Cta861_3;
using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::Smpte2086;
#endif // HAS_NO_AIDL_METADATA

/*static*/
std::unique_ptr<cros_gralloc_buffer>
cros_gralloc_buffer::create(struct bo *acquire_bo,
			    const struct cros_gralloc_handle *borrowed_handle)
{
	auto acquire_hnd =
	    reinterpret_cast<struct cros_gralloc_handle *>(native_handle_clone(borrowed_handle));
	if (!acquire_hnd) {
		ALOGE("Failed to create cros_gralloc_buffer: failed to clone handle.");
		return {};
	}

	std::unique_ptr<cros_gralloc_buffer> buffer(
	    new cros_gralloc_buffer(acquire_bo, acquire_hnd));
	if (!buffer) {
		ALOGE("Failed to create cros_gralloc_buffer: failed to allocate.");
		native_handle_close(acquire_hnd);
		native_handle_delete(acquire_hnd);
		return {};
	}

	return buffer;
}

int32_t
cros_gralloc_buffer::initialize_metadata(const struct cros_gralloc_buffer_descriptor *descriptor)
{
	struct cros_gralloc_buffer_metadata *metadata;

	assert(descriptor->enable_metadata_fd);
	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to initialize metadata: failed to get metadata region.");
		return ret;
	}

	if (metadata == nullptr) {
		ALOGE("Failed to initialize metadata: invalid metadata address.");
		return -1;
	}

	new (metadata) cros_gralloc_buffer_metadata();

	snprintf(metadata->name, CROS_GRALLOC_BUFFER_METADATA_MAX_NAME_SIZE, "%s",
		 descriptor->name.c_str());
#ifndef HAS_NO_AIDL_METADATA
	metadata->dataspace = descriptor->dataspace;
	metadata->blend_mode = descriptor->blend;
	metadata->smpte2094_50_size = 0;
#endif // HAS_NO_AIDL_METADATA
	return 0;
}

cros_gralloc_buffer::cros_gralloc_buffer(struct bo *acquire_bo,
					 struct cros_gralloc_handle *acquire_handle)
    : bo_(acquire_bo), hnd_(acquire_handle)
{
	assert(bo_);
	assert(hnd_);
	for (uint32_t plane = 0; plane < DRV_MAX_PLANES; plane++)
		lock_data_[plane] = nullptr;
}

cros_gralloc_buffer::~cros_gralloc_buffer()
{
	drv_bo_destroy(bo_);
	if (reserved_region_addr_) {
		munmap(reserved_region_addr_, hnd_->reserved_region_size);
	}
	native_handle_close(hnd_);
	native_handle_delete(hnd_);
}

uint32_t cros_gralloc_buffer::get_id() const
{
	return hnd_->id;
}

uint32_t cros_gralloc_buffer::get_width() const
{
	return hnd_->width;
}

uint32_t cros_gralloc_buffer::get_pixel_stride() const
{
	return hnd_->pixel_stride;
}

uint32_t cros_gralloc_buffer::get_height() const
{
	return hnd_->height;
}

uint32_t cros_gralloc_buffer::get_format() const
{
	return hnd_->format;
}

uint64_t cros_gralloc_buffer::get_format_modifier() const
{
	return hnd_->format_modifier;
}

uint64_t cros_gralloc_buffer::get_total_size() const
{
	return hnd_->total_size;
}

uint32_t cros_gralloc_buffer::get_num_planes() const
{
	return hnd_->num_planes;
}

uint32_t cros_gralloc_buffer::get_plane_offset(uint32_t plane) const
{
	return hnd_->offsets[plane];
}

uint32_t cros_gralloc_buffer::get_plane_stride(uint32_t plane) const
{
	return hnd_->strides[plane];
}

uint32_t cros_gralloc_buffer::get_plane_size(uint32_t plane) const
{
	return hnd_->sizes[plane];
}

int64_t cros_gralloc_buffer::get_plane_fd(uint32_t plane) const
{
	if (plane >= hnd_->num_planes) {
		return -1;
	}
	return hnd_->fds[plane];
}

int32_t cros_gralloc_buffer::get_android_format() const
{
	return hnd_->droid_format;
}

int64_t cros_gralloc_buffer::get_android_usage() const
{
	return hnd_->usage;
}

int32_t cros_gralloc_buffer::get_name(std::optional<std::string> *name) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_name: failed to get metadata.");
		return ret;
	}

	*name = metadata->name;
	return 0;
}

#ifndef HAS_NO_AIDL_METADATA
int32_t cros_gralloc_buffer::get_blend_mode(std::optional<BlendMode> *blend_mode) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_blend_mode: failed to get metadata.");
		return ret;
	}

	*blend_mode = metadata->blend_mode;
	return 0;
}

int32_t cros_gralloc_buffer::set_blend_mode(BlendMode blend_mode)
{
	struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to set_blend_mode: failed to get metadata.");
		return ret;
	}

	metadata->blend_mode = blend_mode;
	return 0;
}

int32_t cros_gralloc_buffer::get_dataspace(std::optional<Dataspace> *dataspace) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_dataspace: failed to get metadata.");
		return ret;
	}

	*dataspace = metadata->dataspace;
	return 0;
}

int32_t cros_gralloc_buffer::set_dataspace(Dataspace dataspace)
{
	struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to set_dataspace: failed to get metadata.");
		return ret;
	}

	metadata->dataspace = dataspace;
	return 0;
}

int32_t cros_gralloc_buffer::get_cta861_3(std::optional<Cta861_3> *cta) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_cta861_3: failed to get metadata.");
		return ret;
	}

	*cta = metadata->cta861_3.to_std_optional();
	return 0;
}

int32_t cros_gralloc_buffer::set_cta861_3(std::optional<Cta861_3> cta)
{
	struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to set_cta861_3: failed to get metadata.");
		return ret;
	}

	metadata->cta861_3 = cta;
	return 0;
}

int32_t cros_gralloc_buffer::get_smpte2086(std::optional<Smpte2086> *smpte) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_smpte2086: failed to get metadata.");
		return ret;
	}

	*smpte = metadata->smpte2086.to_std_optional();
	return 0;
}

int32_t cros_gralloc_buffer::set_smpte2086(std::optional<Smpte2086> smpte)
{
	struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to set_smpte20986: failed to get metadata.");
		return ret;
	}

	metadata->smpte2086 = smpte;
	return 0;
}

int32_t cros_gralloc_buffer::get_smpte2094_50(std::optional<std::vector<uint8_t>> *smpte) const
{
	const struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to get_smpte2094_50: failed to get metadata.");
		return ret;
	}

	if (metadata->smpte2094_50_size <= 0) {
		smpte->reset();
	} else {
		smpte->emplace(metadata->smpte2094_50.begin(),
			       metadata->smpte2094_50.begin() + metadata->smpte2094_50_size);
	}
	return 0;
}

int32_t cros_gralloc_buffer::set_smpte2094_50(const std::optional<std::vector<uint8_t>> &smpte)
{
	struct cros_gralloc_buffer_metadata *metadata;

	int ret = get_metadata(&metadata);
	if (ret) {
		ALOGE("Failed to set_smpte2094_50: failed to get metadata.");
		return ret;
	}

	if (!smpte.has_value()) {
		metadata->smpte2094_50_size = 0;
		return 0;
	}

	if (smpte->size() > cros_gralloc_buffer_metadata::MAX_SMPTE2094_50_SIZE) {
		ALOGE("Failed to set_smpte2094_50: metadata payload is too large: %zu",
		      smpte->size());
		return -1;
	}

	metadata->smpte2094_50_size = smpte->size();
	std::copy(smpte->begin(), smpte->end(), metadata->smpte2094_50.begin());
	return 0;
}
#endif // HAS_NO_AIDL_METADATA

int32_t cros_gralloc_buffer::increase_refcount()
{
	return ++refcount_;
}

int32_t cros_gralloc_buffer::decrease_refcount()
{
	assert(refcount_ > 0);
	return --refcount_;
}

int32_t cros_gralloc_buffer::lock(const struct rectangle *rect, uint32_t map_flags,
				  uint8_t *addr[DRV_MAX_PLANES])
{
	void *vaddr = nullptr;

	memset(addr, 0, DRV_MAX_PLANES * sizeof(*addr));

	if (map_flags) {
		if (lock_data_[0]) {
			drv_bo_invalidate(bo_, lock_data_[0]);
			vaddr = lock_data_[0]->vma->addr;
		} else {
			struct rectangle r = *rect;

			if (!r.width && !r.height && !r.x && !r.y) {
				/*
				 * Android IMapper.hal: An accessRegion of all-zeros means the
				 * entire buffer.
				 */
				r.width = drv_bo_get_width(bo_);
				r.height = drv_bo_get_height(bo_);
			}

			vaddr = drv_bo_map(bo_, &r, map_flags, &lock_data_[0], 0);
		}

		if (vaddr == MAP_FAILED) {
			ALOGE("Mapping failed.");
			return -EFAULT;
		}
	}

	for (uint32_t plane = 0; plane < hnd_->num_planes; plane++)
		addr[plane] = static_cast<uint8_t *>(vaddr) + drv_bo_get_plane_offset(bo_, plane);

	lockcount_++;
	return 0;
}

int32_t cros_gralloc_buffer::unlock()
{
	if (lockcount_ <= 0) {
		ALOGE("Buffer was not locked.");
		return -EINVAL;
	}

	if (!--lockcount_) {
		if (lock_data_[0]) {
			drv_bo_flush_or_unmap(bo_, lock_data_[0]);
			lock_data_[0] = nullptr;
		}
	}

	return 0;
}

int32_t cros_gralloc_buffer::resource_info(uint32_t strides[DRV_MAX_PLANES],
					   uint32_t offsets[DRV_MAX_PLANES],
					   uint64_t *format_modifier)
{
	return drv_resource_info(bo_, strides, offsets, format_modifier);
}

int32_t cros_gralloc_buffer::invalidate()
{
	if (lockcount_ <= 0) {
		ALOGE("Buffer was not locked.");
		return -EINVAL;
	}

	if (lock_data_[0])
		return drv_bo_invalidate(bo_, lock_data_[0]);

	return 0;
}

int32_t cros_gralloc_buffer::flush()
{
	if (lockcount_ <= 0) {
		ALOGE("Buffer was not locked.");
		return -EINVAL;
	}

	if (lock_data_[0])
		return drv_bo_flush(bo_, lock_data_[0]);

	return 0;
}

int32_t cros_gralloc_buffer::get_reserved_region(void **addr, uint64_t *size) const
{
	int32_t reserved_region_fd = hnd_->fds[hnd_->num_planes];
	if (reserved_region_fd < 0) {
		ALOGE("Buffer does not have reserved region.");
		return -EINVAL;
	}

	if (!reserved_region_addr_) {
		reserved_region_addr_ =
		    mmap(nullptr, hnd_->reserved_region_size, PROT_WRITE | PROT_READ, MAP_SHARED,
			 reserved_region_fd, 0);
		if (reserved_region_addr_ == MAP_FAILED) {
			ALOGE("Failed to mmap reserved region: %s.", strerror(errno));
			return -errno;
		}
	}

	*addr = reserved_region_addr_;
	*size = hnd_->reserved_region_size;
	return 0;
}

int32_t cros_gralloc_buffer::get_client_reserved_region(void **client_reserved_region_addr,
							uint64_t *client_reserved_region_size) const
{
	int32_t ret = get_reserved_region(client_reserved_region_addr, client_reserved_region_size);
	if (ret) {
		return ret;
	}

	*client_reserved_region_addr =
	    reinterpret_cast<void *>(reinterpret_cast<char *>(*client_reserved_region_addr) +
				     sizeof(struct cros_gralloc_buffer_metadata));
	*client_reserved_region_size =
	    *client_reserved_region_size - sizeof(struct cros_gralloc_buffer_metadata);
	return 0;
}

int32_t cros_gralloc_buffer::get_metadata(struct cros_gralloc_buffer_metadata **metadata)
{
	void *metadata_addr;
	uint64_t metadata_region_size;
	int32_t ret = get_reserved_region(&metadata_addr, &metadata_region_size);
	if (ret) {
		return ret;
	}

	if (metadata_addr == nullptr) {
		return -1;
	}

	*metadata = reinterpret_cast<struct cros_gralloc_buffer_metadata *>(metadata_addr);
	return 0;
}

int32_t
cros_gralloc_buffer::get_metadata(const struct cros_gralloc_buffer_metadata **metadata) const
{
	void *metadata_addr;
	uint64_t metadata_region_size;
	int32_t ret = get_reserved_region(&metadata_addr, &metadata_region_size);
	if (ret) {
		return ret;
	}

	if (metadata_addr == nullptr) {
		return -1;
	}

	*metadata = reinterpret_cast<const struct cros_gralloc_buffer_metadata *>(metadata_addr);
	return 0;
}
