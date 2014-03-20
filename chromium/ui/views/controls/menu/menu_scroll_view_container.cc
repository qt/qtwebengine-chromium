// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_scroll_view_container.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/round_rect_painter.h"

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_aura.h"
#endif

using ui::NativeTheme;

namespace views {

namespace {

static const int kBorderPaddingDueToRoundedCorners = 1;

// MenuScrollButton ------------------------------------------------------------

// MenuScrollButton is used for the scroll buttons when not all menu items fit
// on screen. MenuScrollButton forwards appropriate events to the
// MenuController.

class MenuScrollButton : public View {
 public:
  MenuScrollButton(SubmenuView* host, bool is_up)
      : host_(host),
        is_up_(is_up),
        // Make our height the same as that of other MenuItemViews.
        pref_height_(MenuItemView::pref_menu_height()) {
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(
        host_->GetMenuItem()->GetMenuConfig().scroll_arrow_height * 2 - 1,
        pref_height_);
  }

  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    return true;  // Always return true so that drop events are targeted to us.
  }

  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragEnteredScrollButton(
        host_, is_up_);
  }

  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE {
    return ui::DragDropTypes::DRAG_NONE;
  }

  virtual void OnDragExited() OVERRIDE {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragExitedScrollButton(host_);
  }

  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE {
    return ui::DragDropTypes::DRAG_NONE;
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    const MenuConfig& config = host_->GetMenuItem()->GetMenuConfig();

    // The background.
    gfx::Rect item_bounds(0, 0, width(), height());
    NativeTheme::ExtraParams extra;
    extra.menu_item.is_selected = false;
    GetNativeTheme()->Paint(canvas->sk_canvas(),
                            NativeTheme::kMenuItemBackground,
                            NativeTheme::kNormal, item_bounds, extra);

    // Then the arrow.
    int x = width() / 2;
    int y = (height() - config.scroll_arrow_height) / 2;

    int x_left = x - config.scroll_arrow_height;
    int x_right = x + config.scroll_arrow_height;
    int y_bottom;

    if (!is_up_) {
      y_bottom = y;
      y = y_bottom + config.scroll_arrow_height;
    } else {
      y_bottom = y + config.scroll_arrow_height;
    }
    SkPath path;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(SkIntToScalar(x), SkIntToScalar(y));
    path.lineTo(SkIntToScalar(x_left), SkIntToScalar(y_bottom));
    path.lineTo(SkIntToScalar(x_right), SkIntToScalar(y_bottom));
    path.lineTo(SkIntToScalar(x), SkIntToScalar(y));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setColor(config.arrow_color);
    canvas->DrawPath(path, paint);
  }

 private:
  // SubmenuView we were created for.
  SubmenuView* host_;

  // Direction of the button.
  bool is_up_;

  // Preferred height.
  int pref_height_;

  DISALLOW_COPY_AND_ASSIGN(MenuScrollButton);
};

}  // namespace

// MenuScrollView --------------------------------------------------------------

// MenuScrollView is a viewport for the SubmenuView. It's reason to exist is so
// that ScrollRectToVisible works.
//
// NOTE: It is possible to use ScrollView directly (after making it deal with
// null scrollbars), but clicking on a child of ScrollView forces the window to
// become active, which we don't want. As we really only need a fraction of
// what ScrollView does, so we use a one off variant.

class MenuScrollViewContainer::MenuScrollView : public View {
 public:
  explicit MenuScrollView(View* child) {
    AddChildView(child);
  }

  virtual void ScrollRectToVisible(const gfx::Rect& rect) OVERRIDE {
    // NOTE: this assumes we only want to scroll in the y direction.

    // If the rect is already visible, do not scroll.
    if (GetLocalBounds().Contains(rect))
      return;

    // Scroll just enough so that the rect is visible.
    int dy = 0;
    if (rect.bottom() > GetLocalBounds().bottom())
      dy = rect.bottom() - GetLocalBounds().bottom();
    else
      dy = rect.y();

    // Convert rect.y() to view's coordinates and make sure we don't show past
    // the bottom of the view.
    View* child = GetContents();
    child->SetY(-std::max(0, std::min(
        child->GetPreferredSize().height() - this->height(),
        dy - child->y())));
  }

  // Returns the contents, which is the SubmenuView.
  View* GetContents() {
    return child_at(0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuScrollView);
};

// MenuScrollViewContainer ----------------------------------------------------

MenuScrollViewContainer::MenuScrollViewContainer(SubmenuView* content_view)
    : content_view_(content_view),
      arrow_(BubbleBorder::NONE),
      bubble_border_(NULL) {
  scroll_up_button_ = new MenuScrollButton(content_view, true);
  scroll_down_button_ = new MenuScrollButton(content_view, false);
  AddChildView(scroll_up_button_);
  AddChildView(scroll_down_button_);

  scroll_view_ = new MenuScrollView(content_view);
  AddChildView(scroll_view_);

  arrow_ = BubbleBorderTypeFromAnchor(
      content_view_->GetMenuItem()->GetMenuController()->GetAnchorPosition());

  if (arrow_ != BubbleBorder::NONE)
    CreateBubbleBorder();
  else
    CreateDefaultBorder();
}

bool MenuScrollViewContainer::HasBubbleBorder() {
  return arrow_ != BubbleBorder::NONE;
}

void MenuScrollViewContainer::SetBubbleArrowOffset(int offset) {
  DCHECK(HasBubbleBorder());
  bubble_border_->set_arrow_offset(offset);
}

void MenuScrollViewContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (background()) {
    View::OnPaintBackground(canvas);
    return;
  }

  gfx::Rect bounds(0, 0, width(), height());
  NativeTheme::ExtraParams extra;
  const MenuConfig& menu_config = content_view_->GetMenuItem()->GetMenuConfig();
  extra.menu_background.corner_radius = menu_config.corner_radius;
  GetNativeTheme()->Paint(canvas->sk_canvas(),
      NativeTheme::kMenuPopupBackground, NativeTheme::kNormal, bounds, extra);
}

void MenuScrollViewContainer::Layout() {
  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int width = View::width() - insets.width();
  int content_height = height() - insets.height();
  if (!scroll_up_button_->visible()) {
    scroll_view_->SetBounds(x, y, width, content_height);
    scroll_view_->Layout();
    return;
  }

  gfx::Size pref = scroll_up_button_->GetPreferredSize();
  scroll_up_button_->SetBounds(x, y, width, pref.height());
  content_height -= pref.height();

  const int scroll_view_y = y + pref.height();

  pref = scroll_down_button_->GetPreferredSize();
  scroll_down_button_->SetBounds(x, height() - pref.height() - insets.top(),
                                 width, pref.height());
  content_height -= pref.height();

  scroll_view_->SetBounds(x, scroll_view_y, width, content_height);
  scroll_view_->Layout();
}

gfx::Size MenuScrollViewContainer::GetPreferredSize() {
  gfx::Size prefsize = scroll_view_->GetContents()->GetPreferredSize();
  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

void MenuScrollViewContainer::GetAccessibleState(
    ui::AccessibleViewState* state) {
  // Get the name from the submenu view.
  content_view_->GetAccessibleState(state);

  // Now change the role.
  state->role = ui::AccessibilityTypes::ROLE_MENUBAR;
  // Some AT (like NVDA) will not process focus events on menu item children
  // unless a parent claims to be focused.
  state->state = ui::AccessibilityTypes::STATE_FOCUSED;
}

void MenuScrollViewContainer::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  gfx::Size content_pref = scroll_view_->GetContents()->GetPreferredSize();
  scroll_up_button_->SetVisible(content_pref.height() > height());
  scroll_down_button_->SetVisible(content_pref.height() > height());
  Layout();
}

void MenuScrollViewContainer::CreateDefaultBorder() {
  arrow_ = BubbleBorder::NONE;
  bubble_border_ = NULL;

  const MenuConfig& menu_config =
      content_view_->GetMenuItem()->GetMenuConfig();

  bool use_border = true;
  int padding = menu_config.corner_radius > 0 ?
        kBorderPaddingDueToRoundedCorners : 0;

#if defined(USE_AURA)
  if (menu_config.native_theme == ui::NativeThemeAura::instance()) {
    // In case of NativeThemeAura the border gets drawn with the shadow.
    // Furthermore no additional padding is wanted.
    use_border = false;
    padding = 0;
  }
#endif

  int top = menu_config.menu_vertical_border_size + padding;
  int left = menu_config.menu_horizontal_border_size + padding;
  int bottom = menu_config.menu_vertical_border_size + padding;
  int right = menu_config.menu_horizontal_border_size + padding;

  if (use_border) {
    set_border(views::Border::CreateBorderPainter(
        new views::RoundRectPainter(menu_config.native_theme->GetSystemColor(
                ui::NativeTheme::kColorId_MenuBorderColor),
            menu_config.corner_radius),
            gfx::Insets(top, left, bottom, right)));
  } else {
    set_border(Border::CreateEmptyBorder(top, left, bottom, right));
  }
}

void MenuScrollViewContainer::CreateBubbleBorder() {
  bubble_border_ = new BubbleBorder(arrow_,
                                    BubbleBorder::SMALL_SHADOW,
                                    SK_ColorWHITE);
  set_border(bubble_border_);
  set_background(new BubbleBackground(bubble_border_));
}

BubbleBorder::Arrow
MenuScrollViewContainer::BubbleBorderTypeFromAnchor(
    MenuItemView::AnchorPosition anchor) {
  switch (anchor) {
    case views::MenuItemView::BUBBLE_LEFT:
      return BubbleBorder::RIGHT_CENTER;
    case views::MenuItemView::BUBBLE_RIGHT:
      return BubbleBorder::LEFT_CENTER;
    case views::MenuItemView::BUBBLE_ABOVE:
      return BubbleBorder::BOTTOM_CENTER;
    case views::MenuItemView::BUBBLE_BELOW:
      return BubbleBorder::TOP_CENTER;
    default:
      return BubbleBorder::NONE;
  }
}

}  // namespace views
