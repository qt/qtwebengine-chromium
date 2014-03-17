// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_

#include "ash/wm/video_detector.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"

namespace ash {
namespace internal {

// Notifies the power manager when a video is playing.
class VideoActivityNotifier : public VideoDetectorObserver {
 public:
  explicit VideoActivityNotifier(VideoDetector* detector);
  virtual ~VideoActivityNotifier();

  // VideoDetectorObserver implementation.
  virtual void OnVideoDetected(bool is_fullscreen) OVERRIDE;

 private:
  VideoDetector* detector_;  // not owned

  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(VideoActivityNotifier);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
