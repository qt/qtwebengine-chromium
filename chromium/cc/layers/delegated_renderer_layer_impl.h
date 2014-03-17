// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_
#define CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_impl.h"

namespace cc {
class DelegatedFrameData;
class RenderPassSink;

class CC_EXPORT DelegatedRendererLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<DelegatedRendererLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new DelegatedRendererLayerImpl(tree_impl, id));
  }
  virtual ~DelegatedRendererLayerImpl();

  // LayerImpl overrides.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual bool HasDelegatedContent() const OVERRIDE;
  virtual bool HasContributingDelegatedRenderPasses() const OVERRIDE;
  virtual RenderPass::Id FirstContributingRenderPassId() const OVERRIDE;
  virtual RenderPass::Id NextContributingRenderPassId(
      RenderPass::Id previous) const OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;
  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  void AppendContributingRenderPasses(RenderPassSink* render_pass_sink);

  // Creates an ID with the resource provider for the child renderer
  // that will be sending quads to the layer. Registers the callback to
  // inform when resources are no longer in use.
  void CreateChildIdIfNeeded(const ReturnCallback& return_callback);

  void SetFrameData(const DelegatedFrameData* frame_data,
                    gfx::RectF damage_in_frame);

  void SetDisplaySize(gfx::Size size);

 protected:
  DelegatedRendererLayerImpl(LayerTreeImpl* tree_impl, int id);

  int ChildIdForTesting() const { return child_id_; }
  const ScopedPtrVector<RenderPass>& RenderPassesInDrawOrderForTesting() const {
    return render_passes_in_draw_order_;
  }
  const ResourceProvider::ResourceIdArray& ResourcesForTesting() const {
    return resources_;
  }

 private:
  void ClearChildId();

  void AppendRainbowDebugBorder(QuadSink* quad_sink,
                                AppendQuadsData* append_quads_data);

  void SetRenderPasses(
      ScopedPtrVector<RenderPass>* render_passes_in_draw_order);
  void ClearRenderPasses();

  // Returns |true| if the delegated_render_pass_id is part of the current
  // frame and can be converted.
  bool ConvertDelegatedRenderPassId(
      RenderPass::Id delegated_render_pass_id,
      RenderPass::Id* output_render_pass_id) const;

  gfx::Transform DelegatedFrameToLayerSpaceTransform(gfx::Size frame_size)
      const;

  void AppendRenderPassQuads(
      QuadSink* quad_sink,
      AppendQuadsData* append_quads_data,
      const RenderPass* delegated_render_pass,
      gfx::Size frame_size) const;

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;

  bool have_render_passes_to_push_;
  ScopedPtrVector<RenderPass> render_passes_in_draw_order_;
  base::hash_map<RenderPass::Id, int> render_passes_index_by_id_;
  ResourceProvider::ResourceIdArray resources_;

  gfx::Size display_size_;
  int child_id_;
  bool own_child_id_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedRendererLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_
