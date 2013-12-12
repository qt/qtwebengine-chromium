// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_MOCK_RTCP_SENDER_FEEDBACK_H_
#define MEDIA_CAST_RTCP_MOCK_RTCP_SENDER_FEEDBACK_H_

#include <vector>

#include "media/cast/rtcp/rtcp_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockRtcpSenderFeedback : public RtcpSenderFeedback {
 public:
  MOCK_METHOD1(OnReceivedReportBlock,
               void(const RtcpReportBlock& report_block));

  MOCK_METHOD0(OnReceivedIntraFrameRequest, void());

  MOCK_METHOD2(OnReceivedRpsi, void(uint8 payload_type, uint64 picture_id));

  MOCK_METHOD1(OnReceivedRemb, void(uint32 bitrate));

  MOCK_METHOD1(OnReceivedNackRequest,
               void(const std::list<uint16>& nack_sequence_numbers));

  MOCK_METHOD1(OnReceivedCastFeedback,
               void(const RtcpCastMessage& cast_feedback));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_MOCK_RTCP_SENDER_FEEDBACK_H_
