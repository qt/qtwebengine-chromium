// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DELEGATED_RENDERER_LAYER_H_
#define CC_LAYERS_DELEGATED_RENDERER_LAYER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/returned_resource.h"

namespace cc {
class BlockingTaskRunner;
class DelegatedFrameData;
class DelegatedRendererLayerClient;

class CC_EXPORT DelegatedRendererLayer : public Layer {
 public:
  static scoped_refptr<DelegatedRendererLayer> Create(
      DelegatedRendererLayerClient* client);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* impl) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;

  // Set the size at which the frame should be displayed, with the origin at the
  // layer's origin. This must always contain at least the layer's bounds. A
  // value of (0, 0) implies that the frame should be displayed to fit exactly
  // in the layer's bounds.
  void SetDisplaySize(gfx::Size size);

  void SetFrameData(scoped_ptr<DelegatedFrameData> frame_data);

  // Passes ownership of any unused resources that had been given by the child
  // compositor to the given array, so they can be given back to the child.
  void TakeUnusedResourcesForChildCompositor(ReturnedResourceArray* array);

 protected:
  explicit DelegatedRendererLayer(DelegatedRendererLayerClient* client);
  virtual ~DelegatedRendererLayer();

 private:
  void ReceiveUnusedResources(const ReturnedResourceArray& unused);
  static void ReceiveUnusedResourcesOnImplThread(
      scoped_refptr<BlockingTaskRunner> task_runner,
      base::WeakPtr<DelegatedRendererLayer> self,
      const ReturnedResourceArray& unused);

  scoped_ptr<DelegatedFrameData> frame_data_;
  gfx::RectF damage_in_frame_;
  gfx::Size frame_size_;
  gfx::Size display_size_;

  DelegatedRendererLayerClient* client_;
  bool needs_filter_context_;

  ReturnedResourceArray unused_resources_for_child_compositor_;
  scoped_refptr<BlockingTaskRunner> main_thread_runner_;
  base::WeakPtrFactory<DelegatedRendererLayer> weak_ptrs_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedRendererLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_DELEGATED_RENDERER_LAYER_H_
