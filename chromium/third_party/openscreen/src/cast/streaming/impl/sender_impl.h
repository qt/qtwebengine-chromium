// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_SENDER_IMPL_H_
#define CAST_STREAMING_IMPL_SENDER_IMPL_H_

#include <stdint.h>

#include <array>
#include <chrono>
#include <optional>
#include <vector>

#include "cast/streaming/impl/compound_rtcp_parser.h"
#include "cast/streaming/impl/frame_crypto.h"
#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtp_defines.h"
#include "cast/streaming/impl/rtp_packetizer.h"
#include "cast/streaming/impl/sender_report_builder.h"
#include "cast/streaming/impl/statistics_dispatcher.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/public/sender.h"
#include "cast/streaming/public/session_config.h"
#include "cast/streaming/rtp_time.h"
#include "cast/streaming/sender_packet_router.h"
#include "platform/api/time.h"
#include "platform/base/span.h"
#include "util/bit_vector.h"

namespace openscreen::cast {

class Environment;

// The Cast Streaming Sender, a peer corresponding to some Cast Streaming
// Receiver at the other end of a network link. See class level comments for
// Receiver for a high-level overview.
class SenderImpl final : public Sender,
                         public SenderPacketRouter::Sender,
                         public CompoundRtcpParser::Client {
 public:
  // Constructs a Sender that attaches to the given `environment`-provided
  // resources and `packet_router`. The `config` contains the settings that were
  // agreed-upon by both sides from the OFFER/ANSWER exchange (i.e., the part of
  // the overall end-to-end connection process that occurs before Cast Streaming
  // is started). The `rtp_payload_type` does not affect the behavior of this
  // Sender. It is simply passed along to a Receiver in the RTP packet stream.
  SenderImpl(Environment& environment,
             SenderPacketRouter& packet_router,
             SessionConfig config,
             RtpPayloadType rtp_payload_type);

  ~SenderImpl() final;

  // Sender overrides.
  const SessionConfig& config() const override { return config_; }
  void SetObserver(Observer* observer) override;
  size_t GetInFlightFrameCount() const override;
  Clock::duration GetInFlightMediaDuration(
      RtpTimeTicks next_frame_rtp_timestamp) const override;
  Clock::duration GetMaxInFlightMediaDuration() const override;
  bool NeedsKeyFrame() const override;
  FrameId GetNextFrameId() const override;
  Clock::duration GetCurrentRoundTripTime() const override;
  [[nodiscard]] EnqueueFrameResult EnqueueFrame(
      const EncodedFrame& frame) override;
  void CancelInFlightData() override;
  void ReportFrameDropEvent(FrameId frame_id,
                            RtpTimeTicks rtp_timestamp,
                            Clock::time_point drop_time) override;

 private:
  // Tracking/Storage for frames that are ready-to-send, and until they are
  // fully received at the other end.
  struct PendingFrameSlot {
    // The frame to send, or nullopt if this slot is not in use.
    std::optional<EncryptedFrame> frame;

    // Represents which packets need to be sent. Elements are indexed by
    // FramePacketId. A set bit means a packet needs to be sent (or re-sent).
    BitVector send_flags;

    // The time when each of the packets was last sent, or
    // `SenderPacketRouter::kNever` if the packet has not been sent yet.
    // Elements are indexed by FramePacketId. This is used to avoid
    // re-transmitting any given packet too frequently.
    std::vector<Clock::time_point> packet_sent_times;

    PendingFrameSlot();
    ~PendingFrameSlot();

    bool is_active_for_frame(FrameId frame_id) const {
      return frame && frame->frame_id == frame_id;
    }
  };

  // Return value from the ChooseXYZ() helper methods.
  struct ChosenPacket {
    PendingFrameSlot* slot = nullptr;
    FramePacketId packet_id{};

    explicit operator bool() const { return !!slot; }
  };

  // An extension of ChosenPacket that also includes the point-in-time when the
  // packet should be sent.
  struct ChosenPacketAndWhen : public ChosenPacket {
    Clock::time_point when = SenderPacketRouter::kNever;
  };

  // SenderPacketRouter::Sender implementation.
  void OnReceivedRtcpPacket(Clock::time_point arrival_time,
                            ByteView packet) final;
  ByteBuffer GetRtcpPacketForImmediateSend(Clock::time_point send_time,
                                           ByteBuffer buffer) final;
  ByteBuffer GetRtpPacketForImmediateSend(Clock::time_point send_time,
                                          ByteBuffer buffer) final;
  Clock::time_point GetRtpResumeTime() final;
  RtpTimeTicks GetLastRtpTimestamp() const final;
  StreamType GetStreamType() const final;

  // CompoundRtcpParser::Client implementation.
  void OnReceiverReferenceTimeAdvanced(Clock::time_point reference_time) final;
  void OnReceiverReport(const RtcpReportBlock& receiver_report) final;
  void OnCastReceiverFrameLogMessages(
      std::vector<RtcpReceiverFrameLogMessage> messages) final;
  void OnReceiverIndicatesPictureLoss() final;
  void OnReceiverCheckpoint(FrameId frame_id,
                            std::chrono::milliseconds playout_delay) final;
  void OnReceiverHasFrames(std::vector<FrameId> acks) final;
  void OnReceiverIsMissingPackets(std::vector<PacketNack> nacks) final;

  // Helper to choose which packet to send, from those that have been flagged as
  // "need to send." Returns a "false" result if nothing needs to be sent.
  ChosenPacket ChooseNextRtpPacketNeedingSend();

  // Helper that returns the packet that should be used to kick-start the
  // Receiver, and the time at which the packet should be sent. Returns a kNever
  // result if kick-starting is not needed.
  ChosenPacketAndWhen ChooseKickstartPacket();

  // Cancels sending (or resending) the given frame once it is known to have
  // been either:
  //   1. Cancelled by the sender (was_acked must be false);
  //   2. Fully received based on the ACK feedback in a receiver RTCP report
  //      (was_acked must be true);
  //   3. The receiver sent a checkpoint frame ID (was_acked must be true).
  //
  // This clears the corresponding entry in `pending_frames_` and
  // adds `frame_id` to the list of pending cancellations to be dispatched as
  // part of DispatchCancellations().
  //
  // NOTE: Every frame_id ends up being "cancelled" at least once.
  void CancelPendingFrame(FrameId frame_id, bool was_acked);

  // Must be called after one or a series of CancelPendingFrame() calls in order
  // to notify the observer, if any, about cancellations.
  void DispatchCancellations();

  // Inline helper to return the slot that would contain the tracking info for
  // the given `frame_id`.
  const PendingFrameSlot& get_slot_for(FrameId frame_id) const {
    return pending_frames_[(frame_id - FrameId::first()) %
                           pending_frames_.size()];
  }
  PendingFrameSlot& get_slot_for(FrameId frame_id) {
    return pending_frames_[(frame_id - FrameId::first()) %
                           pending_frames_.size()];
  }

  const SessionConfig config_;
  SenderPacketRouter& packet_router_;
  RtcpSession rtcp_session_;
  CompoundRtcpParser rtcp_parser_;
  SenderReportBuilder sender_report_builder_;
  RtpPacketizer rtp_packetizer_;
  const int rtp_timebase_;
  FrameCrypto crypto_;
  StatisticsDispatcher statistics_dispatcher_;

  // Ring buffer of PendingFrameSlots. The frame having FrameId x will always
  // be slotted at position x % pending_frames_.size(). Use get_slot_for() to
  // access the correct slot for a given FrameId.
  std::array<PendingFrameSlot, kMaxUnackedFrames> pending_frames_ = {};

  // A count of the number of frames in-flight (i.e., the number of active
  // entries in `pending_frames_`).
  size_t num_frames_in_flight_ = 0;

  // The ID of the last frame enqueued.
  FrameId last_enqueued_frame_id_ = FrameId::leader();

  // Indicates that all of the packets for all frames up to and including this
  // FrameId have been successfully received (or otherwise do not need to be
  // re-transmitted).
  FrameId checkpoint_frame_id_ = FrameId::leader();

  // The ID of the latest frame the Receiver seems to be aware of.
  FrameId latest_expected_frame_id_ = FrameId::leader();

  // The target playout delay for the last-enqueued frame. This is auto-updated
  // when a frame is enqueued that changes the delay.
  std::chrono::milliseconds target_playout_delay_;
  FrameId playout_delay_change_at_frame_id_ = FrameId::first();

  // The exact arrival time of the last RTCP packet.
  Clock::time_point rtcp_packet_arrival_time_ = SenderPacketRouter::kNever;

  // The near-term average round trip time. This is updated with each Sender
  // Report → Receiver Report round trip. This is initially zero, indicating the
  // round trip time has not been measured yet.
  Clock::duration round_trip_time_ = {};

  // Maintain current stats in a Sender Report that is ready for sending at any
  // time. This includes up-to-date lip-sync information, and packet and byte
  // count stats.
  RtcpSenderReport pending_sender_report_;

  // These are used to determine whether a key frame needs to be sent to the
  // Receiver. When the Receiver provides a picture loss notification, the
  // current checkpoint frame ID is stored in `picture_lost_at_frame_id_`. Then,
  // while `last_enqueued_key_frame_id_` is less than or equal to
  // `picture_lost_at_frame_id_`, the Sender knows it still needs to send a key
  // frame to resolve the picture loss condition. In all other cases, the
  // Receiver is either in a good state or is in the process of receiving the
  // key frame that will make that happen.
  FrameId picture_lost_at_frame_id_ = FrameId::leader();
  FrameId last_enqueued_key_frame_id_ = FrameId::leader();

  // The current observer (optional).
  Observer* observer_ = nullptr;

  // Because the observer may take action when frames are cancelled, such as
  // calling APIs like EnqueueFrame(), `this` must be in a good state before
  // the observer is notified of any pending frame cancellations.
  std::vector<FrameId> pending_cancellations_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_SENDER_IMPL_H_
