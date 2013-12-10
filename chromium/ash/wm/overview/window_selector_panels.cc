// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_panels.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const int kPanelCalloutFadeInDurationMilliseconds = 50;

// This class extends ScopedTransformOverviewMode to hide and show the callout
// widget for a panel window when entering / leaving overview mode.
class ScopedTransformPanelWindow : public ScopedTransformOverviewWindow {
 public:
  ScopedTransformPanelWindow(aura::Window* window);
  virtual ~ScopedTransformPanelWindow();

 protected:
  virtual void OnOverviewStarted() OVERRIDE;

 private:
  // Returns the callout widget for the transformed panel.
  views::Widget* GetCalloutWidget();

  // Restores the callout visibility.
  void RestoreCallout();

  // Trigger relayout
  void Relayout();

  bool callout_visible_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformPanelWindow);
};

ScopedTransformPanelWindow::ScopedTransformPanelWindow(aura::Window* window)
    : ScopedTransformOverviewWindow(window) {
}

ScopedTransformPanelWindow::~ScopedTransformPanelWindow() {
  // window() will be NULL if the window was destroyed.
  if (window())
    RestoreCallout();
}

void ScopedTransformPanelWindow::OnOverviewStarted() {
  ScopedTransformOverviewWindow::OnOverviewStarted();
  GetCalloutWidget()->GetLayer()->SetOpacity(0.0f);
}

views::Widget* ScopedTransformPanelWindow::GetCalloutWidget() {
  DCHECK(window()->parent()->id() == internal::kShellWindowId_PanelContainer);
  internal::PanelLayoutManager* panel_layout_manager =
      static_cast<internal::PanelLayoutManager*>(
          window()->parent()->layout_manager());
  return panel_layout_manager->GetCalloutWidgetForPanel(window());
}

void ScopedTransformPanelWindow::RestoreCallout() {
  scoped_ptr<ui::LayerAnimationSequence> sequence(
      new ui::LayerAnimationSequence);
  ui::LayerAnimationElement::AnimatableProperties paused_properties;
  paused_properties.insert(ui::LayerAnimationElement::OPACITY);
  sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      paused_properties, base::TimeDelta::FromMilliseconds(
          ScopedTransformOverviewWindow::kTransitionMilliseconds)));
  sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(1,
      base::TimeDelta::FromMilliseconds(
          kPanelCalloutFadeInDurationMilliseconds)));
  GetCalloutWidget()->GetLayer()->GetAnimator()->StartAnimation(
      sequence.release());
}

}  // namespace

WindowSelectorPanels::WindowSelectorPanels() {
}

WindowSelectorPanels::~WindowSelectorPanels() {
}

void WindowSelectorPanels::AddWindow(aura::Window* window) {
  transform_windows_.push_back(new ScopedTransformPanelWindow(window));
}

aura::RootWindow* WindowSelectorPanels::GetRootWindow() {
  return transform_windows_.front()->window()->GetRootWindow();
}

aura::Window* WindowSelectorPanels::TargetedWindow(const aura::Window* target) {
  for (WindowList::const_iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->Contains(target))
      return (*iter)->window();
  }
  return NULL;
}

void WindowSelectorPanels::RestoreWindowOnExit(aura::Window* window) {
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->Contains(window)) {
      (*iter)->RestoreWindowOnExit();
      break;
    }
  }
}

aura::Window* WindowSelectorPanels::SelectionWindow() {
  return transform_windows_.front()->window();
}

void WindowSelectorPanels::RemoveWindow(const aura::Window* window) {
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->Contains(window)) {
      (*iter)->OnWindowDestroyed();
      transform_windows_.erase(iter);
      break;
    }
  }
}

bool WindowSelectorPanels::empty() const {
  return transform_windows_.empty();
}

void WindowSelectorPanels::SetItemBounds(aura::RootWindow* root_window,
                                         const gfx::Rect& target_bounds) {
  // Panel windows affect the position of each other. Restore all panel windows
  // first in order to have the correct layout.
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    (*iter)->RestoreWindow();
  }
  gfx::Rect bounding_rect;
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    aura::Window* panel = (*iter)->window();
    gfx::Rect bounds = ScreenAsh::ConvertRectToScreen(
        panel->parent(), panel->GetTargetBounds());
    bounding_rect.Union(bounds);
  }
  gfx::Transform bounding_transform =
      ScopedTransformOverviewWindow::GetTransformForRectPreservingAspectRatio(
          bounding_rect, target_bounds);
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    gfx::Transform transform;
    aura::Window* panel = (*iter)->window();
    gfx::Rect bounds = ScreenAsh::ConvertRectToScreen(
        panel->parent(), panel->GetTargetBounds());
    transform.Translate(bounding_rect.x() - bounds.x(),
                        bounding_rect.y() - bounds.y());
    transform.PreconcatTransform(bounding_transform);
    transform.Translate(bounds.x() - bounding_rect.x(),
                        bounds.y() - bounding_rect.y());
    (*iter)->SetTransform(root_window, transform);
  }
}

}  // namespace ash
