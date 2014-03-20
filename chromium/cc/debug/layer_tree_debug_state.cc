// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/layer_tree_debug_state.h"

#include "base/logging.h"

namespace cc {

// IMPORTANT: new fields must be added to Equal() and Unite()
LayerTreeDebugState::LayerTreeDebugState()
    : show_fps_counter(false),
      show_debug_borders(false),
      continuous_painting(false),
      show_paint_rects(false),
      show_property_changed_rects(false),
      show_surface_damage_rects(false),
      show_screen_space_rects(false),
      show_replica_screen_space_rects(false),
      show_occluding_rects(false),
      show_non_occluding_rects(false),
      show_touch_event_handler_rects(false),
      show_wheel_event_handler_rects(false),
      show_non_fast_scrollable_rects(false),
      show_layer_animation_bounds_rects(false),
      slow_down_raster_scale_factor(0),
      rasterize_only_visible_content(false),
      show_picture_borders(false),
      record_rendering_stats_(false) {}

LayerTreeDebugState::~LayerTreeDebugState() {}

void LayerTreeDebugState::SetRecordRenderingStats(bool enabled) {
  record_rendering_stats_ = enabled;
}

bool LayerTreeDebugState::RecordRenderingStats() const {
  return record_rendering_stats_ || continuous_painting;
}

bool LayerTreeDebugState::ShowHudInfo() const {
  return show_fps_counter || continuous_painting ||
         ShowHudRects();
}

bool LayerTreeDebugState::ShowHudRects() const {
  return show_paint_rects || show_property_changed_rects ||
         show_surface_damage_rects || show_screen_space_rects ||
         show_replica_screen_space_rects || show_occluding_rects ||
         show_non_occluding_rects || show_touch_event_handler_rects ||
         show_wheel_event_handler_rects || show_non_fast_scrollable_rects ||
         show_layer_animation_bounds_rects;
}

bool LayerTreeDebugState::ShowMemoryStats() const {
  return show_fps_counter || continuous_painting;
}

bool LayerTreeDebugState::Equal(const LayerTreeDebugState& a,
                                const LayerTreeDebugState& b) {
  return (a.show_fps_counter == b.show_fps_counter &&
          a.show_debug_borders == b.show_debug_borders &&
          a.continuous_painting == b.continuous_painting &&
          a.show_paint_rects == b.show_paint_rects &&
          a.show_property_changed_rects == b.show_property_changed_rects &&
          a.show_surface_damage_rects == b.show_surface_damage_rects &&
          a.show_screen_space_rects == b.show_screen_space_rects &&
          a.show_replica_screen_space_rects ==
          b.show_replica_screen_space_rects &&
          a.show_occluding_rects == b.show_occluding_rects &&
          a.show_non_occluding_rects == b.show_non_occluding_rects &&
          a.show_touch_event_handler_rects ==
          b.show_touch_event_handler_rects &&
          a.show_wheel_event_handler_rects ==
          b.show_wheel_event_handler_rects &&
          a.show_non_fast_scrollable_rects ==
          b.show_non_fast_scrollable_rects &&
          a.show_layer_animation_bounds_rects ==
          b.show_layer_animation_bounds_rects &&
          a.slow_down_raster_scale_factor == b.slow_down_raster_scale_factor &&
          a.rasterize_only_visible_content ==
          b.rasterize_only_visible_content &&
          a.show_picture_borders == b.show_picture_borders &&
          a.record_rendering_stats_ == b.record_rendering_stats_);
}

LayerTreeDebugState LayerTreeDebugState::Unite(const LayerTreeDebugState& a,
                                               const LayerTreeDebugState& b) {
  LayerTreeDebugState r(a);

  r.show_fps_counter |= b.show_fps_counter;
  r.show_debug_borders |= b.show_debug_borders;
  r.continuous_painting |= b.continuous_painting;

  r.show_paint_rects |= b.show_paint_rects;
  r.show_property_changed_rects |= b.show_property_changed_rects;
  r.show_surface_damage_rects |= b.show_surface_damage_rects;
  r.show_screen_space_rects |= b.show_screen_space_rects;
  r.show_replica_screen_space_rects |= b.show_replica_screen_space_rects;
  r.show_occluding_rects |= b.show_occluding_rects;
  r.show_non_occluding_rects |= b.show_non_occluding_rects;
  r.show_touch_event_handler_rects |= b.show_touch_event_handler_rects;
  r.show_wheel_event_handler_rects |= b.show_wheel_event_handler_rects;
  r.show_non_fast_scrollable_rects |= b.show_non_fast_scrollable_rects;
  r.show_layer_animation_bounds_rects |= b.show_layer_animation_bounds_rects;

  if (b.slow_down_raster_scale_factor)
    r.slow_down_raster_scale_factor = b.slow_down_raster_scale_factor;
  r.rasterize_only_visible_content |= b.rasterize_only_visible_content;
  r.show_picture_borders |= b.show_picture_borders;

  r.record_rendering_stats_ |= b.record_rendering_stats_;

  return r;
}

}  // namespace cc
