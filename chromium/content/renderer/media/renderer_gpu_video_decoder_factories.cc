// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_decoder_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/command_buffer_proxy.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace content {

RendererGpuVideoDecoderFactories::~RendererGpuVideoDecoderFactories() {}
RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    WebGraphicsContext3DCommandBufferImpl* context)
    : message_loop_(message_loop),
      main_message_loop_(base::MessageLoopProxy::current()),
      gpu_channel_host_(gpu_channel_host),
      aborted_waiter_(true, false),
      message_loop_async_waiter_(false, false),
      render_thread_async_waiter_(false, false) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncGetContext(context);
    message_loop_async_waiter_.Reset();
    return;
  }
  // Wait for the context to be acquired.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncGetContext,
      // Unretained to avoid ref/deref'ing |*this|, which is not yet stored in a
      // scoped_refptr.  Safe because the Wait() below keeps us alive until this
      // task completes.
      base::Unretained(this),
      // OK to pass raw because the pointee is only deleted on the compositor
      // thread, and only as the result of a PostTask from the render thread
      // which can only happen after this function returns, so our PostTask will
      // run first.
      context));
  message_loop_async_waiter_.Wait();
}

RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories()
    : aborted_waiter_(true, false),
      message_loop_async_waiter_(false, false),
      render_thread_async_waiter_(false, false) {}

void RendererGpuVideoDecoderFactories::AsyncGetContext(
    WebGraphicsContext3DCommandBufferImpl* context) {
  context_ = context->AsWeakPtr();
  if (context_.get()) {
    if (context_->makeContextCurrent()) {
      // Called once per media player, but is a no-op after the first one in
      // each renderer.
      context_->insertEventMarkerEXT("GpuVDAContext3D");
    }
  }
  message_loop_async_waiter_.Signal();
}

media::VideoDecodeAccelerator*
RendererGpuVideoDecoderFactories::CreateVideoDecodeAccelerator(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncCreateVideoDecodeAccelerator(profile, client);
    message_loop_async_waiter_.Reset();
    return vda_.release();
  }
  // The VDA is returned in the vda_ member variable by the
  // AsyncCreateVideoDecodeAccelerator() function.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator,
      this, profile, client));

  base::WaitableEvent* objects[] = {&aborted_waiter_,
                                    &message_loop_async_waiter_};
  if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0) {
    // If we are aborting and the VDA is created by the
    // AsyncCreateVideoDecodeAccelerator() function later we need to ensure
    // that it is destroyed on the same thread.
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &RendererGpuVideoDecoderFactories::AsyncDestroyVideoDecodeAccelerator,
        this));
    return NULL;
  }
  return vda_.release();
}

void RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (context_.get() && context_->GetCommandBufferProxy()) {
    vda_ = gpu_channel_host_->CreateVideoDecoder(
        context_->GetCommandBufferProxy()->GetRouteID(), profile, client);
  }
  message_loop_async_waiter_.Signal();
}

uint32 RendererGpuVideoDecoderFactories::CreateTextures(
    int32 count, const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32 texture_target) {
  uint32 sync_point = 0;

  if (message_loop_->BelongsToCurrentThread()) {
    AsyncCreateTextures(count, size, texture_target, &sync_point);
    texture_ids->swap(created_textures_);
    texture_mailboxes->swap(created_texture_mailboxes_);
    message_loop_async_waiter_.Reset();
    return sync_point;
  }
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateTextures, this,
      count, size, texture_target, &sync_point));

  base::WaitableEvent* objects[] = {&aborted_waiter_,
                                    &message_loop_async_waiter_};
  if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0)
    return 0;
  texture_ids->swap(created_textures_);
  texture_mailboxes->swap(created_texture_mailboxes_);
  return sync_point;
}

void RendererGpuVideoDecoderFactories::AsyncCreateTextures(
    int32 count, const gfx::Size& size, uint32 texture_target,
    uint32* sync_point) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(texture_target);

  if (!context_.get()) {
    message_loop_async_waiter_.Signal();
    return;
  }
  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  created_textures_.resize(count);
  created_texture_mailboxes_.resize(count);
  gles2->GenTextures(count, &created_textures_[0]);
  for (int i = 0; i < count; ++i) {
    gles2->ActiveTexture(GL_TEXTURE0);
    uint32 texture_id = created_textures_[i];
    gles2->BindTexture(texture_target, texture_id);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (texture_target == GL_TEXTURE_2D) {
      gles2->TexImage2D(texture_target, 0, GL_RGBA, size.width(), size.height(),
                        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    gles2->GenMailboxCHROMIUM(created_texture_mailboxes_[i].name);
    gles2->ProduceTextureCHROMIUM(texture_target,
                                  created_texture_mailboxes_[i].name);
  }

  // We need a glFlush here to guarantee the decoder (in the GPU process) can
  // use the texture ids we return here.  Since textures are expected to be
  // reused, this should not be unacceptably expensive.
  gles2->Flush();
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));

  *sync_point = gles2->InsertSyncPointCHROMIUM();
  message_loop_async_waiter_.Signal();
}

void RendererGpuVideoDecoderFactories::DeleteTexture(uint32 texture_id) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncDeleteTexture(texture_id);
    return;
  }
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncDeleteTexture, this, texture_id));
}

void RendererGpuVideoDecoderFactories::AsyncDeleteTexture(uint32 texture_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_.get())
    return;

  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoDecoderFactories::WaitSyncPoint(uint32 sync_point) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncWaitSyncPoint(sync_point);
    message_loop_async_waiter_.Reset();
    return;
  }

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncWaitSyncPoint,
      this,
      sync_point));
  base::WaitableEvent* objects[] = {&aborted_waiter_,
                                    &message_loop_async_waiter_};
  base::WaitableEvent::WaitMany(objects, arraysize(objects));
}

void RendererGpuVideoDecoderFactories::AsyncWaitSyncPoint(uint32 sync_point) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_) {
    message_loop_async_waiter_.Signal();
    return;
  }

  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  gles2->WaitSyncPointCHROMIUM(sync_point);
  message_loop_async_waiter_.Signal();
}

void RendererGpuVideoDecoderFactories::ReadPixels(
    uint32 texture_id, uint32 texture_target, const gfx::Size& size,
    const SkBitmap& pixels) {
  // SkBitmaps use the SkPixelRef object to refcount the underlying pixels.
  // Multiple SkBitmaps can share a SkPixelRef instance. We use this to
  // ensure that the underlying pixels in the SkBitmap passed in remain valid
  // until the AsyncReadPixels() call completes.
  read_pixels_bitmap_.setPixelRef(pixels.pixelRef());

  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &RendererGpuVideoDecoderFactories::AsyncReadPixels, this,
        texture_id, texture_target, size));
    base::WaitableEvent* objects[] = {&aborted_waiter_,
                                      &message_loop_async_waiter_};
    if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0)
      return;
  } else {
    AsyncReadPixels(texture_id, texture_target, size);
    message_loop_async_waiter_.Reset();
  }
  read_pixels_bitmap_.setPixelRef(NULL);
}

void RendererGpuVideoDecoderFactories::AsyncReadPixels(
    uint32 texture_id, uint32 texture_target, const gfx::Size& size) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_.get()) {
    message_loop_async_waiter_.Signal();
    return;
  }

  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();

  GLuint tmp_texture;
  gles2->GenTextures(1, &tmp_texture);
  gles2->BindTexture(texture_target, tmp_texture);
  gles2->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  context_->copyTextureCHROMIUM(
      texture_target, texture_id, tmp_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE);

  GLuint fb;
  gles2->GenFramebuffers(1, &fb);
  gles2->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gles2->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target, tmp_texture, 0);
  gles2->PixelStorei(GL_PACK_ALIGNMENT, 4);
  gles2->ReadPixels(0, 0, size.width(), size.height(), GL_BGRA_EXT,
      GL_UNSIGNED_BYTE, read_pixels_bitmap_.pixelRef()->pixels());
  gles2->DeleteFramebuffers(1, &fb);
  gles2->DeleteTextures(1, &tmp_texture);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  message_loop_async_waiter_.Signal();
}

base::SharedMemory* RendererGpuVideoDecoderFactories::CreateSharedMemory(
    size_t size) {
  if (main_message_loop_->BelongsToCurrentThread()) {
    return ChildThread::current()->AllocateSharedMemory(size);
  }
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory, this,
      size));

  base::WaitableEvent* objects[] = {&aborted_waiter_,
                                    &render_thread_async_waiter_};
  if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0)
    return NULL;
  return shared_memory_segment_.release();
}

void RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory(size_t size) {
  DCHECK_EQ(base::MessageLoop::current(),
            ChildThread::current()->message_loop());

  shared_memory_segment_.reset(
      ChildThread::current()->AllocateSharedMemory(size));
  render_thread_async_waiter_.Signal();
}

scoped_refptr<base::MessageLoopProxy>
RendererGpuVideoDecoderFactories::GetMessageLoop() {
  return message_loop_;
}

void RendererGpuVideoDecoderFactories::Abort() {
  aborted_waiter_.Signal();
}

bool RendererGpuVideoDecoderFactories::IsAborted() {
  return aborted_waiter_.IsSignaled();
}

scoped_refptr<media::GpuVideoDecoderFactories>
RendererGpuVideoDecoderFactories::Clone() {
  scoped_refptr<RendererGpuVideoDecoderFactories> factories =
      new RendererGpuVideoDecoderFactories();
  factories->message_loop_ = message_loop_;
  factories->main_message_loop_ = main_message_loop_;
  factories->gpu_channel_host_ = gpu_channel_host_;
  factories->context_ = context_;
  return factories;
}

void RendererGpuVideoDecoderFactories::AsyncDestroyVideoDecodeAccelerator() {
  // OK to release because Destroy() will delete the VDA instance.
  if (vda_)
    vda_.release()->Destroy();
}

}  // namespace content
