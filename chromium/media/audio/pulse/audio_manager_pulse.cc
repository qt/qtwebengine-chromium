// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/audio_manager_pulse.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "media/audio/alsa/audio_manager_alsa.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/pulse/pulse_input.h"
#include "media/audio/pulse/pulse_output.h"
#include "media/audio/pulse/pulse_unified.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/channel_layout.h"

#if defined(DLOPEN_PULSEAUDIO)
#include "media/audio/pulse/pulse_stubs.h"

using media_audio_pulse::kModulePulse;
using media_audio_pulse::InitializeStubs;
using media_audio_pulse::StubPathMap;
#endif  // defined(DLOPEN_PULSEAUDIO)

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

static const base::FilePath::CharType kPulseLib[] =
    FILE_PATH_LITERAL("libpulse.so.0");

// static
AudioManager* AudioManagerPulse::Create(AudioLogFactory* audio_log_factory) {
  scoped_ptr<AudioManagerPulse> ret(new AudioManagerPulse(audio_log_factory));
  if (ret->Init())
    return ret.release();

  DVLOG(1) << "PulseAudio is not available on the OS";
  return NULL;
}

AudioManagerPulse::AudioManagerPulse(AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory),
      input_mainloop_(NULL),
      input_context_(NULL),
      devices_(NULL),
      native_input_sample_rate_(0) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerPulse::~AudioManagerPulse() {
  Shutdown();

  // The Pulse objects are the last things to be destroyed since Shutdown()
  // needs them.
  DestroyPulse();
}

// Implementation of AudioManager.
bool AudioManagerPulse::HasAudioOutputDevices() {
  AudioDeviceNames devices;
  GetAudioOutputDeviceNames(&devices);
  return !devices.empty();
}

bool AudioManagerPulse::HasAudioInputDevices() {
  AudioDeviceNames devices;
  GetAudioInputDeviceNames(&devices);
  return !devices.empty();
}

void AudioManagerPulse::ShowAudioInputSettings() {
  AudioManagerAlsa::ShowLinuxAudioInputSettings();
}

void AudioManagerPulse::GetAudioDeviceNames(
    bool input, media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  devices_ = device_names;
  pa_operation* operation = NULL;
  if (input) {
    operation = pa_context_get_source_info_list(
      input_context_, InputDevicesInfoCallback, this);
  } else {
    operation = pa_context_get_sink_info_list(
        input_context_, OutputDevicesInfoCallback, this);
  }
  WaitForOperationCompletion(input_mainloop_, operation);

  // Prepend the default device if the list is not empty.
  if (!device_names->empty()) {
    device_names->push_front(
        AudioDeviceName(AudioManagerBase::kDefaultDeviceName,
                        AudioManagerBase::kDefaultDeviceId));
  }
}

void AudioManagerPulse::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(true, device_names);
}

void AudioManagerPulse::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(false, device_names);
}

AudioParameters AudioManagerPulse::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  // TODO(xians): add support for querying native channel layout for pulse.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      GetNativeSampleRate(), 16, kDefaultInputBufferSize);
}

AudioOutputStream* AudioManagerPulse::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params, std::string());
}

AudioOutputStream* AudioManagerPulse::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params, input_device_id);
}

AudioInputStream* AudioManagerPulse::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerPulse::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerPulse::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  static const int kDefaultOutputBufferSize = 512;

  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  int sample_rate;
  if (input_params.IsValid()) {
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = std::min(buffer_size, input_params.frames_per_buffer());
    sample_rate = input_params.sample_rate();
  } else {
    sample_rate = GetNativeSampleRate();
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size, AudioParameters::NO_EFFECTS);
}

AudioOutputStream* AudioManagerPulse::MakeOutputStream(
    const AudioParameters& params, const std::string& input_device_id) {
  if (params.input_channels()) {
    return new PulseAudioUnifiedStream(params, input_device_id, this);
  }

  return new PulseAudioOutputStream(params, this);
}

AudioInputStream* AudioManagerPulse::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new PulseAudioInputStream(this, device_id, params,
                                   input_mainloop_, input_context_);
}

int AudioManagerPulse::GetNativeSampleRate() {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  pa_operation* operation = pa_context_get_server_info(
      input_context_, SampleRateInfoCallback, this);
  WaitForOperationCompletion(input_mainloop_, operation);

  return native_input_sample_rate_;
}

bool AudioManagerPulse::Init() {
  DCHECK(!input_mainloop_);

#if defined(DLOPEN_PULSEAUDIO)
  StubPathMap paths;

  // Check if the pulse library is avialbale.
  paths[kModulePulse].push_back(kPulseLib);
  if (!InitializeStubs(paths)) {
    DLOG(WARNING) << "Failed on loading the Pulse library and symbols";
    return false;
  }
#endif  // defined(DLOPEN_PULSEAUDIO)

  // Create a mainloop API and connect to the default server.
  // The mainloop is the internal asynchronous API event loop.
  input_mainloop_ = pa_threaded_mainloop_new();
  if (!input_mainloop_)
    return false;

  // Start the threaded mainloop.
  if (pa_threaded_mainloop_start(input_mainloop_))
    return false;

  // Lock the event loop object, effectively blocking the event loop thread
  // from processing events. This is necessary.
  AutoPulseLock auto_lock(input_mainloop_);

  pa_mainloop_api* pa_mainloop_api =
      pa_threaded_mainloop_get_api(input_mainloop_);
  input_context_ = pa_context_new(pa_mainloop_api, "Chrome input");
  if (!input_context_)
    return false;

  pa_context_set_state_callback(input_context_, &pulse::ContextStateCallback,
                                input_mainloop_);
  if (pa_context_connect(input_context_, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
    DLOG(ERROR) << "Failed to connect to the context.  Error: "
                << pa_strerror(pa_context_errno(input_context_));
    return false;
  }

  // Wait until |input_context_| is ready.  pa_threaded_mainloop_wait() must be
  // called after pa_context_get_state() in case the context is already ready,
  // otherwise pa_threaded_mainloop_wait() will hang indefinitely.
  while (true) {
    pa_context_state_t context_state = pa_context_get_state(input_context_);
    if (!PA_CONTEXT_IS_GOOD(context_state))
      return false;
    if (context_state == PA_CONTEXT_READY)
      break;
    pa_threaded_mainloop_wait(input_mainloop_);
  }

  return true;
}

void AudioManagerPulse::DestroyPulse() {
  if (!input_mainloop_) {
    DCHECK(!input_context_);
    return;
  }

  {
    AutoPulseLock auto_lock(input_mainloop_);
    if (input_context_) {
      // Clear our state callback.
      pa_context_set_state_callback(input_context_, NULL, NULL);
      pa_context_disconnect(input_context_);
      pa_context_unref(input_context_);
      input_context_ = NULL;
    }
  }

  pa_threaded_mainloop_stop(input_mainloop_);
  pa_threaded_mainloop_free(input_mainloop_);
  input_mainloop_ = NULL;
}

void AudioManagerPulse::InputDevicesInfoCallback(pa_context* context,
                                                 const pa_source_info* info,
                                                 int error, void *user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (error) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  // Exclude the output devices.
  if (info->monitor_of_sink == PA_INVALID_INDEX) {
    manager->devices_->push_back(AudioDeviceName(info->description,
                                                 info->name));
  }
}

void AudioManagerPulse::OutputDevicesInfoCallback(pa_context* context,
                                                  const pa_sink_info* info,
                                                  int error, void *user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (error) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  manager->devices_->push_back(AudioDeviceName(info->description,
                                               info->name));
}

void AudioManagerPulse::SampleRateInfoCallback(pa_context* context,
                                               const pa_server_info* info,
                                               void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  manager->native_input_sample_rate_ = info->sample_spec.rate;
  pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
}

}  // namespace media
