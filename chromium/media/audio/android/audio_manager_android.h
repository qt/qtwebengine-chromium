// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// Android implemention of AudioManager.
class MEDIA_EXPORT AudioManagerAndroid : public AudioManagerBase {
 public:
  AudioManagerAndroid();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual void ReleaseOutputStream(AudioOutputStream* stream) OVERRIDE;
  virtual void ReleaseInputStream(AudioInputStream* stream) OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;

  static bool RegisterAudioManager(JNIEnv* env);

 protected:
  virtual ~AudioManagerAndroid();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) OVERRIDE;

 private:
  void SetAudioMode(int mode);
  void RegisterHeadsetReceiver();
  void UnregisterHeadsetReceiver();
  int GetNativeOutputSampleRate();
  bool IsAudioLowLatencySupported();
  int GetAudioLowLatencyOutputFrameSize();
  int GetOptimalOutputFrameSize(int sample_rate, int channels);

  // Allow the AudioAndroidTest to access private methods.
  FRIEND_TEST_ALL_PREFIXES(AudioAndroidTest, IsAudioLowLatencySupported);

  // Java AudioManager instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerAndroid);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
