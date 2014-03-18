// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/tracing_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/public/browser/tracing_controller.h"
#include "jni/TracingControllerAndroid_jni.h"

namespace content {

static jlong Init(JNIEnv* env, jobject obj) {
  TracingControllerAndroid* profiler = new TracingControllerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

TracingControllerAndroid::TracingControllerAndroid(JNIEnv* env, jobject obj)
    : weak_java_object_(env, obj),
      weak_factory_(this) {}

TracingControllerAndroid::~TracingControllerAndroid() {}

void TracingControllerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

bool TracingControllerAndroid::StartTracing(JNIEnv* env,
                                            jobject obj,
                                            jstring jfilename,
                                            jstring jcategories,
                                            jboolean record_continuously) {
  file_path_ = base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, jfilename));
  std::string categories =
      base::android::ConvertJavaStringToUTF8(env, jcategories);

  // This log is required by adb_profile_chrome.py.
  LOG(WARNING) << "Logging performance trace to file: " << file_path_.value();

  return TracingController::GetInstance()->EnableRecording(
      categories,
      record_continuously ? TracingController::RECORD_CONTINUOUSLY
                          : TracingController::DEFAULT_OPTIONS,
      TracingController::EnableRecordingDoneCallback());
}

void TracingControllerAndroid::StopTracing(JNIEnv* env, jobject obj) {
  if (!TracingController::GetInstance()->DisableRecording(
      file_path_,
      base::Bind(&TracingControllerAndroid::OnTracingStopped,
                 weak_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "EndTracingAsync failed, forcing an immediate stop";
    OnTracingStopped(file_path_);
  }
}

void TracingControllerAndroid::OnTracingStopped(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj())
    Java_TracingControllerAndroid_onTracingStopped(env, obj.obj());
}

static jstring GetDefaultCategories(JNIEnv* env, jobject obj) {
  return base::android::ConvertUTF8ToJavaString(env,
      base::debug::CategoryFilter::kDefaultCategoryFilterString).Release();
}

bool RegisterTracingControllerAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
