// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class CustomButton;
class Widget;
}

namespace ash {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
class ASH_EXPORT FrameCaptionButtonContainerView
    : public views::View,
      public views::ButtonListener {
 public:
  static const char kViewClassName[];

  // Whether the frame can be minimized (either via the maximize/restore button
  // or via a dedicated button).
  enum MinimizeAllowed {
    MINIMIZE_ALLOWED,
    MINIMIZE_DISALLOWED
  };
  enum HeaderStyle {
    // Dialogs, panels, packaged apps, tabbed maximized/fullscreen browser
    // windows.
    HEADER_STYLE_SHORT,

    // Restored tabbed browser windows, popups for browser windows, restored
    // hosted app windows, popups for hosted app windows.
    HEADER_STYLE_TALL,

    // AppNonClientFrameViewAsh.
    HEADER_STYLE_MAXIMIZED_HOSTED_APP
  };

  // |frame| is the views::Widget that the caption buttons act on.
  // |minimize_allowed| indicates whether the frame can be minimized (either via
  // the maximize/restore button or via a dedicated button).
  FrameCaptionButtonContainerView(views::Widget* frame,
                                  MinimizeAllowed minimize_allowed);
  virtual ~FrameCaptionButtonContainerView();

  // For testing.
  class TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {
    }

    views::CustomButton* minimize_button() const {
      return container_view_->minimize_button_;
    }

    views::CustomButton* size_button() const {
      return container_view_->size_button_;
    }

    views::CustomButton* close_button() const {
      return container_view_->close_button_;
    }

   private:
    FrameCaptionButtonContainerView* container_view_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Determines the window HT* code for the caption button at |point|. Returns
  // HTNOWHERE if |point| is not over any of the caption buttons. |point| must
  // be in the coordinates of the FrameCaptionButtonContainerView.
  int NonClientHitTest(const gfx::Point& point) const;

  // Sets the header style.
  void set_header_style(HeaderStyle header_style) {
    header_style_ = header_style;
  }

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  friend class FrameCaptionButtonContainerViewTest;

  // Returns the distance between buttons which are next to each other. A
  // negative value is returned if the buttons overlap.
  int GetDistanceBetweenButtons() const;

  // Returns the inset of the leftmost visible button from the view's border
  // (if any).
  int GetLeftInset() const;

  // Returns the inset of the rightmost visible button from the view's border
  // (if any).
  int GetRightInset() const;

  // views::ButtonListener override:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Methods specific to normal button style -----------------------------------
  //
  // Sets the images for a button based on the given ids.
  void SetButtonImages(views::CustomButton* button,
                       int normal_image_id,
                       int hot_image_id,
                       int pushed_image_id);

  // The widget that the buttons act on.
  views::Widget* frame_;

  // The close button separator.
  gfx::ImageSkia button_separator_;

  HeaderStyle header_style_;

  // The buttons. In the normal button style, at most one of |minimize_button_|
  // and |size_button_| is visible.
  views::CustomButton* minimize_button_;
  views::CustomButton* size_button_;
  views::CustomButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerView);
};

}  // namesapace ash

#endif  // ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
