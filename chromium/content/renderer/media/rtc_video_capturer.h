// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "content/renderer/media/rtc_video_capture_delegate.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"

namespace content {
class VideoCaptureImplManager;

// RtcVideoCapturer implements a simple cricket::VideoCapturer that is used for
// VideoCapturing in libJingle and especially in PeerConnections.
// The class is created and destroyed on the main render thread.
// PeerConnection access cricket::VideoCapturer from a libJingle worker thread.
// The video frames are delivered in OnFrameCaptured on a thread owned by
// Chrome's video capture implementation.
class RtcVideoCapturer
    : public cricket::VideoCapturer {
 public:
  RtcVideoCapturer(const media::VideoCaptureSessionId id,
                   VideoCaptureImplManager* vc_manager,
                   bool is_screencast);
  virtual ~RtcVideoCapturer();

  // cricket::VideoCapturer implementation.
  // These methods are accessed from a libJingle worker thread.
  virtual cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsRunning() OVERRIDE;
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs) OVERRIDE;
  virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                    cricket::VideoFormat* best_format) OVERRIDE;
  virtual bool IsScreencast() const OVERRIDE;

 private:
  // Frame captured callback method.
  virtual void OnFrameCaptured(const scoped_refptr<media::VideoFrame>& frame);

  // State change callback, must be called on same thread as Start is called.
  void OnStateChange(RtcVideoCaptureDelegate::CaptureState state);

  const bool is_screencast_;
  scoped_refptr<RtcVideoCaptureDelegate> delegate_;
  VideoCaptureState state_;
  base::TimeDelta first_frame_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(RtcVideoCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURER_H_
