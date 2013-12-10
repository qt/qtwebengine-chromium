// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/web_contents_delegate_android/color_chooser_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "jni/WebContentsDelegateAndroid_jni.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::HasClass;
using base::android::ScopedJavaLocalRef;
using content::ColorChooser;
using content::WebContents;
using content::WebContentsDelegate;

namespace web_contents_delegate_android {

WebContentsDelegateAndroid::WebContentsDelegateAndroid(JNIEnv* env, jobject obj)
    : weak_java_delegate_(env, obj) {
}

WebContentsDelegateAndroid::~WebContentsDelegateAndroid() {
}

ScopedJavaLocalRef<jobject>
WebContentsDelegateAndroid::GetJavaDelegate(JNIEnv* env) const {
  return weak_java_delegate_.get(env);
}

// ----------------------------------------------------------------------------
// WebContentsDelegate methods
// ----------------------------------------------------------------------------

ColorChooser* WebContentsDelegateAndroid::OpenColorChooser(WebContents* source,
                                                           SkColor color)  {
  return new ColorChooserAndroid(source, color);
}

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* WebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;
  content::PageTransition transition(
      PageTransitionFromInt(params.transition));

  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::OpenURLFromTab(source, params);

  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> java_url =
        ConvertUTF8ToJavaString(env, url.spec());
    ScopedJavaLocalRef<jstring> extra_headers =
            ConvertUTF8ToJavaString(env, params.extra_headers);
    ScopedJavaLocalRef<jbyteArray> post_data;
    if (params.uses_post &&
        params.browser_initiated_post_data.get() &&
        params.browser_initiated_post_data.get()->size()) {
      post_data = base::android::ToJavaByteArray(
          env,
          reinterpret_cast<const uint8*>(
              params.browser_initiated_post_data.get()->front()),
          params.browser_initiated_post_data.get()->size());
    }
    Java_WebContentsDelegateAndroid_openNewTab(env,
                                               obj.obj(),
                                               java_url.obj(),
                                               extra_headers.obj(),
                                               post_data.obj(),
                                               disposition);
    return NULL;
  }

  source->GetController().LoadURL(url, params.referrer, transition,
                                  std::string());
  return source;
}

void WebContentsDelegateAndroid::NavigationStateChanged(
    const WebContents* source, unsigned changed_flags) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_navigationStateChanged(
      env,
      obj.obj(),
      changed_flags);
}

void WebContentsDelegateAndroid::ActivateContents(WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_activateContents(env, obj.obj());
}

void WebContentsDelegateAndroid::DeactivateContents(WebContents* contents) {
  // On desktop the current window is deactivated here, bringing the next window
  // to focus. Not implemented on Android.
}

void WebContentsDelegateAndroid::LoadingStateChanged(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  bool has_stopped = source == NULL || !source->IsLoading();

  if (has_stopped)
    Java_WebContentsDelegateAndroid_onLoadStopped(env, obj.obj());
  else
    Java_WebContentsDelegateAndroid_onLoadStarted(env, obj.obj());
}

void WebContentsDelegateAndroid::LoadProgressChanged(WebContents* source,
                                                     double progress) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_notifyLoadProgressChanged(
      env,
      obj.obj(),
      progress);
}

void WebContentsDelegateAndroid::RendererUnresponsive(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererUnresponsive(env, obj.obj());
}

void WebContentsDelegateAndroid::RendererResponsive(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererResponsive(env, obj.obj());
}

void WebContentsDelegateAndroid::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_closeContents(env, obj.obj());
}

void WebContentsDelegateAndroid::MoveContents(WebContents* source,
                                              const gfx::Rect& pos) {
  // Do nothing.
}

bool WebContentsDelegateAndroid::AddMessageToConsole(
    WebContents* source,
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::AddMessageToConsole(source, level, message,
                                                    line_no, source_id);
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));
  ScopedJavaLocalRef<jstring> jsource_id(
      ConvertUTF16ToJavaString(env, source_id));
  int jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
  switch (level) {
    case logging::LOG_VERBOSE:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
      break;
    case logging::LOG_INFO:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING;
      break;
    case logging::LOG_ERROR:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  return Java_WebContentsDelegateAndroid_addMessageToConsole(
      env,
      GetJavaDelegate(env).obj(),
      jlevel,
      jmessage.obj(),
      line_no,
      jsource_id.obj());
}

// This is either called from TabContents::DidNavigateMainFramePostCommit() with
// an empty GURL or responding to RenderViewHost::OnMsgUpateTargetURL(). In
// Chrome, the latter is not always called, especially not during history
// navigation. So we only handle the first case and pass the source TabContents'
// url to Java to update the UI.
void WebContentsDelegateAndroid::UpdateTargetURL(WebContents* source,
                                                 int32 page_id,
                                                 const GURL& url) {
  if (!url.is_empty())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, source->GetURL().spec());
  Java_WebContentsDelegateAndroid_onUpdateUrl(env,
                                              obj.obj(),
                                              java_url.obj());
}

void WebContentsDelegateAndroid::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  jobject key_event = event.os_event;
  if (key_event) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null())
      return;
    Java_WebContentsDelegateAndroid_handleKeyboardEvent(
        env, obj.obj(), key_event);
  }
}

bool WebContentsDelegateAndroid::TakeFocus(WebContents* source, bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::TakeFocus(source, reverse);
  return Java_WebContentsDelegateAndroid_takeFocus(
      env, obj.obj(), reverse);
}

void WebContentsDelegateAndroid::ShowRepostFormWarningDialog(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jobject> content_view_core =
      content::ContentViewCore::FromWebContents(source)->GetJavaObject();
  if (content_view_core.is_null())
    return;
  Java_WebContentsDelegateAndroid_showRepostFormWarningDialog(env, obj.obj(),
      content_view_core.obj());
}

void WebContentsDelegateAndroid::ToggleFullscreenModeForTab(
    WebContents* web_contents,
    bool enter_fullscreen) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_toggleFullscreenModeForTab(
      env, obj.obj(), enter_fullscreen);
}

bool WebContentsDelegateAndroid::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_isFullscreenForTabOrPending(
      env, obj.obj());
}

void WebContentsDelegateAndroid::DidProgrammaticallyScroll(
    WebContents* web_contents, const gfx::Vector2d& scroll_point) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_didProgrammaticallyScroll(
      env, obj.obj(), scroll_point.x(), scroll_point.y());
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// Register native methods

bool RegisterWebContentsDelegateAndroid(JNIEnv* env) {
  if (!HasClass(env, kWebContentsDelegateAndroidClassPath)) {
    DLOG(ERROR) << "Unable to find class WebContentsDelegateAndroid!";
    return false;
  }
  return RegisterNativesImpl(env);
}

}  // namespace web_contents_delegate_android
