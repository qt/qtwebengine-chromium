// Copyright 2026 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_DISSIDIA_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_DISSIDIA_DBUS_CONSTANTS_H_

namespace dissidia {

inline constexpr char kDissidiaServiceName[] = "com.google.starline.dissidia";
inline constexpr char kDissidiaServicePath[] = "/com/google/starline/dissidia";

// Interface exposed by the dissidia-daemon.
inline constexpr char kDissidiaInterface[] = "com.google.starline.dissidia";

// Methods of the |kDissidiaInterface| interface:
inline constexpr char kPerformUpdateMethod[] = "PerformUpdate";

// Signals of the |kDissidiaInterface| interface:
inline constexpr char kProgressSignal[] = "Progress";
inline constexpr char kCompletedSignal[] = "Completed";

// Status codes returned by |kPerformUpdateMethod|.
enum PerformUpdateStatus : int32_t {
  kUpdateStarted = 0,
  kAlreadyOnRequestedImage = 1,
  kUpdateAlreadyInProgress = 2,
  kOobeAlreadyCompleted = 3,
  kError = 4,
};

// Error codes reported by the |kCompletedSignal| signal.
enum CompletedErrorCode : int32_t {
  kSuccess = 0,
  kGeneralFailure = 1,
  kDownloadFailed = 2,
  kSlotResolutionFailed = 3,
  kExtractFailed = 4,
  kRootdevFailed = 5,
  kCgptFailed = 6,
  kRebootFailed = 7,
};

}  // namespace dissidia

#endif  // SYSTEM_API_DBUS_DISSIDIA_DBUS_CONSTANTS_H_
