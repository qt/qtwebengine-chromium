// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_SERVICE_CONSTANTS_H_
#define MTPD_SERVICE_CONSTANTS_H_

#include "build_config.h"

#if defined(CHROMIUM_LINUX_BUILD)
#include "third_party/cros_system_api/dbus/service_constants.h"
#else
#include <chromeos/dbus/service_constants.h>
#endif

#endif  // MTPD_SERVICE_CONSTANTS_H_
