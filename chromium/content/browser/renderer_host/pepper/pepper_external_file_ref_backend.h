// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTERNAL_FILE_REF_BACKEND_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTERNAL_FILE_REF_BACKEND_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/ppapi_host.h"

namespace content {

// Implementations of FileRef operations for external filesystems.
class PepperExternalFileRefBackend : public PepperFileRefBackend {
 public:
  PepperExternalFileRefBackend(ppapi::host::PpapiHost* host,
                               int render_process_id,
                               const base::FilePath& path);
  virtual ~PepperExternalFileRefBackend();

  // PepperFileRefBackend overrides.
  virtual int32_t MakeDirectory(ppapi::host::ReplyMessageContext context,
                                bool make_ancestors) OVERRIDE;
  virtual int32_t Touch(ppapi::host::ReplyMessageContext context,
                        PP_Time last_accessed_time,
                        PP_Time last_modified_time) OVERRIDE;
  virtual int32_t Delete(ppapi::host::ReplyMessageContext context) OVERRIDE;
  virtual int32_t Rename(ppapi::host::ReplyMessageContext context,
                         PepperFileRefHost* new_file_ref) OVERRIDE;
  virtual int32_t Query(ppapi::host::ReplyMessageContext context) OVERRIDE;
  virtual int32_t ReadDirectoryEntries(
      ppapi::host::ReplyMessageContext context) OVERRIDE;
  virtual int32_t GetAbsolutePath(ppapi::host::ReplyMessageContext context)
      OVERRIDE;
  virtual fileapi::FileSystemURL GetFileSystemURL() const OVERRIDE;

  virtual int32_t CanRead() const OVERRIDE;
  virtual int32_t CanWrite() const OVERRIDE;
  virtual int32_t CanCreate() const OVERRIDE;
  virtual int32_t CanReadWrite() const OVERRIDE;

 private:
  // Generic reply callback.
  void DidFinish(ppapi::host::ReplyMessageContext reply_context,
                 const IPC::Message& msg,
                 base::PlatformFileError error);

  // Operation specific callbacks.
  void GetMetadataComplete(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info);

  ppapi::host::PpapiHost* host_;
  base::FilePath path_;
  int render_process_id_;

  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<PepperExternalFileRefBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperExternalFileRefBackend);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTERNAL_FILE_REF_BACKEND_H_
