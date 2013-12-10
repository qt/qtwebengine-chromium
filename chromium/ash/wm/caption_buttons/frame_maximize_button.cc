// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/frame_maximize_button.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/caption_buttons/maximize_bubble_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::internal::SnapSizer;

namespace ash {

namespace {

// Delay before forcing an update of the snap location.
const int kUpdateDelayMS = 400;

// The delay of the bubble appearance.
const int kBubbleAppearanceDelayMS = 500;

// The minimum sanp size in percent of the screen width.
const int kMinSnapSizePercent = 50;
}

// EscapeEventFilter is installed on the RootWindow to track when the escape key
// is pressed. We use an EventFilter for this as the FrameMaximizeButton
// normally does not get focus.
class FrameMaximizeButton::EscapeEventFilter : public ui::EventHandler {
 public:
  explicit EscapeEventFilter(FrameMaximizeButton* button);
  virtual ~EscapeEventFilter();

  // EventFilter overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

 private:
  FrameMaximizeButton* button_;

  DISALLOW_COPY_AND_ASSIGN(EscapeEventFilter);
};

FrameMaximizeButton::EscapeEventFilter::EscapeEventFilter(
    FrameMaximizeButton* button)
    : button_(button) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

FrameMaximizeButton::EscapeEventFilter::~EscapeEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void FrameMaximizeButton::EscapeEventFilter::OnKeyEvent(
    ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    button_->Cancel(false);
  }
}

// FrameMaximizeButton ---------------------------------------------------------

FrameMaximizeButton::FrameMaximizeButton(views::ButtonListener* listener,
                                         views::Widget* frame)
    : ImageButton(listener),
      frame_(frame),
      observing_frame_(false),
      is_snap_enabled_(false),
      exceeded_drag_threshold_(false),
      press_is_gesture_(false),
      snap_type_(SNAP_NONE),
      bubble_appearance_delay_ms_(kBubbleAppearanceDelayMS) {
  // TODO(sky): nuke this. It's temporary while we don't have good images.
  SetImageAlignment(ALIGN_LEFT, ALIGN_BOTTOM);

  if (Shell::IsForcedMaximizeMode())
    views::View::SetVisible(false);
}

FrameMaximizeButton::~FrameMaximizeButton() {
  // Before the window gets destroyed, the maximizer dialog needs to be shut
  // down since it would otherwise call into a deleted object.
  maximizer_.reset();
  if (observing_frame_)
    OnWindowDestroying(frame_->GetNativeWindow());
}

void FrameMaximizeButton::SnapButtonHovered(SnapType type) {
  // Make sure to only show hover operations when no button is pressed and
  // a similar snap operation in progress does not get re-applied.
  if (is_snap_enabled_ || (type == snap_type_ && snap_sizer_))
    return;
  // Prime the mouse location with the center of the (local) button.
  press_location_ = gfx::Point(width() / 2, height() / 2);
  // Then get an adjusted mouse position to initiate the effect.
  gfx::Point location = press_location_;
  switch (type) {
    case SNAP_LEFT:
      location.set_x(location.x() - width());
      break;
    case SNAP_RIGHT:
      location.set_x(location.x() + width());
      break;
    case SNAP_MINIMIZE:
      location.set_y(location.y() + height());
      break;
    case SNAP_RESTORE:
      // Simulate a mouse button move over the according button.
      if (GetMaximizeBubbleFrameState() == FRAME_STATE_SNAP_LEFT)
        location.set_x(location.x() - width());
      else if (GetMaximizeBubbleFrameState() == FRAME_STATE_SNAP_RIGHT)
        location.set_x(location.x() + width());
      break;
    case SNAP_MAXIMIZE:
      break;
    case SNAP_NONE:
      Cancel(true);
      return;
    default:
      // We should not come here.
      NOTREACHED();
  }
  // Note: There is no hover with touch - we can therefore pass false for touch
  // operations.
  UpdateSnap(location, true, false);
}

void FrameMaximizeButton::ExecuteSnapAndCloseMenu(SnapType snap_type) {
  // We can come here with no snap type set in case that the mouse opened the
  // maximize button and a touch event "touched" a button.
  if (snap_type_ == SNAP_NONE)
    SnapButtonHovered(snap_type);

  Cancel(true);
  // Tell our menu to close.
  maximizer_.reset();
  snap_type_ = snap_type;
  // Since Snap might destroy |this|, but the snap_sizer needs to be destroyed,
  // The ownership of the snap_sizer is taken now.
  scoped_ptr<SnapSizer> snap_sizer(snap_sizer_.release());
  Snap(snap_sizer.get());
}

void FrameMaximizeButton::DestroyMaximizeMenu() {
  Cancel(false);
}

void FrameMaximizeButton::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  Cancel(false);
}

void FrameMaximizeButton::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  Cancel(false);
}

void FrameMaximizeButton::OnWindowDestroying(aura::Window* window) {
  maximizer_.reset();
  if (observing_frame_) {
    CHECK_EQ(frame_->GetNativeWindow(), window);
    frame_->GetNativeWindow()->RemoveObserver(this);
    frame_->RemoveObserver(this);
    observing_frame_ = false;
  }
}

void FrameMaximizeButton::OnWidgetActivationChanged(views::Widget* widget,
                                                    bool active) {
  // Upon losing focus, the control bubble should hide.
  if (!active && maximizer_)
    maximizer_.reset();
}

bool FrameMaximizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // If we are already in a mouse click / drag operation, a second button down
  // call will cancel (this addresses crbug.com/143755).
  if (is_snap_enabled_) {
    Cancel(false);
  } else {
    is_snap_enabled_ = event.IsOnlyLeftMouseButton();
    if (is_snap_enabled_)
      ProcessStartEvent(event);
  }
  ImageButton::OnMousePressed(event);
  return true;
}

void FrameMaximizeButton::OnMouseEntered(const ui::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
  if (!maximizer_) {
    DCHECK(GetWidget());
    if (!observing_frame_) {
      observing_frame_ = true;
      frame_->GetNativeWindow()->AddObserver(this);
      frame_->AddObserver(this);
    }
    maximizer_.reset(new MaximizeBubbleController(
        this,
        GetMaximizeBubbleFrameState(),
        bubble_appearance_delay_ms_));
  }
}

void FrameMaximizeButton::OnMouseExited(const ui::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  // Remove the bubble menu when the button is not pressed and the mouse is not
  // within the bubble.
  if (!is_snap_enabled_ && maximizer_) {
    if (maximizer_->GetBubbleWindow()) {
      gfx::Point screen_location = Shell::GetScreen()->GetCursorScreenPoint();
      if (!maximizer_->GetBubbleWindow()->GetBoundsInScreen().Contains(
              screen_location)) {
        maximizer_.reset();
        // Make sure that all remaining snap hover states get removed.
        SnapButtonHovered(SNAP_NONE);
      }
    } else {
      // The maximize dialog does not show up immediately after creating the
      // |mazimizer_|. Destroy the dialog therefore before it shows up.
      maximizer_.reset();
    }
  }
}

bool FrameMaximizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  if (is_snap_enabled_)
    ProcessUpdateEvent(event);
  return ImageButton::OnMouseDragged(event);
}

void FrameMaximizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  maximizer_.reset();
  bool snap_was_enabled = is_snap_enabled_;
  if (!ProcessEndEvent(event) && snap_was_enabled)
    ImageButton::OnMouseReleased(event);
  // At this point |this| might be already destroyed.
}

void FrameMaximizeButton::OnMouseCaptureLost() {
  Cancel(false);
  ImageButton::OnMouseCaptureLost();
}

void FrameMaximizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    is_snap_enabled_ = true;
    ProcessStartEvent(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      (event->type() == ui::ET_GESTURE_SCROLL_END && is_snap_enabled_) ||
      event->type() == ui::ET_SCROLL_FLING_START) {
    // The position of the event may have changed from the previous event (both
    // for TAP and SCROLL_END). So it is necessary to update the snap-state for
    // the current event.
    ProcessUpdateEvent(*event);
    if (event->type() == ui::ET_GESTURE_TAP) {
      snap_type_ = SnapTypeForLocation(event->location());
      TouchUMA::GetInstance()->RecordGestureAction(
          TouchUMA::GESTURE_FRAMEMAXIMIZE_TAP);
    }
    ProcessEndEvent(*event);
    event->SetHandled();
    return;
  }

  if (is_snap_enabled_) {
    if (event->type() == ui::ET_GESTURE_END &&
        event->details().touch_points() == 1) {
      // The position of the event may have changed from the previous event. So
      // it is necessary to update the snap-state for the current event.
      ProcessUpdateEvent(*event);
      snap_type_ = SnapTypeForLocation(event->location());
      ProcessEndEvent(*event);
      event->SetHandled();
      return;
    }

    if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
        event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      ProcessUpdateEvent(*event);
      event->SetHandled();
      return;
    }
  }

  ImageButton::OnGestureEvent(event);
}

void FrameMaximizeButton::SetVisible(bool visible) {
  // In the enforced maximized mode we do not allow to be made visible.
  if (Shell::IsForcedMaximizeMode())
    return;

  views::View::SetVisible(visible);
}

void FrameMaximizeButton::ProcessStartEvent(const ui::LocatedEvent& event) {
  DCHECK(is_snap_enabled_);
  // Prepare the help menu.
  if (!maximizer_) {
    maximizer_.reset(new MaximizeBubbleController(
        this,
        GetMaximizeBubbleFrameState(),
        bubble_appearance_delay_ms_));
  } else {
    // If the menu did not show up yet, we delay it even a bit more.
    maximizer_->DelayCreation();
  }
  snap_sizer_.reset(NULL);
  InstallEventFilter();
  snap_type_ = SNAP_NONE;
  press_location_ = event.location();
  press_is_gesture_ = event.IsGestureEvent();
  exceeded_drag_threshold_ = false;
  update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kUpdateDelayMS),
      this,
      &FrameMaximizeButton::UpdateSnapFromEventLocation);
}

void FrameMaximizeButton::ProcessUpdateEvent(const ui::LocatedEvent& event) {
  DCHECK(is_snap_enabled_);
  if (!exceeded_drag_threshold_) {
    exceeded_drag_threshold_ = views::View::ExceededDragThreshold(
        event.location() - press_location_);
  }
  if (exceeded_drag_threshold_)
    UpdateSnap(event.location(), false, event.IsGestureEvent());
}

bool FrameMaximizeButton::ProcessEndEvent(const ui::LocatedEvent& event) {
  update_timer_.Stop();
  UninstallEventFilter();
  bool should_snap = is_snap_enabled_;
  is_snap_enabled_ = false;

  // Remove our help bubble.
  maximizer_.reset();

  if (!should_snap || snap_type_ == SNAP_NONE)
    return false;

  SetState(views::CustomButton::STATE_NORMAL);
  // SetState will not call SchedulePaint() if state was already set to
  // STATE_NORMAL during a drag.
  SchedulePaint();
  phantom_window_.reset();
  // Since Snap might destroy |this|, but the snap_sizer needs to be destroyed,
  // The ownership of the snap_sizer is taken now.
  scoped_ptr<SnapSizer> snap_sizer(snap_sizer_.release());
  Snap(snap_sizer.get());
  return true;
}

void FrameMaximizeButton::Cancel(bool keep_menu_open) {
  if (!keep_menu_open) {
    maximizer_.reset();
    UninstallEventFilter();
    is_snap_enabled_ = false;
    snap_sizer_.reset();
  }
  phantom_window_.reset();
  snap_type_ = SNAP_NONE;
  update_timer_.Stop();
  SchedulePaint();
}

void FrameMaximizeButton::InstallEventFilter() {
  if (escape_event_filter_)
    return;

  escape_event_filter_.reset(new EscapeEventFilter(this));
}

void FrameMaximizeButton::UninstallEventFilter() {
  escape_event_filter_.reset(NULL);
}

void FrameMaximizeButton::UpdateSnapFromEventLocation() {
  // If the drag threshold has been exceeded the snap location is up to date.
  if (exceeded_drag_threshold_)
    return;
  exceeded_drag_threshold_ = true;
  UpdateSnap(press_location_, false, press_is_gesture_);
}

void FrameMaximizeButton::UpdateSnap(const gfx::Point& location,
                                     bool select_default,
                                     bool is_touch) {
  SnapType type = SnapTypeForLocation(location);
  if (type == snap_type_) {
    if (snap_sizer_) {
      snap_sizer_->Update(LocationForSnapSizer(location));
      phantom_window_->Show(ScreenAsh::ConvertRectToScreen(
          frame_->GetNativeView()->parent(),
          snap_sizer_->target_bounds()));
    }
    return;
  }

  snap_type_ = type;
  snap_sizer_.reset();
  SchedulePaint();

  if (snap_type_ == SNAP_NONE) {
    phantom_window_.reset();
    return;
  }

  if (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT) {
    SnapSizer::Edge snap_edge = snap_type_ == SNAP_LEFT ?
        SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
    SnapSizer::InputType input_type =
        is_touch ? SnapSizer::TOUCH_MAXIMIZE_BUTTON_INPUT :
                   SnapSizer::OTHER_INPUT;
    snap_sizer_.reset(new SnapSizer(frame_->GetNativeWindow(),
                                    LocationForSnapSizer(location),
                                    snap_edge,
                                    input_type));
    if (select_default)
      snap_sizer_->SelectDefaultSizeAndDisableResize();
  }
  if (!phantom_window_) {
    phantom_window_.reset(new internal::PhantomWindowController(
                              frame_->GetNativeWindow()));
  }
  if (maximizer_) {
    phantom_window_->set_phantom_below_window(maximizer_->GetBubbleWindow());
    maximizer_->SetSnapType(snap_type_);
  }
  phantom_window_->Show(
      ScreenBoundsForType(snap_type_, *snap_sizer_.get()));
}

SnapType FrameMaximizeButton::SnapTypeForLocation(
    const gfx::Point& location) const {
  MaximizeBubbleFrameState maximize_type = GetMaximizeBubbleFrameState();
  gfx::Vector2d delta(location - press_location_);
  if (!views::View::ExceededDragThreshold(delta))
    return maximize_type != FRAME_STATE_FULL ? SNAP_MAXIMIZE : SNAP_RESTORE;
  if (delta.x() < 0 && delta.y() > delta.x() && delta.y() < -delta.x())
    return maximize_type == FRAME_STATE_SNAP_LEFT ? SNAP_RESTORE : SNAP_LEFT;
  if (delta.x() > 0 && delta.y() > -delta.x() && delta.y() < delta.x())
    return maximize_type == FRAME_STATE_SNAP_RIGHT ? SNAP_RESTORE : SNAP_RIGHT;
  if (delta.y() > 0)
    return SNAP_MINIMIZE;
  return maximize_type != FRAME_STATE_FULL ? SNAP_MAXIMIZE : SNAP_RESTORE;
}

gfx::Rect FrameMaximizeButton::ScreenBoundsForType(
    SnapType type,
    const SnapSizer& snap_sizer) const {
  aura::Window* window = frame_->GetNativeWindow();
  switch (type) {
    case SNAP_LEFT:
    case SNAP_RIGHT:
      return ScreenAsh::ConvertRectToScreen(window->parent(),
                                            snap_sizer.target_bounds());
    case SNAP_MAXIMIZE:
      return ScreenAsh::ConvertRectToScreen(
          window->parent(),
          ScreenAsh::GetMaximizedWindowBoundsInParent(window));
    case SNAP_MINIMIZE: {
      gfx::Rect rect = GetMinimizeAnimationTargetBoundsInScreen(window);
      if (!rect.IsEmpty()) {
        // PhantomWindowController insets slightly, outset it so the phantom
        // doesn't appear inset.
        rect.Inset(-8, -8);
      }
      return rect;
    }
    case SNAP_RESTORE: {
      wm::WindowState* window_state = wm::GetWindowState(window);
      return window_state->HasRestoreBounds() ?
          window_state->GetRestoreBoundsInScreen() :
          frame_->GetWindowBoundsInScreen();
    }
    case SNAP_NONE:
      NOTREACHED();
  }
  return gfx::Rect();
}

gfx::Point FrameMaximizeButton::LocationForSnapSizer(
    const gfx::Point& location) const {
  gfx::Point result(location);
  views::View::ConvertPointToScreen(this, &result);
  return result;
}

void FrameMaximizeButton::Snap(SnapSizer* snap_sizer) {
  Shell* shell = Shell::GetInstance();
  wm::WindowState* window_state = wm::GetWindowState(frame_->GetNativeWindow());
  switch (snap_type_) {
    case SNAP_LEFT:
    case SNAP_RIGHT: {
      // Others might also have set up a restore rectangle already. If so, we
      // should not overwrite the restore rectangle.
      gfx::Rect current_bounds_in_screen = frame_->GetWindowBoundsInScreen();
      snap_sizer->SnapWindowToTargetBounds();
      if (!window_state->HasRestoreBounds())
        window_state->SetRestoreBoundsInScreen(current_bounds_in_screen);
      shell->delegate()->RecordUserMetricsAction(
          snap_type_ == SNAP_LEFT ?
              UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT :
              UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT);
      break;
    }
    case SNAP_MAXIMIZE:
      frame_->Maximize();
      shell->delegate()->RecordUserMetricsAction(
          UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE);
      break;
    case SNAP_MINIMIZE:
      frame_->Minimize();
      shell->delegate()->RecordUserMetricsAction(
          UMA_WINDOW_MAXIMIZE_BUTTON_MINIMIZE);
      break;
    case SNAP_RESTORE:
      frame_->Restore();
      shell->delegate()->RecordUserMetricsAction(
          UMA_WINDOW_MAXIMIZE_BUTTON_RESTORE);
      break;
    case SNAP_NONE:
      NOTREACHED();
  }
}

MaximizeBubbleFrameState
FrameMaximizeButton::GetMaximizeBubbleFrameState() const {
  wm::WindowState* window_state =
      wm::GetWindowState(frame_->GetNativeWindow());
  // When there are no restore bounds, we are in normal mode.
  if (!window_state->HasRestoreBounds())
    return FRAME_STATE_NONE;
  // The normal maximized test can be used.
  if (frame_->IsMaximized())
    return FRAME_STATE_FULL;
  // For Left/right maximize we need to check the dimensions.
  gfx::Rect bounds = frame_->GetWindowBoundsInScreen();
  gfx::Rect screen = Shell::GetScreen()->GetDisplayNearestWindow(
      frame_->GetNativeView()).work_area();
  if (bounds.width() < (screen.width() * kMinSnapSizePercent) / 100)
    return FRAME_STATE_NONE;
  // We might still have a horizontally filled window at this point which we
  // treat as no special state.
  if (bounds.y() != screen.y() || bounds.height() != screen.height())
    return FRAME_STATE_NONE;

  // We have to be in a maximize mode at this point.
  if (bounds.x() == screen.x())
    return FRAME_STATE_SNAP_LEFT;
  if (bounds.right() == screen.right())
    return FRAME_STATE_SNAP_RIGHT;
  // If we come here, it is likely caused by the fact that the
  // "VerticalResizeDoubleClick" stored a restore rectangle. In that case
  // we allow all maximize operations (and keep the restore rectangle).
  return FRAME_STATE_NONE;
}

}  // namespace ash
