// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/limits.h"
#include "media/base/scoped_histogram_timer.h"
#include "media/base/user_input_monitor.h"

namespace {
const int kMaxInputChannels = 2;

// TODO(henrika): remove usage of timers and add support for proper
// notification of when the input device is removed.  This was originally added
// to resolve http://crbug.com/79936 for Windows platforms.  This then caused
// breakage (very hard to repro bugs!) on other platforms: See
// http://crbug.com/226327 and http://crbug.com/230972.
const int kTimerResetIntervalSeconds = 1;
// We have received reports that the timer can be too trigger happy on some
// Mac devices and the initial timer interval has therefore been increased
// from 1 second to 5 seconds.
const int kTimerInitialIntervalSeconds = 5;
}

namespace media {

// static
AudioInputController::Factory* AudioInputController::factory_ = NULL;

AudioInputController::AudioInputController(EventHandler* handler,
                                           SyncWriter* sync_writer,
                                           UserInputMonitor* user_input_monitor)
    : creator_loop_(base::MessageLoopProxy::current()),
      handler_(handler),
      stream_(NULL),
      data_is_active_(false),
      state_(kEmpty),
      sync_writer_(sync_writer),
      max_volume_(0.0),
      user_input_monitor_(user_input_monitor),
      prev_key_down_count_(0) {
  DCHECK(creator_loop_.get());
}

AudioInputController::~AudioInputController() {
  DCHECK(kClosed == state_ || kCreated == state_ || kEmpty == state_);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    UserInputMonitor* user_input_monitor) {
  DCHECK(audio_manager);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  if (factory_) {
    return factory_->Create(
        audio_manager, event_handler, params, user_input_monitor);
  }
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, NULL, user_input_monitor));

  controller->message_loop_ = audio_manager->GetMessageLoop();

  // Create and open a new audio input stream from the existing
  // audio-device thread.
  if (!controller->message_loop_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateLowLatency(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    SyncWriter* sync_writer,
    UserInputMonitor* user_input_monitor) {
  DCHECK(audio_manager);
  DCHECK(sync_writer);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  // Create the AudioInputController object and ensure that it runs on
  // the audio-manager thread.
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, sync_writer, user_input_monitor));
  controller->message_loop_ = audio_manager->GetMessageLoop();

  // Create and open a new audio input stream from the existing
  // audio-device thread. Use the provided audio-input device.
  if (!controller->message_loop_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateForStream(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    EventHandler* event_handler,
    AudioInputStream* stream,
    SyncWriter* sync_writer,
    UserInputMonitor* user_input_monitor) {
  DCHECK(sync_writer);
  DCHECK(stream);

  // Create the AudioInputController object and ensure that it runs on
  // the audio-manager thread.
  scoped_refptr<AudioInputController> controller(
      new AudioInputController(event_handler, sync_writer, user_input_monitor));
  controller->message_loop_ = message_loop;

  // TODO(miu): See TODO at top of file.  Until that's resolved, we need to
  // disable the error auto-detection here (since the audio mirroring
  // implementation will reliably report error and close events).  Note, of
  // course, that we're assuming CreateForStream() has been called for the audio
  // mirroring use case only.
  if (!controller->message_loop_->PostTask(
          FROM_HERE,
          base::Bind(&AudioInputController::DoCreateForStream, controller,
                     stream, false))) {
    controller = NULL;
  }

  return controller;
}

void AudioInputController::Record() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoRecord, this));
}

void AudioInputController::Close(const base::Closure& closed_task) {
  DCHECK(!closed_task.is_null());
  DCHECK(creator_loop_->BelongsToCurrentThread());

  message_loop_->PostTaskAndReply(
      FROM_HERE, base::Bind(&AudioInputController::DoClose, this), closed_task);
}

void AudioInputController::SetVolume(double volume) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoSetVolume, this, volume));
}

void AudioInputController::SetAutomaticGainControl(bool enabled) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoSetAutomaticGainControl, this, enabled));
}

void AudioInputController::DoCreate(AudioManager* audio_manager,
                                    const AudioParameters& params,
                                    const std::string& device_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CreateTime");
  // TODO(miu): See TODO at top of file.  Until that's resolved, assume all
  // platform audio input requires the |no_data_timer_| be used to auto-detect
  // errors.  In reality, probably only Windows needs to be treated as
  // unreliable here.
  DoCreateForStream(audio_manager->MakeAudioInputStream(params, device_id),
                    true);
}

void AudioInputController::DoCreateForStream(
    AudioInputStream* stream_to_control, bool enable_nodata_timer) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(!stream_);
  stream_ = stream_to_control;

  if (!stream_) {
    handler_->OnError(this);
    return;
  }

  if (stream_ && !stream_->Open()) {
    stream_->Close();
    stream_ = NULL;
    handler_->OnError(this);
    return;
  }

  DCHECK(!no_data_timer_.get());
  if (enable_nodata_timer) {
    // Create the data timer which will call DoCheckForNoData(). The timer
    // is started in DoRecord() and restarted in each DoCheckForNoData()
    // callback.
    no_data_timer_.reset(new base::Timer(
        FROM_HERE, base::TimeDelta::FromSeconds(kTimerInitialIntervalSeconds),
        base::Bind(&AudioInputController::DoCheckForNoData,
                   base::Unretained(this)), false));
  } else {
    DVLOG(1) << "Disabled: timer check for no data.";
  }

  state_ = kCreated;
  handler_->OnCreated(this);

  if (user_input_monitor_) {
    user_input_monitor_->EnableKeyPressMonitoring();
    prev_key_down_count_ = user_input_monitor_->GetKeyPressCount();
  }
}

void AudioInputController::DoRecord() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.RecordTime");

  if (state_ != kCreated)
    return;

  {
    base::AutoLock auto_lock(lock_);
    state_ = kRecording;
  }

  if (no_data_timer_) {
    // Start the data timer. Once |kTimerResetIntervalSeconds| have passed,
    // a callback to DoCheckForNoData() is made.
    no_data_timer_->Reset();
  }

  stream_->Start(this);
  handler_->OnRecording(this);
}

void AudioInputController::DoClose() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CloseTime");

  // Delete the timer on the same thread that created it.
  no_data_timer_.reset();

  if (state_ != kClosed) {
    DoStopCloseAndClearStream(NULL);
    SetDataIsActive(false);

    if (LowLatencyMode()) {
      sync_writer_->Close();
    }

    state_ = kClosed;

    if (user_input_monitor_)
      user_input_monitor_->DisableKeyPressMonitoring();
  }
}

void AudioInputController::DoReportError() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  handler_->OnError(this);
}

void AudioInputController::DoSetVolume(double volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1.0);

  if (state_ != kCreated && state_ != kRecording)
    return;

  // Only ask for the maximum volume at first call and use cached value
  // for remaining function calls.
  if (!max_volume_) {
    max_volume_ = stream_->GetMaxVolume();
  }

  if (max_volume_ == 0.0) {
    DLOG(WARNING) << "Failed to access input volume control";
    return;
  }

  // Set the stream volume and scale to a range matched to the platform.
  stream_->SetVolume(max_volume_ * volume);
}

void AudioInputController::DoSetAutomaticGainControl(bool enabled) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, kRecording);

  // Ensure that the AGC state only can be modified before streaming starts.
  if (state_ != kCreated || state_ == kRecording)
    return;

  stream_->SetAutomaticGainControl(enabled);
}

void AudioInputController::DoCheckForNoData() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!GetDataIsActive()) {
    // The data-is-active marker will be false only if it has been more than
    // one second since a data packet was recorded. This can happen if a
    // capture device has been removed or disabled.
    handler_->OnError(this);
    return;
  }

  // Mark data as non-active. The flag will be re-enabled in OnData() each
  // time a data packet is received. Hence, under normal conditions, the
  // flag will only be disabled during a very short period.
  SetDataIsActive(false);

  // Restart the timer to ensure that we check the flag again in
  // |kTimerResetIntervalSeconds|.
  no_data_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kTimerResetIntervalSeconds),
      base::Bind(&AudioInputController::DoCheckForNoData,
      base::Unretained(this)));
}

void AudioInputController::OnData(AudioInputStream* stream,
                                  const uint8* data,
                                  uint32 size,
                                  uint32 hardware_delay_bytes,
                                  double volume) {
  {
    base::AutoLock auto_lock(lock_);
    if (state_ != kRecording)
      return;
  }

  bool key_pressed = false;
  if (user_input_monitor_) {
    size_t current_count = user_input_monitor_->GetKeyPressCount();
    key_pressed = current_count != prev_key_down_count_;
    prev_key_down_count_ = current_count;
    DVLOG_IF(6, key_pressed) << "Detected keypress.";
  }

  // Mark data as active to ensure that the periodic calls to
  // DoCheckForNoData() does not report an error to the event handler.
  SetDataIsActive(true);

  // Use SyncSocket if we are in a low-latency mode.
  if (LowLatencyMode()) {
    sync_writer_->Write(data, size, volume, key_pressed);
    sync_writer_->UpdateRecordedBytes(hardware_delay_bytes);
    return;
  }

  handler_->OnData(this, data, size);
}

void AudioInputController::OnClose(AudioInputStream* stream) {
  DVLOG(1) << "AudioInputController::OnClose()";
  // TODO(satish): Sometimes the device driver closes the input stream without
  // us asking for it (may be if the device was unplugged?). Check how to handle
  // such cases here.
}

void AudioInputController::OnError(AudioInputStream* stream) {
  // Handle error on the audio-manager thread.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoReportError, this));
}

void AudioInputController::DoStopCloseAndClearStream(
    base::WaitableEvent* done) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (stream_ != NULL) {
    stream_->Stop();
    stream_->Close();
    stream_ = NULL;
  }

  // Should be last in the method, do not touch "this" from here on.
  if (done != NULL)
    done->Signal();
}

void AudioInputController::SetDataIsActive(bool enabled) {
  base::subtle::Release_Store(&data_is_active_, enabled);
}

bool AudioInputController::GetDataIsActive() {
  return (base::subtle::Acquire_Load(&data_is_active_) != false);
}

}  // namespace media
