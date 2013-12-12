// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_FRAMER_FRAMER_H_
#define MEDIA_CAST_FRAMER_FRAMER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/framer/cast_message_builder.h"
#include "media/cast/framer/frame_buffer.h"
#include "media/cast/framer/frame_id_map.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace media {
namespace cast {

typedef std::map<uint8, linked_ptr<FrameBuffer> > FrameList;

class Framer {
 public:
  Framer(RtpPayloadFeedback* incoming_payload_feedback,
         uint32 ssrc,
         bool decoder_faster_than_max_frame_rate,
         int max_unacked_frames);
  ~Framer();

  void InsertPacket(const uint8* payload_data,
                    int payload_size,
                    const RtpCastHeader& rtp_header);

  // Extracts a complete encoded frame - will only return a complete continuous
  // frame.
  // Returns false if the frame does not exist or if the frame is not complete
  // within the given time frame.
  bool GetEncodedVideoFrame(const base::TimeTicks& timeout,
                            EncodedVideoFrame* video_frame,
                            uint32* rtp_timestamp,
                            bool* next_frame);

  bool GetEncodedAudioFrame(const base::TimeTicks& timeout,
                            EncodedAudioFrame* audio_frame,
                            uint32* rtp_timestamp,
                            bool* next_frame);

  void ReleaseFrame(uint8 frame_id);

  // Reset framer state to original state and flush all pending buffers.
  void Reset();
  bool TimeToSendNextCastMessage(base::TimeTicks* time_to_send);
  void SendCastMessage();

  void set_clock(base::TickClock* clock) {
    clock_ = clock;
    cast_msg_builder_->set_clock(clock);
  }

 private:
  // Return true if we should wait.
  bool WaitForNextFrame(const base::TimeTicks& timeout) const;

  const bool decoder_faster_than_max_frame_rate_;
  FrameList frames_;
  FrameIdMap frame_id_map_;

  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_;

  scoped_ptr<CastMessageBuilder> cast_msg_builder_;

  DISALLOW_COPY_AND_ASSIGN(Framer);
};

}  //  namespace cast
}  //  namespace media

#endif  // MEDIA_CAST_FRAMER_FRAMER_H_
