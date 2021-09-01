// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_RMAD_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_RMAD_DBUS_CONSTANTS_H_

namespace rmad {
const char kRmadInterfaceName[] = "org.chromium.Rmad";
const char kRmadServicePath[] = "/org/chromium/Rmad";
const char kRmadServiceName[] = "org.chromium.Rmad";

// Methods
const char kGetCurrentStateMethod[] = "GetCurrentState";

}  // namespace rmad

#endif  // SYSTEM_API_DBUS_RMAD_DBUS_CONSTANTS_H_
