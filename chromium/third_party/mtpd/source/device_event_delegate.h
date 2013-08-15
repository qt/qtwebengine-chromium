// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_DEVICE_EVENT_DELEGATE_H_
#define MTPD_DEVICE_EVENT_DELEGATE_H_

#include <string>

namespace mtpd {

// An interface to allow a delegate to handle MTP storage attach/detach events.
class DeviceEventDelegate {
 public:
  virtual ~DeviceEventDelegate() {}

  // Called when a new MTP storage is added.
  virtual void StorageAttached(const std::string& storage_name) = 0;

  // Called when a new MTP storage is removed.
  virtual void StorageDetached(const std::string& storage_name) = 0;
};

}  // namespace mtpd

#endif  // MTPD_DEVICE_EVENT_DELEGATE_H_
