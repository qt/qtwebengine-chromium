// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory_android_synchronous_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "content/common/android/surface_texture_peer.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/gl/android/surface_texture.h"

namespace content {

namespace {

class StreamTextureProxyImpl
    : public StreamTextureProxy,
      public base::SupportsWeakPtr<StreamTextureProxyImpl> {
 public:
  explicit StreamTextureProxyImpl(
      StreamTextureFactorySynchronousImpl::ContextProvider* provider);
  virtual ~StreamTextureProxyImpl();

  // StreamTextureProxy implementation:
  virtual void BindToCurrentThread(int32 stream_id) OVERRIDE;
  virtual bool IsBoundToThread() OVERRIDE { return loop_.get() != NULL; }
  virtual void SetClient(cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual void Release() OVERRIDE;

 private:
  void OnFrameAvailable();

  scoped_refptr<base::MessageLoopProxy> loop_;
  base::Lock client_lock_;
  cc::VideoFrameProvider::Client* client_;
  base::Closure callback_;

  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      context_provider_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  float current_matrix_[16];
  bool has_updated_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxyImpl);
};

StreamTextureProxyImpl::StreamTextureProxyImpl(
    StreamTextureFactorySynchronousImpl::ContextProvider* provider)
    : context_provider_(provider), has_updated_(false) {
  std::fill(current_matrix_, current_matrix_ + 16, 0);
}

StreamTextureProxyImpl::~StreamTextureProxyImpl() {}

void StreamTextureProxyImpl::Release() {
  SetClient(NULL);
  if (loop_.get() && !loop_->BelongsToCurrentThread())
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

  surface_texture_ = context_provider_->GetSurfaceTexture(stream_id);
  if (!surface_texture_) {
    LOG(ERROR) << "Failed to get SurfaceTexture for stream.";
    return;
  }

  callback_ =
      base::Bind(&StreamTextureProxyImpl::OnFrameAvailable, AsWeakPtr());
  surface_texture_->SetFrameAvailableCallback(callback_);
}

void StreamTextureProxyImpl::OnFrameAvailable() {
  // GetTransformMatrix only returns something valid after both is true:
  // - OnFrameAvailable was called
  // - we called UpdateTexImage
  if (has_updated_) {
    float matrix[16];
    surface_texture_->GetTransformMatrix(matrix);

    if (memcmp(current_matrix_, matrix, sizeof(matrix)) != 0) {
      memcpy(current_matrix_, matrix, sizeof(matrix));

      base::AutoLock lock(client_lock_);
      if (client_)
        client_->DidUpdateMatrix(current_matrix_);
    }
  }
  // OnFrameAvailable being called a second time implies that we called
  // updateTexImage since after we received the first frame.
  has_updated_ = true;

  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidReceiveFrame();
}

}  // namespace

StreamTextureFactorySynchronousImpl::StreamTextureFactorySynchronousImpl(
    ContextProvider* context_provider,
    int view_id)
    : context_provider_(context_provider), view_id_(view_id) {}

StreamTextureFactorySynchronousImpl::~StreamTextureFactorySynchronousImpl() {}

StreamTextureProxy* StreamTextureFactorySynchronousImpl::CreateProxy() {
  if (!context_provider_)
    return NULL;
  return new StreamTextureProxyImpl(context_provider_);
}

void StreamTextureFactorySynchronousImpl::EstablishPeer(int32 stream_id,
                                                        int player_id) {
  DCHECK(context_provider_);
  scoped_refptr<gfx::SurfaceTexture> surface_texture =
      context_provider_->GetSurfaceTexture(stream_id);
  if (surface_texture) {
    SurfaceTexturePeer::GetInstance()->EstablishSurfaceTexturePeer(
        base::Process::Current().handle(),
        surface_texture,
        view_id_,
        player_id);
  }
}

unsigned StreamTextureFactorySynchronousImpl::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox,
    unsigned* texture_mailbox_sync_point) {
  DCHECK(context_provider_);
  WebKit::WebGraphicsContext3D* context = context_provider_->Context3d();
  unsigned stream_id = 0;
  if (context->makeContextCurrent()) {
    *texture_id = context->createTexture();
    stream_id = context->createStreamTextureCHROMIUM(*texture_id);

    context->genMailboxCHROMIUM(texture_mailbox->name);
    context->bindTexture(texture_target, *texture_id);
    context->produceTextureCHROMIUM(texture_target, texture_mailbox->name);

    context->flush();
    *texture_mailbox_sync_point = context->insertSyncPoint();
  }
  return stream_id;
}

void StreamTextureFactorySynchronousImpl::DestroyStreamTexture(
    unsigned texture_id) {
  DCHECK(context_provider_);
  WebKit::WebGraphicsContext3D* context = context_provider_->Context3d();
  if (context->makeContextCurrent()) {
    context->destroyStreamTextureCHROMIUM(texture_id);
    context->deleteTexture(texture_id);
    context->flush();
  }
}

void StreamTextureFactorySynchronousImpl::SetStreamTextureSize(
    int32 stream_id,
    const gfx::Size& size) {}

}  // namespace content
