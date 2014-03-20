// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_record_input.h"

#include "base/logging.h"
#include "jni/AudioRecordInput_jni.h"
#include "media/audio/android/audio_manager_android.h"

namespace media {

AudioRecordInputStream::AudioRecordInputStream(
    AudioManagerAndroid* audio_manager, const AudioParameters& params)
    : audio_manager_(audio_manager),
      callback_(NULL),
      direct_buffer_address_(NULL) {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(params.IsValid());
  j_audio_record_.Reset(
      Java_AudioRecordInput_createAudioRecordInput(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          params.sample_rate(),
          params.channels(),
          params.bits_per_sample(),
          params.GetBytesPerBuffer(),
          params.effects() & AudioParameters::ECHO_CANCELLER));
}

AudioRecordInputStream::~AudioRecordInputStream() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioRecordInputStream::CacheDirectBufferAddress(JNIEnv* env, jobject obj,
                                                      jobject byte_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  direct_buffer_address_ = static_cast<uint8*>(
      env->GetDirectBufferAddress(byte_buffer));
}

// static
bool AudioRecordInputStream::RegisterAudioRecordInput(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AudioRecordInputStream::OnData(JNIEnv* env, jobject obj, jint size,
                                    jint hardware_delay_bytes) {
  DCHECK(direct_buffer_address_);
  // Passing zero as the volume parameter indicates there is no access to a
  // hardware volume slider.
  callback_->OnData(this, direct_buffer_address_, size, hardware_delay_bytes,
                    0.0);
}

bool AudioRecordInputStream::Open() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  return Java_AudioRecordInput_open(
      base::android::AttachCurrentThread(), j_audio_record_.obj());
}

void AudioRecordInputStream::Start(AudioInputCallback* callback) {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (callback_) {
    // Start() was already called.
    DCHECK_EQ(callback_, callback);
    return;
  }
  // The Java thread has not yet started, so we are free to set |callback_|.
  callback_ = callback;

  Java_AudioRecordInput_start(
      base::android::AttachCurrentThread(), j_audio_record_.obj());
}

void AudioRecordInputStream::Stop() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!callback_) {
    // Start() was never called, or Stop() was already called.
    return;
  }

  Java_AudioRecordInput_stop(
      base::android::AttachCurrentThread(), j_audio_record_.obj());

  // The Java thread must have been stopped at this point, so we are free to
  // set |callback_|.
  callback_->OnClose(this);
  callback_ = NULL;
}

void AudioRecordInputStream::Close() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  Stop();
  DCHECK(!callback_);
  Java_AudioRecordInput_close(
      base::android::AttachCurrentThread(), j_audio_record_.obj());
  audio_manager_->ReleaseInputStream(this);
}

double AudioRecordInputStream::GetMaxVolume() {
  NOTIMPLEMENTED();
  return 0.0;
}

void AudioRecordInputStream::SetVolume(double volume) {
  NOTIMPLEMENTED();
}

double AudioRecordInputStream::GetVolume() {
  NOTIMPLEMENTED();
  return 0.0;
}

void AudioRecordInputStream::SetAutomaticGainControl(bool enabled) {
  NOTIMPLEMENTED();
}

bool AudioRecordInputStream::GetAutomaticGainControl() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
