// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/popup_message.h"

#include "ash/wm/window_animations.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/insets.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
const int kMessageTopBottomMargin = 10;
const int kMessageLeftRightMargin = 10;
const int kMessageMinHeight = 29 - 2 * kMessageTopBottomMargin;
const SkColor kMessageTextColor = SkColorSetRGB(0x22, 0x22, 0x22);

// The maximum width of the Message bubble.  Borrowed the value from
// ash/message/message_controller.cc
const int kMessageMaxWidth = 250;

// The offset for the Message bubble - making sure that the bubble is flush
// with the shelf. The offset includes the arrow size in pixels as well as
// the activation bar and other spacing elements.
const int kArrowOffsetLeftRight = 11;
const int kArrowOffsetTopBottom = 7;

// The number of pixels between the icon and the text.
const int kHorizontalPopupPaddingBetweenItems = 10;

// The number of pixels between the text items.
const int kVerticalPopupPaddingBetweenItems = 10;
}  // namespace

// The implementation of Message of the launcher.
class PopupMessage::MessageBubble : public views::BubbleDelegateView {
 public:
  MessageBubble(const base::string16& caption,
                const base::string16& message,
                IconType message_type,
                views::View* anchor,
                views::BubbleBorder::Arrow arrow_orientation,
                const gfx::Size& size_override,
                int arrow_offset);

  void Close();

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Each component (width/height) can force a size override for that component
  // if not 0.
  gfx::Size size_override_;

  DISALLOW_COPY_AND_ASSIGN(MessageBubble);
};

PopupMessage::MessageBubble::MessageBubble(const base::string16& caption,
                                           const base::string16& message,
                                           IconType message_type,
                                           views::View* anchor,
                                           views::BubbleBorder::Arrow arrow,
                                           const gfx::Size& size_override,
                                           int arrow_offset)
    : views::BubbleDelegateView(anchor, arrow),
      size_override_(size_override) {
  gfx::Insets insets = gfx::Insets(kArrowOffsetTopBottom,
                                   kArrowOffsetLeftRight,
                                   kArrowOffsetTopBottom,
                                   kArrowOffsetLeftRight);
  // An anchor can have an asymmetrical border for spacing reasons. Adjust the
  // anchor location for this.
  if (anchor->border())
    insets += anchor->border()->GetInsets();

  set_anchor_view_insets(insets);
  set_close_on_esc(false);
  set_close_on_deactivate(false);
  set_use_focusless(true);
  set_accept_events(false);

  set_margins(gfx::Insets(kMessageTopBottomMargin, kMessageLeftRightMargin,
                          kMessageTopBottomMargin, kMessageLeftRightMargin));
  set_shadow(views::BubbleBorder::SMALL_SHADOW);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                        kHorizontalPopupPaddingBetweenItems));

  // Here is the layout:
  //         arrow_offset (if not 0)
  //       |-------------|
  //       |             ^
  //       +-------------------------------------------------+
  //      -|                                                 |-
  //  icon |  [!]  Caption in bold which can be multi line   | caption_label
  //      -|                                                 |-
  //       |       Message text which can be multi line      | message_label
  //       |       as well.                                  |
  //       |                                                 |-
  //       +-------------------------------------------------+
  //             |------------details container--------------|
  // Note that the icon, caption and massage are optional.

  // Add the icon to the first column (if there is one).
  if (message_type != ICON_NONE) {
    views::ImageView* icon = new views::ImageView();
    icon->SetImage(
        bundle.GetImageNamed(IDR_AURA_WARNING_ICON).ToImageSkia());
    icon->SetVerticalAlignment(views::ImageView::LEADING);
    AddChildView(icon);
  }

  // Create a container for the text items and use it as second column.
  views::View* details = new views::View();
  AddChildView(details);
  details->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kVerticalPopupPaddingBetweenItems));

  // The caption label.
  if (!caption.empty()) {
    views::Label* caption_label = new views::Label(caption);
    caption_label->SetMultiLine(true);
    caption_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    caption_label->SetFont(bundle.GetFont(ui::ResourceBundle::BoldFont));
    caption_label->SetEnabledColor(kMessageTextColor);
    details->AddChildView(caption_label);
  }

  // The message label.
  if (!message.empty()) {
    views::Label* message_label = new views::Label(message);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_label->SetEnabledColor(kMessageTextColor);
    details->AddChildView(message_label);
  }
  views::BubbleDelegateView::CreateBubble(this);

  // Change the arrow offset if needed.
  if (arrow_offset) {
    // With the creation of the bubble, the bubble got already placed (and
    // possibly re-oriented to fit on the screen). Since it is not possible to
    // set the arrow offset before the creation, we need to set the offset,
    // and the orientation variables again and force a re-placement.
    GetBubbleFrameView()->bubble_border()->set_arrow_offset(arrow_offset);
    GetBubbleFrameView()->bubble_border()->set_arrow(arrow);
    SetAlignment(views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR);
  }
}

void PopupMessage::MessageBubble::Close() {
  if (GetWidget())
    GetWidget()->Close();
}

gfx::Size PopupMessage::MessageBubble::GetPreferredSize() {
  gfx::Size pref_size = views::BubbleDelegateView::GetPreferredSize();
  // Override the size with either the provided size or adjust it to not
  // violate our minimum / maximum sizes.
  if (size_override_.height())
    pref_size.set_height(size_override_.height());
  else if (pref_size.height() < kMessageMinHeight)
    pref_size.set_height(kMessageMinHeight);

  if (size_override_.width())
    pref_size.set_width(size_override_.width());
  else if (pref_size.width() > kMessageMaxWidth)
    pref_size.set_width(kMessageMaxWidth);

  return pref_size;
}

PopupMessage::PopupMessage(const base::string16& caption,
                           const base::string16& message,
                           IconType message_type,
                           views::View* anchor,
                           views::BubbleBorder::Arrow arrow,
                           const gfx::Size& size_override,
                           int arrow_offset)
    : view_(NULL) {
  view_ = new MessageBubble(
      caption, message, message_type, anchor, arrow, size_override,
      arrow_offset);
  widget_ = view_->GetWidget();

  gfx::NativeView native_view = widget_->GetNativeView();
  views::corewm::SetWindowVisibilityAnimationType(
      native_view, views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  views::corewm::SetWindowVisibilityAnimationTransition(
      native_view, views::corewm::ANIMATE_HIDE);
  view_->GetWidget()->Show();
}

PopupMessage::~PopupMessage() {
  CancelHidingAnimation();
  Close();
}

void PopupMessage::Close() {
  if (view_) {
    view_->Close();
    view_ = NULL;
    widget_ = NULL;
  }
}

void PopupMessage::CancelHidingAnimation() {
  if (!widget_ || !widget_->GetNativeView())
    return;

  gfx::NativeView native_view = widget_->GetNativeView();
  views::corewm::SetWindowVisibilityAnimationTransition(
      native_view, views::corewm::ANIMATE_NONE);
}

}  // namespace ash
