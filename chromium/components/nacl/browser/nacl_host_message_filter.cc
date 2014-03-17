// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_host_message_filter.h"

#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_file_host.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "ipc/ipc_platform_file.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace nacl {

NaClHostMessageFilter::NaClHostMessageFilter(
    int render_process_id,
    bool is_off_the_record,
    const base::FilePath& profile_directory,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      off_the_record_(is_off_the_record),
      profile_directory_(profile_directory),
      request_context_(request_context),
      weak_ptr_factory_(this) {
}

NaClHostMessageFilter::~NaClHostMessageFilter() {
}

void NaClHostMessageFilter::OnChannelClosing() {
  pnacl::PnaclHost::GetInstance()->RendererClosing(render_process_id_);
}

bool NaClHostMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(NaClHostMessageFilter, message, *message_was_ok)
#if !defined(DISABLE_NACL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_LaunchNaCl, OnLaunchNaCl)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_GetReadonlyPnaclFD,
                                    OnGetReadonlyPnaclFd)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_NaClCreateTemporaryFile,
                                    OnNaClCreateTemporaryFile)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NexeTempFileRequest,
                        OnGetNexeFd)
    IPC_MESSAGE_HANDLER(NaClHostMsg_ReportTranslationFinished,
                        OnTranslationFinished)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NaClErrorStatus, OnNaClErrorStatus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_OpenNaClExecutable,
                                    OnOpenNaClExecutable)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

net::HostResolver* NaClHostMessageFilter::GetHostResolver() {
  return request_context_->GetURLRequestContext()->host_resolver();
}

#if !defined(DISABLE_NACL)
void NaClHostMessageFilter::OnLaunchNaCl(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(
      GURL(launch_params.manifest_url),
      launch_params.render_view_id,
      launch_params.permission_bits,
      launch_params.uses_irt,
      launch_params.enable_dyncode_syscalls,
      launch_params.enable_exception_handling,
      launch_params.enable_crash_throttling,
      off_the_record_,
      profile_directory_);
  GURL manifest_url(launch_params.manifest_url);
  base::FilePath manifest_path;
  // We're calling MapUrlToLocalFilePath with the non-blocking API
  // because we're running in the I/O thread. Ideally we'd use the other path,
  // which would cover more cases.
  nacl::NaClBrowser::GetDelegate()->MapUrlToLocalFilePath(
      manifest_url, false /* use_blocking_api */, &manifest_path);
  host->Launch(this, reply_msg, manifest_path);
}

void NaClHostMessageFilter::OnGetReadonlyPnaclFd(
    const std::string& filename, IPC::Message* reply_msg) {
  // This posts a task to another thread, but the renderer will
  // block until the reply is sent.
  nacl_file_host::GetReadonlyPnaclFd(this, filename, reply_msg);

  // This is the first message we receive from the renderer once it knows we
  // want to use PNaCl, so start the translation cache initialization here.
  pnacl::PnaclHost::GetInstance()->Init();
}

// Return the temporary file via a reply to the
// NaClHostMsg_NaClCreateTemporaryFile sync message.
void NaClHostMessageFilter::SyncReturnTemporaryFile(
    IPC::Message* reply_msg,
    base::PlatformFile fd) {
  if (fd == base::kInvalidPlatformFileValue) {
    reply_msg->set_reply_error();
  } else {
    NaClHostMsg_NaClCreateTemporaryFile::WriteReplyParams(
        reply_msg,
        IPC::GetFileHandleForProcess(fd, PeerHandle(), true));
  }
  Send(reply_msg);
}

void NaClHostMessageFilter::OnNaClCreateTemporaryFile(
    IPC::Message* reply_msg) {
  pnacl::PnaclHost::GetInstance()->CreateTemporaryFile(
      base::Bind(&NaClHostMessageFilter::SyncReturnTemporaryFile,
                 this,
                 reply_msg));
}

void NaClHostMessageFilter::AsyncReturnTemporaryFile(
    int pp_instance,
    base::PlatformFile fd,
    bool is_hit) {
  Send(new NaClViewMsg_NexeTempFileReply(
      pp_instance,
      is_hit,
      // Don't close our copy of the handle, because PnaclHost will use it
      // when the translation finishes.
      IPC::GetFileHandleForProcess(fd, PeerHandle(), false)));
}

void NaClHostMessageFilter::OnGetNexeFd(
    int render_view_id,
    int pp_instance,
    const nacl::PnaclCacheInfo& cache_info) {
  if (!cache_info.pexe_url.is_valid()) {
    LOG(ERROR) << "Bad URL received from GetNexeFd: " <<
        cache_info.pexe_url.possibly_invalid_spec();
    BadMessageReceived();
    return;
  }

  pnacl::PnaclHost::GetInstance()->GetNexeFd(
      render_process_id_,
      render_view_id,
      pp_instance,
      off_the_record_,
      cache_info,
      base::Bind(&NaClHostMessageFilter::AsyncReturnTemporaryFile,
                 this,
                 pp_instance));
}

void NaClHostMessageFilter::OnTranslationFinished(int instance, bool success) {
  pnacl::PnaclHost::GetInstance()->TranslationFinished(
      render_process_id_, instance, success);
}

void NaClHostMessageFilter::OnNaClErrorStatus(int render_view_id,
                                              int error_id) {
  nacl::NaClBrowser::GetDelegate()->ShowNaClInfobar(render_process_id_,
                                                    render_view_id, error_id);
}

void NaClHostMessageFilter::OnOpenNaClExecutable(int render_view_id,
                                                 const GURL& file_url,
                                                 IPC::Message* reply_msg) {
  nacl_file_host::OpenNaClExecutable(this, render_view_id, file_url,
                                     reply_msg);
}
#endif

}  // namespace nacl
