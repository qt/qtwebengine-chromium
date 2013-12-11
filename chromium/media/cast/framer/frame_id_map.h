// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_
#define MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_

#include <map>
#include <set>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace media {
namespace cast {

class FrameInfo {
 public:
  FrameInfo(uint8 frame_id,
            uint8 referenced_frame_id,
            uint16 max_packet_id,
            bool key_frame);
  ~FrameInfo();

  // Returns true if frame is complete after the insert.
  bool InsertPacket(uint16 packet_id);
  bool Complete() const;
  void GetMissingPackets(bool newest_frame,
                         PacketIdSet* missing_packets) const;

  bool is_key_frame() const { return is_key_frame_; }
  uint8 frame_id() const { return frame_id_; }
  uint8 referenced_frame_id() const { return referenced_frame_id_; }

 private:
  const bool is_key_frame_;
  const uint8 frame_id_;
  const uint8 referenced_frame_id_;

  uint16 max_received_packet_id_;
  PacketIdSet missing_packets_;

  DISALLOW_COPY_AND_ASSIGN(FrameInfo);
};

typedef std::map<uint8, linked_ptr<FrameInfo> > FrameMap;

class FrameIdMap {
 public:
  FrameIdMap();
  ~FrameIdMap();

  // Returns false if not a valid (old) packet, otherwise returns true.
  bool InsertPacket(const RtpCastHeader& rtp_header, bool* complete);

  bool Empty() const;
  bool FrameExists(uint8 frame_id) const;
  uint8 NewestFrameId() const;

  void RemoveOldFrames(uint8 frame_id);
  void Clear();

  // Identifies the next frame to be released (rendered).
  bool NextContinuousFrame(uint8* frame_id) const;
  uint8 LastContinuousFrame() const;

  bool NextAudioFrameAllowingMissingFrames(uint8* frame_id) const;
  bool NextVideoFrameAllowingSkippingFrames(uint8* frame_id) const;

  int NumberOfCompleteFrames() const;
  void GetMissingPackets(uint8 frame_id,
                         bool last_frame,
                         PacketIdSet* missing_packets) const;

 private:
  bool ContinuousFrame(FrameInfo* frame) const;
  bool DecodableVideoFrame(FrameInfo* frame) const;

  FrameMap frame_map_;
  bool waiting_for_key_;
  uint8 last_released_frame_;
  uint8 newest_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameIdMap);
};

}  //  namespace cast
}  //  namespace media

#endif  // MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_
