// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OBSERVER_H_
#define UI_AURA_WINDOW_OBSERVER_H_

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace aura {

class Window;

class AURA_EXPORT WindowObserver {
 public:
  struct HierarchyChangeParams {
    enum HierarchyChangePhase {
      HIERARCHY_CHANGING,
      HIERARCHY_CHANGED
    };

    Window* target;     // The window that was added or removed.
    Window* new_parent;
    Window* old_parent;
    HierarchyChangePhase phase;
    Window* receiver;   // The window receiving the notification.
  };

  // Called when a window is added or removed. Notifications are sent to the
  // following hierarchies in this order:
  // 1. |target|.
  // 2. |target|'s child hierarchy.
  // 3. |target|'s parent hierarchy in its |old_parent|
  //        (only for Changing notifications).
  // 3. |target|'s parent hierarchy in its |new_parent|.
  //        (only for Changed notifications).
  // This sequence is performed via the Changing and Changed notifications below
  // before and after the change is committed.
  virtual void OnWindowHierarchyChanging(const HierarchyChangeParams& params) {}
  virtual void OnWindowHierarchyChanged(const HierarchyChangeParams& params) {}

  // Invoked when |new_window| has been added as a child of this window.
  virtual void OnWindowAdded(Window* new_window) {}

  // Invoked prior to removing |window| as a child of this window.
  virtual void OnWillRemoveWindow(Window* window) {}

  // Invoked when this window's parent window changes.  |parent| may be NULL.
  virtual void OnWindowParentChanged(Window* window, Window* parent) {}

  // Invoked when SetProperty(), ClearProperty(), or
  // NativeWidgetAura::SetNativeWindowProperty() is called on the window.
  // |key| is either a WindowProperty<T>* (SetProperty, ClearProperty)
  // or a const char* (SetNativeWindowProperty). Either way, it can simply be
  // compared for equality with the property constant. |old| is the old property
  // value, which must be cast to the appropriate type before use.
  virtual void OnWindowPropertyChanged(Window* window,
                                       const void* key,
                                       intptr_t old) {}

  // Invoked when SetVisible() is invoked on a window. |visible| is the
  // value supplied to SetVisible(). If |visible| is true, window->IsVisible()
  // may still return false. See description in Window::IsVisible() for details.
  virtual void OnWindowVisibilityChanging(Window* window, bool visible) {}
  virtual void OnWindowVisibilityChanged(Window* window, bool visible) {}

  // Invoked when SetBounds() is invoked on |window|. |old_bounds| and
  // |new_bounds| are in parent coordinates.
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {}

  // Invoked when |window|'s position among its siblings in the stacking order
  // has changed.
  virtual void OnWindowStackingChanged(Window* window) {}

  // Invoked when a region of |window| is scheduled to be redrawn.
  virtual void OnWindowPaintScheduled(Window* window,
                                      const gfx::Rect& region) {}

  // Invoked when the Window is being destroyed (i.e. from the start of its
  // destructor). This is called before the window is removed from its parent.
  virtual void OnWindowDestroying(Window* window) {}

  // Invoked when the Window has been destroyed (i.e. at the end of
  // its destructor). This is called after the window is removed from
  // its parent.  Window automatically removes its WindowObservers
  // before calling this method, so the following code is no op.
  //
  // void MyWindowObserver::OnWindowDestroyed(aura::Window* window) {
  //    window->RemoveObserver(this);
  // }
  virtual void OnWindowDestroyed(Window* window) {}

  // Called when a Window has been added to a RootWindow.
  virtual void OnWindowAddedToRootWindow(Window* window) {}

  // Called when a Window is about to be removed from a RootWindow.
  virtual void OnWindowRemovingFromRootWindow(Window* window) {}

  // Called when a transient child is added to |window|.
  virtual void OnAddTransientChild(Window* window, Window* transient) {}

  // Called when a transient child is removed from |window|.
  virtual void OnRemoveTransientChild(Window* window, Window* transient) {}

 protected:
  virtual ~WindowObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OBSERVER_H_
