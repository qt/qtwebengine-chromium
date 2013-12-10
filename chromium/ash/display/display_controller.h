// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONTROLLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_layout.h"
#include "ash/display/display_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/root_window_observer.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/point.h"

namespace aura {
class Display;
class RootWindow;
}

namespace base {
class Value;
template <typename T> class JSONValueConverter;
}

namespace gfx {
class Display;
class Insets;
}

namespace ash {
namespace internal {
class DisplayInfo;
class DisplayManager;
class FocusActivationStore;
class MirrorWindowController;
class RootWindowController;
}

// DisplayController owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
class ASH_EXPORT DisplayController : public gfx::DisplayObserver,
                                     public aura::RootWindowObserver,
                                     public internal::DisplayManager::Delegate {
 public:
  class ASH_EXPORT Observer {
   public:
    // Invoked when the display configuration change is requested,
    // but before the change is applied to aura/ash.
    virtual void OnDisplayConfigurationChanging() {}

    // Invoked when the all display configuration changes
    // have been applied.
    virtual void OnDisplayConfigurationChanged() {};

   protected:
    virtual ~Observer() {}
  };

  DisplayController();
  virtual ~DisplayController();

  void Start();
  void Shutdown();

  // Returns primary display. This is safe to use after ash::Shell is
  // deleted.
  static const gfx::Display& GetPrimaryDisplay();

  // Returns the number of display. This is safe to use after
  // ash::Shell is deleted.
  static int GetNumDisplays();

  internal::MirrorWindowController* mirror_window_controller() {
    return mirror_window_controller_.get();
  }

  // Initializes primary display.
  void InitPrimaryDisplay();

  // Initialize secondary displays.
  void InitSecondaryDisplays();

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the root window for primary display.
  aura::RootWindow* GetPrimaryRootWindow();

  // Returns the root window for |display_id|.
  aura::RootWindow* GetRootWindowForDisplayId(int64 id);

  // Toggle mirror mode.
  void ToggleMirrorMode();

  // Swap primary and secondary display.
  void SwapPrimaryDisplay();

  // Sets the ID of the primary display.  If the display is not connected, it
  // will switch the primary display when connected.
  void SetPrimaryDisplayId(int64 id);

  // Sets primary display. This re-assigns the current root
  // window to given |display|.
  void SetPrimaryDisplay(const gfx::Display& display);

  // Closes all child windows in the all root windows.
  void CloseChildWindows();

  // Returns all root windows. In non extended desktop mode, this
  // returns the primary root window only.
  std::vector<aura::RootWindow*> GetAllRootWindows();

  // Returns all oot window controllers. In non extended desktop
  // mode, this return a RootWindowController for the primary root window only.
  std::vector<internal::RootWindowController*> GetAllRootWindowControllers();

  // Gets/Sets/Clears the overscan insets for the specified |display_id|. See
  // display_manager.h for the details.
  gfx::Insets GetOverscanInsets(int64 display_id) const;
  void SetOverscanInsets(int64 display_id, const gfx::Insets& insets_in_dip);

  // Sets the layout for the current display pair. The |layout| specifies
  // the locaion of the secondary display relative to the primary.
  void SetLayoutForCurrentDisplays(const DisplayLayout& layout);

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void EnsurePointerInDisplays();

  // Sets the work area's |insets| to the display assigned to |window|.
  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  // Returns the display object nearest given |point|.
  const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const;

  // Returns the display object nearest given |window|.
  const gfx::Display& GetDisplayNearestWindow(
      const aura::Window* window) const;

  // Returns the display that most closely intersects |match_rect|.
  const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect)const;

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

  // RootWindowObserver overrides:
  virtual void OnRootWindowHostResized(const aura::RootWindow* root) OVERRIDE;

  // aura::DisplayManager::Delegate overrides:
  virtual void CreateOrUpdateMirrorWindow(
      const internal::DisplayInfo& info) OVERRIDE;
  virtual void CloseMirrorWindow() OVERRIDE;
  virtual void PreDisplayConfigurationChange(bool dispay_removed) OVERRIDE;
  virtual void PostDisplayConfigurationChange() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayControllerTest, BoundsUpdated);
  FRIEND_TEST_ALL_PREFIXES(DisplayControllerTest, SecondaryDisplayLayout);
  friend class internal::DisplayManager;
  friend class internal::MirrorWindowController;

  // Creates a root window for |display| and stores it in the |root_windows_|
  // map.
  aura::RootWindow* AddRootWindowForDisplay(const gfx::Display& display);

  void UpdateDisplayBoundsForLayout();

  void SetLayoutForDisplayIdPair(const DisplayIdPair& display_pair,
                                 const DisplayLayout& layout);

  void OnFadeOutForSwapDisplayFinished();

  void UpdateHostWindowNames();

  class DisplayChangeLimiter {
   public:
    DisplayChangeLimiter();

    // Sets how long the throttling should last.
    void SetThrottleTimeout(int64 throttle_ms);

    bool IsThrottled() const;

   private:
    // The time when the throttling ends.
    base::Time throttle_timeout_;

    DISALLOW_COPY_AND_ASSIGN(DisplayChangeLimiter);
  };

  // The limiter to throttle how fast a user can
  // change the display configuration.
  scoped_ptr<DisplayChangeLimiter> limiter_;

  // The mapping from display ID to its root window.
  std::map<int64, aura::RootWindow*> root_windows_;

  ObserverList<Observer> observers_;

  // Store the primary root window temporarily while replacing
  // display.
  aura::RootWindow* primary_root_window_for_replace_;

  scoped_ptr<internal::FocusActivationStore> focus_activation_store_;


  scoped_ptr<internal::MirrorWindowController> mirror_window_controller_;

  // Stores the curent cursor location (in native coordinates) used to
  // restore the cursor location when display configuration
  // changed.
  gfx::Point cursor_location_in_native_coords_for_restore_;

  DISALLOW_COPY_AND_ASSIGN(DisplayController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONTROLLER_H_
