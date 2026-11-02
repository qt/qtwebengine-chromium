/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROSGRALLOC4METADATA_H
#define CROSGRALLOC4METADATA_H

#include <optional>
#include <type_traits>

#ifndef HAS_NO_AIDL_METADATA
#include <aidl/android/hardware/graphics/common/BlendMode.h>
#include <aidl/android/hardware/graphics/common/Cta861_3.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/Smpte2086.h>
#endif // HAS_NO_AIDL_METADATA

#include "cros_gralloc_helpers.h"

// Simple replacement for std::optional which is not guaranteed to be memory layout
// stable across ABIs.
template <typename T> struct cros_buffer_optional {

	enum class state : uint32_t { VACANT, OCCUPIED };

	cros_buffer_optional() = default;

	cros_buffer_optional(std::optional<T> v) : occupancy(v ? state::OCCUPIED : state::VACANT)
	{
		if (v) {
			value = *v;
		}
	}

	std::optional<T> to_std_optional() const
	{
		return occupancy == state::VACANT ? std::nullopt : std::make_optional(value);
	}

	state occupancy = state::VACANT;
	T value;
};

/*
 * The metadata for cros_gralloc_buffer-s that should reside in a shared memory region
 * instead of directly in cros_gralloc_handle-s.
 *
 * Any metadata that is mutable must be stored in this shared memory region as
 * cros_gralloc_handle-s can not be tracked and updated across processes.
 */
struct cros_gralloc_buffer_metadata {
	/*
	 * Name is stored in the shared memory metadata to simplify cros_gralloc_handle
	 * creation. This allows us to keep handles small while avoiding variable sized
	 * handles.
	 */
	char name[CROS_GRALLOC_BUFFER_METADATA_MAX_NAME_SIZE];
#ifndef HAS_NO_AIDL_METADATA
	aidl::android::hardware::graphics::common::BlendMode blend_mode;
	aidl::android::hardware::graphics::common::Dataspace dataspace;
	cros_buffer_optional<aidl::android::hardware::graphics::common::Cta861_3> cta861_3;
	cros_buffer_optional<aidl::android::hardware::graphics::common::Smpte2086> smpte2086;
	int32_t smpte2094_50_size;

	static constexpr auto MAX_SMPTE2094_50_SIZE = 1028 * 10;
	// 10 KB ought to be enough
	std::array<uint8_t, MAX_SMPTE2094_50_SIZE> smpte2094_50;
#endif // HAS_NO_AIDL_METADATA
};

static_assert(std::is_standard_layout_v<cros_gralloc_buffer_metadata>);
static_assert(std::is_trivially_copyable_v<cros_gralloc_buffer_metadata>);

#endif
