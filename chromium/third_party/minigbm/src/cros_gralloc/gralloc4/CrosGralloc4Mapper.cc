/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc/gralloc4/CrosGralloc4Mapper.h"

#include <aidl/android/hardware/graphics/common/BlendMode.h>
#include <aidl/android/hardware/graphics/common/Cta861_3.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/PlaneLayout.h>
#include <aidl/android/hardware/graphics/common/Rect.h>
#include <aidl/android/hardware/graphics/common/Smpte2086.h>

#include <cutils/native_handle.h>
#include <gralloctypes/Gralloc4.h>

#include "cros_gralloc/cros_gralloc_arm.h"
#include "cros_gralloc/cros_gralloc_helpers.h"
#include "cros_gralloc/gralloc4/CrosGralloc4Utils.h"

using aidl::android::hardware::graphics::common::BlendMode;
using aidl::android::hardware::graphics::common::Cta861_3;
using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::PlaneLayout;
using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::common::Smpte2086;
using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::graphics::common::V1_2::BufferUsage;
using android::hardware::graphics::common::V1_2::PixelFormat;
using android::hardware::graphics::mapper::V4_0::Error;
using android::hardware::graphics::mapper::V4_0::IMapper;

const static IMapper::MetadataType kArmMetadataTypePlaneFds{
        GRALLOC_ARM_METADATA_TYPE_NAME, static_cast<int64_t>(ArmMetadataType::PLANE_FDS)};
const static IMapper::MetadataType kArmMetadataTypeFormatDataType{
        GRALLOC_ARM_METADATA_TYPE_NAME, static_cast<int64_t>(ArmMetadataType::FORMAT_DATA_TYPE)};

#ifdef HAS_NO_AIDL_METADATA
static_assert(false, "Must not define HAS_NO_AIDL_METADATA when gralloc4 is enabled");
#endif

Return<void> CrosGralloc4Mapper::createDescriptor(const BufferDescriptorInfo& description,
                                                  createDescriptor_cb hidlCb) {
    hidl_vec<uint8_t> descriptor;

    if (description.width == 0) {
        ALOGE("Failed to createDescriptor. Bad width: %d.", description.width);
        hidlCb(Error::BAD_VALUE, descriptor);
        return Void();
    }

    if (description.height == 0) {
        ALOGE("Failed to createDescriptor. Bad height: %d.", description.height);
        hidlCb(Error::BAD_VALUE, descriptor);
        return Void();
    }

    if (description.layerCount == 0) {
        ALOGE("Failed to createDescriptor. Bad layer count: %d.", description.layerCount);
        hidlCb(Error::BAD_VALUE, descriptor);
        return Void();
    }

    int ret = android::gralloc4::encodeBufferDescriptorInfo(description, &descriptor);
    if (ret) {
        ALOGE("Failed to createDescriptor. Failed to encode: %d.", ret);
        hidlCb(Error::BAD_VALUE, descriptor);
        return Void();
    }

    hidlCb(Error::NONE, descriptor);
    return Void();
}

Return<void> CrosGralloc4Mapper::importBuffer(const hidl_handle& handle, importBuffer_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to import buffer. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    const native_handle_t* bufferHandle = handle.getNativeHandle();
    if (!bufferHandle || bufferHandle->numFds == 0) {
        ALOGE("Failed to importBuffer. Bad handle.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    native_handle_t* importedBufferHandle = native_handle_clone(bufferHandle);
    if (!importedBufferHandle) {
        ALOGE("Failed to importBuffer. Handle clone failed: %s.", strerror(errno));
        hidlCb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    int ret = mDriver->retain(importedBufferHandle);
    if (ret) {
        native_handle_close(importedBufferHandle);
        native_handle_delete(importedBufferHandle);
        hidlCb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    hidlCb(Error::NONE, importedBufferHandle);
    return Void();
}

Return<Error> CrosGralloc4Mapper::freeBuffer(void* rawHandle) {
    if (!mDriver) {
        ALOGE("Failed to freeBuffer. Driver is uninitialized.");
        return Error::NO_RESOURCES;
    }

    native_handle_t* bufferHandle = reinterpret_cast<native_handle_t*>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to freeBuffer. Empty handle.");
        return Error::BAD_BUFFER;
    }

    int ret = mDriver->release(bufferHandle);
    if (ret) {
        return Error::BAD_BUFFER;
    }

    native_handle_close(bufferHandle);
    native_handle_delete(bufferHandle);
    return Error::NONE;
}

Return<Error> CrosGralloc4Mapper::validateBufferSize(void* rawHandle,
                                                     const BufferDescriptorInfo& descriptor,
                                                     uint32_t stride) {
    if (!mDriver) {
        ALOGE("Failed to validateBufferSize. Driver is uninitialized.");
        return Error::NO_RESOURCES;
    }

    native_handle_t* bufferHandle = reinterpret_cast<native_handle_t*>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to validateBufferSize. Empty handle.");
        return Error::BAD_BUFFER;
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to validateBufferSize. Invalid handle.");
        return Error::BAD_BUFFER;
    }

    PixelFormat crosHandleFormat = static_cast<PixelFormat>(crosHandle->droid_format);
    if (descriptor.format != crosHandleFormat) {
        ALOGE("Failed to validateBufferSize. Format mismatch.");
        return Error::BAD_BUFFER;
    }

    if (descriptor.width != crosHandle->width) {
        ALOGE("Failed to validateBufferSize. Width mismatch (%d vs %d).", descriptor.width,
              crosHandle->width);
        return Error::BAD_VALUE;
    }

    if (descriptor.height != crosHandle->height) {
        ALOGE("Failed to validateBufferSize. Height mismatch (%d vs %d).", descriptor.height,
              crosHandle->height);
        return Error::BAD_VALUE;
    }

    if (crosHandle->droid_format == HAL_PIXEL_FORMAT_BLOB) {
        if (stride > crosHandle->pixel_stride) {
            ALOGE("Failed to validateBufferSize. Oversized stride (%d vs %d).", stride,
                  crosHandle->pixel_stride);
            return Error::BAD_VALUE;
        }
    } else if (stride != crosHandle->pixel_stride) {
        ALOGE("Failed to validateBufferSize. Stride mismatch (%d vs %d).", stride,
              crosHandle->pixel_stride);
        return Error::BAD_VALUE;
    }

    return Error::NONE;
}

Return<void> CrosGralloc4Mapper::getTransportSize(void* rawHandle, getTransportSize_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to getTransportSize. Driver is uninitialized.");
        hidlCb(Error::BAD_BUFFER, 0, 0);
        return Void();
    }

    native_handle_t* bufferHandle = reinterpret_cast<native_handle_t*>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to getTransportSize. Bad handle.");
        hidlCb(Error::BAD_BUFFER, 0, 0);
        return Void();
    }

    // No local process data is currently stored on the native handle.
    hidlCb(Error::NONE, bufferHandle->numFds, bufferHandle->numInts);
    return Void();
}

Return<void> CrosGralloc4Mapper::lock(void* rawBuffer, uint64_t cpuUsage, const Rect& region,
                                      const hidl_handle& acquireFence, lock_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to lock. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawBuffer);
    if (!bufferHandle) {
        ALOGE("Failed to lock. Empty handle.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    if (cpuUsage == 0) {
        ALOGE("Failed to lock. Bad cpu usage: %" PRIu64 ".", cpuUsage);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    uint32_t mapUsage = 0;
    int ret = convertToMapUsage(cpuUsage, &mapUsage);
    if (ret) {
        ALOGE("Failed to lock. Convert usage failed.");
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (crosHandle == nullptr) {
        ALOGE("Failed to lock. Invalid handle.");
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.left < 0) {
        ALOGE("Failed to lock. Invalid region: negative left value %d.", region.left);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.top < 0) {
        ALOGE("Failed to lock. Invalid region: negative top value %d.", region.top);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.width < 0) {
        ALOGE("Failed to lock. Invalid region: negative width value %d.", region.width);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.height < 0) {
        ALOGE("Failed to lock. Invalid region: negative height value %d.", region.height);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.width > crosHandle->width) {
        ALOGE("Failed to lock. Invalid region: width greater than buffer width (%d vs %d).",
              region.width, crosHandle->width);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    if (region.height > crosHandle->height) {
        ALOGE("Failed to lock. Invalid region: height greater than buffer height (%d vs %d).",
              region.height, crosHandle->height);
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    struct rectangle rect = {static_cast<uint32_t>(region.left), static_cast<uint32_t>(region.top),
                             static_cast<uint32_t>(region.width),
                             static_cast<uint32_t>(region.height)};

    // An access region of all zeros means the entire buffer.
    if (rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0) {
        rect.width = crosHandle->width;
        rect.height = crosHandle->height;
    }

    int acquireFenceFd = -1;
    ret = convertToFenceFd(acquireFence, &acquireFenceFd);
    if (ret) {
        ALOGE("Failed to lock. Bad acquire fence.");
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    uint8_t* addr[DRV_MAX_PLANES];
    ret = mDriver->lock(bufferHandle, acquireFenceFd, /*close_acquire_fence=*/false, &rect,
                        mapUsage, addr);
    if (ret) {
        hidlCb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    hidlCb(Error::NONE, addr[0]);
    return Void();
}

Return<void> CrosGralloc4Mapper::unlock(void* rawHandle, unlock_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to unlock. Driver is uninitialized.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to unlock. Empty handle.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    int releaseFenceFd = -1;
    int ret = mDriver->unlock(bufferHandle, &releaseFenceFd);
    if (ret) {
        ALOGE("Failed to unlock.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    NATIVE_HANDLE_DECLARE_STORAGE(releaseFenceHandleStorage, 1, 0);
    hidlCb(Error::NONE, convertToFenceHandle(releaseFenceFd, releaseFenceHandleStorage));
    return Void();
}

Return<void> CrosGralloc4Mapper::flushLockedBuffer(void* rawHandle, flushLockedBuffer_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to flushLockedBuffer. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to flushLockedBuffer. Empty handle.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    /*
     * From the ANativeWindow::dequeueBuffer documentation:
     *
     * "A value of -1 indicates that the caller may access the buffer immediately without
     * waiting on a fence."
     */
    int releaseFenceFd = -1;
    int ret = mDriver->flush(bufferHandle);
    if (ret) {
        ALOGE("Failed to flushLockedBuffer. Flush failed.");
        hidlCb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    NATIVE_HANDLE_DECLARE_STORAGE(releaseFenceHandleStorage, 1, 0);
    hidlCb(Error::NONE, convertToFenceHandle(releaseFenceFd, releaseFenceHandleStorage));
    return Void();
}

Return<Error> CrosGralloc4Mapper::rereadLockedBuffer(void* rawHandle) {
    if (!mDriver) {
        ALOGE("Failed to rereadLockedBuffer. Driver is uninitialized.");
        return Error::NO_RESOURCES;
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to rereadLockedBuffer. Empty handle.");
        return Error::BAD_BUFFER;
    }

    int ret = mDriver->invalidate(bufferHandle);
    if (ret) {
        ALOGE("Failed to rereadLockedBuffer. Failed to invalidate.");
        return Error::BAD_BUFFER;
    }

    return Error::NONE;
}

Return<void> CrosGralloc4Mapper::isSupported(const BufferDescriptorInfo& descriptor,
                                             isSupported_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to isSupported. Driver is uninitialized.");
        hidlCb(Error::BAD_VALUE, false);
        return Void();
    }

    struct cros_gralloc_buffer_descriptor crosDescriptor;
    if (convertToCrosDescriptor(descriptor, &crosDescriptor)) {
        hidlCb(Error::NONE, false);
        return Void();
    }

    hidlCb(Error::NONE, mDriver->is_supported(&crosDescriptor));
    return Void();
}

Return<void> CrosGralloc4Mapper::get(void* rawHandle, const MetadataType& metadataType,
                                     get_cb hidlCb) {
    hidl_vec<uint8_t> encodedMetadata;

    if (!mDriver) {
        ALOGE("Failed to get. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, encodedMetadata);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to get. Empty handle.");
        hidlCb(Error::BAD_BUFFER, encodedMetadata);
        return Void();
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to get. Invalid handle.");
        hidlCb(Error::BAD_BUFFER, encodedMetadata);
        return Void();
    }

    mDriver->with_buffer(crosHandle, [&, this](cros_gralloc_buffer* crosBuffer) {
        get(crosBuffer, metadataType, hidlCb);
    });
    return Void();
}

Return<void> CrosGralloc4Mapper::get(const cros_gralloc_buffer* crosBuffer,
                                     const MetadataType& metadataType, get_cb hidlCb) {
    hidl_vec<uint8_t> encodedMetadata;

    if (!mDriver) {
        ALOGE("Failed to get. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, encodedMetadata);
        return Void();
    }

    if (!crosBuffer) {
        ALOGE("Failed to get. Invalid buffer.");
        hidlCb(Error::BAD_BUFFER, encodedMetadata);
        return Void();
    }

    android::status_t status = android::NO_ERROR;
    if (metadataType == android::gralloc4::MetadataType_BufferId) {
        status = android::gralloc4::encodeBufferId(crosBuffer->get_id(), &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Name) {
        std::optional<std::string> name;
        int ret = crosBuffer->get_name(&name);
        if (ret) {
            ALOGE("Failed to get. Failed to get name internal.");
            status = android::UNKNOWN_ERROR;
        } else {
            status = android::gralloc4::encodeName(*name, &encodedMetadata);
        }
    } else if (metadataType == android::gralloc4::MetadataType_Width) {
        status = android::gralloc4::encodeWidth(crosBuffer->get_width(), &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Height) {
        status = android::gralloc4::encodeHeight(crosBuffer->get_height(), &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_LayerCount) {
        status = android::gralloc4::encodeLayerCount(1, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatRequested) {
        PixelFormat pixelFormat = static_cast<PixelFormat>(crosBuffer->get_android_format());
        status = android::gralloc4::encodePixelFormatRequested(pixelFormat, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatFourCC) {
        status = android::gralloc4::encodePixelFormatFourCC(
                drv_get_standard_fourcc(crosBuffer->get_format()), &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatModifier) {
        status = android::gralloc4::encodePixelFormatModifier(crosBuffer->get_format_modifier(),
                                                              &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Usage) {
        status = android::gralloc4::encodeUsage(crosBuffer->get_android_usage(), &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_AllocationSize) {
        status = android::gralloc4::encodeAllocationSize(crosBuffer->get_total_size(),
                                                         &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_ProtectedContent) {
        uint64_t hasProtectedContent =
                crosBuffer->get_android_usage() & BufferUsage::PROTECTED ? 1 : 0;
        status = android::gralloc4::encodeProtectedContent(hasProtectedContent, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Compression) {
        ExtendableType compression = android::gralloc4::Compression_None;
        uint64_t modifier = crosBuffer->get_format_modifier();

        if (fourcc_mod_is_vendor(modifier, ARM)) {
            if (((modifier >> 52) & 0xF) == DRM_FORMAT_MOD_ARM_TYPE_AFBC) {
                compression = Compression_AFBC;
            } else if (((modifier >> 52) & 0xF) == DRM_FORMAT_MOD_ARM_TYPE_AFRC) {
                compression = Compression_AFRC;
            }
        }

        status = android::gralloc4::encodeCompression(compression, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Interlaced) {
        status = android::gralloc4::encodeInterlaced(android::gralloc4::Interlaced_None,
                                                     &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_ChromaSiting) {
        status = android::gralloc4::encodeChromaSiting(android::gralloc4::ChromaSiting_None,
                                                       &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PlaneLayouts) {
        std::vector<PlaneLayout> planeLayouts;
        getPlaneLayouts(crosBuffer->get_format(), &planeLayouts);

        for (size_t plane = 0; plane < planeLayouts.size(); plane++) {
            PlaneLayout& planeLayout = planeLayouts[plane];
            planeLayout.offsetInBytes = crosBuffer->get_plane_offset(plane);
            planeLayout.strideInBytes = crosBuffer->get_plane_stride(plane);
            planeLayout.totalSizeInBytes = crosBuffer->get_plane_size(plane);
            planeLayout.widthInSamples =
                    crosBuffer->get_width() / planeLayout.horizontalSubsampling;
            planeLayout.heightInSamples =
                    crosBuffer->get_height() / planeLayout.verticalSubsampling;
        }

        status = android::gralloc4::encodePlaneLayouts(planeLayouts, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Crop) {
        const uint32_t numPlanes = crosBuffer->get_num_planes();
        const uint32_t w = crosBuffer->get_width();
        const uint32_t h = crosBuffer->get_height();
        std::vector<aidl::android::hardware::graphics::common::Rect> crops;
        for (uint32_t plane = 0; plane < numPlanes; plane++) {
            aidl::android::hardware::graphics::common::Rect crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = w;
            crop.bottom = h;
            crops.push_back(crop);
        }

        status = android::gralloc4::encodeCrop(crops, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Dataspace) {
        std::optional<Dataspace> dataspace;
        int ret = crosBuffer->get_dataspace(&dataspace);
        if (ret) {
            ALOGE("Failed to get. Failed to get dataspace internal.");
            status = android::UNKNOWN_ERROR;
        } else {
            status = android::gralloc4::encodeDataspace(*dataspace, &encodedMetadata);
        }
    } else if (metadataType == android::gralloc4::MetadataType_BlendMode) {
        std::optional<BlendMode> blend;
        int ret = crosBuffer->get_blend_mode(&blend);
        if (ret) {
            ALOGE("Failed to get. Failed to get blend mode internal.");
            status = android::UNKNOWN_ERROR;
        } else {
            status = android::gralloc4::encodeBlendMode(*blend, &encodedMetadata);
        }
    } else if (metadataType == android::gralloc4::MetadataType_Smpte2086) {
        std::optional<Smpte2086> smpte;
        int ret = crosBuffer->get_smpte2086(&smpte);
        if (ret) {
            ALOGE("Failed to get. Failed to get smpte2086 internal.");
            status = android::UNKNOWN_ERROR;
        } else {
            status = android::gralloc4::encodeSmpte2086(smpte, &encodedMetadata);
        }
    } else if (metadataType == android::gralloc4::MetadataType_Cta861_3) {
        std::optional<Cta861_3> cta;
        int ret = crosBuffer->get_cta861_3(&cta);
        if (ret) {
            ALOGE("Failed to get. Failed to get cta861_3 internal.");
            status = android::UNKNOWN_ERROR;
        } else {
            status = android::gralloc4::encodeCta861_3(cta, &encodedMetadata);
        }
    } else if (metadataType == android::gralloc4::MetadataType_Smpte2094_40) {
        status = android::gralloc4::encodeSmpte2094_40(std::nullopt, &encodedMetadata);
    } else if (metadataType == kArmMetadataTypePlaneFds) {
        uint32_t num_planes = crosBuffer->get_num_planes();
        int64_t plane_fds[5];
        plane_fds[0] = num_planes;
        for (auto plane = 0; plane < num_planes; plane++) {
            plane_fds[1 + plane] = crosBuffer->get_plane_fd(plane);
        }
        encodedMetadata.resize(sizeof(uint64_t) * (1 + num_planes));
        memcpy(encodedMetadata.data(), plane_fds, encodedMetadata.size());
    } else if (metadataType == kArmMetadataTypeFormatDataType) {
        uint32_t pf = crosBuffer->get_format();
        int64_t fdt = static_cast<int64_t>(DataTypeFromDrmPixelFormat(pf));
        encodedMetadata.resize(sizeof(fdt));
        memcpy(encodedMetadata.data(), &fdt, encodedMetadata.size());
    } else {
        hidlCb(Error::UNSUPPORTED, encodedMetadata);
        return Void();
    }

    if (status != android::NO_ERROR) {
        hidlCb(Error::NO_RESOURCES, encodedMetadata);
        ALOGE("Failed to get. Failed to encode metadata.");
        return Void();
    }

    hidlCb(Error::NONE, encodedMetadata);
    return Void();
}

Return<Error> CrosGralloc4Mapper::set(void* rawHandle, const MetadataType& metadataType,
                                      const hidl_vec<uint8_t>& encodedMetadata) {
    if (!mDriver) {
        ALOGE("Failed to set. Driver is uninitialized.");
        return Error::NO_RESOURCES;
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to set. Empty handle.");
        return Error::BAD_BUFFER;
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to set. Invalid handle.");
        return Error::BAD_BUFFER;
    }

    if (metadataType == android::gralloc4::MetadataType_BufferId) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_Name) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_Width) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_Height) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_LayerCount) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatRequested) {
        return Error::BAD_VALUE;
    } else if (metadataType == android::gralloc4::MetadataType_Usage) {
        return Error::BAD_VALUE;
    }

    if (metadataType != android::gralloc4::MetadataType_BlendMode &&
        metadataType != android::gralloc4::MetadataType_Cta861_3 &&
        metadataType != android::gralloc4::MetadataType_Dataspace &&
        metadataType != android::gralloc4::MetadataType_Smpte2086) {
        return Error::UNSUPPORTED;
    }

    Error error = Error::NONE;
    mDriver->with_buffer(crosHandle, [&, this](cros_gralloc_buffer* crosBuffer) {
        error = set(crosBuffer, metadataType, encodedMetadata);
    });

    return error;
}

Error CrosGralloc4Mapper::set(cros_gralloc_buffer* crosBuffer, const MetadataType& metadataType,
                              const android::hardware::hidl_vec<uint8_t>& encodedMetadata) {
    if (!mDriver) {
        ALOGE("Failed to set. Driver is uninitialized.");
        return Error::NO_RESOURCES;
    }

    if (!crosBuffer) {
        ALOGE("Failed to set. Invalid buffer.");
        return Error::BAD_BUFFER;
    }

    if (metadataType == android::gralloc4::MetadataType_BlendMode) {
        BlendMode blend;
        auto status = android::gralloc4::decodeBlendMode(encodedMetadata, &blend);
        if (status != android::NO_ERROR) {
            ALOGE("Failed to set. Failed to decode blend mode.");
            return Error::UNSUPPORTED;
        }
        int ret = crosBuffer->set_blend_mode(blend);
        if (ret) {
            ALOGE("Failed to set. Failed to set blend mode internal.");
            return Error::NO_RESOURCES;
        }
    } else if (metadataType == android::gralloc4::MetadataType_Cta861_3) {
        std::optional<Cta861_3> cta;
        auto status = android::gralloc4::decodeCta861_3(encodedMetadata, &cta);
        if (status != android::NO_ERROR) {
            ALOGE("Failed to set. Failed to decode cta861_3.");
            return Error::UNSUPPORTED;
        }
        int ret = crosBuffer->set_cta861_3(cta);
        if (ret) {
            ALOGE("Failed to set. Failed to set cta861_3 internal.");
            return Error::NO_RESOURCES;
        }
    } else if (metadataType == android::gralloc4::MetadataType_Dataspace) {
        Dataspace dataspace;
        auto status = android::gralloc4::decodeDataspace(encodedMetadata, &dataspace);
        if (status != android::NO_ERROR) {
            ALOGE("Failed to set. Failed to decode dataspace.");
            return Error::UNSUPPORTED;
        }
        int ret = crosBuffer->set_dataspace(dataspace);
        if (ret) {
            ALOGE("Failed to set. Failed to set dataspace internal.");
            return Error::NO_RESOURCES;
        }
    } else if (metadataType == android::gralloc4::MetadataType_Smpte2086) {
        std::optional<Smpte2086> smpte;
        auto status = android::gralloc4::decodeSmpte2086(encodedMetadata, &smpte);
        if (status != android::NO_ERROR) {
            ALOGE("Failed to set. Failed to decode smpte2086.");
            return Error::UNSUPPORTED;
        }
        int ret = crosBuffer->set_smpte2086(smpte);
        if (ret) {
            ALOGE("Failed to set. Failed to set dataspace internal.");
            return Error::NO_RESOURCES;
        }
    }

    return Error::NONE;
}

int CrosGralloc4Mapper::getResolvedDrmFormat(PixelFormat pixelFormat, uint64_t bufferUsage,
                                             uint32_t* outDrmFormat) {
    uint32_t drmFormat;
    if (convertToDrmFormat(pixelFormat, &drmFormat)) {
        std::string pixelFormatString = getPixelFormatString(pixelFormat);
        ALOGE("Failed to getResolvedDrmFormat. Failed to convert format %s",
              pixelFormatString.c_str());
        return -EINVAL;
    }

    uint64_t usage;
    if (convertToBufferUsage(bufferUsage, &usage)) {
        std::string usageString = getUsageString(bufferUsage);
        ALOGE("Failed to getResolvedDrmFormat. Failed to convert usage %s", usageString.c_str());
        return -EINVAL;
    }

    uint32_t resolvedDrmFormat = mDriver->get_resolved_drm_format(drmFormat, usage);
    if (resolvedDrmFormat == DRM_FORMAT_INVALID) {
        std::string drmFormatString = get_drm_format_string(drmFormat);
        ALOGE("Failed to getResolvedDrmFormat. Failed to resolve drm format %s",
              drmFormatString.c_str());
        return -EINVAL;
    }

    *outDrmFormat = resolvedDrmFormat;

    return 0;
}

Return<void> CrosGralloc4Mapper::getFromBufferDescriptorInfo(
        const BufferDescriptorInfo& descriptor, const MetadataType& metadataType,
        getFromBufferDescriptorInfo_cb hidlCb) {
    hidl_vec<uint8_t> encodedMetadata;

    if (!mDriver) {
        ALOGE("Failed to getFromBufferDescriptorInfo. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, encodedMetadata);
        return Void();
    }

    android::status_t status = android::NO_ERROR;
    if (metadataType == android::gralloc4::MetadataType_Name) {
        status = android::gralloc4::encodeName(descriptor.name, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Width) {
        status = android::gralloc4::encodeWidth(descriptor.width, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Height) {
        status = android::gralloc4::encodeHeight(descriptor.height, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_LayerCount) {
        status = android::gralloc4::encodeLayerCount(1, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatRequested) {
        status = android::gralloc4::encodePixelFormatRequested(descriptor.format, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_PixelFormatFourCC) {
        uint32_t drmFormat;
        if (getResolvedDrmFormat(descriptor.format, descriptor.usage, &drmFormat)) {
            hidlCb(Error::BAD_VALUE, encodedMetadata);
            return Void();
        }
        status = android::gralloc4::encodePixelFormatFourCC(drv_get_standard_fourcc(drmFormat),
                                                            &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Usage) {
        status = android::gralloc4::encodeUsage(descriptor.usage, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_ProtectedContent) {
        uint64_t hasProtectedContent = descriptor.usage & BufferUsage::PROTECTED ? 1 : 0;
        status = android::gralloc4::encodeProtectedContent(hasProtectedContent, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Compression) {
        status = android::gralloc4::encodeCompression(android::gralloc4::Compression_None,
                                                      &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Interlaced) {
        status = android::gralloc4::encodeInterlaced(android::gralloc4::Interlaced_None,
                                                     &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_ChromaSiting) {
        status = android::gralloc4::encodeChromaSiting(android::gralloc4::ChromaSiting_None,
                                                       &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Crop) {
        uint32_t drmFormat;
        if (getResolvedDrmFormat(descriptor.format, descriptor.usage, &drmFormat)) {
            hidlCb(Error::BAD_VALUE, encodedMetadata);
            return Void();
        }

        size_t numPlanes = drv_num_planes_from_format(drmFormat);

        std::vector<aidl::android::hardware::graphics::common::Rect> crops;
        for (size_t plane = 0; plane < numPlanes; plane++) {
            aidl::android::hardware::graphics::common::Rect crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = descriptor.width;
            crop.bottom = descriptor.height;
            crops.push_back(crop);
        }
        status = android::gralloc4::encodeCrop(crops, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Dataspace) {
        status = android::gralloc4::encodeDataspace(Dataspace::UNKNOWN, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_BlendMode) {
        status = android::gralloc4::encodeBlendMode(BlendMode::INVALID, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Smpte2086) {
        status = android::gralloc4::encodeSmpte2086(std::nullopt, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Cta861_3) {
        status = android::gralloc4::encodeCta861_3(std::nullopt, &encodedMetadata);
    } else if (metadataType == android::gralloc4::MetadataType_Smpte2094_40) {
        status = android::gralloc4::encodeSmpte2094_40(std::nullopt, &encodedMetadata);
    } else {
        hidlCb(Error::UNSUPPORTED, encodedMetadata);
        return Void();
    }

    if (status != android::NO_ERROR) {
        hidlCb(Error::NO_RESOURCES, encodedMetadata);
        return Void();
    }

    hidlCb(Error::NONE, encodedMetadata);
    return Void();
}

Return<void> CrosGralloc4Mapper::listSupportedMetadataTypes(listSupportedMetadataTypes_cb hidlCb) {
    hidl_vec<MetadataTypeDescription> supported;

    if (!mDriver) {
        ALOGE("Failed to listSupportedMetadataTypes. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, supported);
        return Void();
    }

    supported = hidl_vec<IMapper::MetadataTypeDescription>({
            {
                    android::gralloc4::MetadataType_BufferId,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Name,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Width,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Height,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_LayerCount,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_PixelFormatRequested,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_PixelFormatFourCC,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_PixelFormatModifier,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Usage,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_AllocationSize,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_ProtectedContent,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Compression,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Interlaced,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_ChromaSiting,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_PlaneLayouts,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Crop,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    android::gralloc4::MetadataType_Dataspace,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/true,
            },
            {
                    android::gralloc4::MetadataType_BlendMode,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/true,
            },
            {
                    android::gralloc4::MetadataType_Smpte2086,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/true,
            },
            {
                    android::gralloc4::MetadataType_Cta861_3,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/true,
            },
            {
                    android::gralloc4::MetadataType_Smpte2094_40,
                    "",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    kArmMetadataTypePlaneFds,
                    "Vector of file descriptors of each plane",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
            {
                    kArmMetadataTypeFormatDataType,
                    "Format data type",
                    /*isGettable=*/true,
                    /*isSettable=*/false,
            },
    });

    hidlCb(Error::NONE, supported);
    return Void();
}

Return<void> CrosGralloc4Mapper::dumpBuffer(void* rawHandle, dumpBuffer_cb hidlCb) {
    BufferDump bufferDump;

    if (!mDriver) {
        ALOGE("Failed to dumpBuffer. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, bufferDump);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to dumpBuffer. Empty handle.");
        hidlCb(Error::BAD_BUFFER, bufferDump);
        return Void();
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to dumpBuffer. Invalid handle.");
        hidlCb(Error::BAD_BUFFER, bufferDump);
        return Void();
    }

    mDriver->with_buffer(crosHandle, [&, this](cros_gralloc_buffer* crosBuffer) {
        dumpBuffer(crosBuffer, hidlCb);
    });
    return Void();
}

Return<void> CrosGralloc4Mapper::dumpBuffer(const cros_gralloc_buffer* crosBuffer,
                                            dumpBuffer_cb hidlCb) {
    BufferDump bufferDump;

    if (!mDriver) {
        ALOGE("Failed to dumpBuffer. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, bufferDump);
        return Void();
    }

    std::vector<MetadataDump> metadataDumps;

    MetadataType metadataType = android::gralloc4::MetadataType_BufferId;
    auto metadata_get_callback = [&](Error, hidl_vec<uint8_t> metadata) {
        MetadataDump metadataDump;
        metadataDump.metadataType = metadataType;
        metadataDump.metadata = metadata;
        metadataDumps.push_back(metadataDump);
    };

    metadataType = android::gralloc4::MetadataType_BufferId;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Name;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Width;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Height;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_LayerCount;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_PixelFormatRequested;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_PixelFormatFourCC;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_PixelFormatModifier;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Usage;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_AllocationSize;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_ProtectedContent;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Compression;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Interlaced;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_ChromaSiting;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_PlaneLayouts;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_Dataspace;
    get(crosBuffer, metadataType, metadata_get_callback);

    metadataType = android::gralloc4::MetadataType_BlendMode;
    get(crosBuffer, metadataType, metadata_get_callback);

    bufferDump.metadataDump = metadataDumps;
    hidlCb(Error::NONE, bufferDump);
    return Void();
}

Return<void> CrosGralloc4Mapper::dumpBuffers(dumpBuffers_cb hidlCb) {
    std::vector<BufferDump> bufferDumps;

    if (!mDriver) {
        ALOGE("Failed to dumpBuffers. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, bufferDumps);
        return Void();
    }

    Error error = Error::NONE;

    const auto dumpBufferCallback = [&](Error err, BufferDump bufferDump) {
        error = err;
        if (error == Error::NONE) {
            bufferDumps.push_back(bufferDump);
        }
    };

    mDriver->with_each_buffer(
            [&](cros_gralloc_buffer* crosBuffer) { dumpBuffer(crosBuffer, dumpBufferCallback); });

    hidlCb(error, bufferDumps);
    return Void();
}

Return<void> CrosGralloc4Mapper::getReservedRegion(void* rawHandle, getReservedRegion_cb hidlCb) {
    if (!mDriver) {
        ALOGE("Failed to getReservedRegion. Driver is uninitialized.");
        hidlCb(Error::NO_RESOURCES, nullptr, 0);
        return Void();
    }

    buffer_handle_t bufferHandle = reinterpret_cast<buffer_handle_t>(rawHandle);
    if (!bufferHandle) {
        ALOGE("Failed to getReservedRegion. Empty handle.");
        hidlCb(Error::BAD_BUFFER, nullptr, 0);
        return Void();
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to getReservedRegion. Invalid handle.");
        hidlCb(Error::BAD_BUFFER, nullptr, 0);
        return Void();
    }

    void* reservedRegionAddr = nullptr;
    uint64_t reservedRegionSize = 0;

    Error error = Error::NONE;
    mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
        int ret = crosBuffer->get_client_reserved_region(&reservedRegionAddr, &reservedRegionSize);
        if (ret) {
            reservedRegionAddr = nullptr;
            reservedRegionSize = 0;
            error = Error::NO_RESOURCES;
        }
    });

    if (error != Error::NONE) {
        ALOGE("Failed to getReservedRegion.");
        hidlCb(Error::BAD_BUFFER, nullptr, 0);
        return Void();
    }

    hidlCb(Error::NONE, reservedRegionAddr, reservedRegionSize);
    return Void();
}

android::hardware::graphics::mapper::V4_0::IMapper* HIDL_FETCH_IMapper(const char* /*name*/) {
    return static_cast<android::hardware::graphics::mapper::V4_0::IMapper*>(new CrosGralloc4Mapper);
}
