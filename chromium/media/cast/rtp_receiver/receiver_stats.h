// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEIVER_RECEIVER_STATS_H_
#define MEDIA_CAST_RTP_RECEIVER_RECEIVER_STATS_H_

#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace media {
namespace cast {

class ReceiverStats {
 public:
  explicit ReceiverStats(uint32 ssrc);
  ~ReceiverStats();
  void GetStatistics(uint8* fraction_lost,
                     uint32* cumulative_lost,  // 24 bits valid.
                     uint32* extended_high_sequence_number,
                     uint32* jitter);
  void UpdateStatistics(const RtpCastHeader& header);

  void set_clock(base::TickClock* clock) {
    clock_ = clock;
  }

 private:
  const uint32 ssrc_;

  // Global metrics.
  uint16 min_sequence_number_;
  uint16 max_sequence_number_;
  uint32 total_number_packets_;
  uint16 sequence_number_cycles_;
  base::TimeDelta last_received_timestamp_;
  base::TimeTicks last_received_packet_time_;
  base::TimeDelta jitter_;

  // Intermediate metrics - between RTCP reports.
  int interval_min_sequence_number_;
  int interval_number_packets_;
  int interval_wrap_count_;
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEIVER_RECEIVER_STATS_H_
