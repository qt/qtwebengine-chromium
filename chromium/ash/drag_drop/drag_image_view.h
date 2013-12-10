// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_
#define ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_

#include "ui/views/controls/image_view.h"

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// This class allows to show a (native) view always on top of everything. It
// does this by creating a widget and setting the content as the given view. The
// caller can then use this object to freely move / drag it around on the
// desktop in screen coordinates.
class DragImageView : public views::ImageView {
 public:
  explicit DragImageView(gfx::NativeView context);
  virtual ~DragImageView();

  // Sets the bounds of the native widget in screen
  // coordinates.
  // TODO(oshima): Looks like this is root window's
  // coordinate. Change this to screen's coordinate.
  void SetBoundsInScreen(const gfx::Rect& bounds);

  // Sets the position of the native widget.
  void SetScreenPosition(const gfx::Point& position);

  // Gets the image position in screen coordinates.
  gfx::Rect GetBoundsInScreen() const;

  // Sets the visibility of the native widget.
  void SetWidgetVisible(bool visible);

  // Sets the |opacity| of the image view between 0.0 and 1.0.
  void SetOpacity(float opacity);

 private:
  // Overridden from views::ImageView.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  scoped_ptr<views::Widget> widget_;
  gfx::Size widget_size_;

  DISALLOW_COPY_AND_ASSIGN(DragImageView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_
