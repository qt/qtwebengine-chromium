// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_PUBLIC_SENDER_H_
#define CAST_STREAMING_PUBLIC_SENDER_H_

#include <stdint.h>

#include <chrono>

#include "cast/streaming/public/encoded_frame.h"
#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/public/session_config.h"
#include "cast/streaming/rtp_time.h"
#include "cast/streaming/ssrc.h"
#include "platform/api/time.h"

namespace openscreen::cast {

// The Cast Streaming Sender, a peer corresponding to some Cast Streaming
// Receiver at the other end of a network link.
//
// The Sender is the peer responsible for enqueuing EncodedFrames for streaming,
// guaranteeing their delivery to a Receiver, and handling feedback events from
// a Receiver. Some feedback events are used for managing the Sender's internal
// queue of in-flight frames, requesting network packet re-transmits, etc.;
// while others are exposed via the Sender's public interface. For example,
// sometimes the Receiver signals that it needs a a key frame to resolve a
// picture loss condition, and the modules upstream of the Sender (e.g., where
// encoding happens) should call NeedsKeyFrame() to check for, and handle that.
//
// There are usually one or two Senders in a streaming session, one for audio
// and one for video. Both senders work with the same SenderPacketRouter
// instance to schedule their transmission of packets, and provide the necessary
// metrics for estimating bandwidth utilization and availability.
//
// It is the responsibility of upstream code modules to handle congestion
// control. With respect to this Sender, that means the media encoding bit rate
// should be throttled based on network bandwidth availability. This Sender does
// not do any throttling, only flow-control. In other words, this Sender can
// only manage its in-flight queue of frames, and if that queue grows too large,
// it will eventually reject further enqueuing.
//
// General usage: A client should check the in-flight media duration frequently
// to decide when to pause encoding, to avoid wasting system resources on
// encoding frames that will likely be rejected by the Sender. The client should
// also frequently call NeedsKeyFrame() and, when this returns true, direct its
// encoder to produce a key frame soon. Finally, when using EnqueueFrame(), an
// EncodedFrame struct should be prepared with its frame_id field set to
// whatever GetNextFrameId() returns. Please see method comments for
// more-detailed usage info.
class Sender {
 public:
  // Interface for receiving notifications about events of possible interest.
  class Observer {
   public:
    // Called when a frame was canceled, which may occur in the following cases:
    //  - The Receiver acknowledged successful receipt of the frame.
    //  - The Receiver decided to skip over the frame (e.g. it was too late).
    //  - The Sender decided to skip the frame (e.g. OnFrameCanceled() called).
    //
    // Note: Frame cancellations may occur out-of-order.
    virtual void OnFrameCanceled(FrameId frame_id) = 0;

    // Called when a Receiver begins reporting picture loss, and there is no key
    // frame currently enqueued in the Sender. The application should enqueue a
    // key frame as soon as possible.
    //
    // This acts as a "push" notification, which is useful for immediately
    // waking up an application that may be waiting for the next capture tick.
    // For "pull" state checking inside a continuous encoding loop, see
    // NeedsKeyFrame().
    virtual void OnPictureLost() = 0;

   protected:
    virtual ~Observer();
  };

  // Result codes for EnqueueFrame().
  enum EnqueueFrameResult {
    // The frame has been queued for sending.
    OK,

    // The frame's payload was too large.
    PAYLOAD_TOO_LARGE,

    // The span of FrameIds is too large.
    REACHED_ID_SPAN_LIMIT,

    // Too-large a media duration is in-flight.
    MAX_DURATION_IN_FLIGHT,
  };

  virtual ~Sender();

  // The session configuration for this sender. The configuration is generated
  // from the offer/answer exchange, and includes critical information like the
  // RTP timebase, SSRCs for sending and receiving, and the AES configuration.
  virtual const SessionConfig& config() const = 0;

  // Sets an observer for receiving notifications. Call with nullptr to stop
  // observing.
  virtual void SetObserver(Observer* observer) = 0;

  // Returns the number of frames currently in-flight. This is only meant to be
  // informative. Clients should use GetInFlightMediaDuration() to make
  // throttling decisions.
  virtual size_t GetInFlightFrameCount() const = 0;

  // Returns the total media duration of the frames currently in-flight,
  // assuming the next not-yet-enqueued frame will have the given RTP timestamp.
  // For a better user experience, the result should be compared to
  // GetMaxInFlightMediaDuration(), and media encoding should be throttled down
  // before additional EnqueueFrame() calls would cause this to reach the
  // current maximum limit.
  virtual Clock::duration GetInFlightMediaDuration(
      RtpTimeTicks next_frame_rtp_timestamp) const = 0;

  // Return the maximum acceptable in-flight media duration, given the current
  // target playout delay setting and end-to-end network/system conditions.
  virtual Clock::duration GetMaxInFlightMediaDuration() const = 0;

  // Returns true if the Receiver requires a key frame. Note that this will
  // return true until a key frame is accepted by EnqueueFrame(). Thus, when
  // encoding is pipelined, care should be taken to instruct the encoder to
  // produce just ONE forced key frame.
  //
  // This acts as a stateful "pull" check, which is useful for an encoder loop
  // to poll right before processing the next image. For "push" notifications
  // to wake up an idle application, see Observer::OnPictureLost().
  virtual bool NeedsKeyFrame() const = 0;

  // Returns the next FrameId, the one after the frame enqueued by the last call
  // to EnqueueFrame(). Note that the next call to EnqueueFrame() assumes this
  // frame ID be used.
  virtual FrameId GetNextFrameId() const = 0;

  // Get the current round trip time, defined as the total time between when the
  // sender report is sent and the receiver report is received. This value is
  // updated with each receiver report using a weighted moving average of 1/8
  // for the new value and 7/8 for the previous value. Will be set to
  // Clock::duration::zero() if no reports have been received yet.
  // TODO(crbug.com/498036656): move to a more modern approach for estimating
  // bandwidth.
  virtual Clock::duration GetCurrentRoundTripTime() const = 0;

  // Enqueues the given `frame` for sending as soon as possible. Returns OK if
  // the frame is accepted, and some time later Observer::OnFrameCanceled() will
  // be called once it is no longer in-flight.
  //
  // All fields of the `frame` must be set to valid values: the `frame_id` must
  // be the same as GetNextFrameId(); both the `rtp_timestamp` and
  // `reference_time` fields must be monotonically increasing relative to the
  // prior frame; and the frame's `data` pointer must be set.
  [[nodiscard]] virtual EnqueueFrameResult EnqueueFrame(
      const EncodedFrame& frame) = 0;

  // Causes all pending operations to discard data when they are processed
  // later. This will notify observers by invoking OnFrameCanceled() for each
  // canceled frame.
  virtual void CancelInFlightData() = 0;

  // May be called by the consumer to report that a frame has been dropped. This
  // is used to report drop statistics to the sender's statistics collector.
  virtual void ReportFrameDropEvent(FrameId frame_id,
                                    RtpTimeTicks rtp_timestamp,
                                    Clock::time_point drop_time) = 0;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_PUBLIC_SENDER_H_
