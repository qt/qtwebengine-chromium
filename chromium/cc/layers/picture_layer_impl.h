// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include <string>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

struct AppendQuadsData;
class QuadSink;

class CC_EXPORT PictureLayerImpl
    : public LayerImpl,
      NON_EXPORTED_BASE(public PictureLayerTilingClient) {
 public:
  static scoped_ptr<PictureLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new PictureLayerImpl(tree_impl, id));
  }
  virtual ~PictureLayerImpl();

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void UpdateTilePriorities() OVERRIDE;
  virtual void DidBecomeActive() OVERRIDE;
  virtual void DidBeginTracing() OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;
  virtual skia::RefPtr<SkPicture> GetPicture() OVERRIDE;

  // PictureLayerTilingClient overrides.
  virtual scoped_refptr<Tile> CreateTile(PictureLayerTiling* tiling,
                                         gfx::Rect content_rect) OVERRIDE;
  virtual void UpdatePile(Tile* tile) OVERRIDE;
  virtual gfx::Size CalculateTileSize(
      gfx::Size content_bounds) const OVERRIDE;
  virtual const Region* GetInvalidation() OVERRIDE;
  virtual const PictureLayerTiling* GetTwinTiling(
      const PictureLayerTiling* tiling) OVERRIDE;

  // PushPropertiesTo active tree => pending tree.
  void SyncTiling(const PictureLayerTiling* tiling);

  // Mask-related functions
  void SetIsMask(bool is_mask);
  virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE;

  virtual size_t GPUMemoryUsageInBytes() const OVERRIDE;

 protected:
  PictureLayerImpl(LayerTreeImpl* tree_impl, int id);
  PictureLayerTiling* AddTiling(float contents_scale);
  void RemoveTiling(float contents_scale);
  void SyncFromActiveLayer(const PictureLayerImpl* other);
  void ManageTilings(bool animating_transform_to_screen);
  virtual bool ShouldAdjustRasterScale(
      bool animating_transform_to_screen) const;
  virtual void CalculateRasterContentsScale(
      bool animating_transform_to_screen,
      float* raster_contents_scale,
      float* low_res_raster_contents_scale) const;
  void CleanUpTilingsOnActiveLayer(
      std::vector<PictureLayerTiling*> used_tilings);
  float MinimumContentsScale() const;
  void UpdateLCDTextStatus(bool new_status);
  void ResetRasterScale();
  void MarkVisibleResourcesAsRequired() const;
  void DoPostCommitInitializationIfNeeded() {
    if (needs_post_commit_initialization_)
      DoPostCommitInitialization();
  }
  void DoPostCommitInitialization();

  bool CanHaveTilings() const;
  bool CanHaveTilingWithScale(float contents_scale) const;
  void SanityCheckTilingState() const;

  virtual void GetDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;
  virtual void AsValueInto(base::DictionaryValue* dict) const OVERRIDE;

  PictureLayerImpl* twin_layer_;

  scoped_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<PicturePileImpl> pile_;
  Region invalidation_;

  gfx::Transform last_screen_space_transform_;
  gfx::Size last_bounds_;
  float last_content_scale_;
  bool is_mask_;

  float ideal_page_scale_;
  float ideal_device_scale_;
  float ideal_source_scale_;
  float ideal_contents_scale_;

  float raster_page_scale_;
  float raster_device_scale_;
  float raster_source_scale_;
  float raster_contents_scale_;
  float low_res_raster_contents_scale_;

  bool raster_source_scale_was_animating_;
  bool is_using_lcd_text_;
  bool needs_post_commit_initialization_;
  // A sanity state check to make sure UpdateTilePriorities only gets called
  // after a CalculateContentsScale/ManageTilings.
  bool should_update_tile_priorities_;

  friend class PictureLayer;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
