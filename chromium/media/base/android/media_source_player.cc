// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/barrier_closure.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/audio_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/video_decoder_job.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;
}

namespace media {

// static
bool MediaSourcePlayer::IsTypeSupported(
    const std::vector<uint8>& scheme_uuid,
    const std::string& security_level,
    const std::string& container,
    const std::vector<std::string>& codecs) {
  if (!MediaDrmBridge::IsCryptoSchemeSupported(scheme_uuid, container)) {
    DVLOG(1) << "UUID and container '" << container << "' not supported.";
    return false;
  }

  if (!MediaDrmBridge::IsSecurityLevelSupported(scheme_uuid, security_level)) {
    DVLOG(1) << "UUID and security level '" << security_level
             << "' not supported.";
    return false;
  }

  bool is_secure = MediaDrmBridge::IsSecureDecoderRequired(security_level);
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!MediaCodecBridge::CanDecode(codecs[i], is_secure)) {
      DVLOG(1) << "Codec '" << codecs[i] << "' "
               << (is_secure ? "in secure mode " : "") << "not supported.";
      return false;
    }
  }

  return true;
}

MediaSourcePlayer::MediaSourcePlayer(
    int player_id,
    MediaPlayerManager* manager,
    int demuxer_client_id,
    DemuxerAndroid* demuxer)
    : MediaPlayerAndroid(player_id, manager),
      demuxer_client_id_(demuxer_client_id),
      demuxer_(demuxer),
      pending_event_(NO_EVENT_PENDING),
      seek_request_id_(0),
      width_(0),
      height_(0),
      audio_codec_(kUnknownAudioCodec),
      video_codec_(kUnknownVideoCodec),
      num_channels_(0),
      sampling_rate_(0),
      audio_finished_(true),
      video_finished_(true),
      playing_(false),
      is_audio_encrypted_(false),
      is_video_encrypted_(false),
      volume_(-1.0),
      clock_(&default_tick_clock_),
      reconfig_audio_decoder_(false),
      reconfig_video_decoder_(false),
      weak_this_(this),
      drm_bridge_(NULL),
      is_waiting_for_key_(false) {
  demuxer_->AddDemuxerClient(demuxer_client_id_, this);
}

MediaSourcePlayer::~MediaSourcePlayer() {
  demuxer_->RemoveDemuxerClient(demuxer_client_id_);
  Release();
}

void MediaSourcePlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  // For an empty surface, always pass it to the decoder job so that it
  // can detach from the current one. Otherwise, don't pass an unprotected
  // surface if the video content requires a protected one.
  if (!surface.IsEmpty() &&
      IsProtectedSurfaceRequired() && !surface.is_protected()) {
    return;
  }

  surface_ =  surface.Pass();

  // If there is a pending surface change event, just wait for it to be
  // processed.
  if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING))
    return;
  SetPendingEvent(SURFACE_CHANGE_EVENT_PENDING);
  if (IsEventPending(SEEK_EVENT_PENDING)) {
    // Waiting for the seek to finish.
    return;
  }

  // Setting a new surface will require a new MediaCodec to be created.
  // Request a seek so that the new decoder will decode an I-frame first.
  // Or otherwise, the new MediaCodec might crash. See b/8950387.
  ScheduleSeekEventAndStopDecoding();
}

void MediaSourcePlayer::ScheduleSeekEventAndStopDecoding() {
  if (audio_decoder_job_ && audio_decoder_job_->is_decoding())
    audio_decoder_job_->StopDecode();
  if (video_decoder_job_ && video_decoder_job_->is_decoding())
    video_decoder_job_->StopDecode();

  if (IsEventPending(SEEK_EVENT_PENDING))
    return;

  SetPendingEvent(SEEK_EVENT_PENDING);
  ProcessPendingEvents();
}

bool MediaSourcePlayer::Seekable() {
  // If the duration TimeDelta, converted to milliseconds from microseconds,
  // is >= 2^31, then the media is assumed to be unbounded and unseekable.
  // 2^31 is the bound due to java player using 32-bit integer for time
  // values at millisecond resolution.
  return duration_ <
         base::TimeDelta::FromMilliseconds(std::numeric_limits<int32>::max());
}

void MediaSourcePlayer::Start() {
  DVLOG(1) << __FUNCTION__;

  playing_ = true;

  if (IsProtectedSurfaceRequired())
    manager()->OnProtectedSurfaceRequested(player_id());

  StartInternal();
}

void MediaSourcePlayer::Pause(bool is_media_related_action) {
  DVLOG(1) << __FUNCTION__;

  // Since decoder jobs have their own thread, decoding is not fully paused
  // until all the decoder jobs call MediaDecoderCallback(). It is possible
  // that Start() is called while the player is waiting for
  // MediaDecoderCallback(). In that case, decoding will continue when
  // MediaDecoderCallback() is called.
  playing_ = false;
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::IsPlaying() {
  return playing_;
}

int MediaSourcePlayer::GetVideoWidth() {
  return width_;
}

int MediaSourcePlayer::GetVideoHeight() {
  return height_;
}

void MediaSourcePlayer::SeekTo(base::TimeDelta timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  clock_.SetTime(timestamp, timestamp);
  if (audio_timestamp_helper_)
    audio_timestamp_helper_->SetBaseTimestamp(timestamp);
  ScheduleSeekEventAndStopDecoding();
}

base::TimeDelta MediaSourcePlayer::GetCurrentTime() {
  return clock_.Elapsed();
}

base::TimeDelta MediaSourcePlayer::GetDuration() {
  return duration_;
}

void MediaSourcePlayer::Release() {
  DVLOG(1) << __FUNCTION__;
  audio_decoder_job_.reset();
  video_decoder_job_.reset();
  reconfig_audio_decoder_ = false;
  reconfig_video_decoder_ = false;
  playing_ = false;
  pending_event_ = NO_EVENT_PENDING;
  decoder_starvation_callback_.Cancel();
  surface_ = gfx::ScopedJavaSurface();
  ReleaseMediaResourcesFromManager();
}

void MediaSourcePlayer::SetVolume(double volume) {
  volume_ = volume;
  SetVolumeInternal();
}

void MediaSourcePlayer::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;
  if (!is_waiting_for_key_)
    return;

  is_waiting_for_key_ = false;
  if (playing_)
    StartInternal();
}

bool MediaSourcePlayer::CanPause() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekForward() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekBackward() {
  return Seekable();
}

bool MediaSourcePlayer::IsPlayerReady() {
  return audio_decoder_job_ || video_decoder_job_;
}

void MediaSourcePlayer::StartInternal() {
  DVLOG(1) << __FUNCTION__;
  // If there are pending events, wait for them finish.
  if (pending_event_ != NO_EVENT_PENDING)
    return;

  // When we start, we'll have new demuxed data coming in. This new data could
  // be clear (not encrypted) or encrypted with different keys. So
  // |is_waiting_for_key_| condition may not be true anymore.
  is_waiting_for_key_ = false;

  // Create decoder jobs if they are not created
  ConfigureAudioDecoderJob();
  ConfigureVideoDecoderJob();

  // If one of the decoder job is not ready, do nothing.
  if ((HasAudio() && !audio_decoder_job_) ||
      (HasVideo() && !video_decoder_job_)) {
    return;
  }

  audio_finished_ = false;
  video_finished_ = false;
  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  DVLOG(1) << __FUNCTION__;
  duration_ = base::TimeDelta::FromMilliseconds(configs.duration_ms);
  clock_.SetDuration(duration_);

  audio_codec_ = configs.audio_codec;
  num_channels_ = configs.audio_channels;
  sampling_rate_ = configs.audio_sampling_rate;
  is_audio_encrypted_ = configs.is_audio_encrypted;
  audio_extra_data_ = configs.audio_extra_data;
  if (HasAudio()) {
    DCHECK_GT(num_channels_, 0);
    audio_timestamp_helper_.reset(new AudioTimestampHelper(sampling_rate_));
    audio_timestamp_helper_->SetBaseTimestamp(GetCurrentTime());
  } else {
    audio_timestamp_helper_.reset();
  }

  video_codec_ = configs.video_codec;
  width_ = configs.video_size.width();
  height_ = configs.video_size.height();
  is_video_encrypted_ = configs.is_video_encrypted;

  OnMediaMetadataChanged(duration_, width_, height_, true);

  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    if (reconfig_audio_decoder_)
      ConfigureAudioDecoderJob();

    // If there is a pending surface change, we can merge it with the config
    // change.
    if (reconfig_video_decoder_) {
      if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING))
        ClearPendingEvent(SURFACE_CHANGE_EVENT_PENDING);
      ConfigureVideoDecoderJob();
    }

    ClearPendingEvent(CONFIG_CHANGE_EVENT_PENDING);

    // Resume decoding after the config change if we are still playing.
    if (playing_)
      StartInternal();
  }
}

void MediaSourcePlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << "(" << data.type << ")";
  DCHECK_LT(0u, data.access_units.size());
  if (data.type == DemuxerStream::AUDIO && audio_decoder_job_)
    audio_decoder_job_->OnDataReceived(data);
  else if (data.type == DemuxerStream::VIDEO && video_decoder_job_)
    video_decoder_job_->OnDataReceived(data);
}

void MediaSourcePlayer::OnDemuxerDurationChanged(base::TimeDelta duration) {
  duration_ = duration;
  clock_.SetDuration(duration_);
}

base::android::ScopedJavaLocalRef<jobject> MediaSourcePlayer::GetMediaCrypto() {
  base::android::ScopedJavaLocalRef<jobject> media_crypto;
  if (drm_bridge_)
    media_crypto = drm_bridge_->GetMediaCrypto();
  return media_crypto;
}

void MediaSourcePlayer::OnMediaCryptoReady() {
  DCHECK(!drm_bridge_->GetMediaCrypto().is_null());
  drm_bridge_->SetMediaCryptoReadyCB(base::Closure());

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::SetDrmBridge(MediaDrmBridge* drm_bridge) {
  // Currently we don't support DRM change during the middle of playback, even
  // if the player is paused.
  // TODO(qinmin): support DRM change after playback has started.
  // http://crbug.com/253792.
  if (GetCurrentTime() > base::TimeDelta()) {
    LOG(INFO) << "Setting DRM bridge after playback has started. "
              << "This is not well supported!";
  }

  drm_bridge_ = drm_bridge;

  if (drm_bridge_->GetMediaCrypto().is_null()) {
    drm_bridge_->SetMediaCryptoReadyCB(base::Bind(
        &MediaSourcePlayer::OnMediaCryptoReady, weak_this_.GetWeakPtr()));
    return;
  }

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::OnDemuxerSeeked(unsigned seek_request_id) {
  DVLOG(1) << __FUNCTION__ << "(" << seek_request_id << ")";
  // Do nothing until the most recent seek request is processed.
  if (seek_request_id_ != seek_request_id)
    return;

  ClearPendingEvent(SEEK_EVENT_PENDING);

  OnSeekComplete();
  ProcessPendingEvents();
}

void MediaSourcePlayer::UpdateTimestamps(
    const base::TimeDelta& presentation_timestamp, size_t audio_output_bytes) {
  base::TimeDelta new_max_time = presentation_timestamp;

  if (audio_output_bytes > 0) {
    audio_timestamp_helper_->AddFrames(
        audio_output_bytes / (kBytesPerAudioOutputSample * num_channels_));
    new_max_time = audio_timestamp_helper_->GetTimestamp();
  }

  clock_.SetMaxTime(new_max_time);
  OnTimeUpdated();
}

void MediaSourcePlayer::ProcessPendingEvents() {
  DVLOG(1) << __FUNCTION__ << " : 0x" << std::hex << pending_event_;
  // Wait for all the decoding jobs to finish before processing pending tasks.
  if (video_decoder_job_ && video_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : A video job is still decoding.";
    return;
  }

  if (audio_decoder_job_ && audio_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : An audio job is still decoding.";
    return;
  }

  if (IsEventPending(PREFETCH_DONE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : PREFETCH_DONE still pending.";
    return;
  }

  if (IsEventPending(SEEK_EVENT_PENDING)) {
    int seek_request_id = ++seek_request_id_;
    DVLOG(1) << __FUNCTION__ << " : Handling SEEK_EVENT: " << seek_request_id;
    ClearDecodingData();
    demuxer_->RequestDemuxerSeek(
        demuxer_client_id_, GetCurrentTime(), seek_request_id);
    return;
  }

  start_time_ticks_ = base::TimeTicks();
  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling CONFIG_CHANGE_EVENT.";
    DCHECK(reconfig_audio_decoder_ || reconfig_video_decoder_);
    demuxer_->RequestDemuxerConfigs(demuxer_client_id_);
    return;
  }

  if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling SURFACE_CHANGE_EVENT.";
    video_decoder_job_.reset();
    ConfigureVideoDecoderJob();
    ClearPendingEvent(SURFACE_CHANGE_EVENT_PENDING);
  }

  if (IsEventPending(PREFETCH_REQUEST_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling PREFETCH_REQUEST_EVENT.";
    int count = (audio_decoder_job_ ? 1 : 0) + (video_decoder_job_ ? 1 : 0);

    base::Closure barrier = BarrierClosure(count, base::Bind(
        &MediaSourcePlayer::OnPrefetchDone, weak_this_.GetWeakPtr()));

    if (audio_decoder_job_)
      audio_decoder_job_->Prefetch(barrier);

    if (video_decoder_job_)
      video_decoder_job_->Prefetch(barrier);

    SetPendingEvent(PREFETCH_DONE_EVENT_PENDING);
    ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
    return;
  }

  DCHECK_EQ(pending_event_, NO_EVENT_PENDING);

  // Now that all pending events have been handled, resume decoding if we are
  // still playing.
  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::MediaDecoderCallback(
    bool is_audio, MediaCodecStatus status,
    const base::TimeDelta& presentation_timestamp, size_t audio_output_bytes) {
  DVLOG(1) << __FUNCTION__ << ": " << is_audio << ", " << status;
  DCHECK(!is_waiting_for_key_);

  bool is_clock_manager = is_audio || !HasAudio();

  if (is_clock_manager)
    decoder_starvation_callback_.Cancel();

  if (status == MEDIA_CODEC_ERROR) {
    Release();
    OnMediaError(MEDIA_ERROR_DECODE);
    return;
  }

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM) {
    PlaybackCompleted(is_audio);
    return;
  }

  if (status == MEDIA_CODEC_OK && is_clock_manager)
    UpdateTimestamps(presentation_timestamp, audio_output_bytes);

  if (!playing_) {
    if (is_clock_manager)
      clock_.Pause();
    return;
  }

  if (status == MEDIA_CODEC_NO_KEY) {
    is_waiting_for_key_ = true;
    return;
  }

  // If the status is MEDIA_CODEC_STOPPED, stop decoding new data. The player is
  // in the middle of a seek or stop event and needs to wait for the IPCs to
  // come.
  if (status == MEDIA_CODEC_STOPPED)
    return;

  if (status == MEDIA_CODEC_OK && is_clock_manager)
    StartStarvationCallback(presentation_timestamp);

  if (is_audio) {
    DecodeMoreAudio();
    return;
  }

  DecodeMoreVideo();
}

void MediaSourcePlayer::DecodeMoreAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_->is_decoding());

  if (audio_decoder_job_->Decode(
          start_time_ticks_, start_presentation_timestamp_, base::Bind(
              &MediaSourcePlayer::MediaDecoderCallback,
              weak_this_.GetWeakPtr(), true))) {
    return;
  }

  // Failed to start the next decode.
  // Wait for demuxer ready message.
  reconfig_audio_decoder_ = true;
  SetPendingEvent(CONFIG_CHANGE_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::DecodeMoreVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!video_decoder_job_->is_decoding());

  if (video_decoder_job_->Decode(
          start_time_ticks_, start_presentation_timestamp_, base::Bind(
              &MediaSourcePlayer::MediaDecoderCallback,
              weak_this_.GetWeakPtr(), false))) {
    return;
  }

  // Failed to start the next decode.
  // Wait for demuxer ready message.
  reconfig_video_decoder_ = true;
  SetPendingEvent(CONFIG_CHANGE_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::PlaybackCompleted(bool is_audio) {
  DVLOG(1) << __FUNCTION__ << "(" << is_audio << ")";
  if (is_audio)
    audio_finished_ = true;
  else
    video_finished_ = true;

  if ((!HasAudio() || audio_finished_) && (!HasVideo() || video_finished_)) {
    playing_ = false;
    clock_.Pause();
    start_time_ticks_ = base::TimeTicks();
    OnPlaybackComplete();
  }
}

void MediaSourcePlayer::ClearDecodingData() {
  DVLOG(1) << __FUNCTION__;
  if (audio_decoder_job_)
    audio_decoder_job_->Flush();
  if (video_decoder_job_)
    video_decoder_job_->Flush();
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::HasVideo() {
  return kUnknownVideoCodec != video_codec_;
}

bool MediaSourcePlayer::HasAudio() {
  return kUnknownAudioCodec != audio_codec_;
}

void MediaSourcePlayer::ConfigureAudioDecoderJob() {
  if (!HasAudio()) {
    audio_decoder_job_.reset();
    return;
  }

  // Create audio decoder job only if config changes.
  if (audio_decoder_job_ && !reconfig_audio_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_crypto = GetMediaCrypto();
  if (is_audio_encrypted_ && media_crypto.is_null())
    return;

  DCHECK(!audio_decoder_job_ || !audio_decoder_job_->is_decoding());

  audio_decoder_job_.reset(AudioDecoderJob::Create(
      audio_codec_, sampling_rate_, num_channels_, &audio_extra_data_[0],
      audio_extra_data_.size(), media_crypto.obj(),
      base::Bind(&DemuxerAndroid::RequestDemuxerData,
                 base::Unretained(demuxer_), demuxer_client_id_,
                 DemuxerStream::AUDIO)));

  if (audio_decoder_job_) {
    SetVolumeInternal();
    reconfig_audio_decoder_ =  false;
  }
}

void MediaSourcePlayer::ConfigureVideoDecoderJob() {
  if (!HasVideo() || surface_.IsEmpty()) {
    video_decoder_job_.reset();
    return;
  }

  // Create video decoder job only if config changes.
  if (video_decoder_job_ && !reconfig_video_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_crypto = GetMediaCrypto();
  if (is_video_encrypted_ && media_crypto.is_null())
    return;

  DCHECK(!video_decoder_job_ || !video_decoder_job_->is_decoding());

  // Release the old VideoDecoderJob first so the surface can get released.
  // Android does not allow 2 MediaCodec instances use the same surface.
  video_decoder_job_.reset();
  // Create the new VideoDecoderJob.
  bool is_secure = IsProtectedSurfaceRequired();
  video_decoder_job_.reset(
      VideoDecoderJob::Create(video_codec_,
                              is_secure,
                              gfx::Size(width_, height_),
                              surface_.j_surface().obj(),
                              media_crypto.obj(),
                              base::Bind(&DemuxerAndroid::RequestDemuxerData,
                                         base::Unretained(demuxer_),
                                         demuxer_client_id_,
                                         DemuxerStream::VIDEO)));
  if (video_decoder_job_)
    reconfig_video_decoder_ = false;

  // Inform the fullscreen view the player is ready.
  // TODO(qinmin): refactor MediaPlayerBridge so that we have a better way
  // to inform ContentVideoView.
  OnMediaMetadataChanged(duration_, width_, height_, true);
}

void MediaSourcePlayer::OnDecoderStarved() {
  DVLOG(1) << __FUNCTION__;
  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::StartStarvationCallback(
    const base::TimeDelta& presentation_timestamp) {
  // 20ms was chosen because it is the typical size of a compressed audio frame.
  // Anything smaller than this would likely cause unnecessary cycling in and
  // out of the prefetch state.
  const base::TimeDelta kMinStarvationTimeout =
      base::TimeDelta::FromMilliseconds(20);

  base::TimeDelta current_timestamp = GetCurrentTime();
  base::TimeDelta timeout;
  if (HasAudio()) {
    timeout = audio_timestamp_helper_->GetTimestamp() - current_timestamp;
  } else {
    DCHECK(current_timestamp <= presentation_timestamp);

    // For video only streams, fps can be estimated from the difference
    // between the previous and current presentation timestamps. The
    // previous presentation timestamp is equal to current_timestamp.
    // TODO(qinmin): determine whether 2 is a good coefficient for estimating
    // video frame timeout.
    timeout = 2 * (presentation_timestamp - current_timestamp);
  }

  timeout = std::max(timeout, kMinStarvationTimeout);

  decoder_starvation_callback_.Reset(
      base::Bind(&MediaSourcePlayer::OnDecoderStarved,
                 weak_this_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, decoder_starvation_callback_.callback(), timeout);
}

void MediaSourcePlayer::SetVolumeInternal() {
  if (audio_decoder_job_ && volume_ >= 0)
    audio_decoder_job_->SetVolume(volume_);
}

bool MediaSourcePlayer::IsProtectedSurfaceRequired() {
  return is_video_encrypted_ &&
      drm_bridge_ && drm_bridge_->IsProtectedSurfaceRequired();
}

void MediaSourcePlayer::OnPrefetchDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_ || !audio_decoder_job_->is_decoding());
  DCHECK(!video_decoder_job_ || !video_decoder_job_->is_decoding());
  DCHECK(IsEventPending(PREFETCH_DONE_EVENT_PENDING));

  ClearPendingEvent(PREFETCH_DONE_EVENT_PENDING);

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  start_time_ticks_ = base::TimeTicks::Now();
  start_presentation_timestamp_ = GetCurrentTime();
  if (!clock_.IsPlaying())
    clock_.Play();

  if (audio_decoder_job_)
    DecodeMoreAudio();
  if (video_decoder_job_)
    DecodeMoreVideo();
}

const char* MediaSourcePlayer::GetEventName(PendingEventFlags event) {
  static const char* kPendingEventNames[] = {
    "SEEK",
    "SURFACE_CHANGE",
    "CONFIG_CHANGE",
    "PREFETCH_REQUEST",
    "PREFETCH_DONE",
  };

  int mask = 1;
  for (size_t i = 0; i < arraysize(kPendingEventNames); ++i, mask <<= 1) {
    if (event & mask)
      return kPendingEventNames[i];
  }

  return "UNKNOWN";
}

bool MediaSourcePlayer::IsEventPending(PendingEventFlags event) const {
  return pending_event_ & event;
}

void MediaSourcePlayer::SetPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(!IsEventPending(event));

  pending_event_ |= event;
}

void MediaSourcePlayer::ClearPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(IsEventPending(event)) << GetEventName(event);

  pending_event_ &= ~event;
}

}  // namespace media
