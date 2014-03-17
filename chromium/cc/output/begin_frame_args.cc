// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/begin_frame_args.h"
#include "ui/gfx/frame_time.h"

namespace cc {

BeginFrameArgs::BeginFrameArgs()
  : frame_time(base::TimeTicks()),
    deadline(base::TimeTicks()),
    interval(base::TimeDelta::FromMicroseconds(-1)) {
}

BeginFrameArgs::BeginFrameArgs(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval)
  : frame_time(frame_time),
    deadline(deadline),
    interval(interval)
{}

BeginFrameArgs BeginFrameArgs::Create(base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval) {
  return BeginFrameArgs(frame_time, deadline, interval);
}

BeginFrameArgs BeginFrameArgs::CreateForSynchronousCompositor() {
  // For WebView/SynchronousCompositor, we always want to draw immediately,
  // so we set the deadline to 0 and guess that the interval is 16 milliseconds.
  return BeginFrameArgs(gfx::FrameTime::Now(),
                        base::TimeTicks(),
                        DefaultInterval());
}

BeginFrameArgs BeginFrameArgs::CreateForTesting() {
  base::TimeTicks now = gfx::FrameTime::Now();
  return BeginFrameArgs(now,
                        now + (DefaultInterval() / 2),
                        DefaultInterval());
}

BeginFrameArgs BeginFrameArgs::CreateExpiredForTesting() {
  base::TimeTicks now = gfx::FrameTime::Now();
  return BeginFrameArgs(now,
                        now - DefaultInterval(),
                        DefaultInterval());
}

// This is a hard-coded deadline adjustment that assumes 60Hz, to be used in
// cases where a good estimated draw time is not known. Using 1/3 of the vsync
// as the default adjustment gives the Browser the last 1/3 of a frame to
// produce output, the Renderer Impl thread the middle 1/3 of a frame to produce
// ouput, and the Renderer Main thread the first 1/3 of a frame to produce
// output.
base::TimeDelta BeginFrameArgs::DefaultDeadlineAdjustment() {
  return base::TimeDelta::FromMicroseconds(-16666 / 3);
}

base::TimeDelta BeginFrameArgs::DefaultInterval() {
  return base::TimeDelta::FromMicroseconds(16666);
}

base::TimeDelta BeginFrameArgs::DefaultRetroactiveBeginFramePeriod() {
  return base::TimeDelta::FromMicroseconds(4444);
}

}  // namespace cc
