// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_input.h"

#include <math.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "media/audio/audio_manager.h"
#include "media/audio/cras/audio_manager_cras.h"
#include "media/audio/linux/alsa_util.h"

namespace media {

CrasInputStream::CrasInputStream(const AudioParameters& params,
                                 AudioManagerCras* manager,
                                 const std::string& device_id)
    : audio_manager_(manager),
      bytes_per_frame_(0),
      callback_(NULL),
      client_(NULL),
      params_(params),
      started_(false),
      stream_id_(0),
      stream_direction_(device_id == AudioManagerBase::kLoopbackInputDeviceId ?
                            CRAS_STREAM_POST_MIX_PRE_DSP : CRAS_STREAM_INPUT) {
  DCHECK(audio_manager_);
}

CrasInputStream::~CrasInputStream() {
  DCHECK(!client_);
}

bool CrasInputStream::Open() {
  if (client_) {
    NOTREACHED() << "CrasInputStream already open";
    return false;  // Already open.
  }

  // Sanity check input values.
  if (params_.sample_rate() <= 0) {
    DLOG(WARNING) << "Unsupported audio frequency.";
    return false;
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params_.format() &&
      AudioParameters::AUDIO_PCM_LOW_LATENCY != params_.format()) {
    DLOG(WARNING) << "Unsupported audio format.";
    return false;
  }

  snd_pcm_format_t pcm_format =
      alsa_util::BitsToFormat(params_.bits_per_sample());
  if (pcm_format == SND_PCM_FORMAT_UNKNOWN) {
    DLOG(WARNING) << "Unsupported bits/sample: " << params_.bits_per_sample();
    return false;
  }

  // Create the client and connect to the CRAS server.
  if (cras_client_create(&client_) < 0) {
    DLOG(WARNING) << "Couldn't create CRAS client.\n";
    client_ = NULL;
    return false;
  }

  if (cras_client_connect(client_)) {
    DLOG(WARNING) << "Couldn't connect CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  // Then start running the client.
  if (cras_client_run_thread(client_)) {
    DLOG(WARNING) << "Couldn't run CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  return true;
}

void CrasInputStream::Close() {
  if (client_) {
    cras_client_stop(client_);
    cras_client_destroy(client_);
    client_ = NULL;
  }

  if (callback_) {
    callback_->OnClose(this);
    callback_ = NULL;
  }

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  audio_manager_->ReleaseInputStream(this);
}

void CrasInputStream::Start(AudioInputCallback* callback) {
  DCHECK(client_);
  DCHECK(callback);

  // If already playing, stop before re-starting.
  if (started_)
    return;

  StartAgc();

  callback_ = callback;

  // Prepare |audio_format| and |stream_params| for the stream we
  // will create.
  cras_audio_format* audio_format = cras_audio_format_create(
      alsa_util::BitsToFormat(params_.bits_per_sample()),
      params_.sample_rate(),
      params_.channels());
  if (!audio_format) {
    DLOG(WARNING) << "Error setting up audio parameters.";
    callback_->OnError(this);
    callback_ = NULL;
    return;
  }

  unsigned int frames_per_packet = params_.frames_per_buffer();
  cras_stream_params* stream_params = cras_client_stream_params_create(
      stream_direction_,
      frames_per_packet,  // Total latency.
      frames_per_packet,  // Call back when this many ready.
      frames_per_packet,  // Minimum Callback level ignored for capture streams.
      CRAS_STREAM_TYPE_DEFAULT,
      0,  // Unused flags.
      this,
      CrasInputStream::SamplesReady,
      CrasInputStream::StreamError,
      audio_format);
  if (!stream_params) {
    DLOG(WARNING) << "Error setting up stream parameters.";
    callback_->OnError(this);
    callback_ = NULL;
    cras_audio_format_destroy(audio_format);
    return;
  }

  // Before starting the stream, save the number of bytes in a frame for use in
  // the callback.
  bytes_per_frame_ = cras_client_format_bytes_per_frame(audio_format);

  // Adding the stream will start the audio callbacks.
  if (cras_client_add_stream(client_, &stream_id_, stream_params)) {
    DLOG(WARNING) << "Failed to add the stream.";
    callback_->OnError(this);
    callback_ = NULL;
  }

  // Done with config params.
  cras_audio_format_destroy(audio_format);
  cras_client_stream_params_destroy(stream_params);

  started_ = true;
}

void CrasInputStream::Stop() {
  DCHECK(client_);

  if (!callback_ || !started_)
    return;

  StopAgc();

  // Removing the stream from the client stops audio.
  cras_client_rm_stream(client_, stream_id_);

  started_ = false;
}

// Static callback asking for samples.  Run on high priority thread.
int CrasInputStream::SamplesReady(cras_client* client,
                                  cras_stream_id_t stream_id,
                                  uint8* samples,
                                  size_t frames,
                                  const timespec* sample_ts,
                                  void* arg) {
  CrasInputStream* me = static_cast<CrasInputStream*>(arg);
  me->ReadAudio(frames, samples, sample_ts);
  return frames;
}

// Static callback for stream errors.
int CrasInputStream::StreamError(cras_client* client,
                                 cras_stream_id_t stream_id,
                                 int err,
                                 void* arg) {
  CrasInputStream* me = static_cast<CrasInputStream*>(arg);
  me->NotifyStreamError(err);
  return 0;
}

void CrasInputStream::ReadAudio(size_t frames,
                                uint8* buffer,
                                const timespec* sample_ts) {
  DCHECK(callback_);

  timespec latency_ts = {0, 0};

  // Determine latency and pass that on to the sink.  sample_ts is the wall time
  // indicating when the first sample in the buffer was captured.  Convert that
  // to latency in bytes.
  cras_client_calc_capture_latency(sample_ts, &latency_ts);
  double latency_usec =
      latency_ts.tv_sec * base::Time::kMicrosecondsPerSecond +
      latency_ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  double frames_latency =
      latency_usec * params_.sample_rate() / base::Time::kMicrosecondsPerSecond;
  unsigned int bytes_latency =
      static_cast<unsigned int>(frames_latency * bytes_per_frame_);

  // Update the AGC volume level once every second. Note that, |volume| is
  // also updated each time SetVolume() is called through IPC by the
  // render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  callback_->OnData(this,
                    buffer,
                    frames * bytes_per_frame_,
                    bytes_latency,
                    normalized_volume);
}

void CrasInputStream::NotifyStreamError(int err) {
  if (callback_)
    callback_->OnError(this);
}

double CrasInputStream::GetMaxVolume() {
  DCHECK(client_);

  // Capture gain is returned as dB * 100 (150 => 1.5dBFS).  Convert the dB
  // value to a ratio before returning.
  double dB = cras_client_get_system_max_capture_gain(client_) / 100.0;
  return GetVolumeRatioFromDecibels(dB);
}

void CrasInputStream::SetVolume(double volume) {
  DCHECK(client_);

  // Convert from the passed volume ratio, to dB * 100.
  double dB = GetDecibelsFromVolumeRatio(volume);
  cras_client_set_system_capture_gain(client_, static_cast<long>(dB * 100.0));

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double CrasInputStream::GetVolume() {
  if (!client_)
    return 0.0;

  long dB = cras_client_get_system_capture_gain(client_) / 100.0;
  return GetVolumeRatioFromDecibels(dB);
}

double CrasInputStream::GetVolumeRatioFromDecibels(double dB) const {
  return pow(10, dB / 20.0);
}

double CrasInputStream::GetDecibelsFromVolumeRatio(double volume_ratio) const {
  return 20 * log10(volume_ratio);
}

}  // namespace media
