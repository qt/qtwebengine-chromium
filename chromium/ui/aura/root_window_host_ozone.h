// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_OZONE_H_
#define UI_AURA_ROOT_WINDOW_HOST_OZONE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/root_window_host.h"
#include "ui/events/ozone/event_factory_ozone.h"
#include "ui/gfx/rect.h"

namespace aura {

class RootWindowHostOzone : public RootWindowHost,
                            public base::MessageLoop::Dispatcher {
 public:
  explicit RootWindowHostOzone(const gfx::Rect& bounds);
  virtual ~RootWindowHostOzone();

 private:
  // Overridden from Dispatcher overrides:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // RootWindowHost Overrides.
  virtual void SetDelegate(RootWindowHostDelegate* delegate) OVERRIDE;
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual void SetInsets(const gfx::Insets& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor_type) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void OnCursorVisibilityChanged(bool show) OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void SetFocusWhenShown(bool focus_when_shown) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  RootWindowHostDelegate* delegate_;
  gfx::AcceleratedWidget widget_;
  gfx::Rect bounds_;

  // EventFactoryOzone creates converters that obtain input events from the
  // underlying input system and dispatch them as |ui::Event| instances into
  // Aura.
  scoped_ptr<ui::EventFactoryOzone> factory_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowHostOzone);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_OZONE_H_
