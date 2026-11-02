/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_BUFFER_H
#define CROS_GRALLOC_BUFFER_H

#include <memory>
#include <optional>

#ifndef HAS_NO_AIDL_METADATA
#include <aidl/android/hardware/graphics/common/BlendMode.h>
#include <aidl/android/hardware/graphics/common/Cta861_3.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/Smpte2086.h>
#endif // HAS_NO_AIDL_METADATA

#include "cros_gralloc_helpers.h"

class cros_gralloc_buffer
{
      public:
	static std::unique_ptr<cros_gralloc_buffer>
	create(struct bo *acquire_bo, const struct cros_gralloc_handle *borrowed_handle);

	~cros_gralloc_buffer();

	int32_t initialize_metadata(const struct cros_gralloc_buffer_descriptor *descriptor);

	uint32_t get_id() const;
	uint32_t get_width() const;
	uint32_t get_pixel_stride() const;
	uint32_t get_height() const;
	uint32_t get_format() const;
	uint64_t get_format_modifier() const;
	uint64_t get_total_size() const;
	uint32_t get_num_planes() const;
	uint32_t get_plane_offset(uint32_t plane) const;
	uint32_t get_plane_stride(uint32_t plane) const;
	uint32_t get_plane_size(uint32_t plane) const;
	int64_t get_plane_fd(uint32_t plane) const;
	int32_t get_android_format() const;
	int64_t get_android_usage() const;

	int32_t get_name(std::optional<std::string> *name) const;

#ifndef HAS_NO_AIDL_METADATA
	int32_t get_blend_mode(
	    std::optional<aidl::android::hardware::graphics::common::BlendMode> *blend_mode) const;
	int32_t set_blend_mode(aidl::android::hardware::graphics::common::BlendMode blend_mode);

	int32_t get_dataspace(
	    std::optional<aidl::android::hardware::graphics::common::Dataspace> *dataspace) const;
	int32_t set_dataspace(aidl::android::hardware::graphics::common::Dataspace dataspace);

	int32_t
	get_cta861_3(std::optional<aidl::android::hardware::graphics::common::Cta861_3> *cta) const;
	int32_t
	set_cta861_3(std::optional<aidl::android::hardware::graphics::common::Cta861_3> cta);

	int32_t get_smpte2086(
	    std::optional<aidl::android::hardware::graphics::common::Smpte2086> *smpte) const;
	int32_t
	set_smpte2086(std::optional<aidl::android::hardware::graphics::common::Smpte2086> smpte);

	int32_t get_smpte2094_50(std::optional<std::vector<uint8_t>> *smpte) const;
	int32_t set_smpte2094_50(const std::optional<std::vector<uint8_t>> &smpte);
#endif // HAS_NO_AIDL_METADATA

	/* The new reference count is returned by both these functions. */
	int32_t increase_refcount();
	int32_t decrease_refcount();

	int32_t lock(const struct rectangle *rect, uint32_t map_flags,
		     uint8_t *addr[DRV_MAX_PLANES]);
	int32_t unlock();
	int32_t resource_info(uint32_t strides[DRV_MAX_PLANES], uint32_t offsets[DRV_MAX_PLANES],
			      uint64_t *format_modifier);

	int32_t invalidate();
	int32_t flush();

	int32_t get_client_reserved_region(void **client_reserved_region_addr,
					   uint64_t *client_reserved_region_size) const;

      private:
	cros_gralloc_buffer(struct bo *acquire_bo, struct cros_gralloc_handle *acquire_handle);

	cros_gralloc_buffer(cros_gralloc_buffer const &);
	cros_gralloc_buffer operator=(cros_gralloc_buffer const &);

	int32_t get_reserved_region(void **reserved_region_addr,
				    uint64_t *reserved_region_size) const;

	int32_t get_metadata(struct cros_gralloc_buffer_metadata **metadata);
	int32_t get_metadata(const struct cros_gralloc_buffer_metadata **metadata) const;

	struct bo *bo_;

	/* Note: this will be nullptr for imported/retained buffers. */
	struct cros_gralloc_handle *hnd_;

	int32_t refcount_ = 1;
	int32_t lockcount_ = 0;

	struct mapping *lock_data_[DRV_MAX_PLANES];

	/* Optional additional shared memory region attached to some gralloc buffers. */
	mutable void *reserved_region_addr_ = nullptr;
};

#endif
