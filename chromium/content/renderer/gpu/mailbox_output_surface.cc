// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/mailbox_output_surface.h"

#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/gl_frame_data.h"
#include "cc/resources/resource_provider.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using cc::CompositorFrame;
using cc::GLFrameData;
using cc::ResourceProvider;
using gpu::Mailbox;

namespace content {

MailboxOutputSurface::MailboxOutputSurface(
    int32 routing_id,
    uint32 output_surface_id,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    cc::ResourceFormat format)
    : CompositorOutputSurface(routing_id,
                              output_surface_id,
                              context_provider,
                              software_device.Pass(),
                              true),
      fbo_(0),
      is_backbuffer_discarded_(false),
      format_(format) {
  pending_textures_.push_back(TransferableFrame());
  capabilities_.max_frames_pending = 1;
  capabilities_.uses_default_gl_framebuffer = false;
}

MailboxOutputSurface::~MailboxOutputSurface() {
  DiscardBackbuffer();
  while (!pending_textures_.empty()) {
    if (pending_textures_.front().texture_id) {
      context_provider_->Context3d()->deleteTexture(
          pending_textures_.front().texture_id);
    }
    pending_textures_.pop_front();
  }
}

void MailboxOutputSurface::EnsureBackbuffer() {
  is_backbuffer_discarded_ = false;

  blink::WebGraphicsContext3D* context3d = context_provider_->Context3d();

  if (!current_backing_.texture_id) {
    // Find a texture of matching size to recycle.
    while (!returned_textures_.empty()) {
      TransferableFrame& texture = returned_textures_.front();
      if (texture.size == surface_size_) {
        current_backing_ = texture;
        if (current_backing_.sync_point)
          context3d->waitSyncPoint(current_backing_.sync_point);
        returned_textures_.pop();
        break;
      }

      context3d->deleteTexture(texture.texture_id);
      returned_textures_.pop();
    }

    if (!current_backing_.texture_id) {
      current_backing_.texture_id = context3d->createTexture();
      current_backing_.size = surface_size_;
      context3d->bindTexture(GL_TEXTURE_2D, current_backing_.texture_id);
      context3d->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      context3d->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      context3d->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      context3d->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      context3d->texImage2D(
          GL_TEXTURE_2D,
          0,
          GLInternalFormat(format_),
          surface_size_.width(),
          surface_size_.height(),
          0,
          GLDataFormat(format_),
          GLDataType(format_),
          NULL);
      context3d->genMailboxCHROMIUM(current_backing_.mailbox.name);
      context3d->produceTextureCHROMIUM(
          GL_TEXTURE_2D, current_backing_.mailbox.name);
    }
  }
}

void MailboxOutputSurface::DiscardBackbuffer() {
  is_backbuffer_discarded_ = true;

  blink::WebGraphicsContext3D* context3d = context_provider_->Context3d();

  if (current_backing_.texture_id) {
    context3d->deleteTexture(current_backing_.texture_id);
    current_backing_ = TransferableFrame();
  }

  while (!returned_textures_.empty()) {
    const TransferableFrame& frame = returned_textures_.front();
    context3d->deleteTexture(frame.texture_id);
    returned_textures_.pop();
  }

  if (fbo_) {
    context3d->bindFramebuffer(GL_FRAMEBUFFER, fbo_);
    context3d->deleteFramebuffer(fbo_);
    fbo_ = 0;
  }
}

void MailboxOutputSurface::Reshape(gfx::Size size, float scale_factor) {
  if (size == surface_size_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void MailboxOutputSurface::BindFramebuffer() {
  EnsureBackbuffer();
  DCHECK(current_backing_.texture_id);

  blink::WebGraphicsContext3D* context3d = context_provider_->Context3d();

  if (!fbo_)
    fbo_ = context3d->createFramebuffer();
  context3d->bindFramebuffer(GL_FRAMEBUFFER, fbo_);
  context3d->framebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      current_backing_.texture_id, 0);
}

void MailboxOutputSurface::OnSwapAck(uint32 output_surface_id,
                                     const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_) {
    CompositorOutputSurface::OnSwapAck(output_surface_id, ack);
    return;
  }
  if (!ack.gl_frame_data->mailbox.IsZero()) {
    DCHECK(!ack.gl_frame_data->size.IsEmpty());
    // The browser could be returning the oldest or any other pending texture
    // if it decided to skip a frame.
    std::deque<TransferableFrame>::iterator it;
    for (it = pending_textures_.begin(); it != pending_textures_.end(); it++) {
      DCHECK(!it->mailbox.IsZero());
      if (!memcmp(it->mailbox.name,
                  ack.gl_frame_data->mailbox.name,
                  sizeof(it->mailbox.name))) {
        DCHECK(it->size == ack.gl_frame_data->size);
        break;
      }
    }
    DCHECK(it != pending_textures_.end());
    it->sync_point = ack.gl_frame_data->sync_point;

    if (!is_backbuffer_discarded_) {
      returned_textures_.push(*it);
    } else {
      context_provider_->Context3d()->deleteTexture(it->texture_id);
    }

    pending_textures_.erase(it);
  } else {
    DCHECK(!pending_textures_.empty());
    // The browser always keeps one texture as the frontbuffer.
    // If it does not return a mailbox, it discarded the frontbuffer which is
    // the oldest texture we sent.
    uint32 texture_id = pending_textures_.front().texture_id;
    if (texture_id)
      context_provider_->Context3d()->deleteTexture(texture_id);
    pending_textures_.pop_front();
  }
  CompositorOutputSurface::OnSwapAck(output_surface_id, ack);
}

void MailboxOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(frame->gl_frame_data);
  DCHECK(!surface_size_.IsEmpty());
  DCHECK(surface_size_ == current_backing_.size);
  DCHECK(frame->gl_frame_data->size == current_backing_.size);
  DCHECK(!current_backing_.mailbox.IsZero() ||
         context_provider_->Context3d()->isContextLost());

  frame->gl_frame_data->mailbox = current_backing_.mailbox;
  context_provider_->Context3d()->flush();
  frame->gl_frame_data->sync_point =
      context_provider_->Context3d()->insertSyncPoint();
  CompositorOutputSurface::SwapBuffers(frame);

  pending_textures_.push_back(current_backing_);
  current_backing_ = TransferableFrame();
}

size_t MailboxOutputSurface::GetNumAcksPending() {
  DCHECK(pending_textures_.size());
  return pending_textures_.size() - 1;
}

}  // namespace content
