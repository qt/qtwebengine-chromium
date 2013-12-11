// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_audio_decoder.h"

#include <cstdlib>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_loop.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"

namespace media {

const int DecryptingAudioDecoder::kSupportedBitsPerChannel = 16;

static inline bool IsOutOfSync(const base::TimeDelta& timestamp_1,
                               const base::TimeDelta& timestamp_2) {
  // Out of sync of 100ms would be pretty noticeable and we should keep any
  // drift below that.
  const int64 kOutOfSyncThresholdInMilliseconds = 100;
  return std::abs(timestamp_1.InMilliseconds() - timestamp_2.InMilliseconds()) >
         kOutOfSyncThresholdInMilliseconds;
}

DecryptingAudioDecoder::DecryptingAudioDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      state_(kUninitialized),
      demuxer_stream_(NULL),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      decryptor_(NULL),
      key_added_while_decode_pending_(false),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0) {
}

void DecryptingAudioDecoder::Initialize(
    DemuxerStream* stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DVLOG(2) << "Initialize()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(stream);

  weak_this_ = weak_factory_.GetWeakPtr();
  init_cb_ = BindToCurrentLoop(status_cb);

  const AudioDecoderConfig& config = stream->audio_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream config.";
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // DecryptingAudioDecoder only accepts potentially encrypted stream.
  if (!config.is_encrypted()) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  DCHECK(!demuxer_stream_);
  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  state_ = kDecryptorRequested;
  set_decryptor_ready_cb_.Run(BindToCurrentLoop(
      base::Bind(&DecryptingAudioDecoder::SetDecryptor, weak_this_)));
}

void DecryptingAudioDecoder::Read(const ReadCB& read_cb) {
  DVLOG(3) << "Read()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle || state_ == kDecodeFinished) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  read_cb_ = BindToCurrentLoop(read_cb);

  // Return empty (end-of-stream) frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    base::ResetAndReturn(&read_cb_).Run(kOk, AudioBuffer::CreateEOSBuffer());
    return;
  }

  if (!queued_audio_frames_.empty()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, queued_audio_frames_.front());
    queued_audio_frames_.pop_front();
    return;
  }

  state_ = kPendingDemuxerRead;
  ReadFromDemuxerStream();
}

void DecryptingAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle ||
         state_ == kPendingConfigChange ||
         state_ == kPendingDemuxerRead ||
         state_ == kPendingDecode ||
         state_ == kWaitingForKey ||
         state_ == kDecodeFinished) << state_;
  DCHECK(init_cb_.is_null());  // No Reset() during pending initialization.
  DCHECK(reset_cb_.is_null());

  reset_cb_ = closure;

  decryptor_->ResetDecoder(Decryptor::kAudio);

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see DecryptAndDecodeBuffer() and
  // DeliverFrame().
  if (state_ == kPendingConfigChange ||
      state_ == kPendingDemuxerRead ||
      state_ == kPendingDecode) {
    DCHECK(!read_cb_.is_null());
    return;
  }

  if (state_ == kWaitingForKey) {
    DCHECK(!read_cb_.is_null());
    pending_buffer_to_decode_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  }

  DCHECK(read_cb_.is_null());
  DoReset();
}

int DecryptingAudioDecoder::bits_per_channel() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return bits_per_channel_;
}

ChannelLayout DecryptingAudioDecoder::channel_layout() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return channel_layout_;
}

int DecryptingAudioDecoder::samples_per_second() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return samples_per_second_;
}

DecryptingAudioDecoder::~DecryptingAudioDecoder() {
}

void DecryptingAudioDecoder::SetDecryptor(Decryptor* decryptor) {
  DVLOG(2) << "SetDecryptor()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecryptorRequested) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(!set_decryptor_ready_cb_.is_null());

  set_decryptor_ready_cb_.Reset();

  if (!decryptor) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    // TODO(xhwang): Add kError state. See http://crbug.com/251503
    state_ = kDecodeFinished;
    return;
  }

  decryptor_ = decryptor;

  const AudioDecoderConfig& input_config =
      demuxer_stream_->audio_decoder_config();
  AudioDecoderConfig config;
  config.Initialize(input_config.codec(),
                    kSampleFormatS16,
                    input_config.channel_layout(),
                    input_config.samples_per_second(),
                    input_config.extra_data(),
                    input_config.extra_data_size(),
                    input_config.is_encrypted(),
                    false,
                    base::TimeDelta(),
                    base::TimeDelta());

  state_ = kPendingDecoderInit;
  decryptor_->InitializeAudioDecoder(
      config,
      BindToCurrentLoop(base::Bind(
          &DecryptingAudioDecoder::FinishInitialization, weak_this_)));
}

void DecryptingAudioDecoder::FinishInitialization(bool success) {
  DVLOG(2) << "FinishInitialization()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecoderInit) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(reset_cb_.is_null());  // No Reset() before initialization finished.
  DCHECK(read_cb_.is_null());  // No Read() before initialization finished.

  if (!success) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    state_ = kDecodeFinished;
    return;
  }

  // Success!
  UpdateDecoderConfig();

  decryptor_->RegisterNewKeyCB(
      Decryptor::kAudio, BindToCurrentLoop(base::Bind(
          &DecryptingAudioDecoder::OnKeyAdded, weak_this_)));

  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingAudioDecoder::FinishConfigChange(bool success) {
  DVLOG(2) << "FinishConfigChange()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingConfigChange) << state_;
  DCHECK(!read_cb_.is_null());

  if (!success) {
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    state_ = kDecodeFinished;
    if (!reset_cb_.is_null())
      base::ResetAndReturn(&reset_cb_).Run();
    return;
  }

  // Config change succeeded.
  UpdateDecoderConfig();

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  state_ = kPendingDemuxerRead;
  ReadFromDemuxerStream();
}

void DecryptingAudioDecoder::ReadFromDemuxerStream() {
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(
      base::Bind(&DecryptingAudioDecoder::DecryptAndDecodeBuffer, weak_this_));
}

void DecryptingAudioDecoder::DecryptAndDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(3) << "DecryptAndDecodeBuffer()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer.get() != NULL, status == DemuxerStream::kOk) << status;

  if (status == DemuxerStream::kConfigChanged) {
    DVLOG(2) << "DecryptAndDecodeBuffer() - kConfigChanged";

  const AudioDecoderConfig& input_config =
      demuxer_stream_->audio_decoder_config();
  AudioDecoderConfig config;
  config.Initialize(input_config.codec(),
                    kSampleFormatS16,
                    input_config.channel_layout(),
                    input_config.samples_per_second(),
                    input_config.extra_data(),
                    input_config.extra_data_size(),
                    input_config.is_encrypted(),
                    false,
                    base::TimeDelta(),
                    base::TimeDelta());

    state_ = kPendingConfigChange;
    decryptor_->DeinitializeDecoder(Decryptor::kAudio);
    decryptor_->InitializeAudioDecoder(
        config, BindToCurrentLoop(base::Bind(
            &DecryptingAudioDecoder::FinishConfigChange, weak_this_)));
    return;
  }

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  if (status == DemuxerStream::kAborted) {
    DVLOG(2) << "DecryptAndDecodeBuffer() - kAborted";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);

  // Initialize the |next_output_timestamp_| to be the timestamp of the first
  // non-EOS buffer.
  if (timestamp_helper_->base_timestamp() == kNoTimestamp() &&
      !buffer->end_of_stream()) {
    timestamp_helper_->SetBaseTimestamp(buffer->timestamp());
  }

  pending_buffer_to_decode_ = buffer;
  state_ = kPendingDecode;
  DecodePendingBuffer();
}

void DecryptingAudioDecoder::DecodePendingBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;

  int buffer_size = 0;
  if (!pending_buffer_to_decode_->end_of_stream()) {
    buffer_size = pending_buffer_to_decode_->data_size();
  }

  decryptor_->DecryptAndDecodeAudio(
      pending_buffer_to_decode_,
      BindToCurrentLoop(base::Bind(
          &DecryptingAudioDecoder::DeliverFrame, weak_this_, buffer_size)));
}

void DecryptingAudioDecoder::DeliverFrame(
    int buffer_size,
    Decryptor::Status status,
    const Decryptor::AudioBuffers& frames) {
  DVLOG(3) << "DeliverFrame() - status: " << status;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK(pending_buffer_to_decode_.get());
  DCHECK(queued_audio_frames_.empty());

  bool need_to_try_again_if_nokey_is_returned = key_added_while_decode_pending_;
  key_added_while_decode_pending_ = false;

  scoped_refptr<DecoderBuffer> scoped_pending_buffer_to_decode =
      pending_buffer_to_decode_;
  pending_buffer_to_decode_ = NULL;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, !frames.empty());

  if (status == Decryptor::kError) {
    DVLOG(2) << "DeliverFrame() - kError";
    state_ = kDecodeFinished;
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  if (status == Decryptor::kNoKey) {
    DVLOG(2) << "DeliverFrame() - kNoKey";
    // Set |pending_buffer_to_decode_| back as we need to try decoding the
    // pending buffer again when new key is added to the decryptor.
    pending_buffer_to_decode_ = scoped_pending_buffer_to_decode;

    if (need_to_try_again_if_nokey_is_returned) {
      // The |state_| is still kPendingDecode.
      DecodePendingBuffer();
      return;
    }

    state_ = kWaitingForKey;
    return;
  }

  // The buffer has been accepted by the decoder, let's report statistics.
  if (buffer_size) {
    PipelineStatistics statistics;
    statistics.audio_bytes_decoded = buffer_size;
    statistics_cb_.Run(statistics);
  }

  if (status == Decryptor::kNeedMoreData) {
    DVLOG(2) << "DeliverFrame() - kNeedMoreData";
    if (scoped_pending_buffer_to_decode->end_of_stream()) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(kOk, AudioBuffer::CreateEOSBuffer());
      return;
    }

    state_ = kPendingDemuxerRead;
    ReadFromDemuxerStream();
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  DCHECK(!frames.empty());
  EnqueueFrames(frames);

  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, queued_audio_frames_.front());
  queued_audio_frames_.pop_front();
}

void DecryptingAudioDecoder::OnKeyAdded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kPendingDecode) {
    key_added_while_decode_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    state_ = kPendingDecode;
    DecodePendingBuffer();
  }
}

void DecryptingAudioDecoder::DoReset() {
  DCHECK(init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  state_ = kIdle;
  base::ResetAndReturn(&reset_cb_).Run();
}

void DecryptingAudioDecoder::UpdateDecoderConfig() {
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();
  bits_per_channel_ = kSupportedBitsPerChannel;
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  timestamp_helper_.reset(new AudioTimestampHelper(samples_per_second_));
}

void DecryptingAudioDecoder::EnqueueFrames(
    const Decryptor::AudioBuffers& frames) {
  queued_audio_frames_ = frames;

  for (Decryptor::AudioBuffers::iterator iter = queued_audio_frames_.begin();
       iter != queued_audio_frames_.end();
       ++iter) {
    scoped_refptr<AudioBuffer>& frame = *iter;

    DCHECK(!frame->end_of_stream()) << "EOS frame returned.";
    DCHECK_GT(frame->frame_count(), 0) << "Empty frame returned.";

    base::TimeDelta current_time = timestamp_helper_->GetTimestamp();
    if (IsOutOfSync(current_time, frame->timestamp())) {
      DVLOG(1) << "Timestamp returned by the decoder ("
               << frame->timestamp().InMilliseconds() << " ms)"
               << " does not match the input timestamp and number of samples"
               << " decoded (" << current_time.InMilliseconds() << " ms).";
    }

    frame->set_timestamp(current_time);
    frame->set_duration(
        timestamp_helper_->GetFrameDuration(frame->frame_count()));
    timestamp_helper_->AddFrames(frame->frame_count());
  }
}

}  // namespace media
