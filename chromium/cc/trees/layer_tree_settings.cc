// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace cc {

LayerTreeSettings::LayerTreeSettings()
    : impl_side_painting(false),
      allow_antialiasing(true),
      throttle_frame_production(true),
      begin_impl_frame_scheduling_enabled(false),
      deadline_scheduling_enabled(true),
      using_synchronous_renderer_compositor(false),
      per_tile_painting_enabled(false),
      partial_swap_enabled(false),
      accelerated_animation_enabled(true),
      background_color_instead_of_checkerboard(false),
      show_overdraw_in_tracing(false),
      can_use_lcd_text(true),
      should_clear_root_render_pass(true),
      gpu_rasterization(false),
      scrollbar_animator(NoAnimator),
      scrollbar_linear_fade_delay_ms(300),
      scrollbar_linear_fade_length_ms(300),
      solid_color_scrollbar_color(SK_ColorWHITE),
      calculate_top_controls_position(false),
      use_memory_management(true),
      timeout_and_draw_when_animation_checkerboards(true),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      layer_transforms_should_scale_layer_contents(false),
      minimum_contents_scale(0.0625f),
      low_res_contents_scale_factor(0.25f),
      top_controls_height(0.f),
      top_controls_show_threshold(0.5f),
      top_controls_hide_threshold(0.5f),
      refresh_rate(60.0),
      max_partial_texture_updates(std::numeric_limits<size_t>::max()),
      num_raster_threads(1),
      default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      use_pinch_zoom_scrollbars(false),
      use_pinch_virtual_viewport(false),
      // At 256x256 tiles, 128 tiles cover an area of 2048x4096 pixels.
      max_tiles_for_interest_area(128),
      max_unused_resource_memory_percentage(100),
      highp_threshold_min(0),
      strict_layer_property_change_checking(false),
      use_map_image(false),
      ignore_root_layer_flings(false),
      use_rgba_4444_textures(false),
      always_overscroll(false),
      touch_hit_testing(true),
      texture_id_allocation_chunk_size(64) {
}

LayerTreeSettings::~LayerTreeSettings() {}

}  // namespace cc
