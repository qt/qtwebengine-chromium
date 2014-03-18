// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"  // ASCIIToUTF16
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Default window position.
const int kWindowLeft = 170;
const int kWindowTop = 200;

// Default window size.
const int kWindowWidth = 400;
const int kWindowHeight = 400;

// A window showing samples of commonly used widgets.
class WidgetsWindow : public views::WidgetDelegateView {
 public:
  WidgetsWindow();
  virtual ~WidgetsWindow();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual bool CanResize() const OVERRIDE;

 private:
  views::LabelButton* button_;
  views::LabelButton* disabled_button_;
  views::Checkbox* checkbox_;
  views::Checkbox* checkbox_disabled_;
  views::Checkbox* checkbox_checked_;
  views::Checkbox* checkbox_checked_disabled_;
  views::RadioButton* radio_button_;
  views::RadioButton* radio_button_disabled_;
  views::RadioButton* radio_button_selected_;
  views::RadioButton* radio_button_selected_disabled_;
};

WidgetsWindow::WidgetsWindow()
    : button_(new views::LabelButton(NULL, ASCIIToUTF16("Button"))),
      disabled_button_(
          new views::LabelButton(NULL, ASCIIToUTF16("Disabled button"))),
      checkbox_(new views::Checkbox(ASCIIToUTF16("Checkbox"))),
      checkbox_disabled_(new views::Checkbox(
          ASCIIToUTF16("Checkbox disabled"))),
      checkbox_checked_(new views::Checkbox(ASCIIToUTF16("Checkbox checked"))),
      checkbox_checked_disabled_(new views::Checkbox(
          ASCIIToUTF16("Checkbox checked disabled"))),
      radio_button_(new views::RadioButton(ASCIIToUTF16("Radio button"), 0)),
      radio_button_disabled_(new views::RadioButton(
          ASCIIToUTF16("Radio button disabled"), 0)),
      radio_button_selected_(new views::RadioButton(
          ASCIIToUTF16("Radio button selected"), 0)),
      radio_button_selected_disabled_(new views::RadioButton(
          ASCIIToUTF16("Radio button selected disabled"), 1)) {
  button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  AddChildView(button_);
  disabled_button_->SetEnabled(false);
  disabled_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  AddChildView(disabled_button_);
  AddChildView(checkbox_);
  checkbox_disabled_->SetEnabled(false);
  AddChildView(checkbox_disabled_);
  checkbox_checked_->SetChecked(true);
  AddChildView(checkbox_checked_);
  checkbox_checked_disabled_->SetChecked(true);
  checkbox_checked_disabled_->SetEnabled(false);
  AddChildView(checkbox_checked_disabled_);
  AddChildView(radio_button_);
  radio_button_disabled_->SetEnabled(false);
  AddChildView(radio_button_disabled_);
  radio_button_selected_->SetChecked(true);
  AddChildView(radio_button_selected_);
  radio_button_selected_disabled_->SetChecked(true);
  radio_button_selected_disabled_->SetEnabled(false);
  AddChildView(radio_button_selected_disabled_);
}

WidgetsWindow::~WidgetsWindow() {
}

void WidgetsWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
}

void WidgetsWindow::Layout() {
  const int kVerticalPad = 5;
  int left = 5;
  int top = kVerticalPad;
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    gfx::Size preferred = view->GetPreferredSize();
    view->SetBounds(left, top, preferred.width(), preferred.height());
    top += preferred.height() + kVerticalPad;
  }
}

gfx::Size WidgetsWindow::GetPreferredSize() {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

views::View* WidgetsWindow::GetContentsView() {
  return this;
}

base::string16 WidgetsWindow::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Widgets");
}

bool WidgetsWindow::CanResize() const {
  return true;
}

}  // namespace

namespace ash {
namespace shell {

void CreateWidgetsWindow() {
  gfx::Rect bounds(kWindowLeft, kWindowTop, kWindowWidth, kWindowHeight);
  views::Widget* widget =
      views::Widget::CreateWindowWithContextAndBounds(
          new WidgetsWindow, Shell::GetPrimaryRootWindow(), bounds);
  widget->GetNativeView()->SetName("WidgetsWindow");
  widget->Show();
}

}  // namespace shell
}  // namespace ash
