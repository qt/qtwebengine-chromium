// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_DEFINES_H_
#define MEDIA_CAST_CAST_DEFINES_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"

namespace media {
namespace cast {

const int64 kDontShowTimeoutMs = 33;
const float kDefaultCongestionControlBackOff = 0.875f;
const uint8 kStartFrameId = 255;
const uint32 kVideoFrequency = 90000;
const int64 kSkippedFramesCheckPeriodkMs = 10000;

// Number of skipped frames threshold in fps (as configured) per period above.
const int kSkippedFramesThreshold = 3;
const size_t kIpPacketSize = 1500;
const int kStartRttMs = 20;
const int64 kCastMessageUpdateIntervalMs = 33;
const int64 kNackRepeatIntervalMs = 30;

enum DefaultSettings {
  kDefaultMaxQp = 56,
  kDefaultMinQp = 4,
  kDefaultMaxFrameRate = 30,
  kDefaultNumberOfVideoBuffers = 1,
  kDefaultRtcpIntervalMs = 500,
  kDefaultRtpHistoryMs = 1000,
  kDefaultRtpMaxDelayMs = 100,
};

const uint16 kRtcpCastAllPacketsLost = 0xffff;

// Each uint16 represents one packet id within a cast frame.
typedef std::set<uint16> PacketIdSet;
// Each uint8 represents one cast frame.
typedef std::map<uint8, PacketIdSet> MissingFramesAndPacketsMap;

// TODO(pwestin): Re-factor the functions bellow into a class with static
// methods.

// Magic fractional unit. Used to convert time (in microseconds) to/from
// fractional NTP seconds.
static const double kMagicFractionalUnit = 4.294967296E3;

// Network Time Protocol (NTP), which is in seconds relative to 0h UTC on
// 1 January 1900.
static const int64 kNtpEpochDeltaSeconds = GG_INT64_C(9435484800);
static const int64 kNtpEpochDeltaMicroseconds =
    kNtpEpochDeltaSeconds * base::Time::kMicrosecondsPerSecond;

inline bool IsNewerFrameId(uint8 frame_id, uint8 prev_frame_id) {
  return (frame_id != prev_frame_id) &&
      static_cast<uint8>(frame_id - prev_frame_id) < 0x80;
}

inline bool IsOlderFrameId(uint8 frame_id, uint8 prev_frame_id) {
  return (frame_id == prev_frame_id) || IsNewerFrameId(prev_frame_id, frame_id);
}

inline bool IsNewerPacketId(uint16 packet_id, uint16 prev_packet_id) {
  return (packet_id != prev_packet_id) &&
      static_cast<uint16>(packet_id - prev_packet_id) < 0x8000;
}

inline bool IsNewerSequenceNumber(uint16 sequence_number,
                                  uint16 prev_sequence_number) {
  // Same function as IsNewerPacketId just different data and name.
  return IsNewerPacketId(sequence_number, prev_sequence_number);
}

// Create a NTP diff from seconds and fractions of seconds; delay_fraction is
// fractions of a second where 0x80000000 is half a second.
inline uint32 ConvertToNtpDiff(uint32 delay_seconds, uint32 delay_fraction) {
  return ((delay_seconds & 0x0000FFFF) << 16) +
         ((delay_fraction & 0xFFFF0000) >> 16);
}

inline base::TimeDelta ConvertFromNtpDiff(uint32 ntp_delay) {
  uint32 delay_ms = (ntp_delay & 0x0000ffff) * 1000;
  delay_ms >>= 16;
  delay_ms += ((ntp_delay & 0xffff0000) >> 16) * 1000;
  return base::TimeDelta::FromMilliseconds(delay_ms);
}

inline void ConvertTimeToFractions(int64 time_us,
                                   uint32* seconds,
                                   uint32* fractions) {
  *seconds = static_cast<uint32>(time_us / base::Time::kMicrosecondsPerSecond);
  *fractions = static_cast<uint32>(
      (time_us % base::Time::kMicrosecondsPerSecond) * kMagicFractionalUnit);
}

inline void ConvertTimeToNtp(const base::TimeTicks& time,
                             uint32* ntp_seconds,
                             uint32* ntp_fractions) {
  int64 time_us = time.ToInternalValue() - kNtpEpochDeltaMicroseconds;
  ConvertTimeToFractions(time_us, ntp_seconds, ntp_fractions);
}

inline base::TimeTicks ConvertNtpToTime(uint32 ntp_seconds,
                                        uint32 ntp_fractions) {
  int64 ntp_time_us = static_cast<int64>(ntp_seconds) *
       base::Time::kMicrosecondsPerSecond;
  ntp_time_us += static_cast<int64>(ntp_fractions) / kMagicFractionalUnit;
  return base::TimeTicks::FromInternalValue(ntp_time_us +
      kNtpEpochDeltaMicroseconds);
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_DEFINES_H_
