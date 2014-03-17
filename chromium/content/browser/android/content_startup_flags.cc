// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_startup_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "cc/base/switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/base/ui_base_switches.h"

namespace content {

void SetContentCommandLineFlags(int max_render_process_count,
                                const std::string& plugin_descriptor) {
  // May be called multiple times, to cover all possible program entry points.
  static bool already_initialized = false;
  if (already_initialized)
    return;
  already_initialized = true;

  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();

  if (parsed_command_line->HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit = parsed_command_line->GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    int value;
    if (base::StringToInt(limit, &value))
      max_render_process_count = value;
  }

  if (max_render_process_count <= 0) {
    // Need to ensure the command line flag is consistent as a lot of chrome
    // internal code checks this directly, but it wouldn't normally get set when
    // we are implementing an embedded WebView.
    parsed_command_line->AppendSwitch(switches::kSingleProcess);
  } else {
    max_render_process_count =
        std::min(max_render_process_count,
                 static_cast<int>(content::kMaxRendererProcessCount));
    content::RenderProcessHost::SetMaxRendererProcessCount(
        max_render_process_count);
  }

  parsed_command_line->AppendSwitch(switches::kForceCompositingMode);
  parsed_command_line->AppendSwitch(switches::kAllowWebUICompositing);
  parsed_command_line->AppendSwitch(switches::kEnableThreadedCompositing);
  parsed_command_line->AppendSwitch(
      switches::kEnableCompositingForFixedPosition);
  parsed_command_line->AppendSwitch(switches::kEnableAcceleratedOverflowScroll);
  parsed_command_line->AppendSwitch(
      switches::kEnableAcceleratedScrollableFrames);
  parsed_command_line->AppendSwitch(
      switches::kEnableCompositedScrollingForFrames);
  parsed_command_line->AppendSwitch(switches::kEnableBeginFrameScheduling);
  parsed_command_line->AppendSwitch(switches::kEnableDeadlineScheduling);

  parsed_command_line->AppendSwitch(switches::kDisableGestureDebounce);
  parsed_command_line->AppendSwitch(switches::kEnableGestureTapHighlight);
  parsed_command_line->AppendSwitch(switches::kEnablePinch);
  parsed_command_line->AppendSwitch(switches::kEnableOverlayFullscreenVideo);
  parsed_command_line->AppendSwitch(switches::kEnableOverlayScrollbars);
  parsed_command_line->AppendSwitch(switches::kEnableOverscrollNotifications);
  parsed_command_line->AppendSwitchASCII(switches::kTouchAckTimeoutDelayMs,
                                         "200");

  // Run the GPU service as a thread in the browser instead of as a
  // standalone process.
  parsed_command_line->AppendSwitch(switches::kInProcessGPU);
  parsed_command_line->AppendSwitch(switches::kDisableGpuShaderDiskCache);

  parsed_command_line->AppendSwitch(switches::kEnableViewport);
  parsed_command_line->AppendSwitch(switches::kEnableViewportMeta);
  parsed_command_line->AppendSwitch(
      switches::kMainFrameResizesAreOrientationChanges);

  // Disable anti-aliasing.
  parsed_command_line->AppendSwitch(
      cc::switches::kDisableCompositedAntialiasing);

  parsed_command_line->AppendSwitch(switches::kUIPrioritizeInGpuProcess);

  if (!plugin_descriptor.empty()) {
    parsed_command_line->AppendSwitchNative(
      switches::kRegisterPepperPlugins, plugin_descriptor);
  }

  // Disable profiler timing by default.
  if (!parsed_command_line->HasSwitch(switches::kProfilerTiming)) {
    parsed_command_line->AppendSwitchASCII(
        switches::kProfilerTiming, switches::kProfilerTimingDisabledValue);
  }
}

}  // namespace content
