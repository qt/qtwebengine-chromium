// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_POOL_H_
#define MEDIA_BASE_VIDEO_FRAME_POOL_H_

#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace media {

// Simple VideoFrame pool used to avoid unnecessarily allocating and destroying
// VideoFrame objects. The pool manages the memory for the VideoFrame
// returned by CreateFrame(). When one of these VideoFrames is destroyed,
// the memory is returned to the pool for use by a subsequent CreateFrame()
// call. The memory in the pool is retained for the life of the
// VideoFramePool object. If the parameters passed to CreateFrame() change
// during the life of this object, then the memory used by frames with the old
// parameter values will be purged from the pool.
class MEDIA_EXPORT VideoFramePool {
 public:
  VideoFramePool();
  ~VideoFramePool();

  // Returns a frame from the pool that matches the specified
  // parameters or creates a new frame if no suitable frame exists in
  // the pool.
  scoped_refptr<VideoFrame> CreateFrame(VideoFrame::Format format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp);

protected:
  friend class VideoFramePoolTest;

  // Returns the number of frames in the pool for testing purposes.
  size_t GetPoolSizeForTesting() const;

 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_;

  DISALLOW_COPY_AND_ASSIGN(VideoFramePool);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_POOL_H_
