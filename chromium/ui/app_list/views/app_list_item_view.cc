// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_item_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/cached_label.h"
#include "ui/app_list/views/progress_bar_view.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/drag_controller.h"

namespace app_list {

namespace {

const int kTopBottomPadding = 10;
const int kTopPadding = 20;
const int kIconTitleSpacing = 7;
const int kProgressBarHorizontalPadding = 12;
const int kProgressBarVerticalPadding = 4;
const int kProgressBarHeight = 4;

const int kLeftRightPaddingChars = 1;

// Scale to transform the icon when a drag starts.
const float kDraggingIconScale = 1.5f;

// Delay in milliseconds of when the dragging UI should be shown for mouse drag.
const int kMouseDragUIDelayInMs = 100;

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ui/app_list/AppListItemView";

AppListItemView::AppListItemView(AppsGridView* apps_grid_view,
                                 AppListItemModel* model)
    : CustomButton(apps_grid_view),
      model_(model),
      apps_grid_view_(apps_grid_view),
      icon_(new views::ImageView),
      title_(new CachedLabel),
      progress_bar_(new ProgressBarView),
      ui_state_(UI_STATE_NORMAL),
      touch_dragging_(false) {
  icon_->set_interactive(false);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetBackgroundColor(0);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);
  title_->SetFont(rb.GetFont(kItemTextFontStyle));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetVisible(!model_->is_installing());
  title_->Invalidate();

  const gfx::ShadowValue kIconShadows[] = {
    gfx::ShadowValue(gfx::Point(0, 2), 2, SkColorSetARGB(0x24, 0, 0, 0)),
  };
  icon_shadows_.assign(kIconShadows, kIconShadows + arraysize(kIconShadows));

  AddChildView(icon_);
  AddChildView(title_);
  AddChildView(progress_bar_);

  ItemIconChanged();
  ItemTitleChanged();
  ItemIsInstallingChanged();
  model_->AddObserver(this);

  set_context_menu_controller(this);
  set_request_focus_on_press(false);

  SetAnimationDuration(0);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
}

void AppListItemView::SetIconSize(const gfx::Size& size) {
  if (icon_size_ == size)
    return;

  icon_size_ = size;
  UpdateIcon();
}

void AppListItemView::UpdateIcon() {
  // Skip if |icon_size_| has not been determined.
  if (icon_size_.IsEmpty())
    return;

  gfx::ImageSkia icon = model_->icon();
  // Clear icon and bail out if model icon is empty.
  if (icon.isNull()) {
    icon_->SetImage(NULL);
    return;
  }

  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(icon,
      skia::ImageOperations::RESIZE_BEST, icon_size_));
  if (model_->has_shadow()) {
    gfx::ImageSkia shadow(
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(resized,
                                                            icon_shadows_));
    icon_->SetImage(shadow);
    return;
  };

  icon_->SetImage(resized);
}

void AppListItemView::UpdateTooltip() {
  title_->SetTooltipText(model_->title() == model_->full_name() ?
                             string16() : UTF8ToUTF16(model_->full_name()));
}

void AppListItemView::SetUIState(UIState state) {
  if (ui_state_ == state)
    return;

  ui_state_ = state;

#if defined(USE_AURA)
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  switch (ui_state_) {
    case UI_STATE_NORMAL:
      title_->SetVisible(!model_->is_installing());
      progress_bar_->SetVisible(model_->is_installing());
      layer()->SetTransform(gfx::Transform());
      break;
    case UI_STATE_DRAGGING:
      title_->SetVisible(false);
      progress_bar_->SetVisible(false);
      const gfx::Rect bounds(layer()->bounds().size());
      layer()->SetTransform(gfx::GetScaleTransform(
          bounds.CenterPoint(),
          kDraggingIconScale));
      break;
  }
#endif
}

void AppListItemView::SetTouchDragging(bool touch_dragging) {
  if (touch_dragging_ == touch_dragging)
    return;

  touch_dragging_ = touch_dragging;
  SetUIState(touch_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);
}

void AppListItemView::OnMouseDragTimer() {
  DCHECK(apps_grid_view_->IsDraggedView(this));
  SetUIState(UI_STATE_DRAGGING);
}

void AppListItemView::Prerender() {
  title_->PaintToBackingImage();
}

void AppListItemView::CancelContextMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

gfx::ImageSkia AppListItemView::GetDragImage() {
  gfx::Canvas canvas(size(), ui::SCALE_FACTOR_100P, false /* is_opaque */);
  gfx::Rect bounds(size());
  canvas.DrawColor(SK_ColorTRANSPARENT);
  PaintChildren(&canvas);
  return gfx::ImageSkia(canvas.ExtractImageRep());
}

void AppListItemView::ItemIconChanged() {
  UpdateIcon();
}

void AppListItemView::ItemTitleChanged() {
  title_->SetText(UTF8ToUTF16(model_->title()));
  title_->Invalidate();
  UpdateTooltip();
  Layout();
}

void AppListItemView::ItemHighlightedChanged() {
  apps_grid_view_->EnsureViewVisible(this);
  SchedulePaint();
}

void AppListItemView::ItemIsInstallingChanged() {
  if (model_->is_installing())
    apps_grid_view_->EnsureViewVisible(this);
  title_->SetVisible(!model_->is_installing());
  progress_bar_->SetVisible(model_->is_installing());
  SchedulePaint();
}

void AppListItemView::ItemPercentDownloadedChanged() {
  // A percent_downloaded() of -1 can mean it's not known how much percent is
  // completed, or the download hasn't been marked complete, as is the case
  // while an extension is being installed after being downloaded.
  if (model_->percent_downloaded() == -1)
    return;
  progress_bar_->SetValue(model_->percent_downloaded() / 100.0);
}

const char* AppListItemView::GetClassName() const {
  return kViewClassName;
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  const int left_right_padding = kLeftRightPaddingChars *
      title_->font().GetAverageCharacterWidth();
  rect.Inset(left_right_padding, kTopPadding, left_right_padding, 0);
  const int y = rect.y();

  gfx::Rect icon_bounds(rect.x(), y, rect.width(), icon_size_.height());
  icon_bounds.Inset(gfx::ShadowValue::GetMargin(icon_shadows_));
  icon_->SetBoundsRect(icon_bounds);

  const gfx::Size title_size = title_->GetPreferredSize();
  gfx::Rect title_bounds(rect.x() + (rect.width() - title_size.width()) / 2,
                         y + icon_size_.height() + kIconTitleSpacing,
                         title_size.width(),
                         title_size.height());
  title_bounds.Intersect(rect);
  title_->SetBoundsRect(title_bounds);

  gfx::Rect progress_bar_bounds(progress_bar_->GetPreferredSize());
  progress_bar_bounds.set_x(GetContentsBounds().x() +
                            kProgressBarHorizontalPadding);
  progress_bar_bounds.set_y(title_bounds.y());
  progress_bar_->SetBoundsRect(progress_bar_bounds);
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  if (apps_grid_view_->IsDraggedView(this))
    return;

  gfx::Rect rect(GetContentsBounds());

  if (apps_grid_view_->IsSelectedView(this)) {
    canvas->FillRect(rect, kSelectedColor);
  } else if (model_->highlighted() && !model_->is_installing()) {
    canvas->FillRect(rect, kHighlightedColor);
  } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    canvas->FillRect(rect, kHighlightedColor);
  }
}

void AppListItemView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = UTF8ToUTF16(model_->title());
}

void AppListItemView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  ui::MenuModel* menu_model = model_->GetContextMenuModel();
  if (!menu_model)
    return;

  context_menu_runner_.reset(new views::MenuRunner(menu_model));
  if (context_menu_runner_->RunMenuAt(
          GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
          views::MenuItemView::TOPLEFT, source_type,
          views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

void AppListItemView::StateChanged() {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    apps_grid_view_->SetSelectedView(this);
    title_->SetEnabledColor(kGridTitleHoverColor);
  } else {
    apps_grid_view_->ClearSelectedView(this);
    model_->SetHighlighted(false);
    title_->SetEnabledColor(kGridTitleColor);
  }
  title_->Invalidate();
}

bool AppListItemView::ShouldEnterPushedState(const ui::Event& event) {
  // Don't enter pushed state for ET_GESTURE_TAP_DOWN so that hover gray
  // background does not show up during scroll.
  if (event.type() == ui::ET_GESTURE_TAP_DOWN)
    return false;

  return views::CustomButton::ShouldEnterPushedState(event);
}

bool AppListItemView::OnMousePressed(const ui::MouseEvent& event) {
  CustomButton::OnMousePressed(event);

  if (!ShouldEnterPushedState(event))
    return true;

  apps_grid_view_->InitiateDrag(this, AppsGridView::MOUSE, event);

  if (apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMouseDragUIDelayInMs),
        this, &AppListItemView::OnMouseDragTimer);
  }
  return true;
}

bool AppListItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Disable space key to press the button. The keyboard events received
  // by this view are forwarded from a Textfield (SearchBoxView) and key
  // released events are not forwarded. This leaves the button in pressed
  // state.
  if (event.key_code() == ui::VKEY_SPACE)
    return false;

  return CustomButton::OnKeyPressed(event);
}

void AppListItemView::OnMouseReleased(const ui::MouseEvent& event) {
  CustomButton::OnMouseReleased(event);
  apps_grid_view_->EndDrag(false);
  mouse_drag_timer_.Stop();
  SetUIState(UI_STATE_NORMAL);
}

void AppListItemView::OnMouseCaptureLost() {
  // We don't cancel the dag on mouse capture lost for windows as entering a
  // synchronous drag causes mouse capture to be lost and pressing escape
  // dismisses the app list anyway.
#if !defined(OS_WIN)
  CustomButton::OnMouseCaptureLost();
  apps_grid_view_->EndDrag(true);
  mouse_drag_timer_.Stop();
  SetUIState(UI_STATE_NORMAL);
#endif
}

bool AppListItemView::OnMouseDragged(const ui::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, event);

  // Shows dragging UI when it's confirmed without waiting for the timer.
  if (ui_state_ != UI_STATE_DRAGGING &&
      apps_grid_view_->dragging() &&
      apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Stop();
    SetUIState(UI_STATE_DRAGGING);
  }
  return true;
}

void AppListItemView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (touch_dragging_) {
        apps_grid_view_->InitiateDrag(this, AppsGridView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (touch_dragging_) {
        apps_grid_view_->UpdateDragFromItem(AppsGridView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (touch_dragging_) {
        SetTouchDragging(false);
        apps_grid_view_->EndDrag(false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      if (!apps_grid_view_->has_dragged_view())
        SetTouchDragging(true);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_END:
      if (touch_dragging_)
        SetTouchDragging(false);
      break;
    default:
      break;
  }
  if (!event->handled())
    CustomButton::OnGestureEvent(event);
}

}  // namespace app_list
