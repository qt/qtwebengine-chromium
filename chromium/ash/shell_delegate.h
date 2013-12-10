// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/strings/string16.h"

namespace app_list {
class AppListViewDelegate;
}

namespace aura {
class RootWindow;
class Window;
namespace client {
class UserActionClient;
}
}

namespace ui {
class MenuModel;
}

namespace views {
class Widget;
}

namespace keyboard {
class KeyboardControllerProxy;
}

namespace ash {

class CapsLockDelegate;
class LauncherDelegate;
class LauncherModel;
struct LauncherItem;
class RootWindowHostFactory;
class SessionStateDelegate;
class SystemTrayDelegate;
class UserWallpaperDelegate;

enum UserMetricsAction {
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6,
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7,
  UMA_ACCEL_LOCK_SCREEN_L,
  UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON,
  UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON,
  UMA_ACCEL_FULLSCREEN_F4,
  UMA_ACCEL_MAXIMIZE_RESTORE_F4,
  UMA_ACCEL_NEWTAB_T,
  UMA_ACCEL_NEXTWINDOW_F5,
  UMA_ACCEL_NEXTWINDOW_TAB,
  UMA_ACCEL_OVERVIEW_F5,
  UMA_ACCEL_PREVWINDOW_F5,
  UMA_ACCEL_PREVWINDOW_TAB,
  UMA_ACCEL_EXIT_FIRST_Q,
  UMA_ACCEL_EXIT_SECOND_Q,
  UMA_ACCEL_SEARCH_LWIN,
  UMA_ACCEL_SHUT_DOWN_POWER_BUTTON,
  UMA_CLOSE_THROUGH_CONTEXT_MENU,
  UMA_LAUNCHER_CLICK_ON_APP,
  UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON,
  UMA_MINIMIZE_PER_KEY,
  UMA_MOUSE_DOWN,
  UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK,
  UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE,
  UMA_TOUCHSCREEN_TAP_DOWN,
  UMA_TRAY_HELP,
  UMA_TRAY_LOCK_SCREEN,
  UMA_TRAY_SHUT_DOWN,
  UMA_WINDOW_APP_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT,
  UMA_WINDOW_MAXIMIZE_BUTTON_MINIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_RESTORE,
  UMA_WINDOW_MAXIMIZE_BUTTON_SHOW_BUBBLE,

  // Thumbnail sized overview of windows triggered. This is a subset of
  // UMA_WINDOW_SELECTION triggered by lingering during alt+tab cycles or
  // pressing the overview key.
  UMA_WINDOW_OVERVIEW,

  // Window selection started by beginning an alt+tab cycle or pressing the
  // overview key. This does not count each step through an alt+tab cycle.
  UMA_WINDOW_SELECTION,
};

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if this is the first time that the shell has been run after
  // the system has booted.  false is returned after the shell has been
  // restarted, typically due to logging in as a guest or logging out.
  virtual bool IsFirstRunAfterBoot() const = 0;

  // Returns true if multi-profiles feature is enabled.
  virtual bool IsMultiProfilesEnabled() const = 0;

  // Returns true if we're running in forced app mode.
  virtual bool IsRunningInForcedAppMode() const = 0;

  // Called before processing |Shell::Init()| so that the delegate
  // can perform tasks necessary before the shell is initialized.
  virtual void PreInit() = 0;

  // Shuts down the environment.
  virtual void Shutdown() = 0;

  // Invoked when the user uses Ctrl-Shift-Q to close chrome.
  virtual void Exit() = 0;

  // Invoked when the user uses Ctrl+T to open a new tab.
  virtual void NewTab() = 0;

  // Invoked when the user uses Ctrl-N or Ctrl-Shift-N to open a new window.
  virtual void NewWindow(bool incognito) = 0;

  // Invoked when the user uses Shift+F4 to toggle the window fullscreen state.
  virtual void ToggleFullscreen() = 0;

  // Invoked when the user uses F4 to toggle window maximized state.
  virtual void ToggleMaximized() = 0;

  // Invoked when an accelerator is used to open the file manager.
  virtual void OpenFileManager(bool as_dialog) = 0;

  // Invoked when the user opens Crosh.
  virtual void OpenCrosh() = 0;

  // Invoked when the user uses Shift+Ctrl+T to restore the closed tab.
  virtual void RestoreTab() = 0;

  // Shows the keyboard shortcut overlay.
  virtual void ShowKeyboardOverlay() = 0;

  // Create a shell-specific keyboard::KeyboardControllerProxy
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() = 0;

  // Shows the task manager window.
  virtual void ShowTaskManager() = 0;

  // Get the current browser context. This will get us the current profile.
  virtual content::BrowserContext* GetCurrentBrowserContext() = 0;

  // Invoked to toggle spoken feedback for accessibility
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) = 0;

  // Returns true if spoken feedback is enabled.
  virtual bool IsSpokenFeedbackEnabled() const = 0;

  // Invoked to toggle high contrast for accessibility.
  virtual void ToggleHighContrast() = 0;

  // Returns true if high contrast mode is enabled.
  virtual bool IsHighContrastEnabled() const = 0;

  // Invoked to enable the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Invoked to change the type of the screen magnifier.
  virtual void SetMagnifierType(MagnifierType type) = 0;

  // Returns if the screen magnifier is enabled or not.
  virtual bool IsMagnifierEnabled() const = 0;

  // Returns the current screen magnifier mode.
  virtual MagnifierType GetMagnifierType() const = 0;

  // Invoked to enable Large Cursor.
  virtual void SetLargeCursorEnabled(bool enabled) = 0;

  // Returns if Large Cursor is enabled or not.
  virtual bool IsLargeCursorEnabled() const = 0;

  // Returns true if the user want to show accesibility menu even when all the
  // accessibility features are disabled.
  virtual bool ShouldAlwaysShowAccessibilityMenu() const = 0;

  // Cancel all current and queued speech immediately.
  virtual void SilenceSpokenFeedback() const = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Creates a new LauncherDelegate. Shell takes ownership of the returned
  // value.
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) = 0;

  // Creates a system-tray delegate. Shell takes ownership of the delegate.
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() = 0;

  // Creates a user wallpaper delegate. Shell takes ownership of the delegate.
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() = 0;

  // Creates a caps lock delegate. Shell takes ownership of the delegate.
  virtual CapsLockDelegate* CreateCapsLockDelegate() = 0;

  // Creates a session state delegate. Shell takes ownership of the delegate.
  virtual SessionStateDelegate* CreateSessionStateDelegate() = 0;

  // Creates a user action client. Shell takes ownership of the object.
  virtual aura::client::UserActionClient* CreateUserActionClient() = 0;

  // Opens the feedback page for "Report Issue".
  virtual void OpenFeedbackPage() = 0;

  // Records that the user performed an action.
  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;

  // Handles the Next Track Media shortcut key.
  virtual void HandleMediaNextTrack() = 0;

  // Handles the Play/Pause Toggle Media shortcut key.
  virtual void HandleMediaPlayPause() = 0;

  // Handles the Previous Track Media shortcut key.
  virtual void HandleMediaPrevTrack() = 0;

  // Saves the zoom scale of the full screen magnifier.
  virtual void SaveScreenMagnifierScale(double scale) = 0;

  // Gets a saved value of the zoom scale of full screen magnifier. If a value
  // is not saved, return a negative value.
  virtual double GetSavedScreenMagnifierScale() = 0;

  // Creates a menu model of the context for the |root_window|.
  virtual ui::MenuModel* CreateContextMenu(aura::RootWindow* root_window) = 0;

  // Creates a root window host factory. Shell takes ownership of the returned
  // value.
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() = 0;

  // Get the product name.
  virtual base::string16 GetProductName() const = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
