// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/android/content_settings.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_render_view.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/download_controller_android_impl.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/surface_texture_peer_browser_impl.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/android/tracing_intent_handler.h"
#include "content/browser/android/vibration_message_filter.h"
#include "content/browser/android/web_contents_observer_android.h"
#include "content/browser/device_orientation/data_fetcher_impl_android.h"
#include "content/browser/geolocation/location_api_adapter_android.h"
#include "content/browser/media/android/media_drm_credential_manager.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/power_save_blocker_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/speech/speech_recognizer_impl_android.h"

using content::SurfaceTexturePeerBrowserImpl;

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
    {"AndroidLocationApiAdapter",
     content::AndroidLocationApiAdapter::RegisterGeolocationService},
    {"BrowserAccessibilityManager",
     content::RegisterBrowserAccessibilityManager},
    {"BrowserStartupController", content::RegisterBrowserStartupController},
    {"ChildProcessLauncher", content::RegisterChildProcessLauncher},
    {"ContentSettings", content::ContentSettings::RegisterContentSettings},
    {"ContentViewRenderView",
     content::ContentViewRenderView::RegisterContentViewRenderView},
    {"ContentVideoView", content::ContentVideoView::RegisterContentVideoView},
    {"ContentViewCore", content::RegisterContentViewCore},
    {"DataFetcherImplAndroid", content::DataFetcherImplAndroid::Register},
    {"DateTimePickerAndroid", content::RegisterDateTimeChooserAndroid},
    {"DownloadControllerAndroidImpl",
     content::DownloadControllerAndroidImpl::RegisterDownloadController},
    {"InterstitialPageDelegateAndroid",
     content::InterstitialPageDelegateAndroid::
         RegisterInterstitialPageDelegateAndroid},
    {"MediaDrmCredentialManager",
     content::MediaDrmCredentialManager::RegisterMediaDrmCredentialManager},
    {"MediaResourceGetterImpl",
     content::MediaResourceGetterImpl::RegisterMediaResourceGetter},
    {"LoadUrlParams", content::RegisterLoadUrlParams},
    {"PowerSaveBlock", content::RegisterPowerSaveBlocker},
    {"RegisterImeAdapter", content::RegisterImeAdapter},
    {"SpeechRecognizerImplAndroid",
     content::SpeechRecognizerImplAndroid::RegisterSpeechRecognizer},
    {"TouchPoint", content::RegisterTouchPoint},
    {"TracingIntentHandler", content::RegisterTracingIntentHandler},
    {"VibrationMessageFilter", content::VibrationMessageFilter::Register},
    {"WebContentsObserverAndroid", content::RegisterWebContentsObserverAndroid},
    {"WebViewStatics", content::RegisterWebViewStatics}, };

}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
