/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <android-base/unique_fd.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>
#include <cutils/native_handle.h>
#include <gralloctypes/Gralloc4.h>

#include <memory>

#include "cros_gralloc/cros_gralloc_arm.h"
#include "cros_gralloc/cros_gralloc_driver.h"
#include "cros_gralloc/cros_gralloc_handle.h"
#include "cros_gralloc/gralloc4/CrosGralloc4Utils.h"

using namespace ::aidl::android::hardware::graphics::common;
using namespace ::android::hardware::graphics::mapper;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using ::android::base::unique_fd;

#define REQUIRE_DRIVER()                                           \
    if (!mDriver) {                                                \
        ALOGE("Failed to %s. Driver is uninitialized.", __func__); \
        return AIMAPPER_ERROR_NO_RESOURCES;                        \
    }

#define VALIDATE_BUFFER_HANDLE(bufferHandle)                    \
    if (!(bufferHandle)) {                                      \
        ALOGE("Failed to %s. Null buffer_handle_t.", __func__); \
        return AIMAPPER_ERROR_BAD_BUFFER;                       \
    }

#define VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle) \
    REQUIRE_DRIVER()                                    \
    VALIDATE_BUFFER_HANDLE(bufferHandle)

#ifdef HAS_NO_AIDL_METADATA
static_assert(false, "Must not define HAS_NO_AIDL_METADATA when gralloc5 is enabled");
#endif

static_assert(CROS_GRALLOC_BUFFER_METADATA_MAX_NAME_SIZE >=
                      decltype(std::declval<BufferDescriptorInfo>().name){}.size(),
              "Metadata name storage too small to fit a BufferDescriptorInfo::name");

constexpr const char* STANDARD_METADATA_NAME =
        "android.hardware.graphics.common.StandardMetadataType";

constexpr const AIMapper_MetadataType kArmMetadataTypePlaneFds{
        GRALLOC_ARM_METADATA_TYPE_NAME, static_cast<int64_t>(ArmMetadataType::PLANE_FDS)};
constexpr const AIMapper_MetadataType kArmMetadataTypeFormatDataType{
        GRALLOC_ARM_METADATA_TYPE_NAME, static_cast<int64_t>(ArmMetadataType::FORMAT_DATA_TYPE)};

static bool isStandardMetadata(AIMapper_MetadataType metadataType) {
    return strcmp(STANDARD_METADATA_NAME, metadataType.name) == 0;
}

static bool isArmMetadata(AIMapper_MetadataType metadataType) {
    return strcmp(GRALLOC_ARM_METADATA_TYPE_NAME, metadataType.name) == 0;
}

class CrosGrallocMapperV5 final : public vendor::mapper::IMapperV5Impl {
  private:
    std::shared_ptr<cros_gralloc_driver> mDriver = cros_gralloc_driver::get_instance();

  public:
    explicit CrosGrallocMapperV5() = default;
    ~CrosGrallocMapperV5() override = default;

    AIMapper_Error importBuffer(const native_handle_t* _Nonnull handle,
                                buffer_handle_t _Nullable* _Nonnull outBufferHandle) override;

    AIMapper_Error freeBuffer(buffer_handle_t _Nonnull buffer) override;

    AIMapper_Error getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t* _Nonnull outNumFds,
                                    uint32_t* _Nonnull outNumInts) override;

    AIMapper_Error lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
                        int acquireFence, void* _Nullable* _Nonnull outData) override;

    AIMapper_Error unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) override;

    AIMapper_Error flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;

    AIMapper_Error rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;

    int32_t getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
                        void* _Nonnull outData, size_t outDataSize) override;

    int32_t getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
                                void* _Nonnull outData, size_t outDataSize) override;

    AIMapper_Error setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
                               const void* _Nonnull metadata, size_t metadataSize) override;

    AIMapper_Error setStandardMetadata(buffer_handle_t _Nonnull buffer,
                                       int64_t standardMetadataType, const void* _Nonnull metadata,
                                       size_t metadataSize) override;

    AIMapper_Error listSupportedMetadataTypes(
            const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
            size_t* _Nonnull outNumberOfDescriptions) override;

    AIMapper_Error dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
                              AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
                              void* _Null_unspecified context) override;

    AIMapper_Error dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
                                  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
                                  void* _Null_unspecified context) override;

    AIMapper_Error getReservedRegion(buffer_handle_t _Nonnull buffer,
                                     void* _Nullable* _Nonnull outReservedRegion,
                                     uint64_t* _Nonnull outReservedSize) override;

  private:
    template <typename F, StandardMetadataType TYPE>
    int32_t getStandardMetadata(const cros_gralloc_buffer* crosBuffer, F&& provide,
                                StandardMetadata<TYPE>);

    template <StandardMetadataType TYPE>
    AIMapper_Error setStandardMetadata(cros_gralloc_buffer* crosBuffer,
                                       typename StandardMetadata<TYPE>::value_type&& value);

    void dumpBuffer(
            const cros_gralloc_buffer* crosBuffer,
            std::function<void(AIMapper_MetadataType, const std::vector<uint8_t>&)> callback);

    int32_t getArmMetadata(buffer_handle_t _Nonnull buffer, int64_t armMetadataType,
                           void* _Nonnull outData, size_t outDataSize);
};

AIMapper_Error CrosGrallocMapperV5::importBuffer(
        const native_handle_t* _Nonnull bufferHandle,
        buffer_handle_t _Nullable* _Nonnull outBufferHandle) {
    REQUIRE_DRIVER()

    if (!bufferHandle || bufferHandle->numFds == 0) {
        ALOGE("Failed to importBuffer. Bad handle.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    native_handle_t* importedBufferHandle = native_handle_clone(bufferHandle);
    if (!importedBufferHandle) {
        ALOGE("Failed to importBuffer. Handle clone failed: %s.", strerror(errno));
        return AIMAPPER_ERROR_NO_RESOURCES;
    }

    int ret = mDriver->retain(importedBufferHandle);
    if (ret) {
        native_handle_close(importedBufferHandle);
        native_handle_delete(importedBufferHandle);
        return AIMAPPER_ERROR_NO_RESOURCES;
    }

    *outBufferHandle = importedBufferHandle;
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::freeBuffer(buffer_handle_t _Nonnull buffer) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(buffer)

    int ret = mDriver->release(buffer);
    if (ret) {
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    native_handle_close(buffer);
    native_handle_delete(const_cast<native_handle_t*>(buffer));
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::getTransportSize(buffer_handle_t _Nonnull bufferHandle,
                                                     uint32_t* _Nonnull outNumFds,
                                                     uint32_t* _Nonnull outNumInts) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle)

    // No local process data is currently stored on the native handle.
    *outNumFds = bufferHandle->numFds;
    *outNumInts = bufferHandle->numInts;
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::lock(buffer_handle_t _Nonnull bufferHandle, uint64_t cpuUsage,
                                         ARect region, int acquireFenceRawFd,
                                         void* _Nullable* _Nonnull outData) {
    // We take ownership of the FD in all cases, even for errors
    unique_fd acquireFence(acquireFenceRawFd);
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle)
    if (cpuUsage == 0) {
        ALOGE("Failed to lock. Bad cpu usage: %" PRIu64 ".", cpuUsage);
        return AIMAPPER_ERROR_BAD_VALUE;
    }

    uint32_t mapUsage = cros_gralloc_convert_map_usage(cpuUsage);

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (crosHandle == nullptr) {
        ALOGE("Failed to lock. Invalid handle.");
        return AIMAPPER_ERROR_BAD_VALUE;
    }

    struct rectangle rect;

    // An access region of all zeros means the entire buffer.
    if (region.left == 0 && region.top == 0 && region.right == 0 && region.bottom == 0) {
        rect = {0, 0, crosHandle->width, crosHandle->height};
    } else {
        if (region.left < 0 || region.top < 0 || region.right <= region.left ||
            region.bottom <= region.top) {
            ALOGE("Failed to lock. Invalid accessRegion: [%d, %d, %d, %d]", region.left, region.top,
                  region.right, region.bottom);
            return AIMAPPER_ERROR_BAD_VALUE;
        }

        if (region.right > crosHandle->width) {
            ALOGE("Failed to lock. Invalid region: width greater than buffer width (%d vs %d).",
                  region.right, crosHandle->width);
            return AIMAPPER_ERROR_BAD_VALUE;
        }

        if (region.bottom > crosHandle->height) {
            ALOGE("Failed to lock. Invalid region: height greater than buffer height (%d vs "
                  "%d).",
                  region.bottom, crosHandle->height);
            return AIMAPPER_ERROR_BAD_VALUE;
        }

        rect = {static_cast<uint32_t>(region.left), static_cast<uint32_t>(region.top),
                static_cast<uint32_t>(region.right - region.left),
                static_cast<uint32_t>(region.bottom - region.top)};
    }

    uint8_t* addr[DRV_MAX_PLANES];
    int32_t status = mDriver->lock(bufferHandle, acquireFence.get(),
                                   /*close_acquire_fence=*/false, &rect, mapUsage, addr);
    if (status) {
        return AIMAPPER_ERROR_BAD_VALUE;
    }

    *outData = addr[0];
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::unlock(buffer_handle_t _Nonnull buffer,
                                           int* _Nonnull releaseFence) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(buffer)
    int ret = mDriver->unlock(buffer, releaseFence);
    if (ret) {
        ALOGE("Failed to unlock.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::flushLockedBuffer(buffer_handle_t _Nonnull buffer) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(buffer)
    int ret = mDriver->flush(buffer);
    if (ret) {
        ALOGE("Failed to flushLockedBuffer. Flush failed.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::rereadLockedBuffer(buffer_handle_t _Nonnull buffer) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(buffer)
    int ret = mDriver->invalidate(buffer);
    if (ret) {
        ALOGE("Failed to rereadLockedBuffer. Failed to invalidate.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    return AIMAPPER_ERROR_NONE;
}

int32_t CrosGrallocMapperV5::getMetadata(buffer_handle_t _Nonnull buffer,
                                         AIMapper_MetadataType metadataType, void* _Nonnull outData,
                                         size_t outDataSize) {
    if (isStandardMetadata(metadataType)) {
        return getStandardMetadata(buffer, metadataType.value, outData, outDataSize);
    }

    if (isArmMetadata(metadataType)) {
        return getArmMetadata(buffer, metadataType.value, outData, outDataSize);
    }
    return -AIMAPPER_ERROR_UNSUPPORTED;
}

int32_t CrosGrallocMapperV5::getArmMetadata(buffer_handle_t _Nonnull buffer,
                                            int64_t armMetadataType, void* _Nonnull outData,
                                            size_t outDataSize) {
    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(buffer);
    if (!crosHandle) {
        ALOGE("Failed to get. Invalid handle.");
        return -AIMAPPER_ERROR_BAD_BUFFER;
    }
    int32_t retValue = -AIMAPPER_ERROR_UNSUPPORTED;
    switch (armMetadataType) {
        case ArmMetadataType::PLANE_FDS: {
            mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
                uint32_t num_planes = crosBuffer->get_num_planes();

                retValue = sizeof(int64_t) * (1 + num_planes);
                if (outDataSize >= retValue) {
                    int64_t plane_fds[DRV_MAX_PLANES + 1];

                    plane_fds[0] = num_planes;
                    for (auto plane = 0; plane < num_planes; plane++) {
                        plane_fds[1 + plane] = crosBuffer->get_plane_fd(plane);
                    }

                    memcpy(outData, plane_fds, sizeof(uint64_t) * (1 + num_planes));

                    retValue = sizeof(uint64_t) * (1 + num_planes);
                }
            });
            break;
        }
        case ArmMetadataType::FORMAT_DATA_TYPE: {
            mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
                uint32_t pf = crosBuffer->get_format();
                int64_t fdt = static_cast<int64_t>(DataTypeFromDrmPixelFormat(pf));

                retValue = sizeof(fdt);
                if (outDataSize >= retValue) {
                    memcpy(outData, &fdt, sizeof(fdt));
                    retValue = sizeof(fdt);
                }
            });
            break;
        }
        default:
            return -AIMAPPER_ERROR_UNSUPPORTED;
    }
    return retValue;
}

int32_t CrosGrallocMapperV5::getStandardMetadata(buffer_handle_t _Nonnull bufferHandle,
                                                 int64_t standardType, void* _Nonnull outData,
                                                 size_t outDataSize) {
    // Can't use VALIDATE_DRIVER_AND_BUFFER_HANDLE because we need to negate the error
    // for this call
    if (!mDriver) {
        ALOGE("Failed to %s. Driver is uninitialized.", __func__);
        return -AIMAPPER_ERROR_NO_RESOURCES;
    }
    if (!(bufferHandle)) {
        ALOGE("Failed to %s. Null buffer_handle_t.", __func__);
        return -AIMAPPER_ERROR_BAD_BUFFER;
    }

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to get. Invalid handle.");
        return -AIMAPPER_ERROR_BAD_BUFFER;
    }

    int32_t retValue = -AIMAPPER_ERROR_UNSUPPORTED;
    mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
        auto provider = [&]<StandardMetadataType T>(auto&& provide) -> int32_t {
            return getStandardMetadata(crosBuffer, provide, StandardMetadata<T>{});
        };
        retValue = provideStandardMetadata(static_cast<StandardMetadataType>(standardType), outData,
                                           outDataSize, provider);
    });
    return retValue;
}

template <typename F, StandardMetadataType metadataType>
int32_t CrosGrallocMapperV5::getStandardMetadata(const cros_gralloc_buffer* crosBuffer, F&& provide,
                                                 StandardMetadata<metadataType>) {
    if constexpr (metadataType == StandardMetadataType::BUFFER_ID) {
        return provide(crosBuffer->get_id());
    }
    if constexpr (metadataType == StandardMetadataType::NAME) {
        std::optional<std::string> name;
        if (crosBuffer->get_name(&name)) {
            return -AIMAPPER_ERROR_NO_RESOURCES;
        } else {
            return provide(*name);
        }
    }
    if constexpr (metadataType == StandardMetadataType::WIDTH) {
        return provide(crosBuffer->get_width());
    }
    if constexpr (metadataType == StandardMetadataType::STRIDE) {
        return provide(crosBuffer->get_pixel_stride());
    }
    if constexpr (metadataType == StandardMetadataType::HEIGHT) {
        return provide(crosBuffer->get_height());
    }
    if constexpr (metadataType == StandardMetadataType::LAYER_COUNT) {
        return provide(1);
    }
    if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_REQUESTED) {
        return provide(static_cast<PixelFormat>(crosBuffer->get_android_format()));
    }
    if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_FOURCC) {
        return provide(drv_get_standard_fourcc(crosBuffer->get_format()));
    }
    if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_MODIFIER) {
        return provide(crosBuffer->get_format_modifier());
    }
    if constexpr (metadataType == StandardMetadataType::USAGE) {
        return provide(static_cast<BufferUsage>(crosBuffer->get_android_usage()));
    }
    if constexpr (metadataType == StandardMetadataType::ALLOCATION_SIZE) {
        return provide(crosBuffer->get_total_size());
    }
    if constexpr (metadataType == StandardMetadataType::PROTECTED_CONTENT) {
        uint64_t hasProtectedContent =
                crosBuffer->get_android_usage() & static_cast<int64_t>(BufferUsage::PROTECTED) ? 1
                                                                                               : 0;
        return provide(hasProtectedContent);
    }
    if constexpr (metadataType == StandardMetadataType::COMPRESSION) {
        ExtendableType compression = android::gralloc4::Compression_None;
        uint64_t modifier = crosBuffer->get_format_modifier();

        if (fourcc_mod_is_vendor(modifier, ARM)) {
            if (((modifier >> 52) & 0xF) == DRM_FORMAT_MOD_ARM_TYPE_AFBC) {
                compression = Compression_AFBC;
            } else if (((modifier >> 52) & 0xF) == DRM_FORMAT_MOD_ARM_TYPE_AFRC) {
                compression = Compression_AFRC;
            }
        }

        return provide(compression);
    }
    if constexpr (metadataType == StandardMetadataType::INTERLACED) {
        return provide(android::gralloc4::Interlaced_None);
    }
    if constexpr (metadataType == StandardMetadataType::CHROMA_SITING) {
        return provide(android::gralloc4::ChromaSiting_None);
    }
    if constexpr (metadataType == StandardMetadataType::PLANE_LAYOUTS) {
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

        return provide(planeLayouts);
    }
    if constexpr (metadataType == StandardMetadataType::CROP) {
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

        return provide(crops);
    }
    if constexpr (metadataType == StandardMetadataType::DATASPACE) {
        std::optional<Dataspace> dataspace;
        if (crosBuffer->get_dataspace(&dataspace)) {
            return -AIMAPPER_ERROR_NO_RESOURCES;
        } else {
            return provide(*dataspace);
        }
    }
    if constexpr (metadataType == StandardMetadataType::BLEND_MODE) {
        std::optional<BlendMode> blend;
        if (crosBuffer->get_blend_mode(&blend)) {
            return -AIMAPPER_ERROR_NO_RESOURCES;
        } else {
            return provide(*blend);
        }
    }
    if constexpr (metadataType == StandardMetadataType::SMPTE2086) {
        std::optional<Smpte2086> smpte;
        if (crosBuffer->get_smpte2086(&smpte)) {
            return -AIMAPPER_ERROR_NO_RESOURCES;
        } else {
            return smpte ? provide(*smpte) : 0;
        }
    }
    if constexpr (metadataType == StandardMetadataType::CTA861_3) {
        std::optional<Cta861_3> cta;
        if (crosBuffer->get_cta861_3(&cta)) {
            return -AIMAPPER_ERROR_NO_RESOURCES;
        } else {
            return cta ? provide(*cta) : 0;
        }
    }
    return -AIMAPPER_ERROR_UNSUPPORTED;
}

AIMapper_Error CrosGrallocMapperV5::setMetadata(buffer_handle_t _Nonnull buffer,
                                                AIMapper_MetadataType metadataType,
                                                const void* _Nonnull metadata,
                                                size_t metadataSize) {
    // We don't have any vendor-specific metadata, so divert to setStandardMetadata after validating
    // that this is a standard metadata request
    if (isStandardMetadata(metadataType)) {
        return setStandardMetadata(buffer, metadataType.value, metadata, metadataSize);
    }
    return AIMAPPER_ERROR_UNSUPPORTED;
}

AIMapper_Error CrosGrallocMapperV5::setStandardMetadata(buffer_handle_t _Nonnull bufferHandle,
                                                        int64_t standardTypeRaw,
                                                        const void* _Nonnull metadata,
                                                        size_t metadataSize) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle)

    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to get. Invalid handle.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    auto standardType = static_cast<StandardMetadataType>(standardTypeRaw);

    switch (standardType) {
        // Read-only values
        case StandardMetadataType::BUFFER_ID:
        case StandardMetadataType::NAME:
        case StandardMetadataType::WIDTH:
        case StandardMetadataType::HEIGHT:
        case StandardMetadataType::LAYER_COUNT:
        case StandardMetadataType::PIXEL_FORMAT_REQUESTED:
        case StandardMetadataType::USAGE:
            return AIMAPPER_ERROR_BAD_VALUE;

        // Supported to set
        case StandardMetadataType::BLEND_MODE:
        case StandardMetadataType::CTA861_3:
        case StandardMetadataType::DATASPACE:
        case StandardMetadataType::SMPTE2086:
            break;

        // Everything else unsupported
        default:
            return AIMAPPER_ERROR_UNSUPPORTED;
    }

    AIMapper_Error status = AIMAPPER_ERROR_UNSUPPORTED;
    mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
        auto applier = [&]<StandardMetadataType T>(auto&& value) -> AIMapper_Error {
            return setStandardMetadata<T>(crosBuffer, std::forward<decltype(value)>(value));
        };

        status = applyStandardMetadata(standardType, metadata, metadataSize, applier);
    });
    return status;
}

template <StandardMetadataType TYPE>
AIMapper_Error CrosGrallocMapperV5::setStandardMetadata(
        cros_gralloc_buffer* crosBuffer, typename StandardMetadata<TYPE>::value_type&& value) {
    int ret = 0;
    if constexpr (TYPE == StandardMetadataType::BLEND_MODE) {
        ret = crosBuffer->set_blend_mode(value);
    }
    if constexpr (TYPE == StandardMetadataType::CTA861_3) {
        ret = crosBuffer->set_cta861_3(value);
    }
    if constexpr (TYPE == StandardMetadataType::DATASPACE) {
        ret = crosBuffer->set_dataspace(value);
    }
    if constexpr (TYPE == StandardMetadataType::SMPTE2086) {
        ret = crosBuffer->set_smpte2086(value);
    }

    if (ret) {
        return AIMAPPER_ERROR_NO_RESOURCES;
    }

    // Unsupported metadatas were already filtered before we reached this point
    return AIMAPPER_ERROR_NONE;
}

constexpr AIMapper_MetadataTypeDescription describeStandard(StandardMetadataType type,
                                                            bool isGettable, bool isSettable) {
    return {{STANDARD_METADATA_NAME, static_cast<int64_t>(type)},
            nullptr,
            isGettable,
            isSettable,
            {0}};
}

AIMapper_Error CrosGrallocMapperV5::listSupportedMetadataTypes(
        const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
        size_t* _Nonnull outNumberOfDescriptions) {
    static constexpr std::array<AIMapper_MetadataTypeDescription, 24> sSupportedMetadaTypes{
            describeStandard(StandardMetadataType::BUFFER_ID, true, false),
            describeStandard(StandardMetadataType::NAME, true, false),
            describeStandard(StandardMetadataType::WIDTH, true, false),
            describeStandard(StandardMetadataType::HEIGHT, true, false),
            describeStandard(StandardMetadataType::LAYER_COUNT, true, false),
            describeStandard(StandardMetadataType::PIXEL_FORMAT_REQUESTED, true, false),
            describeStandard(StandardMetadataType::PIXEL_FORMAT_FOURCC, true, false),
            describeStandard(StandardMetadataType::PIXEL_FORMAT_MODIFIER, true, false),
            describeStandard(StandardMetadataType::USAGE, true, false),
            describeStandard(StandardMetadataType::ALLOCATION_SIZE, true, false),
            describeStandard(StandardMetadataType::PROTECTED_CONTENT, true, false),
            describeStandard(StandardMetadataType::COMPRESSION, true, false),
            describeStandard(StandardMetadataType::INTERLACED, true, false),
            describeStandard(StandardMetadataType::CHROMA_SITING, true, false),
            describeStandard(StandardMetadataType::PLANE_LAYOUTS, true, false),
            describeStandard(StandardMetadataType::CROP, true, false),
            describeStandard(StandardMetadataType::DATASPACE, true, true),
            describeStandard(StandardMetadataType::COMPRESSION, true, false),
            describeStandard(StandardMetadataType::BLEND_MODE, true, true),
            describeStandard(StandardMetadataType::SMPTE2086, true, true),
            describeStandard(StandardMetadataType::CTA861_3, true, true),
            describeStandard(StandardMetadataType::STRIDE, true, false),
            {kArmMetadataTypePlaneFds, "Vector of file descriptors of each plane", true, false, {0}},
            {kArmMetadataTypeFormatDataType, "Format data type", true, false, {0}},
    };
    *outDescriptionList = sSupportedMetadaTypes.data();
    *outNumberOfDescriptions = sSupportedMetadaTypes.size();
    return AIMAPPER_ERROR_NONE;
}

void CrosGrallocMapperV5::dumpBuffer(
        const cros_gralloc_buffer* crosBuffer,
        std::function<void(AIMapper_MetadataType, const std::vector<uint8_t>&)> callback) {
    // Temp buffer of ~10kb, should be large enough for any of the metadata we want to dump
    std::vector<uint8_t> tempBuffer;
    tempBuffer.resize(10000);
    AIMapper_MetadataType metadataType;
    metadataType.name = STANDARD_METADATA_NAME;

    // Take an instance of the empty StandardMetadat<T> class just to allow auto-deduction
    // to happen as explicit template invocation on lambdas is ugly
    auto dump = [&]<StandardMetadataType T>(StandardMetadata<T>) {
        // Nested templated lambdas! Woo! But the cleanness of the result is worth it
        // The outer lambda exists basically just to capture the StandardMetadataType that's
        // being dumped, as the `provider` parameter of getStandardMetadata only knows
        // the value_type that the enum maps to but not the enum value itself, which we need to
        // construct the `AIMapper_MetadataType` to pass to the dump callback
        auto dumpInner = [&](const typename StandardMetadata<T>::value_type& value) -> int32_t {
            int32_t size =
                    StandardMetadata<T>::value::encode(value, tempBuffer.data(), tempBuffer.size());
            // The initial size should always be large enough, but just in case...
            if (size > tempBuffer.size()) {
                tempBuffer.resize(size * 2);
                size = StandardMetadata<T>::value::encode(value, tempBuffer.data(),
                                                          tempBuffer.size());
            }
            // If the first resize failed _somehow_, just give up. Also don't notify if any
            // errors occurred during encoding.
            if (size >= 0 && size <= tempBuffer.size()) {
                metadataType.value = static_cast<int64_t>(T);
                callback(metadataType, tempBuffer);
            }
            // We don't actually care about the return value in this case, but why not use the
            // real value anyway
            return size;
        };
        getStandardMetadata(crosBuffer, dumpInner, StandardMetadata<T>{});
    };

    // So clean. So pretty.
    dump(StandardMetadata<StandardMetadataType::BUFFER_ID>{});
    dump(StandardMetadata<StandardMetadataType::NAME>{});
    dump(StandardMetadata<StandardMetadataType::WIDTH>{});
    dump(StandardMetadata<StandardMetadataType::HEIGHT>{});
    dump(StandardMetadata<StandardMetadataType::LAYER_COUNT>{});
    dump(StandardMetadata<StandardMetadataType::PIXEL_FORMAT_REQUESTED>{});
    dump(StandardMetadata<StandardMetadataType::PIXEL_FORMAT_FOURCC>{});
    dump(StandardMetadata<StandardMetadataType::PIXEL_FORMAT_MODIFIER>{});
    dump(StandardMetadata<StandardMetadataType::USAGE>{});
    dump(StandardMetadata<StandardMetadataType::ALLOCATION_SIZE>{});
    dump(StandardMetadata<StandardMetadataType::PROTECTED_CONTENT>{});
    dump(StandardMetadata<StandardMetadataType::COMPRESSION>{});
    dump(StandardMetadata<StandardMetadataType::INTERLACED>{});
    dump(StandardMetadata<StandardMetadataType::CHROMA_SITING>{});
    dump(StandardMetadata<StandardMetadataType::PLANE_LAYOUTS>{});
    dump(StandardMetadata<StandardMetadataType::DATASPACE>{});
    dump(StandardMetadata<StandardMetadataType::BLEND_MODE>{});
}

AIMapper_Error CrosGrallocMapperV5::dumpBuffer(
        buffer_handle_t _Nonnull bufferHandle,
        AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback, void* _Null_unspecified context) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle)
    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(bufferHandle);
    if (!crosHandle) {
        ALOGE("Failed to get. Invalid handle.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }
    auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
        dumpBufferCallback(context, type, buffer.data(), buffer.size());
    };
    mDriver->with_buffer(
            crosHandle, [&](cros_gralloc_buffer* crosBuffer) { dumpBuffer(crosBuffer, callback); });
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::dumpAllBuffers(
        AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
        AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback, void* _Null_unspecified context) {
    REQUIRE_DRIVER()
    auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
        dumpBufferCallback(context, type, buffer.data(), buffer.size());
    };
    mDriver->with_each_buffer([&](cros_gralloc_buffer* crosBuffer) {
        beginDumpBufferCallback(context);
        dumpBuffer(crosBuffer, callback);
    });
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error CrosGrallocMapperV5::getReservedRegion(buffer_handle_t _Nonnull buffer,
                                                      void* _Nullable* _Nonnull outReservedRegion,
                                                      uint64_t* _Nonnull outReservedSize) {
    VALIDATE_DRIVER_AND_BUFFER_HANDLE(buffer)
    cros_gralloc_handle_t crosHandle = cros_gralloc_convert_handle(buffer);
    if (!crosHandle) {
        ALOGE("Failed to getReservedRegion. Invalid handle.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    void* reservedRegionAddr = nullptr;
    uint64_t reservedRegionSize = 0;

    AIMapper_Error error = AIMAPPER_ERROR_NONE;
    mDriver->with_buffer(crosHandle, [&](cros_gralloc_buffer* crosBuffer) {
        int ret = crosBuffer->get_client_reserved_region(&reservedRegionAddr, &reservedRegionSize);
        if (ret) {
            reservedRegionAddr = nullptr;
            reservedRegionSize = 0;
            error = AIMAPPER_ERROR_NO_RESOURCES;
        }
    });

    if (error != AIMAPPER_ERROR_NONE) {
        ALOGE("Failed to getReservedRegion. Failed to getReservedRegion.");
        return AIMAPPER_ERROR_BAD_BUFFER;
    }

    return AIMAPPER_ERROR_NONE;
}

extern "C" uint32_t ANDROID_HAL_MAPPER_VERSION = AIMAPPER_VERSION_5;

extern "C" AIMapper_Error AIMapper_loadIMapper(AIMapper* _Nullable* _Nonnull outImplementation) {
    static vendor::mapper::IMapperProvider<CrosGrallocMapperV5> provider;
    return provider.load(outImplementation);
}
