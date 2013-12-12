// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_
#define UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Returns true only if Chrome should use an inverted color scheme - which is
// only true if the system has high-contrast mode enabled and and is using a
// light-on-dark color scheme. To be notified when this status changes, use
// ScopedSysColorChangeListener, below.
UI_EXPORT bool IsInvertedColorScheme();

// Interface for classes that want to listen to system color changes.
class UI_EXPORT SysColorChangeListener {
 public:
  virtual void OnSysColorChange() = 0;

 protected:
  virtual ~SysColorChangeListener() {}
};

// Create an instance of this class in any object that wants to listen
// for system color changes.
class UI_EXPORT ScopedSysColorChangeListener {
 public:
  explicit ScopedSysColorChangeListener(SysColorChangeListener* listener);
  ~ScopedSysColorChangeListener();

 private:
  SysColorChangeListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSysColorChangeListener);
};

}  // namespace gfx;

#endif  // UI_GFX_SYS_COLOR_CHANGE_LISTENER_H_
