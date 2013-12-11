// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_feature_type.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

namespace {

bool CanDoAcceleratedCompositing() {
  const GpuDataManager* manager = GpuDataManager::GetInstance();

  // Don't run the field trial if gpu access has been blocked or
  // accelerated compositing is blacklisted.
  if (!manager->GpuAccessAllowed(NULL) ||
      manager->IsFeatureBlacklisted(
          gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING))
    return false;

  // Check for SwiftShader.
  if (manager->ShouldUseSwiftShader())
    return false;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableAcceleratedCompositing))
    return false;

  return true;
}

bool IsForceCompositingModeBlacklisted() {
  return GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE);
}

}  // namespace

bool IsThreadedCompositingEnabled() {
#if defined(USE_AURA)
  // We always want threaded compositing on Aura.
  return true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over blacklist and field trials.
  if (command_line.HasSwitch(switches::kDisableForceCompositingMode) ||
      command_line.HasSwitch(switches::kDisableThreadedCompositing)) {
    return false;
  } else if (command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
    return true;
  }

  if (!CanDoAcceleratedCompositing() || IsForceCompositingModeBlacklisted())
    return false;

  base::FieldTrial* trial =
      base::FieldTrialList::Find(kGpuCompositingFieldTrialName);
  return trial &&
         trial->group_name() == kGpuCompositingFieldTrialThreadEnabledName;
}

bool IsForceCompositingModeEnabled() {
  // Force compositing mode is a subset of threaded compositing mode.
  if (IsThreadedCompositingEnabled())
    return true;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over blacklisting and field trials.
  if (command_line.HasSwitch(switches::kDisableForceCompositingMode))
    return false;
  else if (command_line.HasSwitch(switches::kForceCompositingMode))
    return true;

  if (!CanDoAcceleratedCompositing() || IsForceCompositingModeBlacklisted())
    return false;

  // Hardcode some platforms to use FCM, this has to be done here instead of via
  // the field trial so that this configuration is used on try bots as well.
  // TODO(gab): Do the same thing in IsThreadedCompositingEnabled() once this is
  // stable.
  // TODO(gab): Use the GPU blacklist instead of hardcoding OS versions here
  // https://codereview.chromium.org/23534006.
#if defined(OS_MACOSX)
  // Mac OSX 10.8+ has been shipping with FCM enabled at 100% since M28.
  return base::mac::IsOSMountainLionOrLater();
#elif defined(OS_WIN)
  // Windows Vista+ has been shipping with FCM enabled at 100% since M24.
  return base::win::GetVersion() >= base::win::VERSION_VISTA;
#endif

  return false;
}

bool IsDelegatedRendererEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool enabled = false;

  // Flags override.
  enabled |= command_line.HasSwitch(switches::kEnableDelegatedRenderer);
  enabled &= !command_line.HasSwitch(switches::kDisableDelegatedRenderer);

  // Needs compositing, and thread.
  if (enabled &&
      (!IsForceCompositingModeEnabled() || !IsThreadedCompositingEnabled())) {
    enabled = false;
    LOG(ERROR) << "Disabling delegated-rendering because it needs "
               << "force-compositing-mode and threaded-compositing.";
  }

  return enabled;
}

bool IsDeadlineSchedulingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Default to disabled.
  bool enabled = false;

  // Flags override.
  enabled |= command_line.HasSwitch(switches::kEnableDeadlineScheduling);
  enabled &= !command_line.HasSwitch(switches::kDisableDeadlineScheduling);

  return enabled;
}

}  // namespace content
