// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_RECEIVER_IMPL_H_
#define CAST_STREAMING_IMPL_RECEIVER_IMPL_H_

#include <stdint.h>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "cast/streaming/impl/clock_drift_smoother.h"
#include "cast/streaming/impl/compound_rtcp_builder.h"
#include "cast/streaming/impl/frame_collector.h"
#include "cast/streaming/impl/packet_receive_stats_tracker.h"
#include "cast/streaming/impl/receiver_packet_router.h"
#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtcp_session.h"
#include "cast/streaming/impl/rtp_packet_parser.h"
#include "cast/streaming/impl/sender_report_parser.h"
#include "cast/streaming/public/environment.h"
#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/public/receiver.h"
#include "cast/streaming/public/session_config.h"
#include "cast/streaming/ssrc.h"
#include "platform/api/time.h"
#include "platform/base/span.h"
#include "util/alarm.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {

struct EncodedFrame;
class ReceiverTest;

class ReceiverImpl : public Receiver,
                     public ReceiverPacketRouter::PacketConsumer {
 public:
  using Receiver::Consumer;

  // Constructs a Receiver that attaches to the given `environment` and
  // `packet_router`. The config contains the settings that were
  // agreed-upon by both sides from the OFFER/ANSWER exchange (i.e., the part of
  // the overall end-to-end connection process that occurs before Cast Streaming
  // is started).
  ReceiverImpl(Environment& environment,
               ReceiverPacketRouter& packet_router,
               SessionConfig config);
  ~ReceiverImpl() override;

  // Receiver overrides.
  const SessionConfig& config() const override;
  void SetConsumer(Consumer* consumer) override;
  void SetPlayerProcessingTime(Clock::duration needed_time) override;
  Error ReportPlayoutEvent(FrameId frame_id,
                           RtpTimeTicks rtp_timestamp,
                           Clock::time_point playout_time) override;
  void RequestKeyFrame() override;
  std::optional<size_t> AdvanceToNextFrame() override;
  EncodedFrame ConsumeNextFrame(ByteBuffer buffer) override;

  // The default "player processing time" amount. See SetPlayerProcessingTime().
  using openscreen::cast::Receiver::kDefaultPlayerProcessingTime;

 protected:
  // ReceiverPacketRouter::PacketConsumer implementation.
  void OnReceivedRtpPacket(Clock::time_point arrival_time,
                           std::vector<uint8_t> packet) override;
  void OnReceivedRtcpPacket(Clock::time_point arrival_time,
                            std::span<const uint8_t> packet) override;

 private:
  // An entry in the circular queue (see `pending_frames_`).
  struct PendingFrame {
    // NOTE: to free resources, the collector may be Reset().
    FrameCollector collector;

    // The Receiver's [local] Clock time when this frame was originally captured
    // at the Sender. This is computed and assigned when the RTP packet with ID
    // 0 is processed. Add the target playout delay to this to get the target
    // playout time.
    std::optional<Clock::time_point> estimated_capture_time;

    // The timestamp associated with the frame.
    std::optional<RtpTimeTicks> rtp_timestamp;
  };

  // Get/Set the checkpoint FrameId. This indicates that all of the packets for
  // all frames up to and including this FrameId have been successfully received
  // (or otherwise do not need to be re-transmitted).
  FrameId checkpoint_frame() const { return rtcp_builder_->checkpoint_frame(); }
  void set_checkpoint_frame(FrameId frame_id) {
    rtcp_builder_->SetCheckpointFrame(frame_id);
  }

  // Send an RTCP packet to the Sender immediately, to acknowledge the complete
  // reception of one or more additional frames, to reply to a Sender Report, or
  // to request re-transmits. Calling this also schedules additional RTCP
  // packets to be sent periodically for the life of this Receiver.
  void SendRtcp();

  // Helpers to map the given `frame_id` to the element in the `pending_frames_`
  // circular queue. There are both const and non-const versions, but neither
  // mutate any state (i.e., they are just look-ups).
  const PendingFrame& GetQueueEntry(FrameId frame_id) const;
  PendingFrame& GetQueueEntry(FrameId frame_id);

  // Record that the target playout delay has changed starting with the given
  // FrameId.
  void RecordNewTargetPlayoutDelay(FrameId as_of_frame,
                                   std::chrono::milliseconds delay);

  // Examine the known target playout delay changes to determine what setting is
  // in-effect for the given frame.
  std::chrono::milliseconds ResolveTargetPlayoutDelay(FrameId frame_id) const;

  // Called to move the checkpoint forward. This scans the queue, starting from
  // `new_checkpoint`, to find the latest in a contiguous sequence of completed
  // frames. Then, it records that frame as the new checkpoint, and immediately
  // sends a feedback RTCP packet to the Sender.
  void AdvanceCheckpoint(FrameId new_checkpoint);

  // Helper to force-drop all frames before `first_kept_frame`, even if they
  // were never consumed. This will also auto-cancel frames that were never
  // completely received, artificially moving the checkpoint forward, and
  // notifying the Sender of that. The caller of this method is responsible for
  // making sure that frame data dependencies will not be broken by dropping the
  // frames.
  void DropAllFramesBefore(FrameId first_kept_frame);

  // Sets the `consumption_alarm_` to check whether any frames are ready,
  // including possibly skipping over late frames in order to make not-yet-late
  // frames become ready. The default argument value means "without delay."
  void ScheduleFrameReadyCheck(Clock::time_point when = Alarm::kImmediately);

  void AddEventToPendingLogs(RtpTimeTicks rtp_timestamp,
                             RtcpReceiverEventLogMessage event_log);

  const ClockNowFunctionPtr now_;
  ReceiverPacketRouter& packet_router_;
  const SessionConfig config_;
  RtcpSession rtcp_session_;
  SenderReportParser rtcp_parser_;
  std::unique_ptr<CompoundRtcpBuilder> rtcp_builder_;
  PacketReceiveStatsTracker stats_tracker_;  // Tracks transmission stats.
  RtpPacketParser rtp_parser_;
  const int rtp_timebase_;    // RTP timestamp ticks per second.
  const FrameCrypto crypto_;  // Decrypts assembled frames.
  bool is_pli_enabled_;       // Whether picture loss indication is enabled.

  // Buffer for serializing/sending RTCP packets.
  std::vector<uint8_t> rtcp_buffer_;

  // Schedules tasks to ensure RTCP reports are sent within a bounded interval.
  // Not scheduled until after this Receiver has processed the first packet from
  // the Sender.
  Alarm rtcp_alarm_;
  Clock::time_point last_rtcp_send_time_ = Clock::time_point::min();

  // The last Sender Report received and when the packet containing it had
  // arrived. This contains lip-sync timestamps used as part of the calculation
  // of playout times for the received frames, as well as ping-pong data bounced
  // back to the Sender in the Receiver Reports. It is nullopt until the first
  // parseable Sender Report is received.
  std::optional<SenderReportParser::SenderReportWithId> last_sender_report_;
  Clock::time_point last_sender_report_arrival_time_;

  // Tracks the offset between the Receiver's [local] clock and the Sender's
  // clock. This is invalid until the first Sender Report has been successfully
  // processed (i.e., `last_sender_report_` is not nullopt).
  ClockDriftSmoother smoothed_clock_offset_;

  // The ID of the latest frame whose existence is known to this Receiver. This
  // value must always be greater than or equal to `checkpoint_frame()`.
  FrameId latest_frame_expected_ = FrameId::leader();

  // The ID of the last frame consumed. This value must always be less than or
  // equal to `checkpoint_frame()`, since it's impossible to consume incomplete
  // frames!
  FrameId last_frame_consumed_ = FrameId::leader();

  // The ID of the latest key frame known to be in-flight. This is used by
  // RequestKeyFrame() to ensure the PLI condition doesn't get set again until
  // after the consumer has seen a key frame that would clear the condition.
  FrameId last_key_frame_received_;

  // The frame queue (circular), which tracks which frames are in-flight, stores
  // data for partially-received frames, and holds onto completed frames until
  // the consumer consumes them. After the frame has been consumed, its capture
  // time and rtp_timestamp are intentionally left valid so they may be used
  // for statistics gathering. The consumer then has until the slot is reused
  // to report playout events, after which an error will be thrown.
  //
  // Use GetQueueEntry() to access a slot. The currently-active slots are those
  // for the frames after `last_frame_consumed_` and up-to/including
  // `latest_frame_expected_`.
  std::array<PendingFrame, kMaxUnackedFrames> pending_frames_ = {};

  // A vector containing the RTP timestamps of all of the frames that are
  // implicitly ACKed by the checkpoint frame ID advancing.
  std::vector<RtpTimeTicks> pending_frame_acks_;

  // Tracks the recent changes to the target playout delay, which is controlled
  // by the Sender. The FrameId indicates the first frame where a new delay
  // setting takes effect. This vector is never empty, is kept sorted, and is
  // pruned to remain as small as possible.
  //
  // The target playout delay is the amount of time between a frame's
  // capture/recording on the Sender and when it should be played-out at the
  // Receiver.
  std::vector<std::pair<FrameId, std::chrono::milliseconds>>
      playout_delay_changes_;

  // The consumer to notify when there are one or more frames completed and
  // ready to be consumed.
  Consumer* consumer_ = nullptr;

  // The additional time needed to decode/play-out each frame after being
  // consumed from this Receiver.
  Clock::duration player_processing_time_ = kDefaultPlayerProcessingTime;

  // Scheduled to check whether there are frames ready and, if there are, to
  // notify the Consumer via OnFramesReady().
  Alarm consumption_alarm_;

  std::vector<RtcpReceiverFrameLogMessage> pending_logs_;

  // The interval between sending ACK/NACK feedback RTCP messages while
  // incomplete frames exist in the queue.
  //
  // TODO(jophba): This should be a function of the current target playout
  // delay, similar to the Sender's kickstart interval logic.
  static constexpr milliseconds kNackFeedbackInterval = milliseconds(30);
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_RECEIVER_IMPL_H_
