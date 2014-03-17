// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_editing_menu.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kMenuCommands[] = {IDS_APP_CUT,
                             IDS_APP_COPY,
                             IDS_APP_PASTE};
const int kSpacingBetweenButtons = 2;
const int kButtonSeparatorColor = SkColorSetARGB(13, 0, 0, 0);
const int kMenuButtonHeight = 38;
const int kMenuButtonWidth = 63;
const int kMenuMargin = 1;

const char* kEllipsesButtonText = "...";
const int kEllipsesButtonTag = -1;
}  // namespace

namespace views {

TouchEditingMenuView::TouchEditingMenuView(
    TouchEditingMenuController* controller,
    gfx::Rect anchor_rect,
    gfx::NativeView context)
    : BubbleDelegateView(NULL, views::BubbleBorder::BOTTOM_CENTER),
      controller_(controller) {
  SetAnchorRect(anchor_rect);
  set_shadow(views::BubbleBorder::SMALL_SHADOW);
  set_parent_window(context);
  set_margins(gfx::Insets(kMenuMargin, kMenuMargin, kMenuMargin, kMenuMargin));
  set_use_focusless(true);
  set_adjust_if_offscreen(true);

  SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0,
      kSpacingBetweenButtons));
  CreateButtons();
  views::BubbleDelegateView::CreateBubble(this);
  GetWidget()->Show();
}

TouchEditingMenuView::~TouchEditingMenuView() {
}

// static
TouchEditingMenuView* TouchEditingMenuView::Create(
    TouchEditingMenuController* controller,
    gfx::Rect anchor_rect,
    gfx::NativeView context) {
  if (controller) {
    for (size_t i = 0; i < arraysize(kMenuCommands); i++) {
      if (controller->IsCommandIdEnabled(kMenuCommands[i]))
        return new TouchEditingMenuView(controller, anchor_rect, context);
    }
  }
  return NULL;
}

void TouchEditingMenuView::Close() {
  if (GetWidget()) {
    controller_ = NULL;
    GetWidget()->Close();
  }
}

void TouchEditingMenuView::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  if (controller_)
    controller_->OnMenuClosed(this);
}

void TouchEditingMenuView::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  if (controller_) {
    if (sender->tag() != kEllipsesButtonTag)
      controller_->ExecuteCommand(sender->tag(), event.flags());
    else
      controller_->OpenContextMenu();
  }
}

void TouchEditingMenuView::OnPaint(gfx::Canvas* canvas) {
  BubbleDelegateView::OnPaint(canvas);

  // Draw separator bars.
  for (int i = 0; i < child_count() - 1; ++i) {
    View* child = child_at(i);
    int x = child->bounds().right() + kSpacingBetweenButtons / 2;
    canvas->FillRect(gfx::Rect(x, 0, 1, child->height()),
        kButtonSeparatorColor);
  }
}

void TouchEditingMenuView::CreateButtons() {
  RemoveAllChildViews(true);
  for (size_t i = 0; i < arraysize(kMenuCommands); i++) {
    int command_id = kMenuCommands[i];
    if (controller_ && controller_->IsCommandIdEnabled(command_id)) {
      Button* button = CreateButton(l10n_util::GetStringUTF16(command_id),
          command_id);
      AddChildView(button);
    }
  }

  // Finally, add ellipses button.
  AddChildView(CreateButton(
      UTF8ToUTF16(kEllipsesButtonText), kEllipsesButtonTag));
  Layout();
}

Button* TouchEditingMenuView::CreateButton(const string16& title, int tag) {
  string16 label = gfx::RemoveAcceleratorChar(title, '&', NULL, NULL);
  LabelButton* button = new LabelButton(this, label);
  button->SetFocusable(true);
  button->set_request_focus_on_press(false);
  gfx::Font font = ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallFont);
  scoped_ptr<LabelButtonBorder> button_border(
      new LabelButtonBorder(button->style()));
  int v_border = (kMenuButtonHeight - font.GetHeight()) / 2;
  int h_border = (kMenuButtonWidth - font.GetStringWidth(label)) / 2;
  button_border->set_insets(
      gfx::Insets(v_border, h_border, v_border, h_border));
  button->set_border(button_border.release());
  button->SetFont(font);
  button->set_tag(tag);
  return button;
}

}  // namespace views
