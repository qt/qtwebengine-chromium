// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_H_
#define CC_RESOURCES_TILE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/raster_mode.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class PicturePileImpl;

class CC_EXPORT Tile : public RefCountedManaged<Tile> {
 public:
  enum TileRasterFlags {
    USE_LCD_TEXT = 1 << 0,
    USE_GPU_RASTERIZATION = 1 << 1
  };

  typedef uint64 Id;

  Id id() const {
    return id_;
  }

  PicturePileImpl* picture_pile() {
    return picture_pile_.get();
  }

  const PicturePileImpl* picture_pile() const {
    return picture_pile_.get();
  }

  const TilePriority& priority(WhichTree tree) const {
    return priority_[tree];
  }

  TilePriority combined_priority() const {
    return TilePriority(priority_[ACTIVE_TREE],
                        priority_[PENDING_TREE]);
  }

  void SetPriority(WhichTree tree, const TilePriority& priority);

  void MarkRequiredForActivation();

  bool required_for_activation() const {
    return priority_[PENDING_TREE].required_for_activation;
  }

  void set_can_use_lcd_text(bool can_use_lcd_text) {
    if (can_use_lcd_text)
      flags_ |= USE_LCD_TEXT;
    else
      flags_ &= ~USE_LCD_TEXT;
  }

  bool can_use_lcd_text() const {
    return !!(flags_ & USE_LCD_TEXT);
  }

  void set_use_gpu_rasterization(bool use_gpu_rasterization) {
    if (use_gpu_rasterization)
      flags_ |= USE_GPU_RASTERIZATION;
    else
      flags_ &= ~USE_GPU_RASTERIZATION;
  }

  bool use_gpu_rasterization() const {
    return !!(flags_ & USE_GPU_RASTERIZATION);
  }

  scoped_ptr<base::Value> AsValue() const;

  inline bool IsReadyToDraw() const {
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (managed_state_.tile_versions[mode].IsReadyToDraw())
        return true;
    }
    return false;
  }

  const ManagedTileState::TileVersion& GetTileVersionForDrawing() const {
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (managed_state_.tile_versions[mode].IsReadyToDraw())
        return managed_state_.tile_versions[mode];
    }
    return managed_state_.tile_versions[HIGH_QUALITY_RASTER_MODE];
  }

  gfx::Rect opaque_rect() const { return opaque_rect_; }
  bool has_text(RasterMode mode) const {
    return managed_state_.tile_versions[mode].has_text_;
  }

  float contents_scale() const { return contents_scale_; }
  gfx::Rect content_rect() const { return content_rect_; }

  int layer_id() const { return layer_id_; }

  int source_frame_number() const { return source_frame_number_; }

  void set_picture_pile(scoped_refptr<PicturePileImpl> pile) {
    DCHECK(pile->CanRaster(contents_scale_, content_rect_));
    picture_pile_ = pile;
  }

  size_t GPUMemoryUsageInBytes() const;

  RasterMode GetRasterModeForTesting() const {
    return managed_state().raster_mode;
  }
  ManagedTileState::TileVersion& GetTileVersionForTesting(RasterMode mode) {
    return managed_state_.tile_versions[mode];
  }

  gfx::Size size() const { return tile_size_.size(); }

 private:
  friend class TileManager;
  friend class PrioritizedTileSet;
  friend class FakeTileManager;
  friend class BinComparator;
  friend class FakePictureLayerImpl;

  // Methods called by by tile manager.
  Tile(TileManager* tile_manager,
       PicturePileImpl* picture_pile,
       gfx::Size tile_size,
       gfx::Rect content_rect,
       gfx::Rect opaque_rect,
       float contents_scale,
       int layer_id,
       int source_frame_number,
       int flags);
  ~Tile();

  ManagedTileState& managed_state() { return managed_state_; }
  const ManagedTileState& managed_state() const { return managed_state_; }

  TileManager* tile_manager_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect tile_size_;
  gfx::Rect content_rect_;
  float contents_scale_;
  gfx::Rect opaque_rect_;

  TilePriority priority_[NUM_TREES];
  ManagedTileState managed_state_;
  int layer_id_;
  int source_frame_number_;
  int flags_;

  Id id_;
  static Id s_next_id_;

  DISALLOW_COPY_AND_ASSIGN(Tile);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_H_
