// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/framer.h"

#include "base/logging.h"

namespace media {
namespace cast {

typedef FrameList::const_iterator ConstFrameIterator;

Framer::Framer(RtpPayloadFeedback* incoming_payload_feedback,
               uint32 ssrc,
               bool decoder_faster_than_max_frame_rate,
               int max_unacked_frames)
    : decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      clock_(&default_tick_clock_),
      cast_msg_builder_(new CastMessageBuilder(incoming_payload_feedback,
          &frame_id_map_, ssrc, decoder_faster_than_max_frame_rate,
          max_unacked_frames)) {
  DCHECK(incoming_payload_feedback) << "Invalid argument";
}

Framer::~Framer() {}

void Framer::InsertPacket(const uint8* payload_data,
                          int payload_size,
                          const RtpCastHeader& rtp_header) {
  bool complete = false;
  if (!frame_id_map_.InsertPacket(rtp_header, &complete)) return;

  // Does this packet belong to a new frame?
  FrameList::iterator it = frames_.find(rtp_header.frame_id);
  if (it == frames_.end()) {
    // New frame.
    linked_ptr<FrameBuffer> frame_buffer(new FrameBuffer());
    frame_buffer->InsertPacket(payload_data, payload_size, rtp_header);
    frames_.insert(std::make_pair(rtp_header.frame_id, frame_buffer));
  } else {
    // Insert packet to existing frame buffer.
    it->second->InsertPacket(payload_data, payload_size, rtp_header);
  }

  if (complete) {
    // ACK as soon as possible.
    cast_msg_builder_->CompleteFrameReceived(rtp_header.frame_id,
                                             rtp_header.is_key_frame);
  }
}

// This does not release the frame.
bool Framer::GetEncodedAudioFrame(const base::TimeTicks& timeout,
                                  EncodedAudioFrame* audio_frame,
                                  uint32* rtp_timestamp,
                                  bool* next_frame) {
  uint8 frame_id;
  // Find frame id.
  if (frame_id_map_.NextContinuousFrame(&frame_id)) {
    // We have our next frame.
    *next_frame = true;
  } else {
    if (WaitForNextFrame(timeout)) return false;

    if (!frame_id_map_.NextAudioFrameAllowingMissingFrames(&frame_id)) {
      return false;
    }
    *next_frame = false;
  }

  ConstFrameIterator it = frames_.find(frame_id);
  DCHECK(it != frames_.end());
  if (it == frames_.end()) return false;

  return it->second->GetEncodedAudioFrame(audio_frame, rtp_timestamp);
}

// This does not release the frame.
bool Framer::GetEncodedVideoFrame(const base::TimeTicks& timeout,
                                  EncodedVideoFrame* video_frame,
                                  uint32* rtp_timestamp,
                                  bool* next_frame) {
  uint8 frame_id;
  // Find frame id.
  if (frame_id_map_.NextContinuousFrame(&frame_id)) {
    // We have our next frame.
    *next_frame = true;
  } else {
    if (WaitForNextFrame(timeout)) return false;

    // Check if we can skip frames when our decoder is too slow.
    if (!decoder_faster_than_max_frame_rate_) return false;

    if (!frame_id_map_.NextVideoFrameAllowingSkippingFrames(&frame_id)) {
      return false;
    }
    *next_frame = false;
  }

  ConstFrameIterator it = frames_.find(frame_id);
  DCHECK(it != frames_.end());
  if (it == frames_.end())  return false;

  return it->second->GetEncodedVideoFrame(video_frame, rtp_timestamp);
}

bool Framer::WaitForNextFrame(const base::TimeTicks& timeout) const {
  base::TimeDelta wait_time = timeout - clock_->NowTicks();
  if (wait_time.InMilliseconds() > 0)
    return true;

  return false;
}

void Framer::Reset() {
  frame_id_map_.Clear();
  frames_.clear();
  cast_msg_builder_->Reset();
}

void Framer::ReleaseFrame(uint8 frame_id) {
  frame_id_map_.RemoveOldFrames(frame_id);
  frames_.erase(frame_id);

  // We have a frame - remove all frames with lower frame id.
  FrameList::iterator it;
  for (it = frames_.begin(); it != frames_.end(); ) {
    if (IsOlderFrameId(it->first, frame_id)) {
      frames_.erase(it++);
    } else {
      ++it;
    }
  }
}

bool Framer::TimeToSendNextCastMessage(base::TimeTicks* time_to_send) {
  return cast_msg_builder_->TimeToSendNextCastMessage(time_to_send);
}

void Framer::SendCastMessage() {
  cast_msg_builder_->UpdateCastMessage();
}

}  // namespace cast
}  // namespace media
