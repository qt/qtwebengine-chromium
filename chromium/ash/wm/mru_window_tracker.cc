// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include <algorithm>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ash {

namespace {

// Adds the windows that can be cycled through for the specified window id to
// |windows|.
void AddTrackedWindows(aura::Window* root,
                       int container_id,
                       MruWindowTracker::WindowList* windows) {
  aura::Window* container = Shell::GetContainer(root, container_id);
  const MruWindowTracker::WindowList& children(container->children());
  windows->insert(windows->end(), children.begin(), children.end());
}

// Returns true if |window| is a container whose windows can be cycled to.
bool IsSwitchableContainer(aura::Window* window) {
  if (!window)
    return false;
  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (window->id() == kSwitchableWindowContainerIds[i])
      return true;
  }
  return false;
}

// Returns whether |w1| should be considered less recently used than |w2|. This
// is used for a stable sort to move minimized windows to the LRU end of the
// list.
bool CompareWindowState(aura::Window* w1, aura::Window* w2) {
  return ash::wm::IsWindowMinimized(w1) && !ash::wm::IsWindowMinimized(w2);
}

// Returns a list of windows ordered by their stacking order.
// If |mru_windows| is passed, these windows are moved to the front of the list.
// If |top_most_at_end|, the list is returned in descending (bottom-most / least
// recently used) order.
MruWindowTracker::WindowList BuildWindowListInternal(
    const std::list<aura::Window*>* mru_windows,
    bool top_most_at_end) {
  MruWindowTracker::WindowList windows;
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Window* active_root = Shell::GetTargetRootWindow();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    if (*iter == active_root)
      continue;
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i)
      AddTrackedWindows(*iter, kSwitchableWindowContainerIds[i], &windows);
  }

  // Add windows in the active root windows last so that the topmost window
  // in the active root window becomes the front of the list.
  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i)
    AddTrackedWindows(active_root, kSwitchableWindowContainerIds[i], &windows);

  // Removes unfocusable windows.
  MruWindowTracker::WindowList::iterator last =
      std::remove_if(
          windows.begin(),
          windows.end(),
          std::not1(std::ptr_fun(ash::wm::CanActivateWindow)));
  windows.erase(last, windows.end());

  // Put the windows in the mru_windows list at the head, if it's available.
  if (mru_windows) {
    // Iterate through the list backwards, so that we can move each window to
    // the front of the windows list as we find them.
    for (std::list<aura::Window*>::const_reverse_iterator ix =
         mru_windows->rbegin();
         ix != mru_windows->rend(); ++ix) {
      // Exclude windows in non-switchable containers and those which cannot
      // be activated.
      if (!IsSwitchableContainer((*ix)->parent()) ||
          !ash::wm::CanActivateWindow(*ix)) {
        continue;
      }

      MruWindowTracker::WindowList::iterator window =
          std::find(windows.begin(), windows.end(), *ix);
      if (window != windows.end()) {
        windows.erase(window);
        windows.push_back(*ix);
      }
    }
  }

  // Move minimized windows to the beginning (LRU end) of the list.
  std::stable_sort(windows.begin(), windows.end(), CompareWindowState);

  // Window cycling expects the topmost window at the front of the list.
  if (!top_most_at_end)
    std::reverse(windows.begin(), windows.end());

  return windows;
}

}  // namespace

const int kSwitchableWindowContainerIds[] = {
  internal::kShellWindowId_DefaultContainer,
  internal::kShellWindowId_AlwaysOnTopContainer,
  internal::kShellWindowId_PanelContainer
};

const size_t kSwitchableWindowContainerIdsLength =
    arraysize(kSwitchableWindowContainerIds);

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, public:

MruWindowTracker::MruWindowTracker(
    aura::client::ActivationClient* activation_client)
    : activation_client_(activation_client),
      ignore_window_activations_(false) {
  activation_client_->AddObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  for (std::list<aura::Window*>::iterator iter = mru_windows_.begin();
       iter != mru_windows_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
  }

  activation_client_->RemoveObserver(this);
}

// static
MruWindowTracker::WindowList MruWindowTracker::BuildWindowList(
    bool top_most_at_end) {
  return BuildWindowListInternal(NULL, top_most_at_end);
}

MruWindowTracker::WindowList MruWindowTracker::BuildMruWindowList() {
  return BuildWindowListInternal(&mru_windows_, false);
}

void MruWindowTracker::SetIgnoreActivations(bool ignore) {
  ignore_window_activations_ = ignore;

  // If no longer ignoring window activations, move currently active window
  // to front.
  if (!ignore)
    SetActiveWindow(wm::GetActiveWindow());
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, private:

void MruWindowTracker::SetActiveWindow(aura::Window* active_window) {
  if (!active_window)
    return;

  std::list<aura::Window*>::iterator iter =
      std::find(mru_windows_.begin(), mru_windows_.end(), active_window);
  // Observe all newly tracked windows.
  if (iter == mru_windows_.end())
    active_window->AddObserver(this);
  else
    mru_windows_.erase(iter);
  // TODO(flackr): Remove this check if this doesn't fire for a while. This
  // should verify that all tracked windows start with a layer, see
  // http://crbug.com/291354.
  CHECK(active_window->layer());
  mru_windows_.push_front(active_window);
}

void MruWindowTracker::OnWindowActivated(aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!ignore_window_activations_)
    SetActiveWindow(gained_active);
}

void MruWindowTracker::OnWindowDestroying(aura::Window* window) {
  mru_windows_.remove(window);
  window->RemoveObserver(this);
}

}  // namespace ash
