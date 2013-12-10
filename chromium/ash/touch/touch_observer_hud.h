// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display_observer.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif  // defined(OS_CHROMEOS)

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// An event filter which handles system level gesture events. Objects of this
// class manage their own lifetime.
class ASH_EXPORT TouchObserverHUD
    : public ui::EventHandler,
      public views::WidgetObserver,
      public gfx::DisplayObserver,
#if defined(OS_CHROMEOS)
      public chromeos::OutputConfigurator::Observer,
#endif  // defined(OS_CHROMEOS)
      public DisplayController::Observer {
 public:
  // Called to clear touch points and traces from the screen. Default
  // implementation does nothing. Sub-classes should implement appropriately.
  virtual void Clear();

  // Removes the HUD from the screen.
  void Remove();

  int64 display_id() const { return display_id_; }

 protected:
  explicit TouchObserverHUD(aura::RootWindow* initial_root);

  virtual ~TouchObserverHUD();

  virtual void SetHudForRootWindowController(
      RootWindowController* controller) = 0;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) = 0;

  views::Widget* widget() { return widget_; }

  // Overriden from ui::EventHandler.
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from gfx::DisplayObserver.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overriden from chromeos::OutputConfigurator::Observer.
  virtual void OnDisplayModeChanged(
      const std::vector<chromeos::OutputConfigurator::OutputSnapshot>& outputs)
      OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  // Overriden form DisplayController::Observer.
  virtual void OnDisplayConfigurationChanging() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  friend class TouchHudTest;

  const int64 display_id_;
  aura::RootWindow* root_window_;

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
