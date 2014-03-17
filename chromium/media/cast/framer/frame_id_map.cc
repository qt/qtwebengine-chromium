// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/frame_id_map.h"

#include "base/logging.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

FrameInfo::FrameInfo(uint32 frame_id,
                     uint32 referenced_frame_id,
                     uint16 max_packet_id,
                     bool key_frame)
    : is_key_frame_(key_frame),
      frame_id_(frame_id),
      referenced_frame_id_(referenced_frame_id),
      max_received_packet_id_(0) {
  if (max_packet_id > 0) {
    // Create the set with all packets missing.
    for (uint16 i = 0; i <= max_packet_id; i++) {
      missing_packets_.insert(i);
    }
  }
}

FrameInfo::~FrameInfo() {}

bool FrameInfo::InsertPacket(uint16 packet_id) {
  // Update the last received packet id.
  if (IsNewerPacketId(packet_id, max_received_packet_id_)) {
    max_received_packet_id_ = packet_id;
  }
  missing_packets_.erase(packet_id);
  return missing_packets_.empty();
}

bool FrameInfo::Complete() const {
  return missing_packets_.empty();
}

void FrameInfo::GetMissingPackets(bool newest_frame,
                                  PacketIdSet* missing_packets) const {
  if (newest_frame) {
    // Missing packets capped by max_received_packet_id_.
    PacketIdSet::const_iterator it_after_last_received =
        missing_packets_.lower_bound(max_received_packet_id_);
    missing_packets->insert(missing_packets_.begin(), it_after_last_received);
  } else {
    missing_packets->insert(missing_packets_.begin(), missing_packets_.end());
  }
}


FrameIdMap::FrameIdMap()
    : waiting_for_key_(true),
      last_released_frame_(kStartFrameId),
      newest_frame_id_(kStartFrameId) {
}

FrameIdMap::~FrameIdMap() {}

bool FrameIdMap::InsertPacket(const RtpCastHeader& rtp_header, bool* complete) {
  uint32 frame_id = rtp_header.frame_id;
  uint32 reference_frame_id;
  if (rtp_header.is_reference) {
    reference_frame_id = rtp_header.reference_frame_id;
  } else {
    reference_frame_id = static_cast<uint32>(frame_id - 1);
  }

  if (rtp_header.is_key_frame && waiting_for_key_) {
    last_released_frame_ = static_cast<uint32>(frame_id - 1);
    waiting_for_key_ = false;
  }

  VLOG(1) << "InsertPacket frame:" << frame_id
          << " packet:" << static_cast<int>(rtp_header.packet_id)
          << " max packet:" << static_cast<int>(rtp_header.max_packet_id);

  if (IsOlderFrameId(frame_id, last_released_frame_) && !waiting_for_key_) {
    return false;
  }

  // Update the last received frame id.
  if (IsNewerFrameId(frame_id, newest_frame_id_)) {
    newest_frame_id_ = frame_id;
  }

  // Does this packet belong to a new frame?
  FrameMap::iterator it = frame_map_.find(frame_id);
  if (it == frame_map_.end()) {
    // New frame.
    linked_ptr<FrameInfo> frame_info(new FrameInfo(frame_id,
                                                   reference_frame_id,
                                                   rtp_header.max_packet_id,
                                                   rtp_header.is_key_frame));
    std::pair<FrameMap::iterator, bool> retval =
        frame_map_.insert(std::make_pair(frame_id, frame_info));

    *complete = retval.first->second->InsertPacket(rtp_header.packet_id);
  } else {
    // Insert packet to existing frame.
    *complete = it->second->InsertPacket(rtp_header.packet_id);
  }
  return true;
}

void FrameIdMap::RemoveOldFrames(uint32 frame_id) {
  FrameMap::iterator it = frame_map_.begin();

  while (it != frame_map_.end()) {
    if (IsNewerFrameId(it->first, frame_id)) {
      ++it;
    } else {
      // Older or equal; erase.
      frame_map_.erase(it++);
    }
  }
  last_released_frame_ = frame_id;
}

void FrameIdMap::Clear() {
  frame_map_.clear();
  waiting_for_key_ = true;
  last_released_frame_ = kStartFrameId;
  newest_frame_id_ = kStartFrameId;
}

uint32 FrameIdMap::NewestFrameId() const {
  return newest_frame_id_;
}

bool FrameIdMap::NextContinuousFrame(uint32* frame_id) const {
  FrameMap::const_iterator it;

  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete() && ContinuousFrame(it->second.get())) {
      *frame_id = it->first;
      return true;
    }
  }
  return false;
}

uint32 FrameIdMap::LastContinuousFrame() const {
  uint32 last_continuous_frame_id = last_released_frame_;
  uint32 next_expected_frame = last_released_frame_;

  FrameMap::const_iterator it;

  do {
    next_expected_frame++;
    it = frame_map_.find(next_expected_frame);
    if (it == frame_map_.end()) break;
    if (!it->second->Complete()) break;

    // We found the next continuous frame.
    last_continuous_frame_id = it->first;
  } while (next_expected_frame != newest_frame_id_);
  return last_continuous_frame_id;
}

bool FrameIdMap::NextAudioFrameAllowingMissingFrames(uint32* frame_id) const {
  // First check if we have continuous frames.
  if (NextContinuousFrame(frame_id)) return true;

  // Find the oldest frame.
  FrameMap::const_iterator it_best_match = frame_map_.end();
  FrameMap::const_iterator it;

  // Find first complete frame.
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete()) {
      it_best_match = it;
      break;
    }
  }
  if (it_best_match == frame_map_.end()) return false;  // No complete frame.

  ++it;
  for (; it != frame_map_.end(); ++it) {
    if (it->second->Complete() &&
        IsOlderFrameId(it->first, it_best_match->first)) {
      it_best_match = it;
    }
  }
  *frame_id = it_best_match->first;
  return true;
}

bool FrameIdMap::NextVideoFrameAllowingSkippingFrames(uint32* frame_id) const {
  // Find the oldest decodable frame.
  FrameMap::const_iterator it_best_match = frame_map_.end();
  FrameMap::const_iterator it;
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete() && DecodableVideoFrame(it->second.get())) {
      it_best_match = it;
    }
  }
  if (it_best_match == frame_map_.end()) return false;

  *frame_id = it_best_match->first;
  return true;
}

bool FrameIdMap::Empty() const {
  return frame_map_.empty();
}

int FrameIdMap::NumberOfCompleteFrames() const {
  int count = 0;
  FrameMap::const_iterator it;
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete()) {
      ++count;
    }
  }
  return count;
}

bool FrameIdMap::FrameExists(uint32 frame_id) const {
  return frame_map_.end() != frame_map_.find(frame_id);
}

void FrameIdMap::GetMissingPackets(uint32 frame_id,
                                   bool last_frame,
                                   PacketIdSet* missing_packets) const {
  FrameMap::const_iterator it = frame_map_.find(frame_id);
  if (it == frame_map_.end()) return;

  it->second->GetMissingPackets(last_frame, missing_packets);
}

bool FrameIdMap::ContinuousFrame(FrameInfo* frame) const {
  DCHECK(frame);
  if (waiting_for_key_ && !frame->is_key_frame()) return false;
  return static_cast<uint32>(last_released_frame_ + 1) == frame->frame_id();
}

bool FrameIdMap::DecodableVideoFrame(FrameInfo* frame) const {
  if (frame->is_key_frame()) return true;
  if (waiting_for_key_ && !frame->is_key_frame()) return false;

  // Current frame is not necessarily referencing the last frame.
  // Do we have the reference frame?
  if (IsOlderFrameId(frame->referenced_frame_id(), last_released_frame_)) {
    return true;
  }
  return frame->referenced_frame_id() == last_released_frame_;
}

}  //  namespace cast
}  //  namespace media
