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

// MemoryPressureChrome signal contains 2 arguments:
//   1. pressure_level, BYTE
//     0: no memory pressure level
//     1: moderate memory pressure level
//     2: critical memory pressure level
//   2. delta, UINT64, memory amount to free in KB to leave the current
//   pressure level.
//   E.g. argument (0, 10000): Chrome should free 10000 KB to leave the critical
//   memory pressure level (to moderate pressure level).
const char kMemoryPressureChrome[] = "MemoryPressureChrome";

// Values.
enum GameMode {
  // Game mode is off.
  OFF = 0,
  // Game mode is on, borealis is the foreground subsystem.
  BOREALIS = 1,
};

enum PressureLevelChrome {
  // There is enough memory to use.
  NONE = 0,
  // Chrome is advised to free buffers that are cheap to re-allocate and not
  // immediately needed.
  MODERATE = 1,
  // Chrome is advised to free all possible memory.
  CRITICAL = 2,
};

}  // namespace resource_manager

#endif  // SYSTEM_API_DBUS_RESOURCE_MANAGER_DBUS_CONSTANTS_H_
