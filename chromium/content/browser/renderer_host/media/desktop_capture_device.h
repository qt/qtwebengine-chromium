// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_DESKTOP_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_DESKTOP_CAPTURE_DEVICE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopCapturer;
}  // namespace webrtc

namespace content {

struct DesktopMediaID;

// DesktopCaptureDevice implements VideoCaptureDevice for screens and windows.
// It's essentially an adapter between webrtc::DesktopCapturer and
// VideoCaptureDevice.
class CONTENT_EXPORT DesktopCaptureDevice : public media::VideoCaptureDevice1 {
 public:
  // Creates capturer for the specified |source| and then creates
  // DesktopCaptureDevice for it. May return NULL in case of a failure (e.g. if
  // requested window was destroyed).
  static scoped_ptr<media::VideoCaptureDevice> Create(
      const DesktopMediaID& source);

  DesktopCaptureDevice(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       scoped_ptr<webrtc::DesktopCapturer> desktop_capturer);
  virtual ~DesktopCaptureDevice();

  // VideoCaptureDevice interface.
  virtual void Allocate(const media::VideoCaptureCapability& capture_format,
                        EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;
  virtual const Name& device_name() OVERRIDE;

 private:
  class Core;
  scoped_refptr<Core> core_;
  Name name_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureDevice);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_DESKTOP_CAPTURE_DEVICE_H_
