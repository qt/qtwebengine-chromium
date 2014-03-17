// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
#define UI_VIEWS_LAYOUT_FILL_LAYOUT_H_

#include "base/compiler_specific.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// FillLayout
//  A simple LayoutManager that causes the associated view's one child to be
//  sized to match the bounds of its parent.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT FillLayout : public LayoutManager {
 public:
  FillLayout();
  virtual ~FillLayout();

  // Overridden from LayoutManager:
  virtual void Layout(View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(View* host) OVERRIDE;
  virtual int GetPreferredHeightForWidth(View* host, int width) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
