// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/file_io_state_manager.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "url/gurl.h"
#include "webkit/common/quota/quota_types.h"

using ppapi::host::ReplyMessageContext;

namespace content {
class QuotaFileIO;

class PepperFileIOHost : public ppapi::host::ResourceHost,
                         public base::SupportsWeakPtr<PepperFileIOHost>,
                         public IPC::Listener {
 public:
  typedef base::Callback<void (base::PlatformFileError)>
      NotifyCloseFileCallback;

  PepperFileIOHost(RendererPpapiHost* host,
                   PP_Instance instance,
                   PP_Resource resource);
  virtual ~PepperFileIOHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnAsyncFileOpened(
      base::PlatformFileError error_code,
      IPC::PlatformFileForTransit file_for_transit,
      int message_id);

  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        PP_Resource file_ref_resource,
                        int32_t open_flags);
  int32_t OnHostMsgTouch(ppapi::host::HostMessageContext* context,
                         PP_Time last_access_time,
                         PP_Time last_modified_time);
  int32_t OnHostMsgWrite(ppapi::host::HostMessageContext* context,
                         int64_t offset,
                         const std::string& buffer);
  int32_t OnHostMsgSetLength(ppapi::host::HostMessageContext* context,
                             int64_t length);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgFlush(ppapi::host::HostMessageContext* context);
  // Private API.
  int32_t OnHostMsgRequestOSFileHandle(
      ppapi::host::HostMessageContext* context);
  // Trusted API.
  int32_t OnHostMsgGetOSFileDescriptor(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgWillWrite(ppapi::host::HostMessageContext* context,
                             int64_t offset,
                             int32_t bytes_to_write);
  int32_t OnHostMsgWillSetLength(ppapi::host::HostMessageContext* context,
                                 int64_t length);

  // Callback handlers. These mostly convert the PlatformFileError to the
  // PP_Error code and send back the reply. Note that the argument
  // ReplyMessageContext is copied so that we have a closure containing all
  // necessary information to reply.
  void ExecutePlatformGeneralCallback(ReplyMessageContext reply_context,
                                      base::PlatformFileError error_code);
  void ExecutePlatformOpenFileCallback(ReplyMessageContext reply_context,
                                       base::PlatformFileError error_code,
                                       base::PassPlatformFile file);
  void ExecutePlatformOpenFileSystemURLCallback(
      ReplyMessageContext reply_context,
      base::PlatformFileError error_code,
      base::PassPlatformFile file,
      quota::QuotaLimitType quota_policy,
      const NotifyCloseFileCallback& callback);
  void ExecutePlatformQueryCallback(ReplyMessageContext reply_context,
                                    base::PlatformFileError error_code,
                                    const base::PlatformFileInfo& file_info);
  void ExecutePlatformReadCallback(ReplyMessageContext reply_context,
                                   base::PlatformFileError error_code,
                                   const char* data, int bytes_read);
  void ExecutePlatformWriteCallback(ReplyMessageContext reply_context,
                                    base::PlatformFileError error_code,
                                    int bytes_written);

  RendererPpapiHost* renderer_ppapi_host_;
  base::PlatformFile file_;

  // The file system type specified in the Open() call. This will be
  // PP_FILESYSTEMTYPE_INVALID before open was called. This value does not
  // indicate that the open command actually succeeded.
  PP_FileSystemType file_system_type_;

  // Valid only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
  GURL file_system_url_;

  // Used to check if we can pass file handle to plugins.
  quota::QuotaLimitType quota_policy_;

  // Callback function for notifying when the file handle is closed.
  NotifyCloseFileCallback notify_close_file_callback_;

  // Pointer to a QuotaFileIO instance, which is valid only while a file
  // of type PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY} is opened.
  scoped_ptr<QuotaFileIO> quota_file_io_;

  bool is_running_in_process_;

  int32_t open_flags_;

  ppapi::FileIOStateManager state_manager_;

  int routing_id_;

  typedef base::Callback<void (base::PlatformFileError, base::PassPlatformFile)>
      AsyncOpenFileCallback;

  IDMap<AsyncOpenFileCallback> pending_async_open_files_;

  base::WeakPtrFactory<PepperFileIOHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileIOHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_
