// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/display.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/switches.h"

namespace gfx {
namespace {

bool HasForceDeviceScaleFactorImpl() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceDeviceScaleFactor);
}

float GetForcedDeviceScaleFactorImpl() {
  double scale_in_double = 1.0;
  if (HasForceDeviceScaleFactorImpl()) {
    std::string value = CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kForceDeviceScaleFactor);
    if (!base::StringToDouble(value, &scale_in_double))
      LOG(ERROR) << "Failed to parse the default device scale factor:" << value;
  }
  return static_cast<float>(scale_in_double);
}

const int64 kInvalidDisplayIDForCompileTimeInit = -1;
int64 internal_display_id_ = kInvalidDisplayIDForCompileTimeInit;

}  // namespace

const int64 Display::kInvalidDisplayID = kInvalidDisplayIDForCompileTimeInit;

// static
float Display::GetForcedDeviceScaleFactor() {
  static const float kForcedDeviceScaleFactor =
      GetForcedDeviceScaleFactorImpl();
  return kForcedDeviceScaleFactor;
}

//static
bool Display::HasForceDeviceScaleFactor() {
  return HasForceDeviceScaleFactorImpl();
}

Display::Display()
    : id_(kInvalidDisplayID),
      device_scale_factor_(GetForcedDeviceScaleFactor()),
      rotation_(ROTATE_0),
      touch_support_(TOUCH_SUPPORT_UNKNOWN) {
}

Display::Display(int64 id)
    : id_(id),
      device_scale_factor_(GetForcedDeviceScaleFactor()),
      rotation_(ROTATE_0),
      touch_support_(TOUCH_SUPPORT_UNKNOWN) {
}

Display::Display(int64 id, const gfx::Rect& bounds)
    : id_(id),
      bounds_(bounds),
      work_area_(bounds),
      device_scale_factor_(GetForcedDeviceScaleFactor()),
      rotation_(ROTATE_0),
      touch_support_(TOUCH_SUPPORT_UNKNOWN) {
#if defined(USE_AURA)
  SetScaleAndBounds(device_scale_factor_, bounds);
#endif
}

Display::~Display() {
}

Insets Display::GetWorkAreaInsets() const {
  return gfx::Insets(work_area_.y() - bounds_.y(),
                     work_area_.x() - bounds_.x(),
                     bounds_.bottom() - work_area_.bottom(),
                     bounds_.right() - work_area_.right());
}

void Display::SetScaleAndBounds(
    float device_scale_factor,
    const gfx::Rect& bounds_in_pixel) {
  Insets insets = bounds_.InsetsFrom(work_area_);
  if (!HasForceDeviceScaleFactor()) {
#if defined(OS_MACOSX)
    // Unless an explicit scale factor was provided for testing, ensure the
    // scale is integral.
    device_scale_factor = static_cast<int>(device_scale_factor);
#endif
    device_scale_factor_ = device_scale_factor;
  }
  device_scale_factor_ = std::max(1.0f, device_scale_factor_);
  bounds_ = gfx::Rect(
      gfx::ToFlooredPoint(gfx::ScalePoint(bounds_in_pixel.origin(),
                                          1.0f / device_scale_factor_)),
      gfx::ToFlooredSize(gfx::ScaleSize(bounds_in_pixel.size(),
                                        1.0f / device_scale_factor_)));
  UpdateWorkAreaFromInsets(insets);
}

void Display::SetSize(const gfx::Size& size_in_pixel) {
  gfx::Point origin = bounds_.origin();
#if defined(USE_AURA)
  gfx::PointF origin_f = origin;
  origin_f.Scale(device_scale_factor_);
  origin.SetPoint(origin_f.x(), origin_f.y());
#endif
  SetScaleAndBounds(device_scale_factor_, gfx::Rect(origin, size_in_pixel));
}

void Display::UpdateWorkAreaFromInsets(const gfx::Insets& insets) {
  work_area_ = bounds_;
  work_area_.Inset(insets);
}

gfx::Size Display::GetSizeInPixel() const {
  return gfx::ToFlooredSize(gfx::ScaleSize(size(), device_scale_factor_));
}

std::string Display::ToString() const {
  return base::StringPrintf(
      "Display[%lld] bounds=%s, workarea=%s, scale=%f, %s",
      static_cast<long long int>(id_),
      bounds_.ToString().c_str(),
      work_area_.ToString().c_str(),
      device_scale_factor_,
      IsInternal() ? "internal" : "external");
}

bool Display::IsInternal() const {
  return is_valid() && (id_ == internal_display_id_);
}

int64 Display::InternalDisplayId() {
  return internal_display_id_;
}

void Display::SetInternalDisplayId(int64 internal_display_id) {
  internal_display_id_ = internal_display_id;
}

}  // namespace gfx
