// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_demuxer_android.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediaplayer_android.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

RendererDemuxerAndroid::RendererDemuxerAndroid()
    : thread_safe_sender_(RenderThreadImpl::current()->thread_safe_sender()),
      media_message_loop_(
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy()) {}

RendererDemuxerAndroid::~RendererDemuxerAndroid() {}

int RendererDemuxerAndroid::GetNextDemuxerClientID() {
  // Don't use zero for IDs since it can be interpreted as having no ID.
  return next_demuxer_client_id_.GetNext() + 1;
}

void RendererDemuxerAndroid::AddDelegate(int demuxer_client_id,
                                         MediaSourceDelegate* delegate) {
  DCHECK(media_message_loop_->BelongsToCurrentThread());
  delegates_.AddWithID(delegate, demuxer_client_id);
}

void RendererDemuxerAndroid::RemoveDelegate(int demuxer_client_id) {
  DCHECK(media_message_loop_->BelongsToCurrentThread());
  delegates_.Remove(demuxer_client_id);
}

bool RendererDemuxerAndroid::OnMessageReceived(const IPC::Message& message) {
  switch (message.type()) {
    case MediaPlayerMsg_MediaSeekRequest::ID:
    case MediaPlayerMsg_ReadFromDemuxer::ID:
    case MediaPlayerMsg_MediaConfigRequest::ID:
      media_message_loop_->PostTask(FROM_HERE, base::Bind(
          &RendererDemuxerAndroid::DispatchMessage, this, message));
      return true;
   }
  return false;
}

void RendererDemuxerAndroid::DemuxerReady(
    int demuxer_client_id,
    const media::DemuxerConfigs& configs) {
  thread_safe_sender_->Send(new MediaPlayerHostMsg_DemuxerReady(
      demuxer_client_id, configs));
}

void RendererDemuxerAndroid::ReadFromDemuxerAck(
    int demuxer_client_id,
    const media::DemuxerData& data) {
  thread_safe_sender_->Send(new MediaPlayerHostMsg_ReadFromDemuxerAck(
      demuxer_client_id, data));
}

void RendererDemuxerAndroid::SeekRequestAck(int demuxer_client_id,
                                            unsigned seek_request_id) {
  thread_safe_sender_->Send(new MediaPlayerHostMsg_MediaSeekRequestAck(
      demuxer_client_id, seek_request_id));
}

void RendererDemuxerAndroid::DurationChanged(int demuxer_client_id,
                                             const base::TimeDelta& duration) {
  thread_safe_sender_->Send(new MediaPlayerHostMsg_DurationChanged(
      demuxer_client_id, duration));
}

void RendererDemuxerAndroid::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(RendererDemuxerAndroid, message)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaSeekRequest, OnMediaSeekRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ReadFromDemuxer, OnReadFromDemuxer)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaConfigRequest, OnMediaConfigRequest)
  IPC_END_MESSAGE_MAP()
}

void RendererDemuxerAndroid::OnReadFromDemuxer(
    int demuxer_client_id,
    media::DemuxerStream::Type type) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->OnReadFromDemuxer(type);
}

void RendererDemuxerAndroid::OnMediaSeekRequest(
    int demuxer_client_id,
    const base::TimeDelta& time_to_seek,
    unsigned seek_request_id) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->Seek(time_to_seek, seek_request_id);
}

void RendererDemuxerAndroid::OnMediaConfigRequest(int demuxer_client_id) {
  MediaSourceDelegate* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->OnMediaConfigRequest();
}

}  // namespace content
