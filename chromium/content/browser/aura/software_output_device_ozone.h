// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_DEVICE_OZONE_H_
#define CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_DEVICE_OZONE_H_

#include "cc/output/software_output_device.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Compositor;
}

namespace content {

// Ozone implementation which relies on software rendering. Ozone will present
// an accelerated widget as a SkCanvas. SoftwareOutputDevice will then use the
// Ozone provided canvas to draw.
class CONTENT_EXPORT SoftwareOutputDeviceOzone
    : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceOzone(ui::Compositor* compositor);
  virtual ~SoftwareOutputDeviceOzone();

  virtual void Resize(gfx::Size viewport_size) OVERRIDE;
  virtual SkCanvas* BeginPaint(gfx::Rect damage_rect) OVERRIDE;
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

 private:
  ui::Compositor* compositor_;

  gfx::AcceleratedWidget realized_widget_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceOzone);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_DEVICE_OZONE_H_
