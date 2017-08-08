LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libdrm_mediatek
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libdrm

LOCAL_SRC_FILES := mediatek_drm.c
LOCAL_EXPORT_C_INCLUDE_DIRS += \
	$(LOCAL_PATH)/mediatek

LOCAL_C_INCLUDES := \
	$(LIBDRM_TOP) \
	$(LIBDRM_TOP)/mediatek \
	$(LIBDRM_TOP)/include/drm

# This makes sure mmap64 is used, as mmap offsets can be larger than 32-bit
LOCAL_CFLAGS := \
	-DHAVE_LIBDRM_ATOMIC_PRIMITIVES=1 \
	-D_LARGEFILE_SOURCE \
	-D_LARGEFILE64_SOURCE \
	-D_FILE_OFFSET_BITS=64

LOCAL_COPY_HEADERS := mediatek_drm.h mediatek_drmif.h
LOCAL_COPY_HEADERS_TO := libdrm

LOCAL_SHARED_LIBRARIES := \
	libdrm

include $(BUILD_SHARED_LIBRARY)
