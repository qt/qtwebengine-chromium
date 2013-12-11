// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_sender/rtp_sender.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpAudioSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpAudioSenderFeedback(AudioSender* audio_sender)
      : audio_sender_(audio_sender) {
  }

  virtual void OnReceivedSendReportRequest() OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

  virtual void OnReceivedReportBlock(
      const RtcpReportBlock& report_block) OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

  virtual void OnReceivedIntraFrameRequest() OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }


  virtual void OnReceivedRpsi(uint8 payload_type,
                              uint64 picture_id) OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

  virtual void OnReceivedRemb(uint32 bitrate) OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

  virtual void OnReceivedNackRequest(
      const std::list<uint16>& nack_sequence_numbers) OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_feedback) OVERRIDE {
    if (!cast_feedback.missing_frames_and_packets_.empty()) {
      audio_sender_->ResendPackets(cast_feedback.missing_frames_and_packets_);
    }
    VLOG(1) << "Received audio ACK "
            << static_cast<int>(cast_feedback.ack_frame_id_);
  }

 private:
  AudioSender* audio_sender_;
};

class LocalRtpSenderStatistics : public RtpSenderStatistics {
 public:
  explicit LocalRtpSenderStatistics(RtpSender* rtp_sender)
     : rtp_sender_(rtp_sender) {
  }

  virtual void GetStatistics(const base::TimeTicks& now,
                             RtcpSenderInfo* sender_info) OVERRIDE {
    rtp_sender_->RtpStatistics(now, sender_info);
  }

 private:
  RtpSender* rtp_sender_;
};

AudioSender::AudioSender(scoped_refptr<CastThread> cast_thread,
                         const AudioSenderConfig& audio_config,
                         PacedPacketSender* const paced_packet_sender)
      : incoming_feedback_ssrc_(audio_config.incoming_feedback_ssrc),
        cast_thread_(cast_thread),
        rtp_sender_(&audio_config, NULL, paced_packet_sender),
        rtcp_feedback_(new LocalRtcpAudioSenderFeedback(this)),
        rtp_audio_sender_statistics_(
            new LocalRtpSenderStatistics(&rtp_sender_)),
        rtcp_(rtcp_feedback_.get(),
              paced_packet_sender,
              rtp_audio_sender_statistics_.get(),
              NULL,
              audio_config.rtcp_mode,
              base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
              true,
              audio_config.sender_ssrc,
              audio_config.rtcp_c_name),
        clock_(&default_tick_clock_),
        weak_factory_(this) {

  rtcp_.SetRemoteSSRC(audio_config.incoming_feedback_ssrc);

  if (!audio_config.use_external_encoder) {
    audio_encoder_ = new AudioEncoder(cast_thread, audio_config);
  }
  ScheduleNextRtcpReport();
}

AudioSender::~AudioSender() {}

void AudioSender::InsertRawAudioFrame(
    const PcmAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time,
    const base::Closure callback) {
  DCHECK(audio_encoder_.get()) << "Invalid internal state";


  audio_encoder_->InsertRawAudioFrame(audio_frame, recorded_time,
      base::Bind(&AudioSender::SendEncodedAudioFrame,
                 weak_factory_.GetWeakPtr()),
      callback);
}

void AudioSender::InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                        const base::TimeTicks& recorded_time,
                                        const base::Closure callback) {
  DCHECK(audio_encoder_.get() == NULL) << "Invalid internal state";
  rtp_sender_.IncomingEncodedAudioFrame(audio_frame, recorded_time);
  callback.Run();
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<EncodedAudioFrame> audio_frame,
    const base::TimeTicks& recorded_time) {
  rtp_sender_.IncomingEncodedAudioFrame(audio_frame.get(), recorded_time);
}

void AudioSender::ResendPackets(
   const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

void AudioSender::IncomingRtcpPacket(const uint8* packet, int length,
                                     const base::Closure callback) {
  rtcp_.IncomingRtcpPacket(packet, length);
  cast_thread_->PostTask(CastThread::MAIN, FROM_HERE, callback);
}

void AudioSender::ScheduleNextRtcpReport() {
  base::TimeDelta time_to_next =
      rtcp_.TimeToSendNextRtcpReport() - clock_->NowTicks();

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&AudioSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void AudioSender::SendRtcpReport() {
  rtcp_.SendRtcpReport(incoming_feedback_ssrc_);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
