// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/single_split_view_example.h"

#include "ui/views/background.h"
#include "ui/views/controls/single_split_view.h"
#include "ui/views/layout/grid_layout.h"

namespace views {
namespace examples {
namespace {

// SingleSplitView's content, which draws gradient color on background.
class SplittedView : public View {
 public:
  SplittedView();
  virtual ~SplittedView();

  void SetColor(SkColor from, SkColor to);

 private:
  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SplittedView);
};

SplittedView::SplittedView() {
  SetColor(SK_ColorRED, SK_ColorGREEN);
}

SplittedView::~SplittedView() {
}

void SplittedView::SetColor(SkColor from, SkColor to) {
  set_background(Background::CreateVerticalGradientBackground(from, to));
}

gfx::Size SplittedView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

gfx::Size SplittedView::GetMinimumSize() {
  return gfx::Size(10, 10);
}

void SplittedView::Layout() {
  SizeToPreferredSize();
}

}  // namespace

SingleSplitViewExample::SingleSplitViewExample()
    : ExampleBase("Single Split View") {
}

SingleSplitViewExample::~SingleSplitViewExample() {
}

void SingleSplitViewExample::CreateExampleView(View* container) {
  SplittedView* splitted_view_1 = new SplittedView;
  SplittedView* splitted_view_2 = new SplittedView;

  splitted_view_1->SetColor(SK_ColorYELLOW, SK_ColorCYAN);

  single_split_view_ = new SingleSplitView(
      splitted_view_1, splitted_view_2,
      SingleSplitView::HORIZONTAL_SPLIT,
      this);

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(single_split_view_);
}

bool SingleSplitViewExample::SplitHandleMoved(SingleSplitView* sender) {
  PrintStatus("Splitter moved");
  return true;
}

}  // namespace examples
}  // namespace views
