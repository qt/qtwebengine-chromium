// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "media/base/android/media_codec_bridge.h"
#endif

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
#if !defined(GOOGLE_TV)
  // MSE/EME implementation needs Android MediaCodec API.
  if (!media::MediaCodecBridge::IsAvailable()) {
    WebRuntimeFeatures::enableWebKitMediaSource(false);
    WebRuntimeFeatures::enableMediaSource(false);
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);
  }
#endif  // !defined(GOOGLE_TV)
  // WebAudio is enabled by default only on ARM and only when the
  // MediaCodec API is available.
  WebRuntimeFeatures::enableWebAudio(
      media::MediaCodecBridge::IsAvailable() &&
      (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM));
  // Android does not support the Gamepad API.
  WebRuntimeFeatures::enableGamepad(false);
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // Android does not yet support the Web Notification API. crbug.com/115320
  WebRuntimeFeatures::enableNotifications(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::enableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::enableNavigatorContentUtils(false);
#endif  // defined(OS_ANDROID)
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const CommandLine& command_line) {
  WebRuntimeFeatures::enableStableFeatures(true);

  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableApplicationCache))
    WebRuntimeFeatures::enableApplicationCache(false);

  if (command_line.HasSwitch(switches::kDisableDesktopNotifications))
    WebRuntimeFeatures::enableNotifications(false);

  if (command_line.HasSwitch(switches::kDisableNavigatorContentUtils))
    WebRuntimeFeatures::enableNavigatorContentUtils(false);

  if (command_line.HasSwitch(switches::kDisableLocalStorage))
    WebRuntimeFeatures::enableLocalStorage(false);

  if (command_line.HasSwitch(switches::kDisableSessionStorage))
    WebRuntimeFeatures::enableSessionStorage(false);

  if (command_line.HasSwitch(switches::kDisableGeolocation))
    WebRuntimeFeatures::enableGeolocation(false);

  if (command_line.HasSwitch(switches::kDisableWebKitMediaSource))
    WebRuntimeFeatures::enableWebKitMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableUnprefixedMediaSource))
    WebRuntimeFeatures::enableMediaSource(false);

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableWebRTC)) {
    WebRuntimeFeatures::enableMediaStream(false);
    WebRuntimeFeatures::enablePeerConnection(false);
  }

  if (!command_line.HasSwitch(switches::kEnableSpeechRecognition))
    WebRuntimeFeatures::enableScriptedSpeech(false);
#endif

  if (command_line.HasSwitch(switches::kEnableServiceWorker))
    WebRuntimeFeatures::enableServiceWorker(true);

#if defined(OS_ANDROID)
  // WebAudio requires the MediaCodec API.
#if defined(ARCH_CPU_X86)
  // WebAudio is disabled by default on x86.
  WebRuntimeFeatures::enableWebAudio(
      command_line.HasSwitch(switches::kEnableWebAudio) &&
      media::MediaCodecBridge::IsAvailable());
#elif defined(ARCH_CPU_ARMEL)
  // WebAudio is enabled by default on ARM.
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio) &&
      media::MediaCodecBridge::IsAvailable());
#else
  WebRuntimeFeatures::enableWebAudio(false);
#endif
#else
  if (command_line.HasSwitch(switches::kDisableWebAudio))
    WebRuntimeFeatures::enableWebAudio(false);
#endif

  if (command_line.HasSwitch(switches::kDisableFullScreen))
    WebRuntimeFeatures::enableFullscreen(false);

  if (command_line.HasSwitch(switches::kEnableEncryptedMedia))
    WebRuntimeFeatures::enableEncryptedMedia(true);

  if (command_line.HasSwitch(switches::kDisablePrefixedEncryptedMedia))
    WebRuntimeFeatures::enablePrefixedEncryptedMedia(false);

  // FIXME: Remove the enable switch once Web Animations CSS is enabled by
  // default in Blink.
  if (command_line.HasSwitch(switches::kEnableWebAnimationsCSS))
    WebRuntimeFeatures::enableWebAnimationsCSS(true);
  else if (command_line.HasSwitch(switches::kDisableWebAnimationsCSS))
    WebRuntimeFeatures::enableWebAnimationsCSS(false);

  if (command_line.HasSwitch(switches::kEnableWebAnimationsSVG))
    WebRuntimeFeatures::enableWebAnimationsSVG(true);

  if (command_line.HasSwitch(switches::kEnableWebMIDI))
    WebRuntimeFeatures::enableWebMIDI(true);

  if (command_line.HasSwitch(switches::kDisableDeviceMotion))
    WebRuntimeFeatures::enableDeviceMotion(false);

  if (command_line.HasSwitch(switches::kDisableDeviceOrientation))
    WebRuntimeFeatures::enableDeviceOrientation(false);

  if (command_line.HasSwitch(switches::kDisableSpeechInput))
    WebRuntimeFeatures::enableSpeechInput(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::enableFileSystem(false);

#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kEnableDirectWrite))
    WebRuntimeFeatures::enableDirectWrite(true);
#endif

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::enableExperimentalCanvasFeatures(true);

  if (command_line.HasSwitch(switches::kEnableSpeechSynthesis))
    WebRuntimeFeatures::enableSpeechSynthesis(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableHTMLImports))
    WebRuntimeFeatures::enableHTMLImports(true);

  if (command_line.HasSwitch(switches::kEnableOverlayFullscreenVideo))
    WebRuntimeFeatures::enableOverlayFullscreenVideo(true);

  if (command_line.HasSwitch(switches::kEnableOverlayScrollbars))
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnableInputModeAttribute))
    WebRuntimeFeatures::enableInputModeAttribute(true);

  if (command_line.HasSwitch(switches::kEnableFastTextAutosizing))
    WebRuntimeFeatures::enableFastTextAutosizing(true);

  if (command_line.HasSwitch(switches::kEnableRepaintAfterLayout))
    WebRuntimeFeatures::enableRepaintAfterLayout(true);
}

}  // namespace content
