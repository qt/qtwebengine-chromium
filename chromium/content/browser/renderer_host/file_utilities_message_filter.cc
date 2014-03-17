// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_message_filter.h"

#include "base/file_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/file_utilities_messages.h"

namespace content {

FileUtilitiesMessageFilter::FileUtilitiesMessageFilter(int process_id)
    : process_id_(process_id) {
}

FileUtilitiesMessageFilter::~FileUtilitiesMessageFilter() {
}

void FileUtilitiesMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == FileUtilitiesMsgStart)
    *thread = BrowserThread::FILE;
}

bool FileUtilitiesMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileUtilitiesMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_GetFileInfo, OnGetFileInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileUtilitiesMessageFilter::OnGetFileInfo(
    const base::FilePath& path,
    base::PlatformFileInfo* result,
    base::PlatformFileError* status) {
  *result = base::PlatformFileInfo();
  *status = base::PLATFORM_FILE_OK;

  // Get file metadata only when the child process has been granted
  // permission to read the file.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path)) {
    return;
  }

  if (!base::GetFileInfo(path, result))
    *status = base::PLATFORM_FILE_ERROR_FAILED;
}

}  // namespace content
