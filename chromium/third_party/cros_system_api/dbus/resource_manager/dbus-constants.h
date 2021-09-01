// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_RESOURCE_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_RESOURCE_MANAGER_DBUS_CONSTANTS_H_

namespace resource_manager {

const char kResourceManagerInterface[] = "org.chromium.ResourceManager";
const char kResourceManagerServicePath[] = "/org/chromium/ResourceManager";
const char kResourceManagerServiceName[] = "org.chromium.ResourceManager";

// Methods.
const char kGetAvailableMemoryKBMethod[] = "GetAvailableMemoryKB";
const char kGetForegroundAvailableMemoryKBMethod[] =
    "GetForegroundAvailableMemoryKB";
const char kGetMemoryMarginsKBMethod[] = "GetMemoryMarginsKB";
const char kGetGameModeMethod[] = "GetGameMode";
const char kSetGameModeMethod[] = "SetGameMode";

// Signals.
const char kCriticalMemoryPressure[] = "CriticalMemoryPressure";
const char kModerateMemoryPressure[] = "ModerateMemoryPressure";

}  // namespace resource_manager

#endif  // SYSTEM_API_DBUS_RESOURCE_MANAGER_DBUS_CONSTANTS_H_
