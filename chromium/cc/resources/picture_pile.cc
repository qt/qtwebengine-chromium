// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_pile.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "cc/base/region.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/tile_priority.h"

namespace {
// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;

// TODO(humper): The density threshold here is somewhat arbitrary; need a
// way to set // this from the command line so we can write a benchmark
// script and find a sweet spot.
const float kDensityThreshold = 0.5f;

bool rect_sort_y(const gfx::Rect &r1, const gfx::Rect &r2) {
  return r1.y() < r2.y() || (r1.y() == r2.y() && r1.x() < r2.x());
}

bool rect_sort_x(const gfx::Rect &r1, const gfx::Rect &r2) {
  return r1.x() < r2.x() || (r1.x() == r2.x() && r1.y() < r2.y());
}

float do_clustering(const std::vector<gfx::Rect>& tiles,
                    std::vector<gfx::Rect>* clustered_rects) {
  // These variables track the record area and invalid area
  // for the entire clustering
  int total_record_area = 0;
  int total_invalid_area = 0;

  // These variables track the record area and invalid area
  // for the current cluster being constructed.
  gfx::Rect cur_record_rect;
  int cluster_record_area = 0, cluster_invalid_area = 0;

  for (std::vector<gfx::Rect>::const_iterator it = tiles.begin();
        it != tiles.end();
        it++) {
    gfx::Rect invalid_tile = *it;

    // For each tile, we consider adding the invalid tile to the
    // current record rectangle.  Only add it if the amount of empty
    // space created is below a density threshold.
    int tile_area = invalid_tile.width() * invalid_tile.height();

    gfx::Rect proposed_union = cur_record_rect;
    proposed_union.Union(invalid_tile);
    int proposed_area = proposed_union.width() * proposed_union.height();
    float proposed_density =
      static_cast<float>(cluster_invalid_area + tile_area) /
      static_cast<float>(proposed_area);

    if (proposed_density >= kDensityThreshold) {
      // It's okay to add this invalid tile to the
      // current recording rectangle.
      cur_record_rect = proposed_union;
      cluster_record_area = proposed_area;
      cluster_invalid_area += tile_area;
      total_invalid_area += tile_area;
    } else {
      // Adding this invalid tile to the current recording rectangle
      // would exceed our badness threshold, so put the current rectangle
      // in the list of recording rects, and start a new one.
      clustered_rects->push_back(cur_record_rect);
      total_record_area += cluster_record_area;
      cur_record_rect = invalid_tile;
      cluster_invalid_area = tile_area;
      cluster_record_area = tile_area;
    }
  }

  DCHECK(!cur_record_rect.IsEmpty());
  clustered_rects->push_back(cur_record_rect);
  total_record_area += cluster_record_area;;

  DCHECK_NE(total_record_area, 0);

  return static_cast<float>(total_invalid_area) /
         static_cast<float>(total_record_area);
  }

float ClusterTiles(const std::vector<gfx::Rect>& invalid_tiles,
                   std::vector<gfx::Rect>* record_rects) {
  TRACE_EVENT1("cc", "ClusterTiles",
               "count",
               invalid_tiles.size());

  if (invalid_tiles.size() <= 1) {
    // Quickly handle the special case for common
    // single-invalidation update, and also the less common
    // case of no tiles passed in.
    *record_rects = invalid_tiles;
    return 1;
  }

  // Sort the invalid tiles by y coordinate.
  std::vector<gfx::Rect> invalid_tiles_vertical = invalid_tiles;
  std::sort(invalid_tiles_vertical.begin(),
            invalid_tiles_vertical.end(),
            rect_sort_y);

  float vertical_density;
  std::vector<gfx::Rect> vertical_clustering;
  vertical_density = do_clustering(invalid_tiles_vertical,
                                   &vertical_clustering);

  // Now try again with a horizontal sort, see which one is best
  // TODO(humper): Heuristics for skipping this step?
  std::vector<gfx::Rect> invalid_tiles_horizontal = invalid_tiles;
  std::sort(invalid_tiles_vertical.begin(),
            invalid_tiles_vertical.end(),
            rect_sort_x);

  float horizontal_density;
  std::vector<gfx::Rect> horizontal_clustering;
  horizontal_density = do_clustering(invalid_tiles_vertical,
                                     &horizontal_clustering);

  if (vertical_density < horizontal_density) {
    *record_rects = horizontal_clustering;
    return horizontal_density;
  }

  *record_rects = vertical_clustering;
  return vertical_density;
}

}  // namespace

namespace cc {

PicturePile::PicturePile() {
}

PicturePile::~PicturePile() {
}

bool PicturePile::Update(
    ContentLayerClient* painter,
    SkColor background_color,
    bool contents_opaque,
    const Region& invalidation,
    gfx::Rect visible_layer_rect,
    int frame_number,
    RenderingStatsInstrumentation* stats_instrumentation) {
  background_color_ = background_color;
  contents_opaque_ = contents_opaque;

  gfx::Rect interest_rect = visible_layer_rect;
  interest_rect.Inset(
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord);

  bool invalidated = false;
  for (Region::Iterator i(invalidation); i.has_rect(); i.next()) {
    gfx::Rect invalidation = i.rect();
    // Split this inflated invalidation across tile boundaries and apply it
    // to all tiles that it touches.
    for (TilingData::Iterator iter(&tiling_, invalidation);
         iter; ++iter) {
      const PictureMapKey& key = iter.index();

      PictureMap::iterator picture_it = picture_map_.find(key);
      if (picture_it == picture_map_.end())
        continue;

      // Inform the grid cell that it has been invalidated in this frame.
      invalidated = picture_it->second.Invalidate(frame_number) || invalidated;
    }
  }

  // Make a list of all invalid tiles; we will attempt to
  // cluster these into multiple invalidation regions.
  std::vector<gfx::Rect> invalid_tiles;

  for (TilingData::Iterator it(&tiling_, interest_rect);
       it; ++it) {
    const PictureMapKey& key = it.index();
    PictureInfo& info = picture_map_[key];

    gfx::Rect rect = PaddedRect(key);
    int distance_to_visible =
        rect.ManhattanInternalDistance(visible_layer_rect);

    if (info.NeedsRecording(frame_number, distance_to_visible)) {
      gfx::Rect tile = tiling_.TileBounds(key.first, key.second);
      invalid_tiles.push_back(tile);
    }
  }

  std::vector<gfx::Rect> record_rects;
  ClusterTiles(invalid_tiles, &record_rects);

  if (record_rects.empty()) {
    if (invalidated)
      UpdateRecordedRegion();
    return invalidated;
  }

  for (std::vector<gfx::Rect>::iterator it = record_rects.begin();
       it != record_rects.end();
       it++) {
    gfx::Rect record_rect = *it;
    record_rect = PadRect(record_rect);

    int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
    scoped_refptr<Picture> picture = Picture::Create(record_rect);

    {
      base::TimeDelta best_duration = base::TimeDelta::FromInternalValue(
          std::numeric_limits<int64>::max());
      for (int i = 0; i < repeat_count; i++) {
        base::TimeTicks start_time = stats_instrumentation->StartRecording();
        picture->Record(painter, tile_grid_info_);
        base::TimeDelta duration =
            stats_instrumentation->EndRecording(start_time);
        best_duration = std::min(duration, best_duration);
      }
      int recorded_pixel_count =
          picture->LayerRect().width() * picture->LayerRect().height();
      stats_instrumentation->AddRecord(best_duration, recorded_pixel_count);
      if (num_raster_threads_ > 1)
        picture->GatherPixelRefs(tile_grid_info_);
      picture->CloneForDrawing(num_raster_threads_);
    }

    for (TilingData::Iterator it(&tiling_, record_rect);
        it; ++it) {
      const PictureMapKey& key = it.index();
      gfx::Rect tile = PaddedRect(key);
      if (record_rect.Contains(tile)) {
        PictureInfo& info = picture_map_[key];
        info.SetPicture(picture);
      }
    }
  }

  UpdateRecordedRegion();
  return true;
}

}  // namespace cc
