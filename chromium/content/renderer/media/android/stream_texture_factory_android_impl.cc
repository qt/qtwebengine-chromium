// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory_android_impl.h"

#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/stream_texture_host_android.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/gfx/size.h"

namespace content {

namespace {

class StreamTextureProxyImpl : public StreamTextureProxy,
                               public StreamTextureHost::Listener {
 public:
  explicit StreamTextureProxyImpl(StreamTextureHost* host);
  virtual ~StreamTextureProxyImpl();

  // StreamTextureProxy implementation:
  virtual void BindToCurrentThread(int32 stream_id) OVERRIDE;
  virtual bool IsBoundToThread() OVERRIDE { return loop_.get() != NULL; }
  virtual void SetClient(cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual void Release() OVERRIDE;

  // StreamTextureHost::Listener implementation:
  virtual void OnFrameAvailable() OVERRIDE;
  virtual void OnMatrixChanged(const float matrix[16]) OVERRIDE;

 private:
  scoped_ptr<StreamTextureHost> host_;
  scoped_refptr<base::MessageLoopProxy> loop_;

  base::Lock client_lock_;
  cc::VideoFrameProvider::Client* client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxyImpl);
};

StreamTextureProxyImpl::StreamTextureProxyImpl(StreamTextureHost* host)
    : host_(host), client_(NULL) {
  host->SetListener(this);
}

StreamTextureProxyImpl::~StreamTextureProxyImpl() {}

void StreamTextureProxyImpl::Release() {
  SetClient(NULL);
  if (loop_.get() && loop_.get() != base::MessageLoopProxy::current())
    loop_->DeleteSoon(FROM_HERE, this);
  else
    delete this;
}

void StreamTextureProxyImpl::SetClient(cc::VideoFrameProvider::Client* client) {
  base::AutoLock lock(client_lock_);
  client_ = client;
}

void StreamTextureProxyImpl::BindToCurrentThread(int stream_id) {
  loop_ = base::MessageLoopProxy::current();
  host_->Initialize(stream_id);
}

void StreamTextureProxyImpl::OnFrameAvailable() {
  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidReceiveFrame();
}

void StreamTextureProxyImpl::OnMatrixChanged(const float matrix[16]) {
  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidUpdateMatrix(matrix);
}

}  // namespace

StreamTextureFactoryImpl::StreamTextureFactoryImpl(
    blink::WebGraphicsContext3D* context,
    GpuChannelHost* channel,
    int view_id)
    : context_(context), channel_(channel), view_id_(view_id) {
  DCHECK(context_);
  DCHECK(channel);
}

StreamTextureFactoryImpl::~StreamTextureFactoryImpl() {}

StreamTextureProxy* StreamTextureFactoryImpl::CreateProxy() {
  DCHECK(channel_.get());
  StreamTextureHost* host = new StreamTextureHost(channel_.get());
  return new StreamTextureProxyImpl(host);
}

void StreamTextureFactoryImpl::EstablishPeer(int32 stream_id, int player_id) {
  DCHECK(channel_.get());
  channel_->Send(
      new GpuChannelMsg_EstablishStreamTexture(stream_id, view_id_, player_id));
}

unsigned StreamTextureFactoryImpl::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox,
    unsigned* texture_mailbox_sync_point) {
  unsigned stream_id = 0;
  if (context_->makeContextCurrent()) {
    *texture_id = context_->createTexture();
    stream_id = context_->createStreamTextureCHROMIUM(*texture_id);

    context_->genMailboxCHROMIUM(texture_mailbox->name);
    context_->bindTexture(texture_target, *texture_id);
    context_->produceTextureCHROMIUM(texture_target, texture_mailbox->name);

    context_->flush();
    *texture_mailbox_sync_point = context_->insertSyncPoint();
  }
  return stream_id;
}

void StreamTextureFactoryImpl::DestroyStreamTexture(unsigned texture_id) {
  if (context_->makeContextCurrent()) {
    // TODO(sievers): Make the destroyStreamTexture implicit when the last
    // texture referencing it is lost.
    context_->destroyStreamTextureCHROMIUM(texture_id);
    context_->deleteTexture(texture_id);
    context_->flush();
  }
}

void StreamTextureFactoryImpl::SetStreamTextureSize(
    int32 stream_id, const gfx::Size& size) {
  channel_->Send(new GpuChannelMsg_SetStreamTextureSize(stream_id, size));
}

blink::WebGraphicsContext3D* StreamTextureFactoryImpl::Context3d() {
  return context_;
}

}  // namespace content
