// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_FILE_ENTRY_H_
#define MTPD_FILE_ENTRY_H_

#include <libmtp.h>

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "mtp_file_entry.pb.h"

namespace mtpd {

class FileEntry {
 public:
  explicit FileEntry(const LIBMTP_file_struct& file);
  FileEntry();
  ~FileEntry();

  MtpFileEntry ToProtobuf() const;
  std::vector<uint8_t> ToDBusFormat() const;

  static std::vector<uint8_t> EmptyFileEntriesToDBusFormat();
  static std::vector<uint8_t> FileEntriesToDBusFormat(
      std::vector<FileEntry>& entries);

  uint32_t item_id() const { return item_id_; }
  uint32_t parent_id() const { return parent_id_; }
  const std::string& file_name() const { return file_name_; }
  uint64_t file_size() const { return file_size_; }
  time_t modification_time() const { return modification_time_; }
  LIBMTP_filetype_t file_type() const { return file_type_; }

 private:
  uint32_t item_id_;
  uint32_t parent_id_;
  std::string file_name_;
  uint64_t file_size_;
  time_t modification_time_;
  LIBMTP_filetype_t file_type_;
};

}  // namespace mtpd

#endif  // MTPD_FILE_ENTRY_H_
