// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/webpreferences.h"

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/icu/source/common/unicode/uchar.h"

using WebKit::WebSettings;

WebPreferences::WebPreferences()
    : default_font_size(16),
      default_fixed_font_size(13),
      minimum_font_size(0),
      minimum_logical_font_size(6),
      default_encoding("ISO-8859-1"),
      javascript_enabled(true),
      web_security_enabled(true),
      javascript_can_open_windows_automatically(true),
      loads_images_automatically(true),
      images_enabled(true),
      plugins_enabled(true),
      dom_paste_enabled(false),  // enables execCommand("paste")
      site_specific_quirks_enabled(false),
      shrinks_standalone_images_to_fit(true),
      uses_universal_detector(false),  // Disabled: page cycler regression
      text_areas_are_resizable(true),
      java_enabled(true),
      allow_scripts_to_close_windows(false),
      remote_fonts_enabled(true),
      javascript_can_access_clipboard(false),
      xss_auditor_enabled(true),
      dns_prefetching_enabled(true),
      local_storage_enabled(false),
      databases_enabled(false),
      application_cache_enabled(false),
      tabs_to_links(true),
      caret_browsing_enabled(false),
      hyperlink_auditing_enabled(true),
      is_online(true),
      user_style_sheet_enabled(false),
      author_and_user_styles_enabled(true),
      allow_universal_access_from_file_urls(false),
      allow_file_access_from_file_urls(false),
      webaudio_enabled(false),
      experimental_webgl_enabled(false),
      flash_3d_enabled(true),
      flash_stage3d_enabled(false),
      flash_stage3d_baseline_enabled(false),
      gl_multisampling_enabled(true),
      privileged_webgl_extensions_enabled(false),
      webgl_errors_to_console_enabled(true),
      accelerated_compositing_for_overflow_scroll_enabled(false),
      accelerated_compositing_for_scrollable_frames_enabled(false),
      composited_scrolling_for_frames_enabled(false),
      mock_scrollbars_enabled(false),
      threaded_html_parser(true),
      show_paint_rects(false),
      asynchronous_spell_checking_enabled(true),
      unified_textchecker_enabled(false),
      accelerated_compositing_enabled(false),
      force_compositing_mode(false),
      accelerated_compositing_for_3d_transforms_enabled(false),
      accelerated_compositing_for_animation_enabled(false),
      accelerated_compositing_for_video_enabled(false),
      accelerated_2d_canvas_enabled(false),
      minimum_accelerated_2d_canvas_size(257 * 256),
      antialiased_2d_canvas_disabled(false),
      accelerated_filters_enabled(false),
      gesture_tap_highlight_enabled(false),
      accelerated_compositing_for_plugins_enabled(false),
      memory_info_enabled(false),
      fullscreen_enabled(false),
      allow_displaying_insecure_content(true),
      allow_running_insecure_content(false),
      password_echo_enabled(false),
      should_print_backgrounds(false),
      enable_scroll_animator(false),
      visual_word_movement_enabled(false),
      css_sticky_position_enabled(false),
      css_shaders_enabled(false),
      lazy_layout_enabled(false),
      region_based_columns_enabled(false),
      touch_enabled(false),
      device_supports_touch(false),
      device_supports_mouse(true),
      touch_adjustment_enabled(true),
      fixed_position_creates_stacking_context(false),
      sync_xhr_in_documents_enabled(true),
      deferred_image_decoding_enabled(false),
      should_respect_image_orientation(false),
      number_of_cpu_cores(1),
#if defined(OS_MACOSX)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_MAC),
#elif defined(OS_WIN)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_WIN),
#elif defined(OS_ANDROID)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_ANDROID),
#elif defined(OS_POSIX)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_UNIX),
#else
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_MAC),
#endif
      supports_multiple_windows(true),
      viewport_enabled(false),
      initialize_at_minimum_page_scale(true),
#if defined(OS_MACOSX)
      smart_insert_delete_enabled(true),
#else
      smart_insert_delete_enabled(false),
#endif
      spatial_navigation_enabled(false),
      experimental_websocket_enabled(false),
      pinch_virtual_viewport_enabled(false),
      pinch_overlay_scrollbar_thickness(0),
      use_solid_color_scrollbars(false),
      cookie_enabled(true)
#if defined(OS_ANDROID)
      ,
      text_autosizing_enabled(true),
      font_scale_factor(1.0f),
      force_enable_zoom(false),
      double_tap_to_zoom_enabled(true),
      user_gesture_required_for_media_playback(true),
      user_gesture_required_for_media_fullscreen(true),
      support_deprecated_target_density_dpi(false),
      use_legacy_background_size_shorthand_behavior(false),
      wide_viewport_quirk(false),
      use_wide_viewport(true),
      viewport_meta_layout_size_quirk(false),
      viewport_meta_zero_values_quirk(false)
#endif
{
  standard_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  fixed_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Courier New");
  serif_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  sans_serif_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Arial");
  cursive_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Script");
  fantasy_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Impact");
  pictograph_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
}

WebPreferences::~WebPreferences() {
}

namespace webkit_glue {

// "Zyyy" is the ISO 15924 script code for undetermined script aka Common.
const char kCommonScript[] = "Zyyy";

#define COMPILE_ASSERT_MATCHING_ENUMS(webkit_glue_name, webkit_name)         \
    COMPILE_ASSERT(                                                          \
        static_cast<int>(webkit_glue_name) == static_cast<int>(webkit_name), \
        mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_MAC, WebSettings::EditingBehaviorMac);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_WIN, WebSettings::EditingBehaviorWin);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_UNIX, WebSettings::EditingBehaviorUnix);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_ANDROID, WebSettings::EditingBehaviorAndroid);

}  // namespace webkit_glue
