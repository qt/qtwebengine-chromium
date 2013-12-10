// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/audio_util.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/base/media_switches.h"

namespace media {

static const int kStreamCloseDelaySeconds = 5;

// Default maximum number of output streams that can be open simultaneously
// for all platforms.
static const int kDefaultMaxOutputStreams = 16;

// Default maximum number of input streams that can be open simultaneously
// for all platforms.
static const int kDefaultMaxInputStreams = 16;

static const int kMaxInputChannels = 2;

const char AudioManagerBase::kDefaultDeviceName[] = "Default";
const char AudioManagerBase::kDefaultDeviceId[] = "default";
const char AudioManagerBase::kLoopbackInputDeviceId[] = "loopback";

struct AudioManagerBase::DispatcherParams {
  DispatcherParams(const AudioParameters& input,
                   const AudioParameters& output,
                   const std::string& output_device_id,
                   const std::string& input_device_id)
      : input_params(input),
        output_params(output),
        input_device_id(input_device_id),
        output_device_id(output_device_id) {}
  ~DispatcherParams() {}

  const AudioParameters input_params;
  const AudioParameters output_params;
  const std::string input_device_id;
  const std::string output_device_id;
  scoped_refptr<AudioOutputDispatcher> dispatcher;

 private:
  DISALLOW_COPY_AND_ASSIGN(DispatcherParams);
};

class AudioManagerBase::CompareByParams {
 public:
  explicit CompareByParams(const DispatcherParams* dispatcher)
      : dispatcher_(dispatcher) {}
  bool operator()(DispatcherParams* dispatcher_in) const {
    // We will reuse the existing dispatcher when:
    // 1) Unified IO is not used, input_params and output_params of the
    //    existing dispatcher are the same as the requested dispatcher.
    // 2) Unified IO is used, input_params, output_params and input_device_id
    //    of the existing dispatcher are the same as the request dispatcher.
    return (dispatcher_->input_params == dispatcher_in->input_params &&
            dispatcher_->output_params == dispatcher_in->output_params &&
            dispatcher_->output_device_id == dispatcher_in->output_device_id &&
            (!dispatcher_->input_params.input_channels() ||
             dispatcher_->input_device_id == dispatcher_in->input_device_id));
  }

 private:
  const DispatcherParams* dispatcher_;
};

AudioManagerBase::AudioManagerBase()
    : max_num_output_streams_(kDefaultMaxOutputStreams),
      max_num_input_streams_(kDefaultMaxInputStreams),
      num_output_streams_(0),
      num_input_streams_(0),
      // TODO(dalecurtis): Switch this to an ObserverListThreadSafe, so we don't
      // block the UI thread when swapping devices.
      output_listeners_(
          ObserverList<AudioDeviceListener>::NOTIFY_EXISTING_ONLY),
      audio_thread_(new base::Thread("AudioThread")) {
#if defined(OS_WIN)
  audio_thread_->init_com_with_mta(true);
#elif defined(OS_MACOSX)
  // CoreAudio calls must occur on the main thread of the process, which in our
  // case is sadly the browser UI thread.  Failure to execute calls on the right
  // thread leads to crashes and odd behavior.  See http://crbug.com/158170.
  // TODO(dalecurtis): We should require the message loop to be passed in.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableMainThreadAudio) &&
      base::MessageLoopProxy::current().get() &&
      base::MessageLoop::current()->IsType(base::MessageLoop::TYPE_UI)) {
    message_loop_ = base::MessageLoopProxy::current();
    return;
  }
#endif

  CHECK(audio_thread_->Start());
  message_loop_ = audio_thread_->message_loop_proxy();
}

AudioManagerBase::~AudioManagerBase() {
  // The platform specific AudioManager implementation must have already
  // stopped the audio thread. Otherwise, we may destroy audio streams before
  // stopping the thread, resulting an unexpected behavior.
  // This way we make sure activities of the audio streams are all stopped
  // before we destroy them.
  CHECK(!audio_thread_.get());
  // All the output streams should have been deleted.
  DCHECK_EQ(0, num_output_streams_);
  // All the input streams should have been deleted.
  DCHECK_EQ(0, num_input_streams_);
}

string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return string16();
}

scoped_refptr<base::MessageLoopProxy> AudioManagerBase::GetMessageLoop() {
  return message_loop_;
}

scoped_refptr<base::MessageLoopProxy> AudioManagerBase::GetWorkerLoop() {
  // Lazily start the worker thread.
  if (!audio_thread_->IsRunning())
    CHECK(audio_thread_->Start());

  return audio_thread_->message_loop_proxy();
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  // TODO(miu): Fix ~50 call points across several unit test modules to call
  // this method on the audio thread, then uncomment the following:
  // DCHECK(message_loop_->BelongsToCurrentThread());

  if (!params.IsValid()) {
    DLOG(ERROR) << "Audio parameters are invalid";
    return NULL;
  }

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242.
  if (num_output_streams_ >= max_num_output_streams_) {
    DLOG(ERROR) << "Number of opened output audio streams "
                << num_output_streams_
                << " exceed the max allowed number "
                << max_num_output_streams_;
    return NULL;
  }

  AudioOutputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      DCHECK(device_id.empty())
          << "AUDIO_PCM_LINEAR supports only the default device.";
      stream = MakeLinearOutputStream(params);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyOutputStream(params, device_id, input_device_id);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioOutputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream) {
    ++num_output_streams_;
  }

  return stream;
}

AudioInputStream* AudioManagerBase::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  // TODO(miu): Fix ~20 call points across several unit test modules to call
  // this method on the audio thread, then uncomment the following:
  // DCHECK(message_loop_->BelongsToCurrentThread());

  if (!params.IsValid() || (params.channels() > kMaxInputChannels) ||
      device_id.empty()) {
    DLOG(ERROR) << "Audio parameters are invalid for device " << device_id;
    return NULL;
  }

  if (num_input_streams_ >= max_num_input_streams_) {
    DLOG(ERROR) << "Number of opened input audio streams "
                << num_input_streams_
                << " exceed the max allowed number " << max_num_input_streams_;
    return NULL;
  }

  AudioInputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      stream = MakeLinearInputStream(params, device_id);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyInputStream(params, device_id);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioInputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = NULL;
      break;
  }

  if (stream) {
    ++num_input_streams_;
  }

  return stream;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // If the caller supplied an empty device id to select the default device,
  // we fetch the actual device id of the default device so that the lookup
  // will find the correct device regardless of whether it was opened as
  // "default" or via the specific id.
  // NOTE: Implementations that don't yet support opening non-default output
  // devices may return an empty string from GetDefaultOutputDeviceID().
  std::string output_device_id = device_id.empty() ?
      GetDefaultOutputDeviceID() : device_id;

  // If we're not using AudioOutputResampler our output parameters are the same
  // as our input parameters.
  AudioParameters output_params = params;
  if (params.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    output_params =
        GetPreferredOutputStreamParameters(output_device_id, params);

    // Ensure we only pass on valid output parameters.
    if (!output_params.IsValid()) {
      // We've received invalid audio output parameters, so switch to a mock
      // output device based on the input parameters.  This may happen if the OS
      // provided us junk values for the hardware configuration.
      LOG(ERROR) << "Invalid audio output parameters received; using fake "
                 << "audio path. Channels: " << output_params.channels() << ", "
                 << "Sample Rate: " << output_params.sample_rate() << ", "
                 << "Bits Per Sample: " << output_params.bits_per_sample()
                 << ", Frames Per Buffer: "
                 << output_params.frames_per_buffer();

      // Tell the AudioManager to create a fake output device.
      output_params = AudioParameters(
          AudioParameters::AUDIO_FAKE, params.channel_layout(),
          params.sample_rate(), params.bits_per_sample(),
          params.frames_per_buffer());
    }
  }

  DispatcherParams* dispatcher_params =
      new DispatcherParams(params, output_params, output_device_id,
          input_device_id);

  AudioOutputDispatchers::iterator it =
      std::find_if(output_dispatchers_.begin(), output_dispatchers_.end(),
                   CompareByParams(dispatcher_params));
  if (it != output_dispatchers_.end()) {
    delete dispatcher_params;
    return new AudioOutputProxy((*it)->dispatcher.get());
  }

  const base::TimeDelta kCloseDelay =
      base::TimeDelta::FromSeconds(kStreamCloseDelaySeconds);
  scoped_refptr<AudioOutputDispatcher> dispatcher;
  if (output_params.format() != AudioParameters::AUDIO_FAKE) {
    dispatcher = new AudioOutputResampler(this, params, output_params,
                                          output_device_id, input_device_id,
                                          kCloseDelay);
  } else {
    dispatcher = new AudioOutputDispatcherImpl(this, output_params,
                                               output_device_id,
                                               input_device_id, kCloseDelay);
  }

  dispatcher_params->dispatcher = dispatcher;
  output_dispatchers_.push_back(dispatcher_params);
  return new AudioOutputProxy(dispatcher.get());
}

void AudioManagerBase::ShowAudioInputSettings() {
}

void AudioManagerBase::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioOutputStream.
  // For example, pass the ownership to AudioManager so it can delete the
  // streams.
  --num_output_streams_;
  delete stream;
}

void AudioManagerBase::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioInputStream.
  --num_input_streams_;
  delete stream;
}

void AudioManagerBase::Shutdown() {
  // To avoid running into deadlocks while we stop the thread, shut it down
  // via a local variable while not holding the audio thread lock.
  scoped_ptr<base::Thread> audio_thread;
  {
    base::AutoLock lock(audio_thread_lock_);
    audio_thread_.swap(audio_thread);
  }

  if (!audio_thread)
    return;

  // Only true when we're sharing the UI message loop with the browser.  The UI
  // loop is no longer running at this time and browser destruction is imminent.
  if (message_loop_->BelongsToCurrentThread()) {
    ShutdownOnAudioThread();
  } else {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &AudioManagerBase::ShutdownOnAudioThread, base::Unretained(this)));
  }

  // Stop() will wait for any posted messages to be processed first.
  audio_thread->Stop();
}

void AudioManagerBase::ShutdownOnAudioThread() {
  // This should always be running on the audio thread, but since we've cleared
  // the audio_thread_ member pointer when we get here, we can't verify exactly
  // what thread we're running on.  The method is not public though and only
  // called from one place, so we'll leave it at that.
  AudioOutputDispatchers::iterator it = output_dispatchers_.begin();
  for (; it != output_dispatchers_.end(); ++it) {
    scoped_refptr<AudioOutputDispatcher>& dispatcher = (*it)->dispatcher;
    if (dispatcher.get()) {
      dispatcher->Shutdown();
      // All AudioOutputProxies must have been freed before Shutdown is called.
      // If they still exist, things will go bad.  They have direct pointers to
      // both physical audio stream objects that belong to the dispatcher as
      // well as the message loop of the audio thread that will soon go away.
      // So, better crash now than later.
      DCHECK(dispatcher->HasOneRef()) << "AudioOutputProxies are still alive";
      dispatcher = NULL;
    }
  }

  output_dispatchers_.clear();
}

void AudioManagerBase::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  output_listeners_.AddObserver(listener);
}

void AudioManagerBase::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  output_listeners_.RemoveObserver(listener);
}

void AudioManagerBase::NotifyAllOutputDeviceChangeListeners() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DVLOG(1) << "Firing OnDeviceChange() notifications.";
  FOR_EACH_OBSERVER(AudioDeviceListener, output_listeners_, OnDeviceChange());
}

AudioParameters AudioManagerBase::GetDefaultOutputStreamParameters() {
  return GetPreferredOutputStreamParameters(GetDefaultOutputDeviceID(),
      AudioParameters());
}

AudioParameters AudioManagerBase::GetOutputStreamParameters(
    const std::string& device_id) {
  return GetPreferredOutputStreamParameters(device_id,
      AudioParameters());
}

AudioParameters AudioManagerBase::GetInputStreamParameters(
    const std::string& device_id) {
  NOTREACHED();
  return AudioParameters();
}

std::string AudioManagerBase::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  NOTIMPLEMENTED();
  return "";
}

std::string AudioManagerBase::GetDefaultOutputDeviceID() {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace media
