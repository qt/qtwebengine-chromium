// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/accessibility_delegate.h"
#include "ash/caps_lock_delegate_stub.h"
#include "ash/default_accessibility_delegate.h"
#include "ash/default_user_wallpaper_delegate.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/session_state_delegate.h"
#include "ash/session_state_delegate_stub.h"
#include "ash/shell/context_menu.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/keyboard_controller_proxy_stub.h"
#include "ash/shell/shelf_delegate_impl.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "ash/wm/window_state.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/input_method_event_filter.h"

namespace ash {
namespace shell {
namespace {

class NewWindowDelegateImpl : public NewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  virtual ~NewWindowDelegateImpl() {}

  virtual void NewTab() OVERRIDE {}
  virtual void NewWindow(bool incognito) OVERRIDE {
    ash::shell::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
  }
  virtual void OpenFileManager() OVERRIDE {}
  virtual void OpenCrosh() OVERRIDE {}
  virtual void RestoreTab() OVERRIDE {}
  virtual void ShowKeyboardOverlay() OVERRIDE {}
  virtual void ShowTaskManager() OVERRIDE {}
  virtual void OpenFeedbackPage() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() {}
  virtual ~MediaDelegateImpl() {}

  virtual void HandleMediaNextTrack() OVERRIDE {}
  virtual void HandleMediaPlayPause() OVERRIDE {}
  virtual void HandleMediaPrevTrack() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

}  // namespace

ShellDelegateImpl::ShellDelegateImpl()
    : watcher_(NULL),
      shelf_delegate_(NULL),
      browser_context_(NULL) {
}

ShellDelegateImpl::~ShellDelegateImpl() {
}

void ShellDelegateImpl::SetWatcher(WindowWatcher* watcher) {
  watcher_ = watcher;
  if (shelf_delegate_)
    shelf_delegate_->set_watcher(watcher);
}

bool ShellDelegateImpl::IsFirstRunAfterBoot() const {
  return false;
}

bool ShellDelegateImpl::IsIncognitoAllowed() const {
  return true;
}

bool ShellDelegateImpl::IsMultiProfilesEnabled() const {
  return false;
}

bool ShellDelegateImpl::IsRunningInForcedAppMode() const {
  return false;
}

void ShellDelegateImpl::PreInit() {
}

void ShellDelegateImpl::Shutdown() {
}

void ShellDelegateImpl::Exit() {
  base::MessageLoopForUI::current()->Quit();
}

keyboard::KeyboardControllerProxy*
    ShellDelegateImpl::CreateKeyboardControllerProxy() {
  return new KeyboardControllerProxyStub();
}

content::BrowserContext* ShellDelegateImpl::GetActiveBrowserContext() {
  return browser_context_;
}

app_list::AppListViewDelegate* ShellDelegateImpl::CreateAppListViewDelegate() {
  return ash::shell::CreateAppListViewDelegate();
}

ShelfDelegate* ShellDelegateImpl::CreateShelfDelegate(ShelfModel* model) {
  shelf_delegate_ = new ShelfDelegateImpl(watcher_);
  return shelf_delegate_;
}

ash::SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate() {
  return new DefaultSystemTrayDelegate;
}

ash::UserWallpaperDelegate* ShellDelegateImpl::CreateUserWallpaperDelegate() {
  return new DefaultUserWallpaperDelegate();
}

ash::CapsLockDelegate* ShellDelegateImpl::CreateCapsLockDelegate() {
  return new CapsLockDelegateStub;
}

ash::SessionStateDelegate* ShellDelegateImpl::CreateSessionStateDelegate() {
  return new SessionStateDelegateStub;
}

ash::AccessibilityDelegate* ShellDelegateImpl::CreateAccessibilityDelegate() {
  return new internal::DefaultAccessibilityDelegate;
}

ash::NewWindowDelegate* ShellDelegateImpl::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

ash::MediaDelegate* ShellDelegateImpl::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

aura::client::UserActionClient* ShellDelegateImpl::CreateUserActionClient() {
  return NULL;
}

ui::MenuModel* ShellDelegateImpl::CreateContextMenu(aura::Window* root) {
  return new ContextMenu(root);
}

RootWindowHostFactory* ShellDelegateImpl::CreateRootWindowHostFactory() {
  return RootWindowHostFactory::Create();
}

base::string16 ShellDelegateImpl::GetProductName() const {
  return base::string16();
}

}  // namespace shell
}  // namespace ash
