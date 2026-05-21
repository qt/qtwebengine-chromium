// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/receiver.h"

#include <algorithm>
#include <utility>

#include "cast/streaming/impl/receiver_packet_router.h"
#include "cast/streaming/impl/session_config.h"
#include "cast/streaming/public/constants.h"
#include "platform/base/span.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/trace_logging.h"

namespace openscreen::cast {

using clock_operators::operator<<;

// Conveniences for ensuring logging output includes the SSRC of the Receiver,
// to help distinguish one out of multiple instances in a Cast Streaming
// session.
//
#define SSRC() "[SSRC:" << ssrc() << "] "
#define RECEIVER_DLOG(level) OSP_DLOG_##level << SSRC()
#define RECEIVER_LOG(level) OSP_LOG_##level << SSRC()
#define RECEIVER_VLOG OSP_VLOG << SSRC()
#define RECEIVER_DVLOG OSP_DVLOG << SSRC()

Receiver::Receiver(Environment& environment,
                   ReceiverPacketRouter& packet_router,
                   SessionConfig config)
    : now_(environment.now_function()),
      packet_router_(packet_router),
      config_(config),
      rtcp_session_(config.sender_ssrc, config.receiver_ssrc, now_()),
      rtcp_parser_(rtcp_session_),
      rtcp_builder_(std::make_unique<CompoundRtcpBuilder>(rtcp_session_)),
      stats_tracker_(config.rtp_timebase),
      rtp_parser_(config.sender_ssrc),
      rtp_timebase_(config.rtp_timebase),
      crypto_(config.aes_secret_key, config.aes_iv_mask),
      is_pli_enabled_(config.is_pli_enabled),
      rtcp_alarm_(environment.now_function(), environment.task_runner()),
      smoothed_clock_offset_(ClockDriftSmoother::kDefaultTimeConstant),
      consumption_alarm_(environment.now_function(),
                         environment.task_runner()) {
  OSP_CHECK_EQ(checkpoint_frame(), FrameId::leader());

  rtcp_buffer_.assign(environment.GetMaxPacketSize(), 0);
  OSP_CHECK_GT(rtcp_buffer_.size(), 0);

  rtcp_builder_->SetPlayoutDelay(config.target_playout_delay);
  playout_delay_changes_.emplace_back(FrameId::leader(),
                                      config.target_playout_delay);

  packet_router_.OnReceiverCreated(rtcp_session_.sender_ssrc(), this);
}

Receiver::~Receiver() {
  packet_router_.OnReceiverDestroyed(rtcp_session_.sender_ssrc());
}

const SessionConfig& Receiver::config() const {
  return config_;
}
int Receiver::rtp_timebase() const {
  return rtp_timebase_;
}
Ssrc Receiver::ssrc() const {
  return rtcp_session_.receiver_ssrc();
}

void Receiver::SetConsumer(Consumer* consumer) {
  consumer_ = consumer;
  ScheduleFrameReadyCheck();
}

void Receiver::SetPlayerProcessingTime(Clock::duration needed_time) {
  player_processing_time_ = std::max(Clock::duration::zero(), needed_time);
}

Error Receiver::ReportPlayoutEvent(FrameId frame_id,
                                   RtpTimeTicks rtp_timestamp,
                                   Clock::time_point playout_time) {
  if (!config_.are_receiver_event_logs_enabled) {
    return Error(Error::Code::kOperationInvalid,
                 "receiver event logs are disabled. reports are ignored.");
  }

  if (frame_id <= latest_frame_expected_ - kMaxUnackedFrames) {
    return Error(Error::Code::kParameterOutOfRange, "frame is too old.");
  }

  const PendingFrame& entry = GetQueueEntry(frame_id);
  OSP_CHECK(entry.estimated_capture_time);
  const Clock::duration playout_delay =
      std::max(Clock::duration(), playout_time - *entry.estimated_capture_time);

  if (config_.are_receiver_event_logs_enabled) {
    AddEventToPendingLogs(
        rtp_timestamp,
        RtcpReceiverEventLogMessage{
            .type = StatisticsEvent::Type::kFramePlayedOut,
            .timestamp = playout_time,
            .delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                playout_delay)});
  }

  TRACE_FLOW_END_WITH_TIME(TraceCategory::kReceiver, "Frame.PlayedOut",
                           frame_id, playout_time);

  return Error::None();
}

void Receiver::RequestKeyFrame() {
  // If we don't have picture loss indication enabled, we should not request
  // any key frames.
  if (!is_pli_enabled_) {
    RECEIVER_LOG(WARN) << "Should not request any key frames when picture loss "
                          "indication is not enabled";
    return;
  }

  if (!last_key_frame_received_.is_null() &&
      last_frame_consumed_ >= last_key_frame_received_ &&
      !rtcp_builder_->is_picture_loss_indicator_set()) {
    rtcp_builder_->SetPictureLossIndicator(true);
    SendRtcp();
  }
}

int Receiver::AdvanceToNextFrame() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kReceiver);
  const FrameId immediate_next_frame = last_frame_consumed_ + 1;

  // Scan the queue for the next frame that should be consumed. Typically, this
  // is the very next frame; but if it is incomplete and already late for
  // playout, consider skipping-ahead.
  for (FrameId f = immediate_next_frame; f <= latest_frame_expected_; ++f) {
    PendingFrame& entry = GetQueueEntry(f);
    if (entry.collector.is_complete()) {
      const EncryptedFrame& encrypted_frame =
          entry.collector.PeekAtAssembledFrame();

      const bool is_next_frame = f == immediate_next_frame;
      const bool is_independent =
          encrypted_frame.dependency != EncodedFrame::Dependency::kDependent;
      const bool is_ready = is_next_frame || is_independent;
      if (is_ready) {
        // Found a frame after skipping past some frames. Drop the ones being
        // skipped, advancing `last_frame_consumed_` before returning.
        if (!is_next_frame) {
          DropAllFramesBefore(f);
        }
        TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Ready", f);
        return FrameCrypto::GetPlaintextSize(encrypted_frame);
      }
    }

    // Do not consider skipping past this frame if its estimated capture time is
    // unknown. The implication here is that, if `estimated_capture_time` is
    // set, the Receiver also knows whether any target playout delay changes
    // were communicated from the Sender in the frame's first RTP packet.
    if (!entry.estimated_capture_time) {
      break;
    }

    // If this incomplete frame is not yet late for playout, simply wait for the
    // rest of its packets to come in. However, do schedule a check to
    // re-examine things at the time it should be processed.
    const auto process_time = *entry.estimated_capture_time +
                              ResolveTargetPlayoutDelay(f) -
                              player_processing_time_;
    if (process_time > now_()) {
      ScheduleFrameReadyCheck(process_time);
      break;
    }
  }

  return kNoFramesReady;
}

EncodedFrame Receiver::ConsumeNextFrame(ByteBuffer buffer) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kReceiver);
  // Assumption: The required call to AdvanceToNextFrame() ensures that
  // `last_frame_consumed_` is set to one before the frame to be consumed here.
  const FrameId frame_id = last_frame_consumed_ + 1;
  OSP_CHECK_LE(frame_id, checkpoint_frame());

  TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Consumed", frame_id);

  // Decrypt the frame, populating the given output `frame`.
  PendingFrame& entry = GetQueueEntry(frame_id);
  OSP_CHECK(entry.collector.is_complete());
  OSP_CHECK(entry.estimated_capture_time);

  const EncryptedFrame& encrypted_frame =
      entry.collector.PeekAtAssembledFrame();

  // `buffer` will contain the decrypted frame contents.
  crypto_.Decrypt(encrypted_frame, buffer);
  EncodedFrame frame;
  encrypted_frame.CopyMetadataTo(&frame);
  frame.data = buffer;
  frame.reference_time = *entry.estimated_capture_time +
                         ResolveTargetPlayoutDelay(frame_id) -
                         player_processing_time_;

  RECEIVER_VLOG << "ConsumeNextFrame → " << frame.frame_id << ": "
                << frame.data.size() << " payload bytes, RTP Timestamp "
                << frame.rtp_timestamp.ToTimeSinceOrigin<microseconds>(
                       rtp_timebase_)
                << ", to play-out " << (frame.reference_time - now_())
                << " from now.";

  // Reset the collector to free up memory, and leave the estimated_capture_time
  // for this entry, as it may still be used if the consumer decides to report
  // the playout event.
  entry.collector.Reset();
  last_frame_consumed_ = frame_id;

  // Ensure the Consumer is notified if there are already more frames ready for
  // consumption, and it hasn't explicitly called AdvanceToNextFrame() to check
  // for itself.
  ScheduleFrameReadyCheck();

  return frame;
}

void Receiver::OnReceivedRtpPacket(Clock::time_point arrival_time,
                                   std::vector<uint8_t> packet) {
  const std::optional<RtpPacketParser::ParseResult> part =
      rtp_parser_.Parse(packet);
  if (!part) {
    RECEIVER_LOG(WARN) << "Parsing of " << packet.size()
                       << " bytes as an RTP packet failed.";
    return;
  }
  stats_tracker_.OnReceivedValidRtpPacket(part->sequence_number,
                                          part->rtp_timestamp, arrival_time);

  // Ignore packets for frames the Receiver is no longer interested in.
  if (part->frame_id <= checkpoint_frame()) {
    RECEIVER_VLOG << "ignoring packet for frame " << part->frame_id
                  << " as it has been fully received already.";
    return;
  }

  // Extend the range of frames known to this Receiver, within the capacity of
  // this Receiver's queue. Prepare the FrameCollectors to receive any
  // newly-discovered frames.
  if (part->frame_id > latest_frame_expected_) {
    const FrameId max_allowed_frame_id =
        last_frame_consumed_ + kMaxUnackedFrames;
    if (part->frame_id > max_allowed_frame_id) {
      RECEIVER_VLOG << "ignoring packet for unknown frame " << part->frame_id;
      return;
    }
    do {
      ++latest_frame_expected_;
      PendingFrame& entry = GetQueueEntry(latest_frame_expected_);

      // The collector was already reset, so just reset the capture time.
      entry.estimated_capture_time.reset();
      entry.collector.set_frame_id(latest_frame_expected_);
    } while (latest_frame_expected_ < part->frame_id);
  }

  // Start-up edge case: Blatantly drop the first packet of all frames until the
  // Receiver has processed at least one Sender Report containing the necessary
  // clock-drift and lip-sync information (see OnReceivedRtcpPacket()). This is
  // an inescapable data dependency. Note that this special case should almost
  // never trigger, since a well-behaving Sender will send the first Sender
  // Report RTCP packet before any of the RTP packets.
  if (!last_sender_report_ && part->packet_id == FramePacketId{0}) {
    RECEIVER_LOG(WARN) << "Dropping packet 0 of frame " << part->frame_id
                       << " because it arrived before the first Sender Report.";
    // Note: The Sender will have to re-transmit this dropped packet after the
    // Sender Report to allow the Receiver to move forward.
    return;
  }

  PendingFrame& pending_frame = GetQueueEntry(part->frame_id);
  FrameCollector& collector = pending_frame.collector;
  if (collector.is_complete()) {
    // An extra, redundant `packet` was received. Do nothing since the frame was
    // already complete.
    RECEIVER_VLOG << "ignoring redundant packet for frame " << part->frame_id;
    return;
  }

  if (!collector.CollectRtpPacket(*part, &packet)) {
    RECEIVER_LOG(WARN) << "bad data in parsed packet for frame "
                       << part->frame_id;
    return;  // Bad data in the parsed packet. Ignore it.
  }

  // The first packet in a frame contains timing information critical for
  // computing this frame's (and all future frames') playout time. Process that,
  // but only once.
  if (part->packet_id == FramePacketId{0} &&
      !pending_frame.estimated_capture_time) {
    pending_frame.rtp_timestamp = part->rtp_timestamp;

    // Estimate the original capture time of this frame (at the Sender), in
    // terms of the Receiver's clock: First, start with a reference time point
    // from the Sender's clock (the one from the last Sender Report). Then,
    // translate it into the equivalent reference time point in terms of the
    // Receiver's clock by applying the measured offset between the two clocks.
    // Finally, apply the RTP timestamp difference between the Sender Report and
    // this frame to determine what the original capture time of this frame was.
    const auto smoothed_offset = smoothed_clock_offset_.Current();
    if (!smoothed_offset) {
      return;
    }
    pending_frame.estimated_capture_time =
        last_sender_report_->reference_time + *smoothed_offset +
        (part->rtp_timestamp - last_sender_report_->rtp_timestamp)
            .ToDuration<Clock::duration>(rtp_timebase_);

    // If a target playout delay change was included in this packet, record it.
    if (part->new_playout_delay > milliseconds::zero()) {
      RecordNewTargetPlayoutDelay(part->frame_id, part->new_playout_delay);
    }

    // Now that the estimated capture time is known, other frames may have just
    // become ready, per the frame-skipping logic in AdvanceToNextFrame().
    ScheduleFrameReadyCheck();
  }

  if (config_.are_receiver_event_logs_enabled) {
    AddEventToPendingLogs(part->rtp_timestamp,
                          RtcpReceiverEventLogMessage{
                              .type = StatisticsEvent::Type::kPacketReceived,
                              .timestamp = arrival_time,
                              .packet_id = part->packet_id});
  }

  if (!collector.is_complete()) {
    return;  // Wait for the rest of the packets to come in.
  }
  TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Complete", part->frame_id);

  const EncryptedFrame& encrypted_frame = collector.PeekAtAssembledFrame();

  // Whenever a key frame has been received, the decoder has what it needs to
  // recover. In this case, clear the PLI condition.
  if (encrypted_frame.dependency == EncryptedFrame::Dependency::kKeyFrame) {
    rtcp_builder_->SetPictureLossIndicator(false);
    last_key_frame_received_ = part->frame_id;
  }

  // If this just-completed frame is the one right after the checkpoint frame,
  // advance the checkpoint forward.
  if (part->frame_id == (checkpoint_frame() + 1)) {
    // Make sure we provide a FrameAckSent event to the sender later.
    pending_frame_acks_.push_back(part->rtp_timestamp);
    AdvanceCheckpoint(part->frame_id);
  }

  // Since a frame has become complete, schedule a check to see whether this or
  // any other frames have become ready for consumption.
  ScheduleFrameReadyCheck();
}

void Receiver::OnReceivedRtcpPacket(Clock::time_point arrival_time,
                                    std::vector<uint8_t> packet) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kReceiver);
  std::optional<SenderReportParser::SenderReportWithId> parsed_report =
      rtcp_parser_.Parse(packet);
  if (!parsed_report) {
    TRACE_SCOPED(TraceCategory::kReceiver, "ReceivedInvalidRtcpReport");
    RECEIVER_LOG(WARN) << "Parsing of " << packet.size()
                       << " bytes as an RTCP packet failed.";
    return;
  }

  TRACE_DEFAULT_SCOPED1(TraceCategory::kReceiver, "packet_id",
                        parsed_report->report_id);
  last_sender_report_ = std::move(parsed_report);
  last_sender_report_arrival_time_ = arrival_time;

  // Measure the offset between the Sender's clock and the Receiver's Clock.
  // This will be used to translate reference timestamps from the Sender into
  // timestamps that represent the exact same moment in time at the Receiver.
  //
  // Note: Due to design limitations in the Cast Streaming spec, the Receiver
  // has no way to compute how long it took the Sender Report to travel over the
  // network. The calculation here just ignores that, and so the
  // `measured_offset` below will be larger than the true value by that amount.
  // This will have the effect of a later-than-configured playout delay.
  const Clock::duration measured_offset =
      arrival_time - last_sender_report_->reference_time;
  smoothed_clock_offset_.Update(arrival_time, measured_offset);

  RtcpReportBlock report;
  report.ssrc = rtcp_session_.sender_ssrc();
  stats_tracker_.PopulateNextReport(&report);
  report.last_status_report_id = last_sender_report_->report_id;
  report.SetDelaySinceLastReport(now_() - last_sender_report_arrival_time_);
  rtcp_builder_->IncludeReceiverReportInNextPacket(report);

  SendRtcp();
}

void Receiver::SendRtcp() {
  // Collect ACK/NACK feedback for all active frames in the queue.
  std::vector<PacketNack> packet_nacks;
  std::vector<FrameId> frame_acks;
  for (FrameId f = checkpoint_frame() + 1; f <= latest_frame_expected_; ++f) {
    const PendingFrame& entry = GetQueueEntry(f);
    if (entry.collector.is_complete()) {
      frame_acks.push_back(f);

      if (config_.are_receiver_event_logs_enabled) {
        if (entry.rtp_timestamp) {
          pending_frame_acks_.push_back(entry.rtp_timestamp.value());
        }
      }
    } else {
      entry.collector.GetMissingPackets(&packet_nacks);
    }
  }

  // Fire off events for frames that were implicitly ACKed.
  if (config_.are_receiver_event_logs_enabled) {
    for (auto rtp_timestamp : pending_frame_acks_) {
      AddEventToPendingLogs(rtp_timestamp,
                            RtcpReceiverEventLogMessage{
                                .type = StatisticsEvent::Type::kFrameAckSent,
                                .timestamp = now_(),
                            });
    }
    pending_frame_acks_.clear();

    rtcp_builder_->IncludeReceiverLogsInNextPacket(std::move(pending_logs_));
    pending_logs_.clear();
  }

  // Build and send a compound RTCP packet.
  rtcp_builder_->IncludeFeedbackInNextPacket(std::move(packet_nacks),
                                             std::move(frame_acks));
  last_rtcp_send_time_ = now_();
  packet_router_.SendRtcpPacket(
      rtcp_builder_->BuildPacket(last_rtcp_send_time_, rtcp_buffer_));

  // Schedule the automatic sending of another RTCP packet, if this method is
  // not called within some bounded amount of time. While incomplete frames
  // exist in the queue, send RTCP packets (with ACK/NACK feedback) frequently.
  // When there are no incomplete frames, use a longer "keepalive" interval.
  const Clock::duration interval =
      (packet_nacks.empty() ? kRtcpReportInterval : kNackFeedbackInterval);
  rtcp_alarm_.Schedule([this] { SendRtcp(); }, last_rtcp_send_time_ + interval);
}

const Receiver::PendingFrame& Receiver::GetQueueEntry(FrameId frame_id) const {
  return const_cast<Receiver*>(this)->GetQueueEntry(frame_id);
}

Receiver::PendingFrame& Receiver::GetQueueEntry(FrameId frame_id) {
  return pending_frames_[(frame_id - FrameId::first()) %
                         pending_frames_.size()];
}

void Receiver::RecordNewTargetPlayoutDelay(FrameId as_of_frame,
                                           milliseconds delay) {
  OSP_CHECK_GT(as_of_frame, checkpoint_frame());

  // Prune-out entries from `playout_delay_changes_` that are no longer needed.
  // At least one entry must always be kept (i.e., there must always be a
  // "current" setting).
  const FrameId next_frame = last_frame_consumed_ - kMaxUnackedFrames + 1;
  const auto keep_one_before_it = std::find_if(
      std::next(playout_delay_changes_.begin()), playout_delay_changes_.end(),
      [&](const auto& entry) { return entry.first > next_frame; });
  playout_delay_changes_.erase(playout_delay_changes_.begin(),
                               std::prev(keep_one_before_it));

  // Insert the delay change entry, maintaining the ascending ordering of the
  // vector.
  const auto insert_it = std::find_if(
      playout_delay_changes_.begin(), playout_delay_changes_.end(),
      [&](const auto& entry) { return entry.first > as_of_frame; });
  playout_delay_changes_.emplace(insert_it, as_of_frame, delay);

  OSP_DCHECK(AreElementsSortedAndUnique(playout_delay_changes_));
}

milliseconds Receiver::ResolveTargetPlayoutDelay(FrameId frame_id) const {
  const FrameId first_possible =
      last_frame_consumed_ > FrameId::first() + kMaxUnackedFrames
          ? last_frame_consumed_ - kMaxUnackedFrames
          : FrameId::first();
  OSP_CHECK_GE(frame_id, first_possible);

#if OSP_DCHECK_IS_ON()
  // Extra precaution: Ensure all possible playout delay changes are known. In
  // other words, every unconsumed frame in the queue, up to (and including)
  // `frame_id`, must have an assigned estimated_capture_time.
  for (FrameId f = first_possible; f <= frame_id; ++f) {
    OSP_CHECK(GetQueueEntry(f).estimated_capture_time)
        << " don't know whether there was a playout delay change for frame "
        << f;
  }
#endif

  const auto it = std::find_if(
      playout_delay_changes_.crbegin(), playout_delay_changes_.crend(),
      [&](const auto& entry) { return entry.first <= frame_id; });
  OSP_CHECK(it != playout_delay_changes_.crend());
  return it->second;
}

void Receiver::AdvanceCheckpoint(FrameId new_checkpoint) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kReceiver);
  OSP_CHECK_GT(new_checkpoint, checkpoint_frame());
  OSP_CHECK_LE(new_checkpoint, latest_frame_expected_);

  while (new_checkpoint < latest_frame_expected_) {
    const FrameId next = new_checkpoint + 1;
    if (!GetQueueEntry(next).collector.is_complete()) {
      break;
    }
    new_checkpoint = next;
  }

  set_checkpoint_frame(new_checkpoint);
  rtcp_builder_->SetPlayoutDelay(ResolveTargetPlayoutDelay(new_checkpoint));
  SendRtcp();
}

void Receiver::DropAllFramesBefore(FrameId first_kept_frame) {
  // The following CHECKs are verifying that this method is only being called
  // because one or more incomplete frames are being skipped-over.
  const FrameId first_to_drop = last_frame_consumed_ + 1;
  OSP_CHECK_GT(first_kept_frame, first_to_drop);
  OSP_CHECK_GT(first_kept_frame, checkpoint_frame());
  OSP_CHECK_LE(first_kept_frame, latest_frame_expected_);

  // Reset each of the frames being dropped, pretending that they were consumed.
  for (FrameId f = first_to_drop; f < first_kept_frame; ++f) {
    PendingFrame& entry = GetQueueEntry(f);
    // Pedantic sanity-check: Ensure the "target playout delay change" data
    // dependency was satisfied. See comments in AdvanceToNextFrame().
    OSP_CHECK(entry.estimated_capture_time);
    entry.collector.Reset();
  }
  last_frame_consumed_ = first_kept_frame - 1;

  RECEIVER_LOG(INFO) << "Artificially advancing checkpoint after skipping.";
  AdvanceCheckpoint(first_kept_frame);
}

void Receiver::ScheduleFrameReadyCheck(Clock::time_point when) {
  consumption_alarm_.Schedule(
      [this] {
        if (consumer_) {
          const int next_frame_buffer_size = AdvanceToNextFrame();
          if (next_frame_buffer_size != kNoFramesReady) {
            consumer_->OnFramesReady(next_frame_buffer_size);
          }
        }
      },
      when);
}

void Receiver::AddEventToPendingLogs(RtpTimeTicks rtp_timestamp,
                                     RtcpReceiverEventLogMessage event_log) {
  OSP_CHECK(config_.are_receiver_event_logs_enabled);

  // Find or create a frame log for this RTP timestamp.
  auto it = std::find_if(
      pending_logs_.begin(), pending_logs_.end(),
      [&](const auto& log) { return log.rtp_timestamp == rtp_timestamp; });
  if (it == pending_logs_.end()) {
    pending_logs_.push_back({rtp_timestamp, {}});
    it = pending_logs_.end() - 1;
  }
  it->messages.push_back(event_log);
}

Receiver::PendingFrame::PendingFrame() = default;
Receiver::PendingFrame::~PendingFrame() = default;


// static
constexpr milliseconds Receiver::kDefaultPlayerProcessingTime;
constexpr int Receiver::kNoFramesReady;
constexpr milliseconds Receiver::kNackFeedbackInterval;

}  // namespace openscreen::cast
