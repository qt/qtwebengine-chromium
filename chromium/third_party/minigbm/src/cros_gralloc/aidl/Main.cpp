/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Allocator.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

using aidl::android::hardware::graphics::allocator::impl::Allocator;

int main(int /*argc*/, char** /*argv*/) {
    ALOGI("Minigbm AIDL allocator starting up...");

    // same as SF main thread
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
        ALOGI("%s: failed to set priority: %s", __FUNCTION__, strerror(errno));
    }

    auto allocator = ndk::SharedRefBase::make<Allocator>();
    CHECK(allocator != nullptr);

    if (!allocator->init()) {
        ALOGE("Failed to initialize Minigbm AIDL allocator.");
        return EXIT_FAILURE;
    }

    const std::string instance = std::string() + Allocator::descriptor + "/default";
    binder_status_t status = AServiceManager_addServiceWithFlags(
            allocator->asBinder().get(), instance.c_str(),
            AServiceManager_AddServiceFlag::ADD_SERVICE_ALLOW_ISOLATED);
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();
    ABinderProcess_joinThreadPool();

    return EXIT_FAILURE;
}
