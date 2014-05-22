// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/lock.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/resources/single_release_callback.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<TextureLayer> TextureLayer::Create(TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client, false));
}

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox(
    TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client, true));
}

TextureLayer::TextureLayer(TextureLayerClient* client, bool uses_mailbox)
    : Layer(),
      client_(client),
      uses_mailbox_(uses_mailbox),
      flipped_(true),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f),
      premultiplied_alpha_(true),
      blend_background_color_(false),
      rate_limit_context_(false),
      content_committed_(false),
      texture_id_(0),
      needs_set_mailbox_(false) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

TextureLayer::~TextureLayer() {
}

void TextureLayer::ClearClient() {
  if (rate_limit_context_ && client_ && layer_tree_host())
    layer_tree_host()->StopRateLimiter();
  client_ = NULL;
  if (uses_mailbox_)
    SetTextureMailbox(TextureMailbox(), scoped_ptr<SingleReleaseCallback>());
  else
    SetTextureId(0);
}

scoped_ptr<LayerImpl> TextureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id(), uses_mailbox_).
      PassAs<LayerImpl>();
}

void TextureLayer::SetFlipped(bool flipped) {
  if (flipped_ == flipped)
    return;
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetUV(gfx::PointF top_left, gfx::PointF bottom_right) {
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetVertexOpacity(float bottom_left,
                                    float top_left,
                                    float top_right,
                                    float bottom_right) {
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  if (vertex_opacity_[0] == bottom_left &&
      vertex_opacity_[1] == top_left &&
      vertex_opacity_[2] == top_right &&
      vertex_opacity_[3] == bottom_right)
    return;
  vertex_opacity_[0] = bottom_left;
  vertex_opacity_[1] = top_left;
  vertex_opacity_[2] = top_right;
  vertex_opacity_[3] = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
  if (premultiplied_alpha_ == premultiplied_alpha)
    return;
  premultiplied_alpha_ = premultiplied_alpha;
  SetNeedsCommit();
}

void TextureLayer::SetBlendBackgroundColor(bool blend) {
  if (blend_background_color_ == blend)
    return;
  blend_background_color_ = blend;
  SetNeedsCommit();
}

void TextureLayer::SetRateLimitContext(bool rate_limit) {
  if (!rate_limit && rate_limit_context_ && client_ && layer_tree_host())
    layer_tree_host()->StopRateLimiter();

  rate_limit_context_ = rate_limit;
}

void TextureLayer::SetTextureId(unsigned id) {
  DCHECK(!uses_mailbox_);
  if (texture_id_ == id)
    return;
  if (texture_id_ && layer_tree_host())
    layer_tree_host()->AcquireLayerTextures();
  texture_id_ = id;
  SetNeedsCommit();
  // The texture id needs to be removed from the active tree before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void TextureLayer::SetTextureMailboxInternal(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback,
    bool requires_commit) {
  DCHECK(uses_mailbox_);
  DCHECK(!mailbox.IsValid() || !holder_ref_ ||
         !mailbox.Equals(holder_ref_->holder()->mailbox()));
  DCHECK_EQ(mailbox.IsValid(), !!release_callback);

  // If we never commited the mailbox, we need to release it here.
  if (mailbox.IsValid())
    holder_ref_ = MailboxHolder::Create(mailbox, release_callback.Pass());
  else
    holder_ref_.reset();
  needs_set_mailbox_ = true;
  // If we are within a commit, no need to do it again immediately after.
  if (requires_commit)
    SetNeedsCommit();
  else
    SetNeedsPushProperties();

  // The active frame needs to be replaced and the mailbox returned before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void TextureLayer::SetTextureMailbox(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback) {
  SetTextureMailboxInternal(
      mailbox,
      release_callback.Pass(),
      true /* requires_commit */);
}

void TextureLayer::WillModifyTexture() {
  if (!uses_mailbox_ && layer_tree_host() && (DrawsContent() ||
      content_committed_)) {
    layer_tree_host()->AcquireLayerTextures();
    content_committed_ = false;
  }
}

void TextureLayer::SetNeedsDisplayRect(const gfx::RectF& dirty_rect) {
  Layer::SetNeedsDisplayRect(dirty_rect);

  if (rate_limit_context_ && client_ && layer_tree_host() && DrawsContent())
    layer_tree_host()->StartRateLimiter();
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  if (layer_tree_host()) {
    if (texture_id_) {
      layer_tree_host()->AcquireLayerTextures();
      // The texture id needs to be removed from the active tree before the
      // commit is called complete.
      SetNextCommitWaitsForActivation();
    }
    if (rate_limit_context_ && client_)
      layer_tree_host()->StopRateLimiter();
  }
  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && uses_mailbox_ && holder_ref_) {
    needs_set_mailbox_ = true;
    // The active frame needs to be replaced and the mailbox returned before the
    // commit is called complete.
    SetNextCommitWaitsForActivation();
  }
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::DrawsContent() const {
  return (client_ || texture_id_ || holder_ref_) && Layer::DrawsContent();
}

bool TextureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker* occlusion) {
  bool updated = Layer::Update(queue, occlusion);
  if (client_) {
    if (uses_mailbox_) {
      TextureMailbox mailbox;
      scoped_ptr<SingleReleaseCallback> release_callback;
      if (client_->PrepareTextureMailbox(
              &mailbox,
              &release_callback,
              layer_tree_host()->UsingSharedMemoryResources())) {
        // Already within a commit, no need to do another one immediately.
        SetTextureMailboxInternal(
            mailbox,
            release_callback.Pass(),
            false /* requires_commit */);
        updated = true;
      }
    } else {
      texture_id_ = client_->PrepareTexture();
      updated = true;
      SetNeedsPushProperties();
      // The texture id needs to be removed from the active tree before the
      // commit is called complete.
      SetNextCommitWaitsForActivation();
    }
  }

  // SetTextureMailbox could be called externally and the same mailbox used for
  // different textures.  Such callers notify this layer that the texture has
  // changed by calling SetNeedsDisplay, so check for that here.
  return updated || !update_rect_.IsEmpty();
}

void TextureLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->set_flipped(flipped_);
  texture_layer->set_uv_top_left(uv_top_left_);
  texture_layer->set_uv_bottom_right(uv_bottom_right_);
  texture_layer->set_vertex_opacity(vertex_opacity_);
  texture_layer->set_premultiplied_alpha(premultiplied_alpha_);
  texture_layer->set_blend_background_color(blend_background_color_);
  if (uses_mailbox_ && needs_set_mailbox_) {
    TextureMailbox texture_mailbox;
    scoped_ptr<SingleReleaseCallback> release_callback;
    if (holder_ref_) {
      MailboxHolder* holder = holder_ref_->holder();
      texture_mailbox = holder->mailbox();
      release_callback = holder->GetCallbackForImplThread();
    }
    texture_layer->SetTextureMailbox(texture_mailbox, release_callback.Pass());
    needs_set_mailbox_ = false;
  } else {
    texture_layer->set_texture_id(texture_id_);
    content_committed_ = DrawsContent();
  }
}

Region TextureLayer::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return visible_content_rect();

  if (blend_background_color_ && (SkColorGetA(background_color()) == 0xFF))
    return visible_content_rect();

  return Region();
}

TextureLayer::MailboxHolder::MainThreadReference::MainThreadReference(
    MailboxHolder* holder)
    : holder_(holder) {
  holder_->InternalAddRef();
}

TextureLayer::MailboxHolder::MainThreadReference::~MainThreadReference() {
  holder_->InternalRelease();
}

TextureLayer::MailboxHolder::MailboxHolder(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback)
    : message_loop_(BlockingTaskRunner::current()),
      internal_references_(0),
      mailbox_(mailbox),
      release_callback_(release_callback.Pass()),
      sync_point_(mailbox.sync_point()),
      is_lost_(false) {
}

TextureLayer::MailboxHolder::~MailboxHolder() {
  DCHECK_EQ(0u, internal_references_);
}

scoped_ptr<TextureLayer::MailboxHolder::MainThreadReference>
TextureLayer::MailboxHolder::Create(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback) {
  return scoped_ptr<MainThreadReference>(new MainThreadReference(
      new MailboxHolder(mailbox, release_callback.Pass())));
}

void TextureLayer::MailboxHolder::Return(unsigned sync_point, bool is_lost) {
  base::AutoLock lock(arguments_lock_);
  sync_point_ = sync_point;
  is_lost_ = is_lost;
}

scoped_ptr<SingleReleaseCallback>
TextureLayer::MailboxHolder::GetCallbackForImplThread() {
  // We can't call GetCallbackForImplThread if we released the main thread
  // reference.
  DCHECK_GT(internal_references_, 0u);
  InternalAddRef();
  return SingleReleaseCallback::Create(
      base::Bind(&MailboxHolder::ReturnAndReleaseOnImplThread, this));
}

void TextureLayer::MailboxHolder::InternalAddRef() {
  ++internal_references_;
}

void TextureLayer::MailboxHolder::InternalRelease() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!--internal_references_) {
    release_callback_->Run(sync_point_, is_lost_);
    mailbox_ = TextureMailbox();
    release_callback_.reset();
  }
}

void TextureLayer::MailboxHolder::ReturnAndReleaseOnImplThread(
    unsigned sync_point, bool is_lost) {
  Return(sync_point, is_lost);
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&MailboxHolder::InternalRelease, this));
}

}  // namespace cc
