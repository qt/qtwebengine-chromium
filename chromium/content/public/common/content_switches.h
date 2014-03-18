// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const char kAcceleratedCanvas2dMSAASampleCount[];
CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowNoSandboxJob[];
extern const char kAllowSandboxDebugging[];
CONTENT_EXPORT extern const char kAllowWebUICompositing[];
extern const char kAuditAllHandles[];
extern const char kAuditHandles[];
CONTENT_EXPORT extern const char kBlacklistAcceleratedCompositing[];
CONTENT_EXPORT extern const char kBlacklistWebGL[];
CONTENT_EXPORT extern const char kBlockCrossSiteDocuments[];
CONTENT_EXPORT extern const char kBrowserAssertTest[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
extern const char kDebugPluginLoading[];
CONTENT_EXPORT extern const char kDefaultTileWidth[];
CONTENT_EXPORT extern const char kDefaultTileHeight[];
CONTENT_EXPORT extern const char kDisable2dCanvasAntialiasing[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableAcceleratedCompositing[];
CONTENT_EXPORT extern const char kDisableAcceleratedFixedRootBackground[];
CONTENT_EXPORT extern const char kDisableAcceleratedLayers[];
CONTENT_EXPORT extern const char kDisableAcceleratedOverflowScroll[];
CONTENT_EXPORT extern const char kDisableLayerSquashing[];
CONTENT_EXPORT extern const char kDisableAcceleratedPlugins[];
CONTENT_EXPORT extern const char kDisableAcceleratedScrollableFrames[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideo[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
CONTENT_EXPORT extern const char kDisableAltWinstation[];
CONTENT_EXPORT extern const char kDisableApplicationCache[];
CONTENT_EXPORT extern const char kDisableAudio[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisableBrowserPluginCompositing[];
CONTENT_EXPORT extern const char kDisableCompositedScrollingForFrames[];
CONTENT_EXPORT extern const char kDisableCompositingForFixedPosition[];
CONTENT_EXPORT extern const char kDisableCompositingForTransition[];
CONTENT_EXPORT extern const char kDisableDatabases[];
CONTENT_EXPORT extern const char kDisableDeadlineScheduling[];
CONTENT_EXPORT extern const char kDisableDelegatedRenderer[];
extern const char kDisableDesktopNotifications[];
CONTENT_EXPORT extern const char kDisableDeviceMotion[];
CONTENT_EXPORT extern const char kDisableDeviceOrientation[];
extern const char kDisableDirectNPAPIRequests[];
extern const char kDisableDomainBlockingFor3DAPIs[];
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
CONTENT_EXPORT extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableFiltersOverIPC[];
CONTENT_EXPORT extern const char kDisableFixedPositionCreatesStackingContext[];
CONTENT_EXPORT extern const char kDisableFlash3d[];
CONTENT_EXPORT extern const char kDisableFlashStage3d[];
CONTENT_EXPORT extern const char kDisableForceCompositingMode[];
CONTENT_EXPORT extern const char kDisableFullScreen[];
extern const char kDisableGeolocation[];
extern const char kDisableGestureDebounce[];
CONTENT_EXPORT extern const char kDisableGestureTapHighlight[];
CONTENT_EXPORT extern const char kDisableGLMultisampling[];
CONTENT_EXPORT extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGpuCompositing[];
extern const char kDisableGpuProcessCrashLimit[];
CONTENT_EXPORT extern const char kDisableGpuProcessPrelaunch[];
CONTENT_EXPORT extern const char kDisableGpuSandbox[];
CONTENT_EXPORT extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableHistogramCustomizer[];
extern const char kDisableImageTransportSurface[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
CONTENT_EXPORT extern const char kDisablePrefixedEncryptedMedia[];
extern const char kDisableKillAfterBadIPC[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
extern const char kDisableNavigatorContentUtils[];
extern const char kDisableP2PSocketSTUNFilter[];
extern const char kDisablePepper3d[];
CONTENT_EXPORT extern const char kDisablePinch[];
CONTENT_EXPORT extern const char kDisablePlugins[];
CONTENT_EXPORT extern const char kDisablePluginsDiscovery[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRendererAccessibility[];
CONTENT_EXPORT extern const char kDisableSeccompFilterSandbox[];
extern const char kDisableSessionStorage[];
CONTENT_EXPORT extern const char kDisableSetuidSandbox[];
CONTENT_EXPORT extern const char kDisableSharedWorkers[];
extern const char kDisableSiteSpecificQuirks[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
CONTENT_EXPORT extern const char kDisableSpeechInput[];
CONTENT_EXPORT extern const char kDisableSSLFalseStart[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
CONTENT_EXPORT extern const char kDisableThreadedHTMLParser[];
CONTENT_EXPORT extern const char kDisableUniversalAcceleratedOverflowScroll[];
CONTENT_EXPORT extern const char kDisableUnprefixedMediaSource[];
extern const char kDisableWebAnimationsCSS[];
CONTENT_EXPORT extern const char kDisableWebKitMediaSource[];
CONTENT_EXPORT extern const char kDisableWebSecurity[];
extern const char kDisableXSLT[];
extern const char kDisableXSSAuditor[];
CONTENT_EXPORT extern const char kDomAutomationController[];
CONTENT_EXPORT extern const char kEnableAcceleratedFilters[];
CONTENT_EXPORT extern const char kEnableAcceleratedFixedRootBackground[];
CONTENT_EXPORT extern const char kEnableAcceleratedOverflowScroll[];
CONTENT_EXPORT extern const char kEnableLayerSquashing[];
CONTENT_EXPORT extern const char kEnableAcceleratedScrollableFrames[];
extern const char kEnableAccessibilityLogging[];
CONTENT_EXPORT extern const char kEnableBeginFrameScheduling[];
CONTENT_EXPORT extern const char kEnableBrowserPluginForAllViewTypes[];
CONTENT_EXPORT extern const char kEnableBrowserPluginDragDrop[];
CONTENT_EXPORT extern const char kEnableCompositedScrollingForFrames[];
CONTENT_EXPORT extern const char kEnableCompositingForFixedPosition[];
CONTENT_EXPORT extern const char kEnableCompositingForTransition[];
CONTENT_EXPORT extern const char kEnableDeferredImageDecoding[];
CONTENT_EXPORT extern const char kEnableDeadlineScheduling[];
CONTENT_EXPORT extern const char kEnableDelegatedRenderer[];
CONTENT_EXPORT extern const char kEnableDownloadResumption[];
CONTENT_EXPORT extern const char kEnableEncryptedMedia[];
CONTENT_EXPORT extern const char kEnableExperimentalCanvasFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebPlatformFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebSocket[];
extern const char kEnableFastTextAutosizing[];
CONTENT_EXPORT extern const char kEnableFixedPositionCreatesStackingContext[];
CONTENT_EXPORT extern const char kEnableGestureTapHighlight[];
extern const char kEnableGpuBenchmarking[];
extern const char kEnableGpuClientTracing[];
CONTENT_EXPORT extern const char kEnableHighDpiCompositingForFixedPosition[];
#if defined(OS_WIN)
CONTENT_EXPORT extern const char kEnableDirectWrite[];
CONTENT_EXPORT extern const char kEnableHighResolutionTime[];
#endif
CONTENT_EXPORT extern const char kEnableHTMLImports[];
CONTENT_EXPORT extern const char kEnableInbandTextTracks[];
extern const char kEnableInputModeAttribute[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMemoryBenchmarking[];
extern const char kEnableMonitorProfile[];
CONTENT_EXPORT extern const char kEnableOfflineCacheAccess[];
CONTENT_EXPORT extern const char kEnableOverlayFullscreenVideo[];
CONTENT_EXPORT extern const char kEnableOverlayScrollbars[];
CONTENT_EXPORT extern const char kEnableOverscrollNotifications[];
CONTENT_EXPORT extern const char kEnablePinch[];
extern const char kEnablePreparsedJsCaching[];
CONTENT_EXPORT extern const char kEnablePrivilegedWebGLExtensions[];
extern const char kEnablePruneGpuCommandBuffers[];
CONTENT_EXPORT extern const char kEnableRegionBasedColumns[];
CONTENT_EXPORT extern const char kEnableRepaintAfterLayout[];
CONTENT_EXPORT extern const char kEnableSandboxLogging[];
extern const char kEnableSkiaBenchmarking[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableSoftwareCompositing[];
CONTENT_EXPORT extern const char kEnableSpatialNavigation[];
CONTENT_EXPORT extern const char kEnableSpeechSynthesis[];
extern const char kEnableSSLCachedInfo[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
extern const char kEnableStrictSiteIsolation[];
CONTENT_EXPORT extern const char kEnableServiceWorker[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
CONTENT_EXPORT extern const char kEnableTextServicesFramework[];
CONTENT_EXPORT extern const char kEnableThreadedCompositing[];
CONTENT_EXPORT extern const char kEnableUniversalAcceleratedOverflowScroll[];
CONTENT_EXPORT extern const char kEnableUserMediaScreenCapturing[];
CONTENT_EXPORT extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kEnableViewportMeta[];
CONTENT_EXPORT extern const char kMainFrameResizesAreOrientationChanges[];
extern const char kEnableVisualWordMovement[];
CONTENT_EXPORT extern const char kEnableVtune[];
// FIXME: Remove this once Web Animations CSS is enabled by default in Blink.
extern const char kEnableWebAnimationsCSS[];
extern const char kEnableWebAnimationsSVG[];
CONTENT_EXPORT extern const char kEnableWebGLDraftExtensions[];
CONTENT_EXPORT extern const char kEnableWebMIDI[];
CONTENT_EXPORT extern const char kExtraPluginDir[];
CONTENT_EXPORT extern const char kForceCompositingMode[];
CONTENT_EXPORT extern const char kForceFieldTrials[];
CONTENT_EXPORT extern const char kForceRendererAccessibility[];
extern const char kGpuDeviceID[];
extern const char kGpuDriverVendor[];
extern const char kGpuDriverVersion[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
CONTENT_EXPORT extern const char kGpuSandboxAllowSysVShm[];
CONTENT_EXPORT extern const char kGpuStartupDialog[];
extern const char kGpuVendorID[];
CONTENT_EXPORT extern const char kHostResolverRules[];
CONTENT_EXPORT extern const char kIgnoreCertificateErrors[];
CONTENT_EXPORT extern const char kIgnoreGpuBlacklist[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
CONTENT_EXPORT extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLogGpuControlListDecisions[];
CONTENT_EXPORT extern const char kLoggingLevel[];
CONTENT_EXPORT extern const char kLogNetLog[];
extern const char kLogPluginMessages[];
extern const char kMaxUntiledLayerHeight[];
extern const char kMaxUntiledLayerWidth[];
extern const char kMemoryMetrics[];
CONTENT_EXPORT extern const char kMuteAudio[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
CONTENT_EXPORT extern const char kOverscrollHistoryNavigation[];
extern const char kPluginLauncher[];
CONTENT_EXPORT extern const char kPluginPath[];
CONTENT_EXPORT extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashArgs[];
CONTENT_EXPORT extern const char kPpapiInProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
CONTENT_EXPORT extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
extern const char kReduceGpuSandbox[];
CONTENT_EXPORT extern const char kReduceSecurityForTesting[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
CONTENT_EXPORT extern const char kRendererProcessLimit[];
CONTENT_EXPORT extern const char kRendererStartupDialog[];
extern const char kSandboxIPCProcess[];
CONTENT_EXPORT extern const char kScrollEndEffect[];
extern const char kShowPaintRects[];
CONTENT_EXPORT extern const char kSimulateTouchScreenWithMouse[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSitePerProcess[];
CONTENT_EXPORT extern const char kSkipGpuDataLoading[];
extern const char kSpeechRecognitionWebserviceKey[];
CONTENT_EXPORT extern const char kStatsCollectionController[];
CONTENT_EXPORT extern const char kTabCaptureDownscaleQuality[];
CONTENT_EXPORT extern const char kTabCaptureUpscaleQuality[];
CONTENT_EXPORT extern const char kTestingFixedHttpPort[];
CONTENT_EXPORT extern const char kTestingFixedHttpsPort[];
CONTENT_EXPORT extern const char kTestSandbox[];
CONTENT_EXPORT extern const char kTouchAckTimeoutDelayMs[];
CONTENT_EXPORT extern const char kTraceShutdown[];
extern const char kTraceShutdownFile[];
extern const char kTraceStartup[];
extern const char kTraceStartupDuration[];
extern const char kTraceStartupFile[];
CONTENT_EXPORT extern const char kUIPrioritizeInGpuProcess[];
CONTENT_EXPORT extern const char kUseFakeDeviceForMediaStream[];
CONTENT_EXPORT extern const char kUseFakeUIForMediaStream[];
CONTENT_EXPORT extern const char kUseGpuInTests[];
CONTENT_EXPORT extern const char kUseMobileUserAgent[];
CONTENT_EXPORT extern const char kUserAgent[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kUtilityProcessEnableMDns[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
CONTENT_EXPORT extern const char kWebGLCommandBufferSizeKb[];
CONTENT_EXPORT extern const char kWorkerProcess[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

#if defined(ENABLE_WEBRTC)
CONTENT_EXPORT extern const char kEnableAudioTrackProcessing[];
CONTENT_EXPORT extern const char kDisableDeviceEnumeration[];
CONTENT_EXPORT extern const char kDisableSCTPDataChannels[];
CONTENT_EXPORT extern const char kDisableWebRtcHWDecoding[];
CONTENT_EXPORT extern const char kDisableWebRtcEncryption[];
CONTENT_EXPORT extern const char kDisableWebRtcHWEncoding[];
extern const char kEnableWebRtcAecRecordings[];
extern const char kEnableWebRtcTcpServerSocket[];
CONTENT_EXPORT extern const char kEnableWebRtcHWVp8Encoding[];
#endif

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const char kDisableGestureRequirementForMediaFullscreen[];
CONTENT_EXPORT extern const char kDisableGestureRequirementForMediaPlayback[];
extern const char kDisableMediaHistoryLogging[];
CONTENT_EXPORT extern const char kDisableOverscrollEdgeEffect[];
CONTENT_EXPORT extern const char kDisableWebRTC[];
CONTENT_EXPORT extern const char kEnableSpeechRecognition[];
CONTENT_EXPORT extern const char kHideScrollbars[];
extern const char kNetworkCountryIso[];
CONTENT_EXPORT extern const char kRemoteDebuggingSocketName[];
#endif

#if defined(OS_ANDROID) && defined(ARCH_CPU_X86)
CONTENT_EXPORT extern const char kEnableWebAudio[];
#else
CONTENT_EXPORT extern const char kDisableWebAudio[];
#endif

#if defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kDisablePanelFitting[];
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
extern const char kDisableCarbonInterposing[];
extern const char kDisableCoreAnimationPlugins[];
CONTENT_EXPORT extern const char kUseCoreAnimation[];
#endif

#if defined(OS_POSIX)
extern const char kChildCleanExit[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
