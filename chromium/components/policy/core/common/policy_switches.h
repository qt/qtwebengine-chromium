// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_

#include "components/policy/policy_export.h"

#include "build/build_config.h"

namespace policy {
namespace switches {

POLICY_EXPORT extern const char kCloudPolicyInvalidationDelay[];
POLICY_EXPORT extern const char kDisableCloudPolicyPush[];
POLICY_EXPORT extern const char kEnableComponentCloudPolicy[];

#if defined(OS_ANDROID) || defined(OS_IOS)
POLICY_EXPORT extern const char kFakeCloudPolicyType[];
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace switches
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
