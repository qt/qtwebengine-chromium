// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_SENDER_PACKET_STORAGE_INCLUDE_PACKET_STORAGE_H_
#define MEDIA_CAST_RTP_SENDER_PACKET_STORAGE_INCLUDE_PACKET_STORAGE_H_

#include <list>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {

class StoredPacket;
typedef std::map<uint32, linked_ptr<StoredPacket> > PacketMap;
typedef std::multimap<base::TimeTicks, uint32> TimeToPacketMap;

class PacketStorage {
 public:
  static const int kMaxStoredPackets = 1000;

  explicit PacketStorage(int max_time_stored_ms);
  virtual ~PacketStorage();

  void StorePacket(uint8 frame_id,
                   uint16 packet_id,
                   const std::vector<uint8>& packet);

  // Copies packet into the buffer pointed to by rtp_buffer.
  bool GetPacket(uint8 frame_id,
                 uint16 packet_id,
                 std::vector<uint8>* packet);
  void set_clock(base::TickClock* clock) {
    clock_ = clock;
  }

 private:
  void CleanupOldPackets(base::TimeTicks now);

  base::TimeDelta max_time_stored_;
  PacketMap stored_packets_;
  TimeToPacketMap time_to_packet_map_;
  std::list<linked_ptr<StoredPacket> > free_packets_;
  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;
};

}  // namespace cast
}  // namespace media

#endif // MEDIA_CAST_RTP_SENDER_PACKET_STORAGE_INCLUDE_PACKET_STORAGE_H_
