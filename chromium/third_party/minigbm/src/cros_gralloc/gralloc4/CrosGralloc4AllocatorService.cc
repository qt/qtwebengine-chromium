/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define LOG_TAG "AllocatorService"

#include <hidl/LegacySupport.h>

#include "cros_gralloc/gralloc4/CrosGralloc4Allocator.h"

using android::sp;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::graphics::allocator::V4_0::IAllocator;

int main(int, char**) {
    sp<IAllocator> allocator = new CrosGralloc4Allocator();
    configureRpcThreadpool(4, true /* callerWillJoin */);
    if (allocator->registerAsService() != android::NO_ERROR) {
        ALOGE("failed to register graphics IAllocator 4.0 service");
        return -EINVAL;
    }

    ALOGI("graphics IAllocator 4.0 service is initialized");
    android::hardware::joinRpcThreadpool();
    ALOGI("graphics IAllocator 4.0 service is terminating");
    return 0;
}
