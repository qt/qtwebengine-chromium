// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_INFO_H_
#define ASH_DISPLAY_DISPLAY_INFO_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

// A struct that represents the display's resolution and
// interlaced info.
struct ASH_EXPORT Resolution {
  Resolution(const gfx::Size& size, bool interlaced);

  gfx::Size size;
  bool interlaced;
};

// DisplayInfo contains metadata for each display. This is used to
// create |gfx::Display| as well as to maintain extra infomation
// to manage displays in ash environment.
// This class is intentionally made copiable.
class ASH_EXPORT DisplayInfo {
 public:
  // Creates a DisplayInfo from string spec. 100+200-1440x800 creates display
  // whose size is 1440x800 at the location (100, 200) in host coordinates.
  // The format is
  //
  // [origin-]widthxheight[*device_scale_factor][/<properties>][@ui-scale]
  //
  // where [] are optional:
  // - |origin| is given in x+y- format.
  // - |device_scale_factor| is either 2 or 1 (or empty).
  // - properties can combination of 'o', which adds default overscan insets
  //   (5%), and one rotation property where 'r' is 90 degree clock-wise
  //   (to the 'r'ight) 'u' is 180 degrees ('u'pside-down) and 'l' is
  //   270 degrees (to the 'l'eft).
  // - ui-scale is floating value, e.g. @1.5 or @1.25.
  //
  // A couple of examples:
  // "100x100"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 ui scale.
  // "5+5-300x200*2"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 ui scale.
  // "300x200/ol"
  //      300x200 window at 0,0 origin. 1x device scale factor.
  //      with 5% overscan. rotated to left (90 degree counter clockwise).
  //      1.0 ui scale.
  // "10+20-300x200/u@1.5"
  //      300x200 window at 10,20 origin. 1x device scale factor.
  //      no overscan. flipped upside-down (180 degree) and 1.5 ui scale.
  static DisplayInfo CreateFromSpec(const std::string& spec);

  // Creates a DisplayInfo from string spec using given |id|.
  static DisplayInfo CreateFromSpecWithID(const std::string& spec,
                                          int64 id);

  DisplayInfo();
  DisplayInfo(int64 id, const std::string& name, bool has_overscan);
  ~DisplayInfo();

  int64 id() const { return id_; }

  // The name of the display.
  const std::string& name() const { return name_; }

  // True if the display EDID has the overscan flag. This does not create the
  // actual overscan automatically, but used in the message.
  bool has_overscan() const { return has_overscan_; }

  void set_rotation(gfx::Display::Rotation rotation) { rotation_ = rotation; }
  gfx::Display::Rotation rotation() const { return rotation_; }

  // Gets/Sets the device scale factor of the display.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // The bounds_in_pixel for the display. The size of this can be different from
  // the |size_in_pixel| in case of overscan insets.
  const gfx::Rect bounds_in_pixel() const {
    return bounds_in_pixel_;
  }

  // The size for the display in pixels.
  const gfx::Size& size_in_pixel() const { return size_in_pixel_; }

  // The overscan insets for the display in DIP.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  float ui_scale() const { return ui_scale_; }
  void set_ui_scale(float scale) { ui_scale_ = scale; }

  // Copy the display info except for fields that can be modified by a user
  // (|rotation_| and |ui_scale_|). |rotation_| and |ui_scale_| are copied
  // when the |another_info| isn't native one.
  void Copy(const DisplayInfo& another_info);

  // Update the |bounds_in_pixel_| and |size_in_pixel_| using
  // given |bounds_in_pixel|.
  void SetBounds(const gfx::Rect& bounds_in_pixel);

  // Update the |bounds_in_pixel| according to the current overscan
  // and rotation settings.
  void UpdateDisplaySize();

  // Sets/Clears the overscan insets.
  void SetOverscanInsets(const gfx::Insets& insets_in_dip);
  gfx::Insets GetOverscanInsetsInPixel() const;

  void set_native(bool native) { native_ = native; }
  bool native() const { return native_; }

  const std::vector<Resolution>& resolutions() const {
    return resolutions_;
  }
  void set_resolutions(std::vector<Resolution>& resolution) {
    resolutions_.swap(resolution);
  }

  // Returns a string representation of the DisplayInfo
  // excluding resolutions.
  std::string ToString() const;

  // Returns a string representation of the DisplayInfo
  // including resolutions.
  std::string ToFullString() const;

 private:
  int64 id_;
  std::string name_;
  bool has_overscan_;
  gfx::Display::Rotation rotation_;
  float device_scale_factor_;
  gfx::Rect bounds_in_pixel_;
  // The size of the display in use. The size can be different from the size
  // of |bounds_in_pixel_| if the display has overscan insets and/or rotation.
  gfx::Size size_in_pixel_;
  gfx::Insets overscan_insets_in_dip_;

  // UI scale of the display.
  float ui_scale_;

  // True if this comes from native platform (DisplayChangeObserverX11).
  bool native_;

  // The list of resolutions supported by this display.
  std::vector<Resolution> resolutions_;
};

}  // namespace internal
}  // namespace ash

#endif  //  ASH_DISPLAY_DISPLAY_INFO_H_
