// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_OPEN_FILE_HANDLE_CONTEXT_H_
#define WEBKIT_BROWSER_FILEAPI_OPEN_FILE_HANDLE_CONTEXT_H_

#include <map>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace fileapi {

class QuotaReservationBuffer;

// This class represents a underlying file of a managed FileSystem file.
// The instance keeps alive while at least one consumer keeps an open file
// handle.
// This class is usually manipulated only via OpenFileHandle.
class OpenFileHandleContext : public base::RefCounted<OpenFileHandleContext> {
 public:
  OpenFileHandleContext(const base::FilePath& platform_path,
                        QuotaReservationBuffer* reservation_buffer);

  void UpdateMaxWrittenOffset(int64 offset,
                              int64* new_file_size,
                              int64* growth);

  const base::FilePath& platform_path() const {
    return platform_path_;
  }

  int64 base_file_size() const { return maximum_written_offset_; }

 private:
  friend class base::RefCounted<OpenFileHandleContext>;
  virtual ~OpenFileHandleContext();

  int64 initial_file_size_;
  int64 maximum_written_offset_;
  base::FilePath platform_path_;

  scoped_refptr<QuotaReservationBuffer> reservation_buffer_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(OpenFileHandleContext);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_OPEN_FILE_HANDLE_CONTEXT_H_
