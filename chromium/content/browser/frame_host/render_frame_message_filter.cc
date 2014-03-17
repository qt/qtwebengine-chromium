// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_message_filter.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void CreateChildFrameOnUI(int process_id,
                          int parent_render_frame_id,
                          int64 parent_frame_id,
                          int64 frame_id,
                          const std::string& frame_name,
                          int new_render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, parent_render_frame_id);
  // Handles the RenderFrameHost being deleted on the UI thread while
  // processing a subframe creation message.
  if (render_frame_host) {
    render_frame_host->OnCreateChildFrame(new_render_frame_id,
                                          parent_frame_id, frame_id,
                                          frame_name);
  }
}

}  // namespace

RenderFrameMessageFilter::RenderFrameMessageFilter(
    int render_process_id,
    RenderWidgetHelper* render_widget_helper)
    : render_process_id_(render_process_id),
      render_widget_helper_(render_widget_helper) {
}

RenderFrameMessageFilter::~RenderFrameMessageFilter() {
}

bool RenderFrameMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                 bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderFrameMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void RenderFrameMessageFilter::OnCreateChildFrame(
    int parent_render_frame_id,
    int64 parent_frame_id,
    int64 frame_id,
    const std::string& frame_name,
    int* new_render_frame_id) {
  *new_render_frame_id = render_widget_helper_->GetNextRoutingID();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateChildFrameOnUI, render_process_id_,
                 parent_render_frame_id, parent_frame_id, frame_id, frame_name,
                 *new_render_frame_id));
}

}  // namespace content
