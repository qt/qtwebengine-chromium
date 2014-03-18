// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_result.h"
#include "jni/SpeechRecognition_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;
using base::android::JavaFloatArrayToFloatVector;

namespace content {

SpeechRecognizerImplAndroid::SpeechRecognizerImplAndroid(
    SpeechRecognitionEventListener* listener,
    int session_id)
    : SpeechRecognizer(listener, session_id),
      state_(STATE_IDLE) {
}

SpeechRecognizerImplAndroid::~SpeechRecognizerImplAndroid() { }

void SpeechRecognizerImplAndroid::StartRecognition(
    const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(xians): Open the correct device for speech on Android.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &SpeechRecognitionEventListener::OnRecognitionStart,
      base::Unretained(listener()),
      session_id()));
  SpeechRecognitionSessionConfig config =
      SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &content::SpeechRecognizerImplAndroid::StartRecognitionOnUIThread, this,
      config.language, config.continuous, config.interim_results));
}

void SpeechRecognizerImplAndroid::StartRecognitionOnUIThread(
    std::string language, bool continuous, bool interim_results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  j_recognition_.Reset(Java_SpeechRecognition_createSpeechRecognition(env,
      GetApplicationContext(), reinterpret_cast<intptr_t>(this)));
  Java_SpeechRecognition_startRecognition(env, j_recognition_.obj(),
      ConvertUTF8ToJavaString(env, language).obj(), continuous,
      interim_results);
}

void SpeechRecognizerImplAndroid::AbortRecognition() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    state_ = STATE_IDLE;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &content::SpeechRecognizerImplAndroid::AbortRecognition, this));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  if (!j_recognition_.is_null())
    Java_SpeechRecognition_abortRecognition(env, j_recognition_.obj());
}

void SpeechRecognizerImplAndroid::StopAudioCapture() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &content::SpeechRecognizerImplAndroid::StopAudioCapture, this));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  if (!j_recognition_.is_null())
    Java_SpeechRecognition_stopRecognition(env, j_recognition_.obj());
}

bool SpeechRecognizerImplAndroid::IsActive() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return state_ != STATE_IDLE;
}

bool SpeechRecognizerImplAndroid::IsCapturingAudio() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return state_ == STATE_CAPTURING_AUDIO;
}

void SpeechRecognizerImplAndroid::OnAudioStart(JNIEnv* env, jobject obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnAudioStart, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL)));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = STATE_CAPTURING_AUDIO;
  listener()->OnAudioStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundStart(JNIEnv* env, jobject obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnSoundStart, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL)));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  listener()->OnSoundStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundEnd(JNIEnv* env, jobject obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnSoundEnd, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL)));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  listener()->OnSoundEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnAudioEnd(JNIEnv* env, jobject obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnAudioEnd, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL)));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (state_ == STATE_CAPTURING_AUDIO)
    state_ = STATE_AWAITING_FINAL_RESULT;
  listener()->OnAudioEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnRecognitionResults(JNIEnv* env, jobject obj,
    jobjectArray strings, jfloatArray floats, jboolean provisional) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<base::string16> options;
  AppendJavaStringArrayToStringVector(env, strings, &options);
  std::vector<float> scores(options.size(), 0.0);
  if (floats != NULL)
    JavaFloatArrayToFloatVector(env, floats, &scores);
  SpeechRecognitionResults results;
  results.push_back(SpeechRecognitionResult());
  SpeechRecognitionResult& result = results.back();
  CHECK_EQ(options.size(), scores.size());
  for (size_t i = 0; i < options.size(); ++i) {
    result.hypotheses.push_back(SpeechRecognitionHypothesis(
        options[i], static_cast<double>(scores[i])));
  }
  result.is_provisional = provisional;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread,
      this, results));
}

void SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread(
    SpeechRecognitionResults const &results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  listener()->OnRecognitionResults(session_id(), results);
}

void SpeechRecognizerImplAndroid::OnRecognitionError(JNIEnv* env,
    jobject obj, jint error) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnRecognitionError, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL), error));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SpeechRecognitionErrorCode code =
      static_cast<SpeechRecognitionErrorCode>(error);
  listener()->OnRecognitionError(session_id(), SpeechRecognitionError(code));
}

void SpeechRecognizerImplAndroid::OnRecognitionEnd(JNIEnv* env,
    jobject obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &SpeechRecognizerImplAndroid::OnRecognitionEnd, this,
        static_cast<JNIEnv*>(NULL), static_cast<jobject>(NULL)));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = STATE_IDLE;
  listener()->OnRecognitionEnd(session_id());
}

// static
bool SpeechRecognizerImplAndroid::RegisterSpeechRecognizer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
