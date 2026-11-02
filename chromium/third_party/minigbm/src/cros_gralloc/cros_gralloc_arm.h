#ifndef CROS_GRALLOC_ARM_H
#define CROS_GRALLOC_ARM_H

#include "../drv.h"
#include "aidl/android/hardware/graphics/common/ExtendableType.h"

using aidl::android::hardware::graphics::common::ExtendableType;

/* from AIDL interface */
typedef enum {
	INVALID = 0,
	PLANE_FDS = 1,
	FORMAT_DATA_TYPE = 2,
} ArmMetadataType;

typedef enum {
	UNORM = 0,
	SNORM = 1,
	UINT = 2,
	SINT = 3,
	SFLOAT = 4,
	UNKNOWN = 0xFF,
} ArmDataType;

typedef enum {
	AFBC = 0,
	AFRC = 1,
} ArmCompression;

#define GRALLOC_ARM_COMPRESSION_TYPE_NAME "arm.graphics.Compression"
const static ExtendableType Compression_AFBC{ GRALLOC_ARM_COMPRESSION_TYPE_NAME,
					      static_cast<int64_t>(ArmCompression::AFBC) };

const static ExtendableType Compression_AFRC{ GRALLOC_ARM_COMPRESSION_TYPE_NAME,
					      static_cast<int64_t>(ArmCompression::AFRC) };

#define GRALLOC_ARM_METADATA_TYPE_NAME "arm.graphics.ArmMetadataType"

#define GRALLOC_ARM_FORMAT_DATA_TYPE_NAME "arm.graphics.DataType"

static ArmDataType DataTypeFromDrmPixelFormat(uint32_t pf)
{
	switch (pf) {
	case DRM_FORMAT_R8:
	case DRM_FORMAT_GR88:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU420_ANDROID:
	case DRM_FORMAT_P010:
		return ArmDataType::UNORM;
	case DRM_FORMAT_ABGR16161616F:
		return ArmDataType::SFLOAT;
	default:
		return ArmDataType::UNKNOWN;
	}
}

#endif // CROS_GRALLOC_ARM_H
