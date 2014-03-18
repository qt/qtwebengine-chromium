// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/desktop_cleaner.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {
namespace internal {

namespace {

const int kContainerIdsToHide[] = {
  kShellWindowId_DefaultContainer,
  kShellWindowId_AlwaysOnTopContainer,
  kShellWindowId_PanelContainer,
  // TODO(dzhioev): uncomment this when issue with BrowserView::CanActivate
  // will be fixed.
  // kShellWindowId_SystemModalContainer
};

}  // namespace

class ContainerHider : public aura::WindowObserver,
                       public ui::ImplicitAnimationObserver {
 public:
  explicit ContainerHider(aura::Window* container)
      : container_was_hidden_(!container->IsVisible()),
        container_(container) {
    if (container_was_hidden_)
      return;
    ui::Layer* layer = container_->layer();
    ui::ScopedLayerAnimationSettings animation_settings(layer->GetAnimator());
    animation_settings.AddObserver(this);
    layer->SetOpacity(0.0);
  }

  virtual ~ContainerHider() {
    if (container_was_hidden_ || !container_)
      return;
    if (!WasAnimationCompletedForProperty(ui::LayerAnimationElement::OPACITY)) {
      // We are in the middle of animation.
      StopObservingImplicitAnimations();
    } else {
      container_->Show();
    }
    ui::Layer* layer = container_->layer();
    ui::ScopedLayerAnimationSettings animation_settings(layer->GetAnimator());
    layer->SetOpacity(1.0);
  }

 private:
  // Overriden from ui::ImplicitAnimationObserver.
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    container_->Hide();
  }

  // Overriden from aura::WindowObserver.
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK(window == container_);
    container_ = NULL;
  }

  const bool container_was_hidden_;
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(ContainerHider);
};

class NotificationBlocker : public message_center::NotificationBlocker {
 public:
  NotificationBlocker()
      : message_center::NotificationBlocker(
            message_center::MessageCenter::Get()) {
    NotifyBlockingStateChanged();
  }

  virtual ~NotificationBlocker() {};

 private:
  // Overriden from message_center::NotificationBlocker.
  virtual bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) const OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(NotificationBlocker);
};

DesktopCleaner::DesktopCleaner() {
  // TODO(dzhioev): Add support for secondary displays.
  aura::Window* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  for (size_t i = 0; i < arraysize(kContainerIdsToHide); ++i) {
    aura::Window* container =
        Shell::GetContainer(root_window, kContainerIdsToHide[i]);
    container_hiders_.push_back(make_linked_ptr(new ContainerHider(container)));
  }
  notification_blocker_.reset(new NotificationBlocker());
}

DesktopCleaner::~DesktopCleaner() {}

// static
std::vector<int> DesktopCleaner::GetContainersToHideForTest() {
  return std::vector<int>(kContainerIdsToHide,
                          kContainerIdsToHide + arraysize(kContainerIdsToHide));
}

}  // namespace internal
}  // namespace ash

