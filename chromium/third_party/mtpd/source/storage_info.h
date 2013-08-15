// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_STORAGE_INFO_H_
#define MTPD_STORAGE_INFO_H_

#include <libmtp.h>

#include <string>
#include <vector>

#include <base/basictypes.h>

namespace mtpd {

class StorageInfo {
 public:
  StorageInfo(const std::string& storage_name,
              const LIBMTP_device_entry_t& device,
              const LIBMTP_devicestorage_t& storage,
              const std::string& fallback_vendor,
              const std::string& fallback_product);
  StorageInfo();
  ~StorageInfo();

  std::vector<uint8_t> ToDBusFormat() const;

 private:
  std::string storage_name_;

  // From LIBMTP_device_entry_t.
  std::string vendor_;
  uint16_t vendor_id_;
  std::string product_;
  uint16_t product_id_;
  uint32_t device_flags_;

  // From LIBMTP_devicestorage_t.
  uint16_t storage_type_;
  uint16_t filesystem_type_;
  uint16_t access_capability_;
  uint64_t max_capacity_;
  uint64_t free_space_in_bytes_;
  uint64_t free_space_in_objects_;
  std::string storage_description_;
  std::string volume_identifier_;
};

}  // namespace mtpd

#endif  // MTPD_STORAGE_INFO_H_
