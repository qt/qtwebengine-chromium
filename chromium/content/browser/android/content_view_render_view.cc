// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_render_view.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "cc/layers/layer.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_layer_renderer.h"
#include "jni/ContentViewRenderView_jni.h"
#include "ui/gfx/size.h"

#include <android/native_window_jni.h>

using base::android::ScopedJavaLocalRef;

namespace content {

// static
bool ContentViewRenderView::RegisterContentViewRenderView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentViewRenderView::ContentViewRenderView(JNIEnv* env, jobject obj)
    : buffers_swapped_during_composite_(false) {
  java_obj_.Reset(env, obj);
}

ContentViewRenderView::~ContentViewRenderView() {
}

// static
static jint Init(JNIEnv* env, jobject obj) {
  ContentViewRenderView* content_view_render_view =
      new ContentViewRenderView(env, obj);
  return reinterpret_cast<jint>(content_view_render_view);
}

void ContentViewRenderView::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentViewRenderView::SetCurrentContentView(
    JNIEnv* env, jobject obj, int native_content_view) {
  InitCompositor();
  ContentViewCoreImpl* content_view =
      reinterpret_cast<ContentViewCoreImpl*>(native_content_view);
  if (content_view)
    compositor_->SetRootLayer(content_view->GetLayer());
  else
    compositor_->SetRootLayer(cc::Layer::Create());
}

void ContentViewRenderView::SurfaceCreated(
    JNIEnv* env, jobject obj, jobject jsurface) {
  InitCompositor();
  compositor_->SetSurface(jsurface);
}

void ContentViewRenderView::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  compositor_->SetSurface(NULL);
}

void ContentViewRenderView::SurfaceSetSize(
    JNIEnv* env, jobject obj, jint width, jint height) {
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

jboolean ContentViewRenderView::Composite(JNIEnv* env, jobject obj) {
  if (!compositor_)
    return false;

  buffers_swapped_during_composite_ = false;
  compositor_->Composite();
  return buffers_swapped_during_composite_;
}

void ContentViewRenderView::ScheduleComposite() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentViewRenderView_requestRender(env, java_obj_.obj());
}

void ContentViewRenderView::OnSwapBuffersPosted() {
  buffers_swapped_during_composite_ = true;
}

void ContentViewRenderView::OnSwapBuffersCompleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentViewRenderView_onSwapBuffersCompleted(env, java_obj_.obj());
}

void ContentViewRenderView::InitCompositor() {
  if (!compositor_)
    compositor_.reset(Compositor::Create(this));
}

}  // namespace content
