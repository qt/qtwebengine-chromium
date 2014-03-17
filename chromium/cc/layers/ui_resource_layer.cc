// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer.h"

#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {


namespace {

class ScopedUIResourceHolder : public UIResourceLayer::UIResourceHolder {
 public:
  static scoped_ptr<ScopedUIResourceHolder> Create(LayerTreeHost* host,
                                            const SkBitmap& skbitmap) {
    return make_scoped_ptr(new ScopedUIResourceHolder(host, skbitmap));
  }
  virtual UIResourceId id() OVERRIDE { return resource_->id(); }

 private:
  ScopedUIResourceHolder(LayerTreeHost* host, const SkBitmap& skbitmap) {
    resource_ = ScopedUIResource::Create(host, UIResourceBitmap(skbitmap));
  }

  scoped_ptr<ScopedUIResource> resource_;
};

class SharedUIResourceHolder : public UIResourceLayer::UIResourceHolder {
 public:
  static scoped_ptr<SharedUIResourceHolder> Create(UIResourceId id) {
    return make_scoped_ptr(new SharedUIResourceHolder(id));
  }

  virtual UIResourceId id() OVERRIDE { return id_; }

 private:
  explicit SharedUIResourceHolder(UIResourceId id) : id_(id) {}

  UIResourceId id_;
};

}  // anonymous namespace

UIResourceLayer::UIResourceHolder::~UIResourceHolder() {}

scoped_refptr<UIResourceLayer> UIResourceLayer::Create() {
  return make_scoped_refptr(new UIResourceLayer());
}

UIResourceLayer::UIResourceLayer()
    : Layer(),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

UIResourceLayer::~UIResourceLayer() {}

scoped_ptr<LayerImpl> UIResourceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UIResourceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void UIResourceLayer::SetUV(gfx::PointF top_left, gfx::PointF bottom_right) {
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void UIResourceLayer::SetVertexOpacity(float bottom_left,
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

void UIResourceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host == layer_tree_host())
    return;

  Layer::SetLayerTreeHost(host);

  // Recreate the resource hold against the new LTH.
  RecreateUIResourceHolder();
}

void UIResourceLayer::RecreateUIResourceHolder() {
  ui_resource_holder_.reset();
  if (!layer_tree_host() || bitmap_.empty())
    return;

  ui_resource_holder_ =
    ScopedUIResourceHolder::Create(layer_tree_host(), bitmap_);
}

void UIResourceLayer::SetBitmap(const SkBitmap& skbitmap) {
  bitmap_ = skbitmap;

  RecreateUIResourceHolder();
  SetNeedsCommit();
}

void UIResourceLayer::SetUIResourceId(UIResourceId resource_id) {
  if (ui_resource_holder_ && ui_resource_holder_->id() == resource_id)
    return;

  if (resource_id) {
    ui_resource_holder_ = SharedUIResourceHolder::Create(resource_id);
  } else {
    ui_resource_holder_.reset();
  }

  SetNeedsCommit();
}

bool UIResourceLayer::DrawsContent() const {
  return ui_resource_holder_ && ui_resource_holder_->id() &&
         Layer::DrawsContent();
}

void UIResourceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  UIResourceLayerImpl* layer_impl = static_cast<UIResourceLayerImpl*>(layer);

  if (!ui_resource_holder_) {
    layer_impl->SetUIResourceId(0);
  } else {
    DCHECK(layer_tree_host());

    gfx::Size image_size =
        layer_tree_host()->GetUIResourceSize(ui_resource_holder_->id());
    layer_impl->SetUIResourceId(ui_resource_holder_->id());
    layer_impl->SetImageBounds(image_size);
    layer_impl->SetUV(uv_top_left_, uv_bottom_right_);
    layer_impl->SetVertexOpacity(vertex_opacity_);
  }
}

}  // namespace cc
