// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/touch_editable_impl_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host.h"
#include "grit/ui_strings.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/range/range.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, public:

TouchEditableImplAura::~TouchEditableImplAura() {
  Cleanup();
}

// static
TouchEditableImplAura* TouchEditableImplAura::Create() {
  if (switches::IsTouchEditingEnabled())
    return new TouchEditableImplAura();
  return NULL;
}

void TouchEditableImplAura::AttachToView(RenderWidgetHostViewAura* view) {
  if (rwhva_ == view)
    return;

  Cleanup();
  if (!view)
    return;

  rwhva_ = view;
  rwhva_->set_touch_editing_client(this);
}

void TouchEditableImplAura::UpdateEditingController() {
  if (!rwhva_ || !rwhva_->HasFocus())
    return;

  // If touch editing handles were not visible, we bring them up only if
  // there is non-zero selection on the page. And the current event is a
  // gesture event (we dont want to show handles if the user is selecting
  // using mouse or keyboard).
  if (selection_gesture_in_process_ && !scroll_in_progress_ &&
      selection_anchor_rect_ != selection_focus_rect_)
    StartTouchEditing();

  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE ||
      selection_anchor_rect_ != selection_focus_rect_) {
    if (touch_selection_controller_)
      touch_selection_controller_->SelectionChanged();
  } else {
    EndTouchEditing();
  }
}

void TouchEditableImplAura::OverscrollStarted() {
  overscroll_in_progress_ = true;
}

void TouchEditableImplAura::OverscrollCompleted() {
  // We might receive multiple OverscrollStarted() and OverscrollCompleted()
  // during the same scroll session (for example, when the scroll direction
  // changes). We want to show the handles only when:
  // 1. Overscroll has completed
  // 2. Scrolling session is over, i.e. we have received ET_GESTURE_SCROLL_END.
  // 3. If we had hidden the handles when scrolling started
  // 4. If there is still a need to show handles (there is a non-empty selection
  // or non-NONE |text_input_type_|)
  if (overscroll_in_progress_ && !scroll_in_progress_ &&
      handles_hidden_due_to_scroll_ &&
      (selection_anchor_rect_ != selection_focus_rect_ ||
          text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)) {
    StartTouchEditing();
    UpdateEditingController();
  }
  overscroll_in_progress_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, RenderWidgetHostViewAura::TouchEditingClient
// implementation:

void TouchEditableImplAura::StartTouchEditing() {
  if (!rwhva_ || !rwhva_->HasFocus())
    return;

  if (!touch_selection_controller_) {
    touch_selection_controller_.reset(
        ui::TouchSelectionController::create(this));
  }
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void TouchEditableImplAura::EndTouchEditing() {
  if (touch_selection_controller_) {
    if (touch_selection_controller_->IsHandleDragInProgress())
      touch_selection_controller_->SelectionChanged();
    else
      touch_selection_controller_.reset();
  }
}

void TouchEditableImplAura::OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                                       const gfx::Rect& focus) {
  selection_anchor_rect_ = anchor;
  selection_focus_rect_ = focus;
  UpdateEditingController();
}

void TouchEditableImplAura::OnTextInputTypeChanged(ui::TextInputType type) {
  text_input_type_ = type;
}

bool TouchEditableImplAura::HandleInputEvent(const ui::Event* event) {
  DCHECK(rwhva_);
  if (event->IsTouchEvent())
    return false;

  if (!event->IsGestureEvent()) {
    EndTouchEditing();
    return false;
  }

  const ui::GestureEvent* gesture_event =
      static_cast<const ui::GestureEvent*>(event);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      tap_gesture_tap_count_queue_.push(gesture_event->details().tap_count());
      if (gesture_event->details().tap_count() > 1)
        selection_gesture_in_process_ = true;
      // When the user taps, we want to show touch editing handles if user
      // tapped on selected text.
      if (selection_anchor_rect_ != selection_focus_rect_) {
        // UnionRects only works for rects with non-zero width.
        gfx::Rect anchor(selection_anchor_rect_.origin(),
                         gfx::Size(1, selection_anchor_rect_.height()));
        gfx::Rect focus(selection_focus_rect_.origin(),
                        gfx::Size(1, selection_focus_rect_.height()));
        gfx::Rect selection_rect = gfx::UnionRects(anchor, focus);
        if (selection_rect.Contains(gesture_event->location())) {
          StartTouchEditing();
          return true;
        }
      }
      // For single taps, not inside selected region, we want to show handles
      // only when the tap is on an already focused textfield.
      is_tap_on_focused_textfield_ = false;
      if (gesture_event->details().tap_count() == 1 &&
          text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)
        is_tap_on_focused_textfield_ = true;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      selection_gesture_in_process_ = true;
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      // If selection handles are currently visible, we want to get them back up
      // when scrolling ends. So we set |handles_hidden_due_to_scroll_| so that
      // we can re-start touch editing when we call |UpdateEditingController()|
      // on scroll end gesture.
      scroll_in_progress_ = true;
      handles_hidden_due_to_scroll_ = false;
      if (touch_selection_controller_)
        handles_hidden_due_to_scroll_ = true;
      EndTouchEditing();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      // Scroll has ended, but we might still be in overscroll animation.
      if (handles_hidden_due_to_scroll_ && !overscroll_in_progress_ &&
          (selection_anchor_rect_ != selection_focus_rect_ ||
              text_input_type_ != ui::TEXT_INPUT_TYPE_NONE)) {
        StartTouchEditing();
        UpdateEditingController();
      }
      // fall through to reset |scroll_in_progress_|.
    case ui::ET_SCROLL_FLING_START:
      selection_gesture_in_process_ = false;
      scroll_in_progress_ = false;
      break;
    default:
      break;
  }
  return false;
}

void TouchEditableImplAura::GestureEventAck(int gesture_event_type) {
  DCHECK(rwhva_);
  if (gesture_event_type == blink::WebInputEvent::GestureTap &&
      text_input_type_ != ui::TEXT_INPUT_TYPE_NONE &&
      is_tap_on_focused_textfield_) {
    StartTouchEditing();
    if (touch_selection_controller_)
      touch_selection_controller_->SelectionChanged();
  }

  if (gesture_event_type == blink::WebInputEvent::GestureLongPress)
    selection_gesture_in_process_ = false;
  if (gesture_event_type == blink::WebInputEvent::GestureTap) {
    if (tap_gesture_tap_count_queue_.front() > 1)
      selection_gesture_in_process_ = false;
    tap_gesture_tap_count_queue_.pop();
  }
}

void TouchEditableImplAura::OnViewDestroyed() {
  Cleanup();
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, ui::TouchEditable implementation:

void TouchEditableImplAura::SelectRect(const gfx::Point& start,
                                       const gfx::Point& end) {
  if (!rwhva_)
    return;

  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  host->SelectRange(start, end);
}

void TouchEditableImplAura::MoveCaretTo(const gfx::Point& point) {
  if (!rwhva_)
    return;

  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  host->MoveCaret(point);
}

void TouchEditableImplAura::GetSelectionEndPoints(gfx::Rect* p1,
                                                  gfx::Rect* p2) {
  *p1 = selection_anchor_rect_;
  *p2 = selection_focus_rect_;
}

gfx::Rect TouchEditableImplAura::GetBounds() {
  return rwhva_ ? rwhva_->GetNativeView()->bounds() : gfx::Rect();
}

gfx::NativeView TouchEditableImplAura::GetNativeView() {
  return rwhva_ ? rwhva_->GetNativeView()->GetRootWindow() : NULL;
}

void TouchEditableImplAura::ConvertPointToScreen(gfx::Point* point) {
  if (!rwhva_)
    return;
  aura::Window* window = rwhva_->GetNativeView();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(window, point);
}

void TouchEditableImplAura::ConvertPointFromScreen(gfx::Point* point) {
  if (!rwhva_)
    return;
  aura::Window* window = rwhva_->GetNativeView();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client)
    screen_position_client->ConvertPointFromScreen(window, point);
}

bool TouchEditableImplAura::DrawsHandles() {
  return false;
}

void TouchEditableImplAura::OpenContextMenu(const gfx::Point& anchor) {
  if (!rwhva_)
    return;
  gfx::Point point = anchor;
  ConvertPointFromScreen(&point);
  RenderWidgetHost* host = rwhva_->GetRenderWidgetHost();
  host->Send(new ViewMsg_ShowContextMenu(host->GetRoutingID(), point));
  EndTouchEditing();
}

bool TouchEditableImplAura::IsCommandIdChecked(int command_id) const {
  NOTREACHED();
  return false;
}

bool TouchEditableImplAura::IsCommandIdEnabled(int command_id) const {
  if (!rwhva_)
    return false;
  bool editable = rwhva_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  gfx::Range selection_range;
  rwhva_->GetSelectionRange(&selection_range);
  bool has_selection = !selection_range.is_empty();
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && has_selection;
    case IDS_APP_COPY:
      return has_selection;
    case IDS_APP_PASTE: {
      base::string16 result;
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    }
    case IDS_APP_DELETE:
      return editable && has_selection;
    case IDS_APP_SELECT_ALL:
      return true;
    default:
      return false;
  }
}

bool TouchEditableImplAura::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void TouchEditableImplAura::ExecuteCommand(int command_id, int event_flags) {
  if (!rwhva_)
    return;
  RenderWidgetHost* host = rwhva_->GetRenderWidgetHost();
  switch (command_id) {
    case IDS_APP_CUT:
      host->Cut();
      break;
    case IDS_APP_COPY:
      host->Copy();
      break;
    case IDS_APP_PASTE:
      host->Paste();
      break;
    case IDS_APP_DELETE:
      host->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      host->SelectAll();
      break;
    default:
      NOTREACHED();
      break;
  }
  EndTouchEditing();
}

////////////////////////////////////////////////////////////////////////////////
// TouchEditableImplAura, private:

TouchEditableImplAura::TouchEditableImplAura()
    : text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      rwhva_(NULL),
      selection_gesture_in_process_(false),
      handles_hidden_due_to_scroll_(false),
      scroll_in_progress_(false),
      overscroll_in_progress_(false),
      is_tap_on_focused_textfield_(false) {
}

void TouchEditableImplAura::Cleanup() {
  if (rwhva_) {
    rwhva_->set_touch_editing_client(NULL);
    rwhva_ = NULL;
  }
  text_input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  touch_selection_controller_.reset();
  handles_hidden_due_to_scroll_ = false;
  scroll_in_progress_ = false;
  overscroll_in_progress_ = false;
}

}  // namespace content
