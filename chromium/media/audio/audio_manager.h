// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_parameters.h"

namespace base {
class MessageLoop;
class MessageLoopProxy;
}

namespace media {

class AudioInputStream;
class AudioOutputStream;

// Manages all audio resources. In particular it owns the AudioOutputStream
// objects. Provides some convenience functions that avoid the need to provide
// iterators over the existing streams.
class MEDIA_EXPORT AudioManager {
 public:
  virtual ~AudioManager();

  // Use to construct the audio manager.
  // NOTE: There should only be one instance.
  static AudioManager* Create();

  // Returns the pointer to the last created instance, or NULL if not yet
  // created. This is a utility method for the code outside of media directory,
  // like src/chrome.
  static AudioManager* Get();

  // Returns true if the OS reports existence of audio devices. This does not
  // guarantee that the existing devices support all formats and sample rates.
  virtual bool HasAudioOutputDevices() = 0;

  // Returns true if the OS reports existence of audio recording devices. This
  // does not guarantee that the existing devices support all formats and
  // sample rates.
  virtual bool HasAudioInputDevices() = 0;

  // Returns a human readable string for the model/make of the active audio
  // input device for this computer.
  virtual string16 GetAudioInputDeviceModel() = 0;

  // Opens the platform default audio input settings UI.
  // Note: This could invoke an external application/preferences pane, so
  // ideally must not be called from the UI thread or other time sensitive
  // threads to avoid blocking the rest of the application.
  virtual void ShowAudioInputSettings() = 0;

  // Appends a list of available input devices to |device_names|,
  // which must initially be empty. It is not guaranteed that all the
  // devices in the list support all formats and sample rates for
  // recording.
  virtual void GetAudioInputDeviceNames(AudioDeviceNames* device_names) = 0;

  // Appends a list of available output devices to |device_names|,
  // which must initially be empty.
  virtual void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) = 0;

  // Factory for all the supported stream formats. |params| defines parameters
  // of the audio stream to be created.
  //
  // |params.sample_per_packet| is the requested buffer allocation which the
  // audio source thinks it can usually fill without blocking. Internally two
  // or three buffers are created, one will be locked for playback and one will
  // be ready to be filled in the call to AudioSourceCallback::OnMoreData().
  //
  // To create a stream for the default output device, pass an empty string
  // for |device_id|, otherwise the specified audio device will be opened.
  //
  // The |input_device_id| is used for low-latency unified streams
  // (input+output) only and then only if the audio parameters specify a >0
  // input channel count.  In other cases this id is ignored and should be
  // empty.
  //
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // |params.format| can be set to AUDIO_PCM_LOW_LATENCY and that has two
  // effects:
  // 1- Instead of triple buffered the audio will be double buffered.
  // 2- A low latency driver or alternative audio subsystem will be used when
  //    available.
  //
  // Do not free the returned AudioOutputStream. It is owned by AudioManager.
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) = 0;

  // Creates new audio output proxy. A proxy implements
  // AudioOutputStream interface, but unlike regular output stream
  // created with MakeAudioOutputStream() it opens device only when a
  // sound is actually playing.
  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) = 0;

  // Factory to create audio recording streams.
  // |channels| can be 1 or 2.
  // |sample_rate| is in hertz and can be any value supported by the platform.
  // |bits_per_sample| can be any value supported by the platform.
  // |samples_per_packet| is in hertz as well and can be 0 to |sample_rate|,
  // with 0 suggesting that the implementation use a default value for that
  // platform.
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // Do not free the returned AudioInputStream. It is owned by AudioManager.
  // When you are done with it, call |Stop()| and |Close()| to release it.
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Returns message loop used for audio IO.
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() = 0;

  // Heavyweight tasks should use GetWorkerLoop() instead of GetMessageLoop().
  // On most platforms they are the same, but some share the UI loop with the
  // audio IO loop.
  virtual scoped_refptr<base::MessageLoopProxy> GetWorkerLoop() = 0;

  // Allows clients to listen for device state changes; e.g. preferred sample
  // rate or channel layout changes.  The typical response to receiving this
  // callback is to recreate the stream.
  class AudioDeviceListener {
   public:
    virtual void OnDeviceChange() = 0;
  };

  virtual void AddOutputDeviceChangeListener(AudioDeviceListener* listener) = 0;
  virtual void RemoveOutputDeviceChangeListener(
      AudioDeviceListener* listener) = 0;

  // Returns the default output hardware audio parameters for opening output
  // streams. It is a convenience interface to
  // AudioManagerBase::GetPreferredOutputStreamParameters and each AudioManager
  // does not need their own implementation to this interface.
  // TODO(tommi): Remove this method and use GetOutputStreamParameteres instead.
  virtual AudioParameters GetDefaultOutputStreamParameters() = 0;

  // Returns the output hardware audio parameters for a specific output device.
  virtual AudioParameters GetOutputStreamParameters(
      const std::string& device_id) = 0;

  // Returns the input hardware audio parameters of the specific device
  // for opening input streams. Each AudioManager needs to implement their own
  // version of this interface.
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) = 0;

  // Returns the device id of an output device that belongs to the same hardware
  // as the specified input device.
  // If the hardware has only an input device (e.g. a webcam), the return value
  // will be empty (which the caller can then interpret to be the default output
  // device).  Implementations that don't yet support this feature, must return
  // an empty string.
  virtual std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) = 0;

 protected:
  AudioManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManager);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_H_
