// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
#define PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/thunk/ppb_file_system_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT FileSystemResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_FileSystem_API) {
 public:
  // Creates a new FileSystemResource. The resource must be subsequently opened
  // via Open() before use.
  FileSystemResource(Connection connection,
                     PP_Instance instance,
                     PP_FileSystemType type);
  // Creates a FileSystemResource, attached to an existing pending host
  // resource. The |pending_renderer_id| and |pending_browser_id| must be
  // already-opened file systems.
  FileSystemResource(Connection connection,
                     PP_Instance instance,
                     int pending_renderer_id,
                     int pending_browser_id,
                     PP_FileSystemType type);
  virtual ~FileSystemResource();

  // Resource overrides.
  virtual thunk::PPB_FileSystem_API* AsPPB_FileSystem_API() OVERRIDE;

  // PPB_FileSystem_API implementation.
  virtual int32_t Open(int64_t expected_size,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_FileSystemType GetType() OVERRIDE;

  int32_t InitIsolatedFileSystem(const std::string& fsid,
                                 PP_IsolatedFileSystemType_Private type,
                                 const base::Callback<void(int32_t)>& callback);
 private:
  // Called when the host has responded to our open request.
  void OpenComplete(scoped_refptr<TrackedCallback> callback,
                    const ResourceMessageReplyParams& params);

  // Called when the host has responded to our InitIsolatedFileSystem request.
  void InitIsolatedFileSystemComplete(
      const base::Callback<void(int32_t)>& callback,
      const ResourceMessageReplyParams& params);

  PP_FileSystemType type_;
  bool called_open_;
  uint32_t callback_count_;
  int32_t callback_result_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
