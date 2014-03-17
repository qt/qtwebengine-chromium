// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_device.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace media {

// The number of shared memory buffer segments indicated to browser process
// in order to avoid data overwriting. This number can be any positive number,
// dependent how fast the renderer process can pick up captured data from
// shared memory.
static const int kRequestedSharedMemoryCount = 10;

// Takes care of invoking the capture callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnLowLatencyCreated().
class AudioInputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      int total_segments,
                      CaptureCallback* capture_callback);
  virtual ~AudioThreadCallback();

  virtual void MapSharedMemory() OVERRIDE;

  // Called whenever we receive notifications about pending data.
  virtual void Process(int pending_data) OVERRIDE;

 private:
  int current_segment_id_;
  CaptureCallback* capture_callback_;
  scoped_ptr<AudioBus> audio_bus_;
  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioInputDevice::AudioInputDevice(
    scoped_ptr<AudioInputIPC> ipc,
    const scoped_refptr<base::MessageLoopProxy>& io_loop)
    : ScopedLoopObserver(io_loop),
      callback_(NULL),
      ipc_(ipc.Pass()),
      state_(IDLE),
      session_id_(0),
      agc_is_enabled_(false),
      stopping_hack_(false) {
  CHECK(ipc_);

  // The correctness of the code depends on the relative values assigned in the
  // State enum.
  COMPILE_ASSERT(IPC_CLOSED < IDLE, invalid_enum_value_assignment_0);
  COMPILE_ASSERT(IDLE < CREATING_STREAM, invalid_enum_value_assignment_1);
  COMPILE_ASSERT(CREATING_STREAM < RECORDING, invalid_enum_value_assignment_2);
}

void AudioInputDevice::Initialize(const AudioParameters& params,
                                  CaptureCallback* callback,
                                  int session_id) {
  DCHECK(params.IsValid());
  DCHECK(!callback_);
  DCHECK_EQ(0, session_id_);
  audio_parameters_ = params;
  callback_ = callback;
  session_id_ = session_id;
}

void AudioInputDevice::Start() {
  DCHECK(callback_) << "Initialize hasn't been called";
  DVLOG(1) << "Start()";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::StartUpOnIOThread, this));
}

void AudioInputDevice::Stop() {
  DVLOG(1) << "Stop()";

  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.Stop(base::MessageLoop::current());
    stopping_hack_ = true;
  }

  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::ShutDownOnIOThread, this));
}

void AudioInputDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0) {
    DLOG(ERROR) << "Invalid volume value specified";
    return;
  }

  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetVolumeOnIOThread, this, volume));
}

void AudioInputDevice::SetAutomaticGainControl(bool enabled) {
  DVLOG(1) << "SetAutomaticGainControl(enabled=" << enabled << ")";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioInputDevice::SetAutomaticGainControlOnIOThread,
          this, enabled));
}

void AudioInputDevice::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length,
    int total_segments) {
  DCHECK(message_loop()->BelongsToCurrentThread());
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK_GT(length, 0);

  if (state_ != CREATING_STREAM)
    return;

  base::AutoLock auto_lock(audio_thread_lock_);
  // TODO(miu): See TODO in OnStreamCreated method for AudioOutputDevice.
  // Interface changes need to be made; likely, after AudioInputDevice is merged
  // into AudioOutputDevice (http://crbug.com/179597).
  if (stopping_hack_)
    return;

  DCHECK(audio_thread_.IsStopped());
  audio_callback_.reset(new AudioInputDevice::AudioThreadCallback(
      audio_parameters_, handle, length, total_segments, callback_));
  audio_thread_.Start(
      audio_callback_.get(), socket_handle, "AudioInputDevice", false);

  state_ = RECORDING;
  ipc_->RecordStream();
}

void AudioInputDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

void AudioInputDevice::OnStateChanged(
    AudioInputIPCDelegate::State state) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;

  // TODO(miu): Clean-up inconsistent and incomplete handling here.
  // http://crbug.com/180640
  switch (state) {
    case AudioInputIPCDelegate::kStopped:
      ShutDownOnIOThread();
      break;
    case AudioInputIPCDelegate::kRecording:
      NOTIMPLEMENTED();
      break;
    case AudioInputIPCDelegate::kError:
      DLOG(WARNING) << "AudioInputDevice::OnStateChanged(kError)";
      // Don't dereference the callback object if the audio thread
      // is stopped or stopping.  That could mean that the callback
      // object has been deleted.
      // TODO(tommi): Add an explicit contract for clearing the callback
      // object.  Possibly require calling Initialize again or provide
      // a callback object via Start() and clear it in Stop().
      if (!audio_thread_.IsStopped())
        callback_->OnCaptureError();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioInputDevice::OnIPCClosed() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  state_ = IPC_CLOSED;
  ipc_.reset();
}

AudioInputDevice::~AudioInputDevice() {
  // TODO(henrika): The current design requires that the user calls
  // Stop before deleting this class.
  DCHECK(audio_thread_.IsStopped());
}

void AudioInputDevice::StartUpOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Make sure we don't call Start() more than once.
  if (state_ != IDLE)
    return;

  if (session_id_ <= 0) {
    DLOG(WARNING) << "Invalid session id for the input stream " << session_id_;
    return;
  }

  state_ = CREATING_STREAM;
  ipc_->CreateStream(this, session_id_, audio_parameters_,
                     agc_is_enabled_, kRequestedSharedMemoryCount);
}

void AudioInputDevice::ShutDownOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Close the stream, if we haven't already.
  if (state_ >= CREATING_STREAM) {
    ipc_->CloseStream();
    state_ = IDLE;
    agc_is_enabled_ = false;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  //
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't not rely on the main thread existing either.
  base::AutoLock auto_lock_(audio_thread_lock_);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.Stop(NULL);
  audio_callback_.reset();
  stopping_hack_ = false;
}

void AudioInputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(volume);
}

void AudioInputDevice::SetAutomaticGainControlOnIOThread(bool enabled) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  if (state_ >= CREATING_STREAM) {
    DLOG(WARNING) << "The AGC state can not be modified after starting.";
    return;
  }

  // We simply store the new AGC setting here. This value will be used when
  // a new stream is initialized and by GetAutomaticGainControl().
  agc_is_enabled_ = enabled;
}

void AudioInputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the input device has been stopped";
  ShutDownOnIOThread();
}

// AudioInputDevice::AudioThreadCallback
AudioInputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    base::SharedMemoryHandle memory,
    int memory_length,
    int total_segments,
    CaptureCallback* capture_callback)
    : AudioDeviceThread::Callback(audio_parameters, memory, memory_length,
                                  total_segments),
      current_segment_id_(0),
      capture_callback_(capture_callback) {
  audio_bus_ = AudioBus::Create(audio_parameters_);
}

AudioInputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioInputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_.Map(memory_length_);
}

void AudioInputDevice::AudioThreadCallback::Process(int pending_data) {
  // The shared memory represents parameters, size of the data buffer and the
  // actual data buffer containing audio data. Map the memory into this
  // structure and parse out parameters and the data area.
  uint8* ptr = static_cast<uint8*>(shared_memory_.memory());
  ptr += current_segment_id_ * segment_length_;
  AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
  // Usually this will be equal but in the case of low sample rate (e.g. 8kHz,
  // the buffer may be bigger (on mac at least)).
  DCHECK_GE(buffer->params.size,
            segment_length_ - sizeof(AudioInputBufferParameters));
  double volume = buffer->params.volume;
  bool key_pressed = buffer->params.key_pressed;

  int audio_delay_milliseconds = pending_data / bytes_per_ms_;
  int16* memory = reinterpret_cast<int16*>(&buffer->audio[0]);
  const int bytes_per_sample = sizeof(memory[0]);

  if (++current_segment_id_ >= total_segments_)
    current_segment_id_ = 0;

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0.
  audio_bus_->FromInterleaved(memory, audio_bus_->frames(), bytes_per_sample);

  // Deliver captured data to the client in floating point format
  // and update the audio-delay measurement.
  capture_callback_->Capture(
      audio_bus_.get(), audio_delay_milliseconds, volume, key_pressed);
}

}  // namespace media
