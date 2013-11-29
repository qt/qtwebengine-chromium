// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/dock/docked_window_resizer.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/panels/panel_window_resizer.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace ash {

scoped_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    aura::client::WindowMoveSource source) {
  DCHECK(window);
  // No need to return a resizer when the window cannot get resized.
  if (!wm::CanResizeWindow(window) && window_component != HTCAPTION)
    return scoped_ptr<WindowResizer>();

  // TODO(varkha): The chaining of window resizers causes some of the logic
  // to be repeated and the logic flow difficult to control. With some windows
  // classes using reparenting during drag operations it becomes challenging to
  // implement proper transition from one resizer to another during or at the
  // end of the drag. This also causes http://crbug.com/247085.
  // It seems the only thing the panel or dock resizer needs to do is notify the
  // layout manager when a docked window is being dragged. We should have a
  // better way of doing this, perhaps by having a way of observing drags or
  // having a generic drag window wrapper which informs a layout manager that a
  // drag has started or stopped.
  // It may be possible to refactor and eliminate chaining.
  WindowResizer* window_resizer = NULL;
  if (window->parent() &&
      (window->parent()->id() == internal::kShellWindowId_DefaultContainer ||
       window->parent()->id() == internal::kShellWindowId_DockedContainer ||
       window->parent()->id() == internal::kShellWindowId_PanelContainer)) {
    // Allow dragging maximized windows if it's not tracked by workspace. This
    // is set by tab dragging code.
    if (!wm::IsWindowNormal(window) &&
        (window_component != HTCAPTION || GetTrackedByWorkspace(window)))
      return scoped_ptr<WindowResizer>();
    window_resizer = internal::WorkspaceWindowResizer::Create(
        window,
        point_in_parent,
        window_component,
        source,
        std::vector<aura::Window*>());
  } else if (wm::IsWindowNormal(window)) {
    window_resizer = DefaultWindowResizer::Create(
        window, point_in_parent, window_component, source);
  }
  if (window_resizer) {
    window_resizer = internal::DragWindowResizer::Create(
        window_resizer, window, point_in_parent, window_component, source);
  }
  if (window_resizer && window->type() == aura::client::WINDOW_TYPE_PANEL) {
    window_resizer = PanelWindowResizer::Create(
        window_resizer, window, point_in_parent, window_component, source);
  }
  if (window_resizer) {
    window_resizer = DockedWindowResizer::Create(
        window_resizer, window, point_in_parent, window_component, source);
  }
  return make_scoped_ptr<WindowResizer>(window_resizer);
}

namespace internal {

namespace {

// Distance in pixels that the cursor must move past an edge for a window
// to move or resize beyond that edge.
const int kStickyDistancePixels = 64;

// Snapping distance used instead of WorkspaceWindowResizer::kScreenEdgeInset
// when resizing a window using touchscreen.
const int kScreenEdgeInsetForTouchResize = 32;

// Returns true if the window should stick to the edge.
bool ShouldStickToEdge(int distance_from_edge, int sticky_size) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableStickyEdges) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableDockedWindows)) {
    return distance_from_edge < 0 &&
           distance_from_edge > -sticky_size;
  }
  return distance_from_edge < sticky_size &&
                              distance_from_edge > -sticky_size * 2;
}

// Returns the coordinate along the secondary axis to snap to.
int CoordinateAlongSecondaryAxis(SecondaryMagnetismEdge edge,
                                 int leading,
                                 int trailing,
                                 int none) {
  switch (edge) {
    case SECONDARY_MAGNETISM_EDGE_LEADING:
      return leading;
    case SECONDARY_MAGNETISM_EDGE_TRAILING:
      return trailing;
    case SECONDARY_MAGNETISM_EDGE_NONE:
      return none;
  }
  NOTREACHED();
  return none;
}

// Returns the origin for |src| when magnetically attaching to |attach_to| along
// the edges |edges|. |edges| is a bitmask of the MagnetismEdges.
gfx::Point OriginForMagneticAttach(const gfx::Rect& src,
                                   const gfx::Rect& attach_to,
                                   const MatchedEdge& edge) {
  int x = 0, y = 0;
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
      y = attach_to.bottom();
      break;
    case MAGNETISM_EDGE_LEFT:
      x = attach_to.right();
      break;
    case MAGNETISM_EDGE_BOTTOM:
      y = attach_to.y() - src.height();
      break;
    case MAGNETISM_EDGE_RIGHT:
      x = attach_to.x() - src.width();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      x = CoordinateAlongSecondaryAxis(
          edge.secondary_edge, attach_to.x(), attach_to.right() - src.width(),
          src.x());
      break;
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      y = CoordinateAlongSecondaryAxis(
          edge.secondary_edge, attach_to.y(), attach_to.bottom() - src.height(),
          src.y());
      break;
  }
  return gfx::Point(x, y);
}

// Returns the bounds for a magnetic attach when resizing. |src| is the bounds
// of window being resized, |attach_to| the bounds of the window to attach to
// and |edge| identifies the edge to attach to.
gfx::Rect BoundsForMagneticResizeAttach(const gfx::Rect& src,
                                        const gfx::Rect& attach_to,
                                        const MatchedEdge& edge) {
  int x = src.x();
  int y = src.y();
  int w = src.width();
  int h = src.height();
  gfx::Point attach_origin(OriginForMagneticAttach(src, attach_to, edge));
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
      x = attach_origin.x();
      w = src.right() - x;
      break;
    case MAGNETISM_EDGE_RIGHT:
      w += attach_origin.x() - src.x();
      break;
    case MAGNETISM_EDGE_TOP:
      y = attach_origin.y();
      h = src.bottom() - y;
      break;
    case MAGNETISM_EDGE_BOTTOM:
      h += attach_origin.y() - src.y();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        y = attach_origin.y();
        h = src.bottom() - y;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        h += attach_origin.y() - src.y();
      }
      break;
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        x = attach_origin.x();
        w = src.right() - x;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        w += attach_origin.x() - src.x();
      }
      break;
  }
  return gfx::Rect(x, y, w, h);
}

// Converts a window component edge to the magnetic edge to snap to.
uint32 WindowComponentToMagneticEdge(int window_component) {
  switch (window_component) {
    case HTTOPLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_TOP;
    case HTTOPRIGHT:
      return MAGNETISM_EDGE_TOP | MAGNETISM_EDGE_RIGHT;
    case HTBOTTOMLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_BOTTOM;
    case HTBOTTOMRIGHT:
      return MAGNETISM_EDGE_RIGHT | MAGNETISM_EDGE_BOTTOM;
    case HTTOP:
      return MAGNETISM_EDGE_TOP;
    case HTBOTTOM:
      return MAGNETISM_EDGE_BOTTOM;
    case HTRIGHT:
      return MAGNETISM_EDGE_RIGHT;
    case HTLEFT:
      return MAGNETISM_EDGE_LEFT;
    default:
      break;
  }
  return 0;
}

}  // namespace

// static
const int WorkspaceWindowResizer::kMinOnscreenSize = 20;

// static
const int WorkspaceWindowResizer::kMinOnscreenHeight = 32;

// static
const int WorkspaceWindowResizer::kScreenEdgeInset = 8;

// Represents the width or height of a window with constraints on its minimum
// and maximum size. 0 represents a lack of a constraint.
class WindowSize {
 public:
  WindowSize(int size, int min, int max)
      : size_(size),
        min_(min),
        max_(max) {
    // Grow the min/max bounds to include the starting size.
    if (is_underflowing())
      min_ = size_;
    if (is_overflowing())
      max_ = size_;
  }

  bool is_at_capacity(bool shrinking) {
    return size_ == (shrinking ? min_ : max_);
  }

  int size() const {
    return size_;
  }

  bool has_min() const {
    return min_ != 0;
  }

  bool has_max() const {
    return max_ != 0;
  }

  bool is_valid() const {
    return !is_overflowing() && !is_underflowing();
  }

  bool is_overflowing() const {
    return has_max() && size_ > max_;
  }

  bool is_underflowing() const {
    return has_min() && size_ < min_;
  }

  // Add |amount| to this WindowSize not exceeding min or max size constraints.
  // Returns by how much |size_| + |amount| exceeds the min/max constraints.
  int Add(int amount) {
    DCHECK(is_valid());
    int new_value = size_ + amount;

    if (has_min() && new_value < min_) {
      size_ = min_;
      return new_value - min_;
    }

    if (has_max() && new_value > max_) {
      size_ = max_;
      return new_value - max_;
    }

    size_ = new_value;
    return 0;
  }

 private:
  int size_;
  int min_;
  int max_;
};

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  Shell* shell = Shell::GetInstance();
  shell->cursor_manager()->UnlockCursor();
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    aura::Window* window,
    const gfx::Point& location_in_parent,
    int window_component,
    aura::client::WindowMoveSource source,
    const std::vector<aura::Window*>& attached_windows) {
  Details details(window, location_in_parent, window_component, source);
  return details.is_resizable ?
      new WorkspaceWindowResizer(details, attached_windows) : NULL;
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location_in_parent,
                                  int event_flags) {
  last_mouse_location_ = location_in_parent;

  int sticky_size;
  if (event_flags & ui::EF_CONTROL_DOWN) {
    sticky_size = 0;
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableStickyEdges) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableDockedWindows)) {
    sticky_size = kStickyDistancePixels;
  } else if ((details_.bounds_change & kBoundsChange_Resizes) &&
      details_.source == aura::client::WINDOW_MOVE_SOURCE_TOUCH) {
    sticky_size = kScreenEdgeInsetForTouchResize;
  } else {
    sticky_size = kScreenEdgeInset;
  }
  // |bounds| is in |window()->parent()|'s coordinates.
  gfx::Rect bounds = CalculateBoundsForDrag(details_, location_in_parent);

  if (wm::IsWindowNormal(window()))
    AdjustBoundsForMainWindow(sticky_size, &bounds);

  if (bounds != window()->bounds()) {
    if (!did_move_or_resize_) {
      if (!details_.restore_bounds.IsEmpty())
        ClearRestoreBounds(window());
      RestackWindows();
    }
    did_move_or_resize_ = true;
  }

  gfx::Point location_in_screen = location_in_parent;
  wm::ConvertPointToScreen(window()->parent(), &location_in_screen);
  const bool in_original_root =
      wm::GetRootWindowAt(location_in_screen) == window()->GetRootWindow();
  // Hide a phantom window for snapping if the cursor is in another root window.
  if (in_original_root && wm::CanResizeWindow(window())) {
    UpdateSnapPhantomWindow(location_in_parent, bounds);
  } else {
    snap_type_ = SNAP_NONE;
    snap_phantom_window_controller_.reset();
  }

  if (!attached_windows_.empty())
    LayoutAttachedWindows(&bounds);
  if (bounds != window()->bounds())
    window()->SetBounds(bounds);
}

void WorkspaceWindowResizer::CompleteDrag(int event_flags) {
  wm::SetUserHasChangedWindowPositionOrSize(details_.window, true);
  snap_phantom_window_controller_.reset();
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  // When the window is not in the normal show state, we do not snap the window.
  // This happens when the user minimizes or maximizes the window by keyboard
  // shortcut while dragging it. If the window is the result of dragging a tab
  // out of a maximized window, it's already in the normal show state when this
  // is called, so it does not matter.
  if (wm::IsWindowNormal(window()) &&
      (window()->type() != aura::client::WINDOW_TYPE_PANEL ||
       !window()->GetProperty(kPanelAttachedKey)) &&
      (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE)) {
    if (!GetRestoreBoundsInScreen(window())) {
      gfx::Rect initial_bounds = ScreenAsh::ConvertRectToScreen(
          window()->parent(), details_.initial_bounds_in_parent);
      SetRestoreBoundsInScreen(window(), details_.restore_bounds.IsEmpty() ?
                               initial_bounds :
                               details_.restore_bounds);
    }
    window()->SetBounds(snap_sizer_->target_bounds());
    return;
  }
}

void WorkspaceWindowResizer::RevertDrag() {
  snap_phantom_window_controller_.reset();

  if (!did_move_or_resize_)
    return;

  window()->SetBounds(details_.initial_bounds_in_parent);
  if (!details_.restore_bounds.IsEmpty())
    SetRestoreBoundsInScreen(details_.window, details_.restore_bounds);

  if (details_.window_component == HTRIGHT) {
    int last_x = details_.initial_bounds_in_parent.right();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_x(last_x);
      bounds.set_width(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_x = attached_windows_[i]->bounds().right();
    }
  } else {
    int last_y = details_.initial_bounds_in_parent.bottom();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_y(last_y);
      bounds.set_height(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_y = attached_windows_[i]->bounds().bottom();
    }
  }
}

aura::Window* WorkspaceWindowResizer::GetTarget() {
  return details_.window;
}

const gfx::Point& WorkspaceWindowResizer::GetInitialLocation() const {
  return details_.initial_location_in_parent;
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    const Details& details,
    const std::vector<aura::Window*>& attached_windows)
    : details_(details),
      attached_windows_(attached_windows),
      did_move_or_resize_(false),
      total_min_(0),
      total_initial_size_(0),
      snap_type_(SNAP_NONE),
      num_mouse_moves_since_bounds_change_(0),
      magnetism_window_(NULL) {
  DCHECK(details_.is_resizable);

  Shell* shell = Shell::GetInstance();
  shell->cursor_manager()->LockCursor();

  // Only support attaching to the right/bottom.
  DCHECK(attached_windows_.empty() ||
         (details.window_component == HTRIGHT ||
          details.window_component == HTBOTTOM));

  // TODO: figure out how to deal with window going off the edge.

  // Calculate sizes so that we can maintain the ratios if we need to resize.
  int total_available = 0;
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Size min(attached_windows_[i]->delegate()->GetMinimumSize());
    int initial_size = PrimaryAxisSize(attached_windows_[i]->bounds().size());
    initial_size_.push_back(initial_size);
    // If current size is smaller than the min, use the current size as the min.
    // This way we don't snap on resize.
    int min_size = std::min(initial_size,
                            std::max(PrimaryAxisSize(min), kMinOnscreenSize));
    total_min_ += min_size;
    total_initial_size_ += initial_size;
    total_available += std::max(min_size, initial_size) - min_size;
  }
}

gfx::Rect WorkspaceWindowResizer::GetFinalBounds(
    const gfx::Rect& bounds) const {
  if (snap_phantom_window_controller_.get() &&
      snap_phantom_window_controller_->IsShowing()) {
    return snap_phantom_window_controller_->bounds();
  }
  return bounds;
}

void WorkspaceWindowResizer::LayoutAttachedWindows(
    gfx::Rect* bounds) {
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(window()));
  int initial_size = PrimaryAxisSize(details_.initial_bounds_in_parent.size());
  int current_size = PrimaryAxisSize(bounds->size());
  int start = PrimaryAxisCoordinate(bounds->right(), bounds->bottom());
  int end = PrimaryAxisCoordinate(work_area.right(), work_area.bottom());

  int delta = current_size - initial_size;
  int available_size = end - start;
  std::vector<int> sizes;
  int leftovers = CalculateAttachedSizes(delta, available_size, &sizes);

  // leftovers > 0 means that the attached windows can't grow to compensate for
  // the shrinkage of the main window. This line causes the attached windows to
  // be moved so they are still flush against the main window, rather than the
  // main window being prevented from shrinking.
  leftovers = std::min(0, leftovers);
  // Reallocate any leftover pixels back into the main window. This is
  // necessary when, for example, the main window shrinks, but none of the
  // attached windows can grow without exceeding their max size constraints.
  // Adding the pixels back to the main window effectively prevents the main
  // window from resizing too far.
  if (details_.window_component == HTRIGHT)
    bounds->set_width(bounds->width() + leftovers);
  else
    bounds->set_height(bounds->height() + leftovers);

  DCHECK_EQ(attached_windows_.size(), sizes.size());
  int last = PrimaryAxisCoordinate(bounds->right(), bounds->bottom());
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Rect attached_bounds(attached_windows_[i]->bounds());
    if (details_.window_component == HTRIGHT) {
      attached_bounds.set_x(last);
      attached_bounds.set_width(sizes[i]);
    } else {
      attached_bounds.set_y(last);
      attached_bounds.set_height(sizes[i]);
    }
    attached_windows_[i]->SetBounds(attached_bounds);
    last += sizes[i];
  }
}

int WorkspaceWindowResizer::CalculateAttachedSizes(
    int delta,
    int available_size,
    std::vector<int>* sizes) const {
  std::vector<WindowSize> window_sizes;
  CreateBucketsForAttached(&window_sizes);

  // How much we need to grow the attached by (collectively).
  int grow_attached_by = 0;
  if (delta > 0) {
    // If the attached windows don't fit when at their initial size, we will
    // have to shrink them by how much they overflow.
    if (total_initial_size_ >= available_size)
      grow_attached_by = available_size - total_initial_size_;
  } else {
    // If we're shrinking, we grow the attached so the total size remains
    // constant.
    grow_attached_by = -delta;
  }

  int leftover_pixels = 0;
  while (grow_attached_by != 0) {
    int leftovers = GrowFairly(grow_attached_by, window_sizes);
    if (leftovers == grow_attached_by) {
      leftover_pixels = leftovers;
      break;
    }
    grow_attached_by = leftovers;
  }

  for (size_t i = 0; i < window_sizes.size(); ++i)
    sizes->push_back(window_sizes[i].size());

  return leftover_pixels;
}

int WorkspaceWindowResizer::GrowFairly(
    int pixels,
    std::vector<WindowSize>& sizes) const {
  bool shrinking = pixels < 0;
  std::vector<WindowSize*> nonfull_windows;
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (!sizes[i].is_at_capacity(shrinking))
      nonfull_windows.push_back(&sizes[i]);
  }
  std::vector<float> ratios;
  CalculateGrowthRatios(nonfull_windows, &ratios);

  int remaining_pixels = pixels;
  bool add_leftover_pixels_to_last = true;
  for (size_t i = 0; i < nonfull_windows.size(); ++i) {
    int grow_by = pixels * ratios[i];
    // Put any leftover pixels into the last window.
    if (i == nonfull_windows.size() - 1 && add_leftover_pixels_to_last)
      grow_by = remaining_pixels;
    int remainder = nonfull_windows[i]->Add(grow_by);
    int consumed = grow_by - remainder;
    remaining_pixels -= consumed;
    if (nonfull_windows[i]->is_at_capacity(shrinking) && remainder > 0) {
      // Because this window overflowed, some of the pixels in
      // |remaining_pixels| aren't there due to rounding errors. Rather than
      // unfairly giving all those pixels to the last window, we refrain from
      // allocating them so that this function can be called again to distribute
      // the pixels fairly.
      add_leftover_pixels_to_last = false;
    }
  }
  return remaining_pixels;
}

void WorkspaceWindowResizer::CalculateGrowthRatios(
    const std::vector<WindowSize*>& sizes,
    std::vector<float>* out_ratios) const {
  DCHECK(out_ratios->empty());
  int total_value = 0;
  for (size_t i = 0; i < sizes.size(); ++i)
    total_value += sizes[i]->size();

  for (size_t i = 0; i < sizes.size(); ++i)
    out_ratios->push_back(
        (static_cast<float>(sizes[i]->size())) / total_value);
}

void WorkspaceWindowResizer::CreateBucketsForAttached(
    std::vector<WindowSize>* sizes) const {
  for (size_t i = 0; i < attached_windows_.size(); i++) {
    int initial_size = initial_size_[i];
    aura::WindowDelegate* delegate = attached_windows_[i]->delegate();
    int min = PrimaryAxisSize(delegate->GetMinimumSize());
    int max = PrimaryAxisSize(delegate->GetMaximumSize());

    sizes->push_back(WindowSize(initial_size, min, max));
  }
}

void WorkspaceWindowResizer::MagneticallySnapToOtherWindows(gfx::Rect* bounds) {
  if (UpdateMagnetismWindow(*bounds, kAllMagnetismEdges)) {
    gfx::Point point = OriginForMagneticAttach(
        ScreenAsh::ConvertRectToScreen(window()->parent(), *bounds),
        magnetism_window_->GetBoundsInScreen(),
        magnetism_edge_);
    aura::client::GetScreenPositionClient(window()->GetRootWindow())->
        ConvertPointFromScreen(window()->parent(), &point);
    bounds->set_origin(point);
  }
}

void WorkspaceWindowResizer::MagneticallySnapResizeToOtherWindows(
    gfx::Rect* bounds) {
  const uint32 edges = WindowComponentToMagneticEdge(details_.window_component);
  if (UpdateMagnetismWindow(*bounds, edges)) {
    *bounds = ScreenAsh::ConvertRectFromScreen(
        window()->parent(),
        BoundsForMagneticResizeAttach(
            ScreenAsh::ConvertRectToScreen(window()->parent(), *bounds),
            magnetism_window_->GetBoundsInScreen(),
            magnetism_edge_));
  }
}

bool WorkspaceWindowResizer::UpdateMagnetismWindow(const gfx::Rect& bounds,
                                                   uint32 edges) {
  // |bounds| are in coordinates of original window's parent.
  gfx::Rect bounds_in_screen =
      ScreenAsh::ConvertRectToScreen(window()->parent(), bounds);
  MagnetismMatcher matcher(bounds_in_screen, edges);

  // If we snapped to a window then check it first. That way we don't bounce
  // around when close to multiple edges.
  if (magnetism_window_) {
    if (window_tracker_.Contains(magnetism_window_) &&
        matcher.ShouldAttach(magnetism_window_->GetBoundsInScreen(),
                             &magnetism_edge_)) {
      return true;
    }
    window_tracker_.Remove(magnetism_window_);
    magnetism_window_ = NULL;
  }

  // Avoid magnetically snapping to popups, menus, tooltips, controls and
  // windows that are not tracked by workspace.
  if (!wm::CanResizeWindow(window()) || !GetTrackedByWorkspace(window()))
    return false;

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    const aura::RootWindow* root_window = *iter;
    // Test all children from the desktop in each root window.
    const aura::Window::Windows& children = Shell::GetContainer(
        root_window, kShellWindowId_DefaultContainer)->children();
    for (aura::Window::Windows::const_reverse_iterator i = children.rbegin();
         i != children.rend() && !matcher.AreEdgesObscured(); ++i) {
      aura::Window* other = *i;
      if (other == window() ||
          !other->IsVisible() ||
          !wm::IsWindowNormal(other) ||
          !wm::CanResizeWindow(other)) {
        continue;
      }
      if (matcher.ShouldAttach(other->GetBoundsInScreen(), &magnetism_edge_)) {
        magnetism_window_ = other;
        window_tracker_.Add(magnetism_window_);
        return true;
      }
    }
  }
  return false;
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    int sticky_size,
    gfx::Rect* bounds) {
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  wm::ConvertPointToScreen(window()->parent(), &last_mouse_location_in_screen);
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestPoint(
      last_mouse_location_in_screen);
  gfx::Rect work_area =
      ScreenAsh::ConvertRectFromScreen(window()->parent(), display.work_area());
  if (details_.window_component == HTCAPTION) {
    // Adjust the bounds to the work area where the mouse cursor is located.
    // Always keep kMinOnscreenHeight on the bottom.
    int max_y = work_area.bottom() - kMinOnscreenHeight;
    if (bounds->y() > max_y) {
      bounds->set_y(max_y);
    } else if (bounds->y() <= work_area.y()) {
      // Don't allow dragging above the top of the display until the mouse
      // cursor reaches the work area above if any.
      bounds->set_y(work_area.y());
    }

    if (sticky_size > 0) {
      if (!StickToWorkAreaOnMove(work_area, sticky_size, bounds))
        MagneticallySnapToOtherWindows(bounds);
    }
  } else if (sticky_size > 0) {
    MagneticallySnapResizeToOtherWindows(bounds);
    if (!magnetism_window_ && sticky_size > 0)
      StickToWorkAreaOnResize(work_area, sticky_size, bounds);
  }

  if (attached_windows_.empty())
    return;

  if (details_.window_component == HTRIGHT) {
    bounds->set_width(std::min(bounds->width(),
                               work_area.right() - total_min_ - bounds->x()));
  } else {
    DCHECK_EQ(HTBOTTOM, details_.window_component);
    bounds->set_height(std::min(bounds->height(),
                                work_area.bottom() - total_min_ - bounds->y()));
  }
}

bool WorkspaceWindowResizer::StickToWorkAreaOnMove(
    const gfx::Rect& work_area,
    int sticky_size,
    gfx::Rect* bounds) const {
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  if (ShouldStickToEdge(bounds->x() - left_edge, sticky_size)) {
    bounds->set_x(left_edge);
    return true;
  } else if (ShouldStickToEdge(right_edge - bounds->right(), sticky_size)) {
    bounds->set_x(right_edge - bounds->width());
    return true;
  }
  if (ShouldStickToEdge(bounds->y() - top_edge, sticky_size)) {
    bounds->set_y(top_edge);
    return true;
  } else if (ShouldStickToEdge(bottom_edge - bounds->bottom(), sticky_size) &&
             bounds->height() < (bottom_edge - top_edge)) {
    // Only snap to the bottom if the window is smaller than the work area.
    // Doing otherwise can lead to window snapping in weird ways as it bounces
    // between snapping to top then bottom.
    bounds->set_y(bottom_edge - bounds->height());
    return true;
  }
  return false;
}

void WorkspaceWindowResizer::StickToWorkAreaOnResize(
    const gfx::Rect& work_area,
    int sticky_size,
    gfx::Rect* bounds) const {
  const uint32 edges = WindowComponentToMagneticEdge(details_.window_component);
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  if (edges & MAGNETISM_EDGE_TOP &&
      ShouldStickToEdge(bounds->y() - top_edge, sticky_size)) {
    bounds->set_height(bounds->bottom() - top_edge);
    bounds->set_y(top_edge);
  }
  if (edges & MAGNETISM_EDGE_LEFT &&
      ShouldStickToEdge(bounds->x() - left_edge, sticky_size)) {
    bounds->set_width(bounds->right() - left_edge);
    bounds->set_x(left_edge);
  }
  if (edges & MAGNETISM_EDGE_BOTTOM &&
      ShouldStickToEdge(bottom_edge - bounds->bottom(), sticky_size)) {
    bounds->set_height(bottom_edge - bounds->y());
  }
  if (edges & MAGNETISM_EDGE_RIGHT &&
      ShouldStickToEdge(right_edge - bounds->right(), sticky_size)) {
    bounds->set_width(right_edge - bounds->x());
  }
}

int WorkspaceWindowResizer::PrimaryAxisSize(const gfx::Size& size) const {
  return PrimaryAxisCoordinate(size.width(), size.height());
}

int WorkspaceWindowResizer::PrimaryAxisCoordinate(int x, int y) const {
  switch (details_.window_component) {
    case HTRIGHT:
      return x;
    case HTBOTTOM:
      return y;
    default:
      NOTREACHED();
  }
  return 0;
}

void WorkspaceWindowResizer::UpdateSnapPhantomWindow(const gfx::Point& location,
                                                     const gfx::Rect& bounds) {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  if (!wm::CanSnapWindow(window()))
    return;

  if (window()->type() == aura::client::WINDOW_TYPE_PANEL &&
      window()->GetProperty(kPanelAttachedKey)) {
    return;
  }

  SnapType last_type = snap_type_;
  snap_type_ = GetSnapType(location);
  if (snap_type_ == SNAP_NONE || snap_type_ != last_type) {
    snap_phantom_window_controller_.reset();
    snap_sizer_.reset();
    if (snap_type_ == SNAP_NONE)
      return;
  }
  if (!snap_sizer_) {
    SnapSizer::Edge edge = (snap_type_ == SNAP_LEFT_EDGE) ?
        SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
    snap_sizer_.reset(new SnapSizer(window(),
                                    location,
                                    edge,
                                    internal::SnapSizer::OTHER_INPUT));
  } else {
    snap_sizer_->Update(location);
  }
  if (!snap_phantom_window_controller_) {
    snap_phantom_window_controller_.reset(
        new PhantomWindowController(window()));
  }
  snap_phantom_window_controller_->Show(ScreenAsh::ConvertRectToScreen(
      window()->parent(), snap_sizer_->target_bounds()));
}

void WorkspaceWindowResizer::RestackWindows() {
  if (attached_windows_.empty())
    return;
  // Build a map from index in children to window, returning if there is a
  // window with a different parent.
  typedef std::map<size_t, aura::Window*> IndexToWindowMap;
  IndexToWindowMap map;
  aura::Window* parent = window()->parent();
  const aura::Window::Windows& windows(parent->children());
  map[std::find(windows.begin(), windows.end(), window()) -
      windows.begin()] = window();
  for (std::vector<aura::Window*>::const_iterator i =
           attached_windows_.begin(); i != attached_windows_.end(); ++i) {
    if ((*i)->parent() != parent)
      return;
    size_t index =
        std::find(windows.begin(), windows.end(), *i) - windows.begin();
    map[index] = *i;
  }

  // Reorder the windows starting at the topmost.
  parent->StackChildAtTop(map.rbegin()->second);
  for (IndexToWindowMap::const_reverse_iterator i = map.rbegin();
       i != map.rend(); ) {
    aura::Window* window = i->second;
    ++i;
    if (i != map.rend())
      parent->StackChildBelow(i->second, window);
  }
}

WorkspaceWindowResizer::SnapType WorkspaceWindowResizer::GetSnapType(
    const gfx::Point& location) const {
  // TODO: this likely only wants total display area, not the area of a single
  // display.
  gfx::Rect area(ScreenAsh::GetDisplayBoundsInParent(window()));
  if (location.x() <= area.x())
    return SNAP_LEFT_EDGE;
  if (location.x() >= area.right() - 1)
    return SNAP_RIGHT_EDGE;
  return SNAP_NONE;
}

}  // namespace internal
}  // namespace ash
