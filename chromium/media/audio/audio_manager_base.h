// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_manager.h"

#include "media/audio/audio_output_dispatcher.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace base {
class Thread;
}

namespace media {

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  // TODO(sergeyu): The constants below belong to AudioManager interface, not
  // to the base implementation.

  // Name of the generic "default" device.
  static const char kDefaultDeviceName[];
  // Unique Id of the generic "default" device.
  static const char kDefaultDeviceId[];

  // Input device ID used to capture the default system playback stream. When
  // this device ID is passed to MakeAudioInputStream() the returned
  // AudioInputStream will be capturing audio currently being played on the
  // default playback device. At the moment this feature is supported only on
  // some platforms. AudioInputStream::Intialize() will return an error on
  // platforms that don't support it. GetInputStreamParameters() must be used
  // to get the parameters of the loopback device before creating a loopback
  // stream, otherwise stream initialization may fail.
  static const char kLoopbackInputDeviceId[];

  virtual ~AudioManagerBase();

  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetWorkerLoop() OVERRIDE;

  virtual string16 GetAudioInputDeviceModel() OVERRIDE;

  virtual void ShowAudioInputSettings() OVERRIDE;

  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;

  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) OVERRIDE;

  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) OVERRIDE;

  // Called internally by the audio stream when it has been closed.
  virtual void ReleaseOutputStream(AudioOutputStream* stream);
  virtual void ReleaseInputStream(AudioInputStream* stream);

  // Creates the output stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) = 0;

  // Creates the output stream for the |AUDIO_PCM_LOW_LATENCY| format.
  // |input_device_id| is used by unified IO to open the correct input device.
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) = 0;

  // Creates the input stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Creates the input stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Listeners will be notified on the AudioManager::GetMessageLoop() loop.
  virtual void AddOutputDeviceChangeListener(
      AudioDeviceListener* listener) OVERRIDE;
  virtual void RemoveOutputDeviceChangeListener(
      AudioDeviceListener* listener) OVERRIDE;

  virtual AudioParameters GetDefaultOutputStreamParameters() OVERRIDE;
  virtual AudioParameters GetOutputStreamParameters(
      const std::string& device_id) OVERRIDE;

  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;

  virtual std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) OVERRIDE;

 protected:
  AudioManagerBase();


  // Shuts down the audio thread and releases all the audio output dispatchers
  // on the audio thread.  All audio streams should be freed before Shutdown()
  // is called.  This must be called in the destructor of every AudioManagerBase
  // implementation.
  void Shutdown();

  void SetMaxOutputStreamsAllowed(int max) { max_num_output_streams_ = max; }

  // Called by each platform specific AudioManager to notify output state change
  // listeners that a state change has occurred.  Must be called from the audio
  // thread.
  void NotifyAllOutputDeviceChangeListeners();

  // Returns the preferred hardware audio output parameters for opening output
  // streams. If the users inject a valid |input_params|, each AudioManager
  // will decide if they should return the values from |input_params| or the
  // default hardware values. If the |input_params| is invalid, it will return
  // the default hardware audio parameters.
  // If |output_device_id| is empty, the implementation must treat that as
  // a request for the default output device.
  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) = 0;

  // Returns the ID of the default audio output device.
  // Implementations that don't yet support this should return an empty string.
  virtual std::string GetDefaultOutputDeviceID();

  // Get number of input or output streams.
  int input_stream_count() { return num_input_streams_; }
  int output_stream_count() { return num_output_streams_; }

 private:
  struct DispatcherParams;
  typedef ScopedVector<DispatcherParams> AudioOutputDispatchers;

  class CompareByParams;

  // Called by Shutdown().
  void ShutdownOnAudioThread();

  // Max number of open output streams, modified by
  // SetMaxOutputStreamsAllowed().
  int max_num_output_streams_;

  // Max number of open input streams.
  int max_num_input_streams_;

  // Number of currently open output streams.
  int num_output_streams_;

  // Number of currently open input streams.
  int num_input_streams_;

  // Track output state change listeners.
  ObserverList<AudioDeviceListener> output_listeners_;

  // Thread used to interact with audio streams created by this audio manager.
  scoped_ptr<base::Thread> audio_thread_;
  mutable base::Lock audio_thread_lock_;

  // The message loop of the audio thread this object runs on. Used for internal
  // tasks which run on the audio thread even after Shutdown() has been started
  // and GetMessageLoop() starts returning NULL.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // Map of cached AudioOutputDispatcher instances.  Must only be touched
  // from the audio thread (no locking).
  AudioOutputDispatchers output_dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
