// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

namespace switches {

// The number of MSAA samples for canvas2D. Requires MSAA support by GPU to
// have an effect. 0 disables MSAA.
const char kAcceleratedCanvas2dMSAASampleCount[] = "canvas-msaa-sample-count";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Enables the sandboxed processes to run without a job object assigned to them.
// This flag is required to allow Chrome to run in RemoteApps or Citrix. This
// flag can reduce the security of the sandboxed processes and allow them to do
// certain API calls like shut down Windows or access the clipboard. Also we
// lose the chance to kill some processes until the outer job that owns them
// finishes.
const char kAllowNoSandboxJob[]             = "allow-no-sandbox-job";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Allow compositing on chrome:// pages.
const char kAllowWebUICompositing[]         = "allow-webui-compositing";

// The same as kAuditHandles except all handles are enumerated.
const char kAuditAllHandles[]               = "enable-handle-auditing-all";

// Enumerates and prints a child process' most dangerous handles when it
// is terminated.
const char kAuditHandles[]                  = "enable-handle-auditing";

// Blacklist the GPU for accelerated compositing.
const char kBlacklistAcceleratedCompositing[] =
    "blacklist-accelerated-compositing";

// Blacklist the GPU for WebGL.
const char kBlacklistWebGL[]                = "blacklist-webgl";

// Block cross-site documents (i.e., HTML/XML/JSON) from being loaded in
// subresources when a document is not supposed to read them.  This will later
// allow us to block them from the entire renderer process when site isolation
// is enabled.
const char kBlockCrossSiteDocuments[]     = "block-cross-site-documents";

// Causes the browser process to throw an assertion on startup.
const char kBrowserAssertTest[]             = "assert-test";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Dumps extra logging about plugin loading to the log file.
const char kDebugPluginLoading[] = "debug-plugin-loading";

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[]              = "default-tile-width";
const char kDefaultTileHeight[]             = "default-tile-height";

// Disable antialiasing on 2d canvas.
const char kDisable2dCanvasAntialiasing[]   = "disable-canvas-aa";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Disables accelerated compositing.
const char kDisableAcceleratedCompositing[] = "disable-accelerated-compositing";

// Disables accelerated compositing for backgrounds of root layers with
// background-attachment: fixed.
const char kDisableAcceleratedFixedRootBackground[] =
    "disable-accelerated-fixed-root-background";

// Disables the hardware acceleration of 3D CSS and animation.
const char kDisableAcceleratedLayers[]      = "disable-accelerated-layers";

// Disables accelerated compositing for overflow scroll.
const char kDisableAcceleratedOverflowScroll[] =
    "disable-accelerated-overflow-scroll";

// Disables layer squashing.
const char kDisableLayerSquashing[] =
    "disable-layer-squashing";

// Disable accelerated compositing for scrollable frames.
const char kDisableAcceleratedScrollableFrames[] =
     "disable-accelerated-scrollable-frames";

// Disables the hardware acceleration of plugins.
const char kDisableAcceleratedPlugins[]     = "disable-accelerated-plugins";

// Disables GPU accelerated video display.
const char kDisableAcceleratedVideo[]       = "disable-accelerated-video";

// Disables hardware acceleration of video decode, where available.
const char kDisableAcceleratedVideoDecode[] =
    "disable-accelerated-video-decode";

// Disables the alternate window station for the renderer.
const char kDisableAltWinstation[]          = "disable-winsta";

// Disable the ApplicationCache.
const char kDisableApplicationCache[]       = "disable-application-cache";
//
// TODO(scherkus): remove --disable-audio when we have a proper fallback
// mechanism.
const char kDisableAudio[]                  = "disable-audio";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable browser plugin compositing experiment.
const char kDisableBrowserPluginCompositing[] =
    "disable-browser-plugin-compositing";

// Disable accelerated scrolling by the compositor for frames.
const char kDisableCompositedScrollingForFrames[] =
     "disable-composited-scrolling-for-frames";

// See comment for kEnableCompositingForFixedPosition.
const char kDisableCompositingForFixedPosition[] =
     "disable-fixed-position-compositing";

// See comment for kEnableCompositingForTransition.
const char kDisableCompositingForTransition[] =
     "disable-transition-compositing";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables the deadline scheduler.
const char kDisableDeadlineScheduling[]     = "disable-deadline-scheduling";

// Disables delegated renderer.
const char kDisableDelegatedRenderer[]      = "disable-delegated-renderer";

// Disables desktop notifications (default enabled on windows).
const char kDisableDesktopNotifications[]   = "disable-desktop-notifications";

// Disables experimental navigator content utils implementation.
const char kDisableNavigatorContentUtils[]  =
      "disable-navigator-content-utils";

// Disable device motion events.
const char kDisableDeviceMotion[]           = "disable-device-motion";

// Disable device orientation events.
const char kDisableDeviceOrientation[]      = "disable-device-orientation";

// Handles URL requests by NPAPI plugins through the renderer.
const char kDisableDirectNPAPIRequests[]    = "disable-direct-npapi-requests";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
extern const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disables sending filters (SkImageFilter objects) between processes over IPC
const char kDisableFiltersOverIPC[]         = "disable-filters-over-ipc";

const char kDisableFixedPositionCreatesStackingContext[]
    = "disable-fixed-position-creates-stacking-context";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

// This flag disables force compositing mode and prevents it from being enabled
// via field trials.
const char kDisableForceCompositingMode[]   = "disable-force-compositing-mode";

// Disable the JavaScript Full Screen API.
const char kDisableFullScreen[]             = "disable-fullscreen";

// Suppresses support for the Geolocation javascript API.
const char kDisableGeolocation[]            = "disable-geolocation";

// Disable deferral of scroll-ending gesture events when a scroll is active.
const char kDisableGestureDebounce[]        = "disable-gesture-debounce";

const char kDisableGestureTapHighlight[]    = "disable-gesture-tap-highlight";

// Disable GL multisampling.
const char kDisableGLMultisampling[]        = "disable-gl-multisampling";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Prevent the compositor from using its GPU implementation.
const char kDisableGpuCompositing[]         = "disable-gpu-compositing";

// Disable the limit on the number of times the GPU process may be restarted
// This switch is intended only for tests.
extern const char kDisableGpuProcessCrashLimit[] =
    "disable-gpu-process-crash-limit";

// Do not launch the GPU process shortly after browser process launch. Instead
// launch it when it is first needed.
const char kDisableGpuProcessPrelaunch[]    = "disable-gpu-process-prelaunch";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[]            = "disable-gpu-watchdog";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the RenderThread's HistogramCustomizer.
const char kDisableHistogramCustomizer[]    = "disable-histogram-customizer";

// Disable the use of an ImageTransportSurface. This means the GPU process
// will present the rendered page rather than the browser process.
const char kDisableImageTransportSurface[]  = "disable-image-transport-surface";

// Prevent Java from running.
const char kDisableJava[]                   = "disable-java";

// Don't execute JavaScript (browser JS like the new tab page still runs).
const char kDisableJavaScript[]             = "disable-javascript";

// Don't kill a child process when it sends a bad IPC message.  Apart
// from testing, it is a bad idea from a security perspective to enable
// this switch.
const char kDisableKillAfterBadIPC[]        = "disable-kill-after-bad-ipc";

// Disables prefixed Encrypted Media API (e.g. webkitGenerateKeyRequest()).
const char kDisablePrefixedEncryptedMedia[] =
    "disable-prefixed-encrypted-media";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Allows P2P sockets to talk UDP to other servers without using STUN first.
// For development only, use with caution.
// TODO(hubbe): Remove this flag.
const char kDisableP2PSocketSTUNFilter[]    = "disable-p2psocket-stun-filter";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Prevent plugins from running.
const char kDisablePlugins[]                = "disable-plugins";

// Disable discovering third-party plug-ins. Effectively loading only
// ones shipped with the browser plus third-party ones as specified by
// --extra-plugin-dir and --load-plugin switches.
const char kDisablePluginsDiscovery[]       = "disable-plugins-discovery";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[]   = "disable-seccomp-filter-sandbox";

// Disable session storage.
const char kDisableSessionStorage[]         = "disable-session-storage";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[]          = "disable-setuid-sandbox";

// Enable shared workers. Functionality not yet complete.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disables site-specific tailoring to compatibility issues in WebKit.
const char kDisableSiteSpecificQuirks[]     = "disable-site-specific-quirks";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables speech input.
const char kDisableSpeechInput[]            = "disable-speech-input";

// Disable False Start in SSL and TLS connections.
const char kDisableSSLFalseStart[]          = "disable-ssl-false-start";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]     = "disable-threaded-compositing";

// Disables the threaded HTML parser in Blink
const char kDisableThreadedHTMLParser[]     = "disable-threaded-html-parser";

// Disable accelerated overflow scrolling in corner cases (that would not be
// handled by enable-accelerated-overflow-scroll).
const char kDisableUniversalAcceleratedOverflowScroll[] =
    "disable-universal-accelerated-overflow-scroll";

// Disables unprefixed Media Source API (i.e., the MediaSource object).
const char kDisableUnprefixedMediaSource[]  = "disable-unprefixed-media-source";

// Disable CSS Transitions / Animations on the Web Animations model.
const char kDisableWebAnimationsCSS[]        = "disable-web-animations-css";

// Disables prefixed Media Source API (i.e., the WebKitMediaSource object).
const char kDisableWebKitMediaSource[]      = "disable-webkit-media-source";

// Don't enforce the same-origin policy. (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disables support for XSLT.
const char kDisableXSLT[]                   = "disable-xslt";

// Disables Blink's XSSAuditor. The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests.
const char kDomAutomationController[]       = "dom-automation";

// Enable gpu-accelerated SVG/W3C filters.
const char kEnableAcceleratedFilters[]      = "enable-accelerated-filters";

// Enables accelerated compositing for backgrounds of root layers with
// background-attachment: fixed. Requires kForceCompositingMode.
const char kEnableAcceleratedFixedRootBackground[] =
    "enable-accelerated-fixed-root-background";

// Enables accelerated compositing for overflow scroll. Promotes eligible
// overflow:scroll elements to layers to enable accelerated scrolling for them.
const char kEnableAcceleratedOverflowScroll[] =
    "enable-accelerated-overflow-scroll";

// Enables experimental feature that maps multiple RenderLayers to
// one composited layer to avoid pathological layer counts.
const char kEnableLayerSquashing[] =
    "enable-layer-squashing";

// Enables accelerated compositing for scrollable frames for accelerated
// scrolling for them. Requires kForceCompositingMode.
const char kEnableAcceleratedScrollableFrames[] =
     "enable-accelerated-scrollable-frames";

// Turns on extremely verbose logging of accessibility events.
const char kEnableAccessibilityLogging[]    = "enable-accessibility-logging";

// Use a BeginImplFrame signal from browser to renderer to schedule rendering.
const char kEnableBeginFrameScheduling[]    = "enable-begin-frame-scheduling";

// Enables browser plugin for all types of pages.
const char kEnableBrowserPluginForAllViewTypes[] =
    "enable-browser-plugin-for-all-view-types";

// Enables Drag and Drop into and out of Browser Plugin.
// kEnableBrowserPluginGuestViews must also be set at this time.
const char kEnableBrowserPluginDragDrop[]   = "enable-browser-plugin-drag-drop";

// Enables accelerated scrolling by the compositor for frames. Requires
// kForceCompositingMode and kEnableAcceleratedScrollableFrames.
const char kEnableCompositedScrollingForFrames[] =
     "enable-composited-scrolling-for-frames";

// Enable the creation of compositing layers for fixed position
// elements. Three options are needed to support four possible scenarios:
//  1. Default (disabled)
//  2. Enabled always (to allow dogfooding)
//  3. Disabled always (to give safety fallback for users)
//  4. Enabled only if we detect a highDPI display
//
// Option #4 may soon be the default, because the feature is needed soon for
// high DPI, but cannot be used (yet) for low DPI. Options #2 and #3 will
// override Option #4.
const char kEnableCompositingForFixedPosition[] =
     "enable-fixed-position-compositing";

// Enable/Disable the creation of compositing layers for RenderLayers with a
// transition on a property that supports accelerated animation (that is,
// opacity, -webkit-transform, and -webkit-filter), even when no animation is
// running. These options allow for three possible scenarios:
//  1. Default (enabled only if we dectect a highDPI display)
//  2. Enabled always.
//  3. Disabled always.
const char kEnableCompositingForTransition[] =
     "enable-transition-compositing";

// Defer image decoding in WebKit until painting.
const char kEnableDeferredImageDecoding[]   = "enable-deferred-image-decoding";

// Enables the deadline scheduler.
const char kEnableDeadlineScheduling[]      = "enable-deadline-scheduling";

// Enables delegated renderer.
const char kEnableDelegatedRenderer[]       = "enable-delegated-renderer";

// Enables restarting interrupted downloads.
const char kEnableDownloadResumption[]      = "enable-download-resumption";

// Enables support for Encrypted Media Extensions (e.g. MediaKeys).
const char kEnableEncryptedMedia[] = "enable-encrypted-media";

// Enable experimental canvas features, e.g. canvas 2D context attributes
const char kEnableExperimentalCanvasFeatures[] =
    "enable-experimental-canvas-features";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// Enable an experimental WebSocket implementation.
const char kEnableExperimentalWebSocket[]   = "enable-experimental-websocket";

// Enable the fast text autosizing implementation.
const char kEnableFastTextAutosizing[]      = "enable-fast-text-autosizing";

const char kEnableFixedPositionCreatesStackingContext[]
    = "enable-fixed-position-creates-stacking-context";

// Enable Gesture Tap Highlight
const char kEnableGestureTapHighlight[]     = "enable-gesture-tap-highlight";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[]         = "enable-gpu-benchmarking";

// Enables TRACE for GL calls in the renderer.
const char kEnableGpuClientTracing[]        = "enable-gpu-client-tracing";

// See comment for kEnableCompositingForFixedPosition.
const char kEnableHighDpiCompositingForFixedPosition[] =
     "enable-high-dpi-fixed-position-compositing";

#if defined(OS_WIN)
// Enables the DirectWrite font rendering system on windows.
const char kEnableDirectWrite[]             = "enable-direct-write";

// Use high resolution timers for TimeTicks.
const char kEnableHighResolutionTime[]      = "enable-high-resolution-time";
#endif

// Enable HTML Imports
extern const char kEnableHTMLImports[]      = "enable-html-imports";

// Enables support for inband text tracks in media content.
const char kEnableInbandTextTracks[]        = "enable-inband-text-tracks";

// Enable inputmode attribute of HTML input or text element.
extern const char kEnableInputModeAttribute[] = "enable-input-mode-attribute";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Enables the memory benchmarking extension
const char kEnableMemoryBenchmarking[]      = "enable-memory-benchmarking";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Enables use of cache if offline, even if it's stale
const char kEnableOfflineCacheAccess[]      = "enable-offline-cache-access";

// Enables use of hardware overlay for fullscreen video playback. Android only.
const char kEnableOverlayFullscreenVideo[]  = "enable-overlay-fullscreen-video";

// Enables overlay scrollbars on Aura or Linux. Does nothing on Mac.
const char kEnableOverlayScrollbars[]       = "enable-overlay-scrollbars";

// Forward overscroll event data from the renderer to the browser.
const char kEnableOverscrollNotifications[] = "enable-overscroll-notifications";

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Enable caching of pre-parsed JS script data.  See http://crbug.com/32407.
const char kEnablePreparsedJsCaching[]      = "enable-preparsed-js-caching";

// Enable privileged WebGL extensions; without this switch such extensions are
// available only to Chrome extensions.
const char kEnablePrivilegedWebGLExtensions[] =
    "enable-privileged-webgl-extensions";

// Aggressively free GPU command buffers belonging to hidden tabs.
const char kEnablePruneGpuCommandBuffers[] =
    "enable-prune-gpu-command-buffers";

// Enables the CSS multicol implementation that uses the regions implementation.
const char kEnableRegionBasedColumns[] =
    "enable-region-based-columns";

// Enables the new layout/paint system which paints after layout is complete.
const char kEnableRepaintAfterLayout[] =
    "enable-repaint-after-layout";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enables the Skia benchmarking extension
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Allow the compositor to use its software implementation if GL fails.
const char kEnableSoftwareCompositing[]     = "enable-software-compositing";

// Enable spatial navigation
const char kEnableSpatialNavigation[]       = "enable-spatial-navigation";

// Enables the synthesis part of the Web Speech API.
const char kEnableSpeechSynthesis[]         = "enable-speech-synthesis";

// Enables TLS cached info extension.
const char kEnableSSLCachedInfo[]           = "enable-ssl-cached-info";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Experimentally ensures that each renderer process:
// 1) Only handles rendering for pages from a single site, apart from iframes.
// (Note that a page can reference content from multiple origins due to images,
// JavaScript files, etc.  Cross-site iframes are also loaded in-process.)
// 2) Only has authority to see or use cookies for the page's top-level origin.
// (So if a.com iframes b.com, the b.com network request will be sent without
// cookies.)
// This is expected to break compatibility with many pages for now.  Unlike the
// --site-per-process flag, this allows cross-site iframes, but it blocks all
// cookies on cross-site requests.
const char kEnableStrictSiteIsolation[]     = "enable-strict-site-isolation";

// Enable support for ServiceWorker. See
// https://github.com/slightlyoff/ServiceWorker for more information.
const char kEnableServiceWorker[]           = "enable-service-worker";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Enable Text Service Framework(TSF) for text inputting instead of IMM32. This
// flag is ignored on Metro environment.
const char kEnableTextServicesFramework[]   = "enable-text-services-framework";

// Enable multithreaded GPU compositing of web content.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Enable accelerated overflow scrolling in all cases.
const char kEnableUniversalAcceleratedOverflowScroll[] =
    "enable-universal-accelerated-overflow-scroll";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enables the use of the legacy viewport meta tag. Turning this on also
// turns on the @viewport CSS rule
const char kEnableViewportMeta[]            = "enable-viewport-meta";

// Resizes of the main frame are the caused by changing between landscape
// and portrait mode (i.e. Android) so the page should be rescaled to fit
const char kMainFrameResizesAreOrientationChanges[] =
    "main-frame-resizes-are-orientation-changes";

// Enables moving cursor by word in visual order.
const char kEnableVisualWordMovement[]      = "enable-visual-word-movement";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enable CSS Transitions / Animations on the Web Animations model.
const char kEnableWebAnimationsCSS[]        = "enable-web-animations-css";

// Enable SVG Animations on the Web Animations model.
const char kEnableWebAnimationsSVG[]        = "enable-web-animations-svg";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables Web MIDI API.
const char kEnableWebMIDI[]                 = "enable-web-midi";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// If accelerated compositing is supported, always enter compositing mode for
// the base layer even when compositing is not strictly required.
const char kForceCompositingMode[]          = "force-compositing-mode";

// Some field trials may be randomized in the browser, and the randomly selected
// outcome needs to be propagated to the renderer. For instance, this is used
// to modify histograms recorded in the renderer, or to get the renderer to
// also set of its state (initialize, or not initialize components) to match the
// experiment(s). The option is also useful for forcing field trials when
// testing changes locally. The argument is a list of name and value pairs,
// separated by slashes. See FieldTrialList::CreateTrialsFromString() in
// field_trial.h for details.
const char kForceFieldTrials[]              = "force-fieldtrials";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// Passes gpu device_id from browser process to GPU process.
const char kGpuDeviceID[]                   = "gpu-device-id";

// Passes gpu driver_vendor from browser process to GPU process.
const char kGpuDriverVendor[]               = "gpu-driver-vendor";

// Passes gpu driver_version from browser process to GPU process.
const char kGpuDriverVersion[]              = "gpu-driver-version";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Allow shmat system call in GPU sandbox.
const char kGpuSandboxAllowSysVShm[]        = "gpu-sandbox-allow-sysv-shm";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Passes gpu vendor_id from browser process to GPU process.
const char kGpuVendorID[]                   = "gpu-vendor-id";

// These mappings only apply to the host resolver.
const char kHostResolverRules[]             = "host-resolver-rules";

// Ignores certificate-related errors.
const char kIgnoreCertificateErrors[]       = "ignore-certificate-errors";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[]            = "ignore-gpu-blacklist";

// Run the GPU process as a thread in the browser process.
const char kInProcessGPU[]                  = "in-process-gpu";

// Runs plugins inside the renderer process
const char kInProcessPlugins[]              = "in-process-plugins";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Logs GPU control list decisions when enforcing blacklist rules.
const char kLogGpuControlListDecisions[]    = "log-gpu-control-list-decisions";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Enables displaying net log events on the command line, or writing the events
// to a separate file if a file name is given.
const char kLogNetLog[]                     = "log-net-log";

// Make plugin processes log their sent and received messages to VLOG(1).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[]         = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[]          = "max-untiled-layer-width";

// Sample memory usage with high frequency and store the results to the
// Renderer.Memory histogram. Used in memory tests.
const char kMemoryMetrics[]                 = "memory-metrics";

// Mutes audio sent to the audio device so it is not audible during
// automated testing.
const char kMuteAudio[]                     = "mute-audio";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Disables the sandbox for all process types that are normally sandboxed.
const char kNoSandbox[]                     = "no-sandbox";

// Enables or disables history navigation in response to horizontal overscroll.
// Set the value to '1' to enable the feature, and set to '0' to disable.
// Defaults to enabled.
const char kOverscrollHistoryNavigation[] =
    "overscroll-history-navigation";

// Specifies a command that should be used to launch the plugin process.  Useful
// for running the plugin process through purify or quantify.  Ex:
//   --plugin-launcher="path\to\purify /Run=yes"
const char kPluginLauncher[]                = "plugin-launcher";

// Tells the plugin process the path of the plugin to load
const char kPluginPath[]                    = "plugin-path";

// Causes the process to run as a plugin subprocess.
const char kPluginProcess[]                 = "plugin";

// Causes the plugin process to display a dialog on launch.
const char kPluginStartupDialog[]           = "plugin-startup-dialog";

// Argument to the process type that indicates a PPAPI broker process type.
const char kPpapiBrokerProcess[]            = "ppapi-broker";

// "Command-line" arguments for the PPAPI Flash; used for debugging options.
const char kPpapiFlashArgs[]                = "ppapi-flash-args";

// Runs PPAPI (Pepper) plugins in-process.
const char kPpapiInProcess[]                = "ppapi-in-process";

// Like kPluginLauncher for PPAPI plugins.
const char kPpapiPluginLauncher[]           = "ppapi-plugin-launcher";

// Argument to the process type that indicates a PPAPI plugin process type.
const char kPpapiPluginProcess[]            = "ppapi";

// Causes the PPAPI sub process to display a dialog on launch. Be sure to use
// --no-sandbox as well or the sandbox won't allow the dialog to display.
const char kPpapiStartupDialog[]            = "ppapi-startup-dialog";

// Runs a single process for each site (i.e., group of pages from the same
// registered domain) the user visits.  We default to using a renderer process
// for each site instance (i.e., group of pages from the same registered
// domain with script connections to each other).
const char kProcessPerSite[]                = "process-per-site";

// Runs each set of script-connected tabs (i.e., a BrowsingInstance) in its own
// renderer process.  We default to using a renderer process for each
// site instance (i.e., group of pages from the same registered domain with
// script connections to each other).
const char kProcessPerTab[]                 = "process-per-tab";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Reduces the GPU process sandbox to be less strict.
const char kReduceGpuSandbox[]              = "reduce-gpu-sandbox";

// Enables more web features over insecure connections. Designed to be used
// for testing purposes only.
const char kReduceSecurityForTesting[]      = "reduce-security-for-testing";

// Register Pepper plugins (see pepper_plugin_list.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

// Causes the renderer process to throw an assertion on launch.
const char kRendererAssertTest[]            = "renderer-assert-test";

// On POSIX only: the contents of this flag are prepended to the renderer
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Causes the process to run as a sandbox IPC subprocess.
const char kSandboxIPCProcess[]             = "sandbox-ipc";

// Enables or disables scroll end effect in response to vertical overscroll.
// Set the value to '1' to enable the feature, and set to '0' to disable.
// Defaults to disabled.
const char kScrollEndEffect[] = "scroll-end-effect";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Map mouse input events into touch gesture events.  Useful for debugging touch
// gestures without needing a touchscreen.
const char kSimulateTouchScreenWithMouse[]  =
    "simulate-touch-screen-with-mouse";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Experimentally enforces a one-site-per-process security policy.
// All cross-site navigations force process swaps, and we can restrict a
// renderer process's access rights based on its site.  For details, see:
// http://www.chromium.org/developers/design-documents/site-isolation
//
// Unlike --enable-strict-site-isolation (which allows cross-site iframes),
// this flag does not affect which cookies are attached to cross-site requests.
// Support is being added to render cross-site iframes in a different process
// than their parent pages.
const char kSitePerProcess[]                = "site-per-process";

// Skip gpu info collection, blacklist loading, and blacklist auto-update
// scheduling at browser startup time.
// Therefore, all GPU features are available, and about:gpu page shows empty
// content. The switch is intended only for layout tests.
// TODO(gab): Get rid of this switch entirely.
const char kSkipGpuDataLoading[]            = "skip-gpu-data-loading";

// Specifies the request key for the continuous speech recognition webservice.
const char kSpeechRecognitionWebserviceKey[] = "speech-service-key";

// Specifies if the |StatsCollectionController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when running a test
// that needs to access the provided statistics.
const char kStatsCollectionController[] =
    "enable-stats-collection-bindings";

// Upscale defaults to "good".
const char kTabCaptureDownscaleQuality[]    = "tab-capture-downscale-quality";

// Scaling quality for capturing tab. Should be one of "fast", "good" or "best".
// One flag for upscaling, one for downscaling.
// Upscale defaults to "best".
const char kTabCaptureUpscaleQuality[]      = "tab-capture-upscale-quality";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kTestingFixedHttpPort[]          = "testing-fixed-http-port";
const char kTestingFixedHttpsPort[]         = "testing-fixed-https-port";

// Runs the security test for the renderer sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// Enable timeout-based touch event cancellation if a touch ack is delayed.
// If unspecified, touch timeout behavior will be disabled.
const char kTouchAckTimeoutDelayMs[]        = "touch-ack-timeout-delay-ms";

// Causes TRACE_EVENT flags to be recorded beginning with shutdown. Optionally,
// can specify the specific trace categories to include (e.g.
// --trace-shutdown=base,net) otherwise, all events are recorded.
// --trace-shutdown-file can be used to control where the trace log gets stored
// to since there is otherwise no way to access the result.
const char kTraceShutdown[]                 = "trace-shutdown";

// If supplied, sets the file which shutdown tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-shutdown is also supplied.
// Example: --trace-shutdown --trace-shutdown-file=/tmp/trace_event.log
const char kTraceShutdownFile[]             = "trace-shutdown-file";

// Causes TRACE_EVENT flags to be recorded from startup. Optionally, can
// specify the specific trace categories to include (e.g.
// --trace-startup=base,net) otherwise, all events are recorded. Setting this
// flag results in the first call to BeginTracing() to receive all trace events
// since startup. In Chrome, you may find --trace-startup-file and
// --trace-startup-duration to control the auto-saving of the trace (not
// supported in the base-only TraceLog component).
const char kTraceStartup[]                  = "trace-startup";

// Sets the time in seconds until startup tracing ends. If omitted a default of
// 5 seconds is used. Has no effect without --trace-startup, or if
// --startup-trace-file=none was supplied.
const char kTraceStartupDuration[]          = "trace-startup-duration";

// If supplied, sets the file which startup tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-startup is also supplied.
// Example: --trace-startup --trace-startup-file=/tmp/trace_event.log
// As a special case, can be set to 'none' - this disables automatically saving
// the result to a file and the first manually recorded trace will then receive
// all events since startup.
const char kTraceStartupFile[]              = "trace-startup-file";



// Prioritizes the UI's command stream in the GPU process
extern const char kUIPrioritizeInGpuProcess[] =
    "ui-prioritize-in-gpu-process";

// Use fake device for MediaStream to replace actual camera and microphone.
const char kUseFakeDeviceForMediaStream[] = "use-fake-device-for-media-stream";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[]                 = "use-gpu-in-tests";

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// On POSIX only: the contents of this flag are prepended to the utility
// process command line. Useful values might be "valgrind" or "xterm -e gdb
// --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

// Allows MDns to access network in sandboxed process.
const char kUtilityProcessEnableMDns[]      = "utility-enable-mdns";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// Choose which logging channels in WebCore to activate.  See
// Logging.cpp in WebKit's WebCore for a list of available channels.
const char kWebCoreLogChannels[]            = "webcore-log-channels";

// Overrides the amount of shared memory the webgl command buffer allocates
const char kWebGLCommandBufferSizeKb[]      = "webgl-command-buffer-size-kb";

// Causes the process to run as a worker subprocess.
const char kWorkerProcess[]                 = "worker";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

#if defined(ENABLE_WEBRTC)
// Enables audio processing in a MediaStreamTrack. When this flag is on, AEC,
// NS and AGC will be done per MediaStreamTrack instead of in PeerConnection.
const char kEnableAudioTrackProcessing[]    = "enable-audio-track-processing";

// Disables WebRTC device enumeration.
const char kDisableDeviceEnumeration[]      = "disable-device-enumeration";

// Disables WebRTC DataChannels SCTP wire protocol support.
const char kDisableSCTPDataChannels[]       = "disable-sctp-data-channels";

// Disables HW decode acceleration for WebRTC.
const char kDisableWebRtcHWDecoding[]       = "disable-webrtc-hw-decoding";

// Disables encryption of RTP Media for WebRTC. When Chrome embeds Content, it
// ignores this switch on its stable and beta channels.
const char kDisableWebRtcEncryption[]      = "disable-webrtc-encryption";

// Disables HW encode acceleration for WebRTC.
const char kDisableWebRtcHWEncoding[]       = "disable-webrtc-hw-encoding";

// Enables WebRTC AEC recordings.
const char kEnableWebRtcAecRecordings[]     = "enable-webrtc-aec-recordings";

// Enables WebRTC to open TCP server sockets.
const char kEnableWebRtcTcpServerSocket[]   = "enable-webrtc-tcp-server-socket";

// Enables VP8 HW encode acceleration for WebRTC.
const char kEnableWebRtcHWVp8Encoding[]     = "enable-webrtc-hw-vp8-encoding";
#endif

#if defined(OS_ANDROID)
// Disable user gesture requirement for the media element to enter fullscreen.
const char kDisableGestureRequirementForMediaFullscreen[] =
    "disable-gesture-requirement-for-media-fullscreen";

// Disable user gesture requirement for media playback.
const char kDisableGestureRequirementForMediaPlayback[] =
    "disable-gesture-requirement-for-media-playback";

// Disable history logging for media elements.
const char kDisableMediaHistoryLogging[]    = "disable-media-history";

// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[]   = "disable-overscroll-edge-effect";

// WebRTC is enabled by default on Android.
const char kDisableWebRTC[]                 = "disable-webrtc";

// Enable the recognition part of the Web Speech API.
const char kEnableSpeechRecognition[]       = "enable-speech-recognition";

// Don't display any scrollbars. This is useful for Android WebView where
// the system manages the scrollbars instead.
const char kHideScrollbars[]                = "hide-scrollbars";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";
#endif

#if defined(OS_ANDROID) && defined(ARCH_CPU_X86)
const char kEnableWebAudio[]                = "enable-webaudio";
#else
// Disable web audio API.
const char kDisableWebAudio[]               = "disable-webaudio";
#endif

#if defined(OS_CHROMEOS)
// Disables panel fitting (used for mirror mode).
const char kDisablePanelFitting[]           = "disable-panel-fitting";
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
const char kDisableCarbonInterposing[]      = "disable-carbon-interposing";

// Disables support for Core Animation plugins. This is triggered when
// accelerated compositing is disabled. See http://crbug.com/122430 .
const char kDisableCoreAnimationPlugins[] =
    "disable-core-animation-plugins";

// Use core animation to draw the RenderWidgetHostView on Mac.
const char kUseCoreAnimation[]              = "use-core-animation";
#endif

#if defined(OS_POSIX)
// Causes the child processes to cleanly exit via calling exit().
const char kChildCleanExit[]                = "child-clean-exit";
#endif

// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
