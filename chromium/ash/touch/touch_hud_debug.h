// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_HUD_DEBUG_H_
#define ASH_TOUCH_TOUCH_HUD_DEBUG_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/touch/touch_observer_hud.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace views {
class Label;
class View;
}

namespace ash {
namespace internal {

class TouchHudCanvas;
class TouchLog;

// A heads-up display to show touch traces on the screen and log touch events.
// As a derivative of TouchObserverHUD, objects of this class manage their own
// lifetime.
class ASH_EXPORT TouchHudDebug : public TouchObserverHUD {
 public:
  enum Mode {
    FULLSCREEN,
    REDUCED_SCALE,
    INVISIBLE,
  };

  explicit TouchHudDebug(aura::Window* initial_root);

  // Returns the log of touch events for all displays as a dictionary mapping id
  // of each display to its touch log.
  static scoped_ptr<DictionaryValue> GetAllAsDictionary();

  // Changes the display mode (e.g. scale, visibility). Calling this repeatedly
  // cycles between a fixed number of display modes.
  void ChangeToNextMode();

  // Returns log of touch events as a list value. Each item in the list is a
  // trace of one touch point.
  scoped_ptr<ListValue> GetLogAsList() const;

  Mode mode() const { return mode_; }

  // Overriden from TouchObserverHUD.
  virtual void Clear() OVERRIDE;

 private:
  virtual ~TouchHudDebug();

  void SetMode(Mode mode);

  void UpdateTouchPointLabel(int index);

  // Overriden from TouchObserverHUD.
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void SetHudForRootWindowController(
      RootWindowController* controller) OVERRIDE;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) OVERRIDE;

  static const int kMaxTouchPoints = 32;

  Mode mode_;

  scoped_ptr<TouchLog> touch_log_;

  TouchHudCanvas* canvas_;
  views::View* label_container_;
  views::Label* touch_labels_[kMaxTouchPoints];

  DISALLOW_COPY_AND_ASSIGN(TouchHudDebug);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_HUD_DEBUG_H_
