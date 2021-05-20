// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SYSTEM_API_DBUS_DLP_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_DLP_DBUS_CONSTANTS_H_

namespace dlp {
// General
constexpr char kDlpInterface[] = "org.chromium.Dlp";
constexpr char kDlpServicePath[] = "/org/chromium/Dlp";
constexpr char kDlpServiceName[] = "org.chromium.Dlp";

// Methods
constexpr char kSetDlpFilesPolicyMethod[] = "SetDlpFilesPolicy";
constexpr char kAddFileMethod[] = "AddFile";

}  // namespace dlp
#endif  // SYSTEM_API_DBUS_DLP_DBUS_CONSTANTS_H_
