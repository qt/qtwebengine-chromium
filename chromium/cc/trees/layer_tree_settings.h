// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  ~LayerTreeSettings();

  bool impl_side_painting;
  bool allow_antialiasing;
  bool throttle_frame_production;
  bool begin_impl_frame_scheduling_enabled;
  bool deadline_scheduling_enabled;
  bool using_synchronous_renderer_compositor;
  bool per_tile_painting_enabled;
  bool partial_swap_enabled;
  bool accelerated_animation_enabled;
  bool background_color_instead_of_checkerboard;
  bool show_overdraw_in_tracing;
  bool can_use_lcd_text;
  bool should_clear_root_render_pass;
  bool gpu_rasterization;

  enum ScrollbarAnimator {
    NoAnimator,
    LinearFade,
    Thinning,
  };
  ScrollbarAnimator scrollbar_animator;
  int scrollbar_linear_fade_delay_ms;
  int scrollbar_linear_fade_length_ms;
  SkColor solid_color_scrollbar_color;
  bool calculate_top_controls_position;
  bool use_memory_management;
  bool timeout_and_draw_when_animation_checkerboards;
  int maximum_number_of_failed_draws_before_draw_is_forced_;
  bool layer_transforms_should_scale_layer_contents;
  float minimum_contents_scale;
  float low_res_contents_scale_factor;
  float top_controls_height;
  float top_controls_show_threshold;
  float top_controls_hide_threshold;
  double refresh_rate;
  size_t max_partial_texture_updates;
  size_t num_raster_threads;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size minimum_occlusion_tracking_size;
  bool use_pinch_zoom_scrollbars;
  bool use_pinch_virtual_viewport;
  size_t max_tiles_for_interest_area;
  size_t max_unused_resource_memory_percentage;
  int highp_threshold_min;
  bool strict_layer_property_change_checking;
  bool use_map_image;
  bool ignore_root_layer_flings;
  bool use_rgba_4444_textures;
  bool always_overscroll;
  bool touch_hit_testing;
  size_t texture_id_allocation_chunk_size;

  LayerTreeDebugState initial_debug_state;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_
