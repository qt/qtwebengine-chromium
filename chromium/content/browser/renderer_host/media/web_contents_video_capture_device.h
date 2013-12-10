// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

class RenderWidgetHost;

// A virtualized VideoCaptureDevice that mirrors the displayed contents of a
// tab (accessed via its associated WebContents instance), producing a stream of
// video frames.
//
// An instance is created by providing a device_id.  The device_id contains the
// routing ID for a RenderViewHost, and from the RenderViewHost instance, a
// reference to its associated WebContents instance is acquired.  From then on,
// WebContentsVideoCaptureDevice will capture from whatever render view is
// currently associated with that WebContents instance.  This allows the
// underlying render view to be swapped out (e.g., due to navigation or
// crashes/reloads), without any interruption in capturing.
class CONTENT_EXPORT WebContentsVideoCaptureDevice
    : public media::VideoCaptureDevice1 {
 public:
  // Construct from a |device_id| string of the form:
  //   "virtual-media-stream://render_process_id:render_view_id", where
  // |render_process_id| and |render_view_id| are decimal integers.
  // |destroy_cb| is invoked on an outside thread once all outstanding objects
  // are completely destroyed -- this will be some time after the
  // WebContentsVideoCaptureDevice is itself deleted.
  // TODO(miu): Passing a destroy callback suggests needing to revisit the
  // design philosophy of an asynchronous DeAllocate().  http://crbug.com/158641
  static media::VideoCaptureDevice1* Create(const std::string& device_id);

  virtual ~WebContentsVideoCaptureDevice();

  // VideoCaptureDevice implementation.
  virtual void Allocate(const media::VideoCaptureCapability& capture_format,
                        VideoCaptureDevice::EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;

  // Note: The following is just a pass-through of the device_id provided to the
  // constructor.  It does not change when the content of the page changes
  // (e.g., due to navigation), or when the underlying RenderView is
  // swapped-out.
  virtual const Name& device_name() OVERRIDE;

 private:
  class Impl;

  WebContentsVideoCaptureDevice(const Name& name,
                                int render_process_id,
                                int render_view_id);

  Name device_name_;
  const scoped_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDevice);
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
