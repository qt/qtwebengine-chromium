// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class DockedWindowLayoutManagerTest
    : public test::AshTestBase,
      public testing::WithParamInterface<aura::client::WindowType> {
 public:
  DockedWindowLayoutManagerTest() : window_type_(GetParam()) {}
  virtual ~DockedWindowLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedWindows);
    AshTestBase::SetUp();
    UpdateDisplay("600x600");
    ASSERT_TRUE(test::TestShelfDelegate::instance());

    shelf_view_test_.reset(new test::ShelfViewTestAPI(
        test::LauncherTestAPI(Launcher::ForPrimaryDisplay()).shelf_view()));
    shelf_view_test_->SetAnimationDuration(1);
  }

 protected:
  enum DockedEdge {
    DOCKED_EDGE_NONE,
    DOCKED_EDGE_LEFT,
    DOCKED_EDGE_RIGHT,
  };

  int min_dock_gap() const { return DockedWindowLayoutManager::kMinDockGap; }
  int ideal_width() const { return DockedWindowLayoutManager::kIdealWidth; }
  int docked_width(const DockedWindowLayoutManager* layout_manager) const {
    return layout_manager->docked_width_;
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, window_type_, 0, bounds);
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      test::TestShelfDelegate* shelf_delegate =
          test::TestShelfDelegate::instance();
      shelf_delegate->AddLauncherItem(window);
      PanelLayoutManager* manager =
          static_cast<PanelLayoutManager*>(GetPanelContainer(window)->
              layout_manager());
      manager->Relayout();
    }
    return window;
  }

  aura::Window* CreateTestWindowWithDelegate(
      const gfx::Rect& bounds,
      aura::test::TestWindowDelegate* delegate) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        delegate, window_type_, 0, bounds);
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      test::TestShelfDelegate* shelf_delegate =
          test::TestShelfDelegate::instance();
      shelf_delegate->AddLauncherItem(window);
      PanelLayoutManager* manager =
          static_cast<PanelLayoutManager*>(GetPanelContainer(window)->
              layout_manager());
      manager->Relayout();
    }
    return window;
  }

  aura::Window* GetPanelContainer(aura::Window* panel) {
    return Shell::GetContainer(panel->GetRootWindow(),
                               internal::kShellWindowId_PanelContainer);
  }

  static WindowResizer* CreateSomeWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return CreateWindowResizer(
        window,
        point_in_parent,
        window_component,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE).release();
  }

  void DragStart(aura::Window* window) {
    DragStartAtOffsetFromwindowOrigin(window, 0, 0);
  }

  void DragStartAtOffsetFromwindowOrigin(aura::Window* window,
                                         int dx, int dy) {
    initial_location_in_parent_ =
        window->bounds().origin() + gfx::Vector2d(dx, dy);
    resizer_.reset(CreateSomeWindowResizer(window,
                                           initial_location_in_parent_,
                                           HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void DragMove(int dx, int dy) {
    resizer_->Drag(initial_location_in_parent_ + gfx::Vector2d(dx, dy), 0);
  }

  void DragEnd() {
    resizer_->CompleteDrag(0);
    resizer_.reset();
  }

  void DragRevert() {
    resizer_->RevertDrag();
    resizer_.reset();
  }

  // Panels are parented by panel container during drags.
  // Docked windows are parented by dock container during drags.
  // All other windows that we are testing here have default container as a
  // parent.
  int CorrectContainerIdDuringDrag() {
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL)
      return internal::kShellWindowId_PanelContainer;
    return internal::kShellWindowId_DockedContainer;
  }

  // Test dragging the window vertically (to detach if it is a panel) and then
  // horizontally to the edge with an added offset from the edge of |dx|.
  void DragRelativeToEdge(DockedEdge edge,
                          aura::Window* window,
                          int dx) {
    DragVerticallyAndRelativeToEdge(
        edge,
        window,
        dx,
        window_type_ == aura::client::WINDOW_TYPE_PANEL ? -100 : 20);
  }

  void DragToVerticalPositionAndToEdge(DockedEdge edge,
                                       aura::Window* window,
                                       int y) {
    DragToVerticalPositionRelativeToEdge(edge, window, 0, y);
  }

  void DragToVerticalPositionRelativeToEdge(DockedEdge edge,
                                            aura::Window* window,
                                            int dx,
                                            int y) {
    gfx::Rect initial_bounds = window->GetBoundsInScreen();
    DragVerticallyAndRelativeToEdge(edge, window, dx, y - initial_bounds.y());
  }

  // Detach if our window is a panel, then drag it vertically by |dy| and
  // horizontally to the edge with an added offset from the edge of |dx|.
  void DragVerticallyAndRelativeToEdge(DockedEdge edge,
                                       aura::Window* window,
                                       int dx, int dy) {
    gfx::Rect initial_bounds = window->GetBoundsInScreen();
    // avoid snap by clicking away from the border
    ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(window, 25, 5));

    gfx::Rect work_area =
        Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
    gfx::Point initial_location_in_screen = initial_location_in_parent_;
    wm::ConvertPointToScreen(window->parent(), &initial_location_in_screen);
    // Drag the window left or right to the edge (or almost to it).
    if (edge == DOCKED_EDGE_LEFT)
      dx += work_area.x() - initial_location_in_screen.x();
    else if (edge == DOCKED_EDGE_RIGHT)
      dx += work_area.right() - 1 - initial_location_in_screen.x();
    DragMove(dx, dy);
    EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
    // Release the mouse and the panel should be attached to the dock.
    DragEnd();

    // x-coordinate can get adjusted by snapping or sticking.
    // y-coordinate could be changed by possible automatic layout if docked.
    if (window->parent()->id() != internal::kShellWindowId_DockedContainer &&
        !wm::GetWindowState(window)->HasRestoreBounds()) {
      EXPECT_EQ(initial_bounds.y() + dy, window->GetBoundsInScreen().y());
    }
  }

 private:
  scoped_ptr<WindowResizer> resizer_;
  scoped_ptr<test::ShelfViewTestAPI> shelf_view_test_;
  aura::client::WindowType window_type_;

  // Location at start of the drag in |window->parent()|'s coordinates.
  gfx::Point initial_location_in_parent_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowLayoutManagerTest);
};

// Tests that a created window is successfully added to the dock
// layout manager.
TEST_P(DockedWindowLayoutManagerTest, AddOneWindow) {
  if (!SupportsHostWindowResize())
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and docked at the right edge.
  // Its width should shrink or grow to ideal width.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(ideal_width(), window->bounds().width());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Tests that with a window docked on the left the auto-placing logic in
// RearrangeVisibleWindowOnShow places windows flush with work area edges.
TEST_P(DockedWindowLayoutManagerTest, AutoPlacingLeft) {
  if (!SupportsHostWindowResize())
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), 0);

  // The window should be attached and snapped to the right side of the screen.
  EXPECT_EQ(window->GetRootWindow()->bounds().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  DockedWindowLayoutManager* manager = static_cast<DockedWindowLayoutManager*>(
      window->parent()->layout_manager());

  // Create two additional windows and test their auto-placement
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  gfx::Rect desktop_area = window1->parent()->bounds();
  wm::GetWindowState(window1.get())->set_window_position_managed(true);
  window1->Hide();
  window1->SetBounds(gfx::Rect(250, 32, 231, 320));
  window1->Show();
  // |window1| should be centered in work area.
  EXPECT_EQ(base::IntToString(
      docked_width(manager) + min_dock_gap() +
      (desktop_area.width() - docked_width(manager) -
       min_dock_gap() - window1->bounds().width()) / 2) +
      ",32 231x320", window1->bounds().ToString());

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  wm::GetWindowState(window2.get())->set_window_position_managed(true);
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window2->Hide();
  window2->SetBounds(gfx::Rect(250, 48, 150, 300));
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ(
      base::IntToString(docked_width(manager) + min_dock_gap()) +
      ",32 231x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::IntToString(
          desktop_area.width() - window2->bounds().width()) +
      ",48 150x300", window2->bounds().ToString());
}

// Tests that with a window docked on the right the auto-placing logic in
// RearrangeVisibleWindowOnShow places windows flush with work area edges.
TEST_P(DockedWindowLayoutManagerTest, AutoPlacingRight) {
  if (!SupportsHostWindowResize())
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right side of the screen.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  DockedWindowLayoutManager* manager = static_cast<DockedWindowLayoutManager*>(
      window->parent()->layout_manager());

  // Create two additional windows and test their auto-placement
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  gfx::Rect desktop_area = window1->parent()->bounds();
  wm::GetWindowState(window1.get())->set_window_position_managed(true);
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 231, 320));
  window1->Show();

  // |window1| should be centered in work area.
  EXPECT_EQ(base::IntToString(
      (desktop_area.width() - docked_width(manager) -
       min_dock_gap() - window1->bounds().width()) / 2) +
      ",32 231x320", window1->bounds().ToString());

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  wm::GetWindowState(window2.get())->set_window_position_managed(true);
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ("0,32 231x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::IntToString(
          desktop_area.width() - window2->bounds().width() -
          docked_width(manager) - min_dock_gap()) +
      ",48 256x512", window2->bounds().ToString());
}

// Tests that with a window docked on the right the auto-placing logic in
// RearrangeVisibleWindowOnShow places windows flush with work area edges.
// Test case for the secondary screen.
TEST_P(DockedWindowLayoutManagerTest, AutoPlacingRightSecondScreen) {
  if (!SupportsMultipleDisplays() || !SupportsHostWindowResize())
    return;

  // Create two screen layout.
  UpdateDisplay("600x600,600x600");

  gfx::Rect bounds(600, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Drag pointer to the right edge of the second screen.
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right side of the screen.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  DockedWindowLayoutManager* manager = static_cast<DockedWindowLayoutManager*>(
      window->parent()->layout_manager());

  // Create two additional windows and test their auto-placement
  bounds = gfx::Rect(616, 32, 231, 320);
  scoped_ptr<aura::Window> window1(
      CreateTestWindowInShellWithDelegate(NULL, 1, bounds));
  gfx::Rect desktop_area = window1->parent()->bounds();
  wm::GetWindowState(window1.get())->set_window_position_managed(true);
  window1->Hide();
  window1->Show();

  // |window1| should be centered in work area.
  EXPECT_EQ(base::IntToString(
      600 + (desktop_area.width() - docked_width(manager) -
          min_dock_gap() - window1->bounds().width()) / 2) +
      ",32 231x320", window1->GetBoundsInScreen().ToString());

  bounds = gfx::Rect(632, 48, 256, 512);
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithDelegate(NULL, 2, bounds));
  wm::GetWindowState(window2.get())->set_window_position_managed(true);
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window2->Hide();
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ("600,32 231x320", window1->GetBoundsInScreen().ToString());
  EXPECT_EQ(
      base::IntToString(
          600 + desktop_area.width() - window2->bounds().width() -
          docked_width(manager) - min_dock_gap()) +
      ",48 256x512", window2->GetBoundsInScreen().ToString());
}

// Adds two windows and tests that the gaps are evenly distributed.
TEST_P(DockedWindowLayoutManagerTest, AddTwoWindows) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);

  // The windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Test that the gaps differ at most by a single pixel.
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  int gap1 = w1->GetBoundsInScreen().y();
  int gap2 = w2->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  int gap3 = work_area.bottom() - w2->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
}

// Adds two non-overlapping windows and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsDragging) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);

  // The windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Drag w2 above w1.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w2.get(), 0, 20));
  DragMove(0, -w2->bounds().height() / 2 - min_dock_gap() - 1);
  DragEnd();

  // Test the new windows order and that the gaps differ at most by a pixel.
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  int gap1 = w2->GetBoundsInScreen().y() - work_area.y();
  int gap2 = w1->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  int gap3 = work_area.bottom() - w1->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
}

// Adds three overlapping windows and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsDragging) {
  if (!SupportsHostWindowResize())
    return;
  UpdateDisplay("600x1000");

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 500);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 220, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 600);

  // All windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(w3->GetRootWindow()->bounds().right(),
            w3->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w3->parent()->id());

  // Test that the top and bottom windows are clamped in work area and
  // that the gaps between the windows differ at most by a pixel.
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  int gap1 = w1->GetBoundsInScreen().y() - work_area.y();
  int gap2 = w2->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  int gap3 = w3->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  int gap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
  EXPECT_NEAR(gap4, min_dock_gap(), 1);

  // Drag w1 below the point where w1 and w2 would swap places. This point is
  // half way between the tops of those two windows.
  // A bit more vertical drag is needed to account for a window bounds changing
  // to its restore bounds during the drag.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 0, 20));
  DragMove(0, min_dock_gap() + w2->bounds().height() / 2 + 10);

  // During the drag the windows get rearranged and the top and the bottom
  // should be limited by the work area + |kMinDockGap|.
  EXPECT_EQ(min_dock_gap(), w2->GetBoundsInScreen().y());
  EXPECT_GT(w1->GetBoundsInScreen().y(), w2->GetBoundsInScreen().y());
  EXPECT_EQ(min_dock_gap(),
            work_area.bottom() - w3->GetBoundsInScreen().bottom());
  DragEnd();

  // Test the new windows order and that the gaps differ at most by a pixel.
  gap1 = w2->GetBoundsInScreen().y() - work_area.y();
  gap2 = w1->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  gap3 = w3->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  gap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
  EXPECT_NEAR(gap4, min_dock_gap(), 1);
}

// Adds three windows in bottom display and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsDraggingSecondScreen) {
  if (!SupportsMultipleDisplays() || !SupportsHostWindowResize())
    return;

  // Create two screen vertical layout.
  UpdateDisplay("600x1000,600x1000");
  // Layout the secondary display to the bottom of the primary.
  DisplayLayout layout(DisplayLayout::BOTTOM, 0);
  ASSERT_GT(Shell::GetScreen()->GetNumDisplays(), 1);
  Shell::GetInstance()->display_manager()->
      SetLayoutForCurrentDisplays(layout);

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 1000, 201, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 1000 + 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 1000, 210, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 1000 + 500);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 1000, 220, 310)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 1000 + 600);

  // All windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(w3->GetRootWindow()->bounds().right(),
            w3->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w3->parent()->id());

  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  // Test that the top and bottom windows are clamped in work area and
  // that the overlaps between the windows differ at most by a pixel.
  int gap1 = w1->GetBoundsInScreen().y() - work_area.y();
  int gap2 = w2->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  int gap3 = w3->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  int gap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
  EXPECT_NEAR(gap4, min_dock_gap(), 1);

  // Drag w1 below the point where w1 and w2 would swap places. This point is
  // half way between the tops of those two windows.
  // A bit more vertical drag is needed to account for a window bounds changing
  // to its restore bounds during the drag.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 0, 20));
  DragMove(0, min_dock_gap() + w2->bounds().height() / 2 + 10);

  // During the drag the windows get rearranged and the top and the bottom
  // should be limited by the work area + |kMinDockGap|.
  EXPECT_EQ(work_area.y() + min_dock_gap(), w2->GetBoundsInScreen().y());
  EXPECT_GT(w1->GetBoundsInScreen().y(), w2->GetBoundsInScreen().y());
  EXPECT_EQ(min_dock_gap(),
            work_area.bottom() - w3->GetBoundsInScreen().bottom());
  DragEnd();

  // Test the new windows order and that the overlaps differ at most by a pixel.
  gap1 = w2->GetBoundsInScreen().y() - work_area.y();
  gap2 = w1->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  gap3 = w3->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  gap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_NEAR(gap1, min_dock_gap(), 1);
  EXPECT_NEAR(gap2, min_dock_gap(), 1);
  EXPECT_NEAR(gap3, min_dock_gap(), 1);
  EXPECT_NEAR(gap4, min_dock_gap(), 1);
}

// Tests that a second window added to the dock is resized to match.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsWidthNew) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 280, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // The first window should get resized to ideal width.
  EXPECT_EQ(ideal_width(), w1->bounds().width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);
  // The second window should get resized to the existing dock.
  EXPECT_EQ(ideal_width(), w2->bounds().width());
}

// Tests that a first non-resizable window added to the dock is not resized.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsWidthNonResizableFirst) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  w1->SetProperty(aura::client::kCanResizeKey, false);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 280, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // The first window should not get resized.
  EXPECT_EQ(201, w1->bounds().width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);
  // The second window should get resized to the first window's width.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
}

// Tests that a second non-resizable window added to the dock is not resized.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsWidthNonResizableSecond) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 280, 202)));
  w2->SetProperty(aura::client::kCanResizeKey, false);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // The first window should get resized to ideal width.
  EXPECT_EQ(ideal_width(), w1->bounds().width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);
  // The second window should not get resized.
  EXPECT_EQ(280, w2->bounds().width());

  // The first window should get resized again - to match the second window.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
}

// Test that restrictions on minimum and maximum width of windows are honored.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsWidthRestrictions) {
  if (!SupportsHostWindowResize())
    return;

  aura::test::TestWindowDelegate delegate1;
  delegate1.set_maximum_size(gfx::Size(240, 0));
  scoped_ptr<aura::Window> w1(CreateTestWindowWithDelegate(
      gfx::Rect(0, 0, 201, 201), &delegate1));
  aura::test::TestWindowDelegate delegate2;
  delegate2.set_minimum_size(gfx::Size(260, 0));
  scoped_ptr<aura::Window> w2(CreateTestWindowWithDelegate(
      gfx::Rect(0, 0, 280, 202), &delegate2));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // The first window should get resized to its maximum width.
  EXPECT_EQ(240, w1->bounds().width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);
  // The second window should get resized to its minimum width.
  EXPECT_EQ(260, w2->bounds().width());

  // The first window should be centered relative to the second.
  EXPECT_EQ(w1->bounds().CenterPoint().x(), w2->bounds().CenterPoint().x());
}

// Test that restrictions on minimum width of windows are honored.
TEST_P(DockedWindowLayoutManagerTest, WidthMoreThanMax) {
  if (!SupportsHostWindowResize())
    return;

  aura::test::TestWindowDelegate delegate;
  delegate.set_minimum_size(gfx::Size(400, 0));
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      gfx::Rect(0, 0, 400, 201), &delegate));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, window.get(), 20);

  // Secondary drag ensures that we are testing the minimum size restriction
  // and not just failure to get past the tiling step in SnapSizer.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(window.get(),
                                                            25, 5));
  DragMove(150,0);
  DragEnd();

  // The window should not get docked even though it is dragged past the edge.
  EXPECT_NE(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_NE(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Docks three windows and tests that the very first window gets minimized.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsMinimize) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 200);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 220, 204)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 300);

  // The last two windows should be attached and snapped to the right edge.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(w3->GetRootWindow()->bounds().right(),
            w3->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w3->parent()->id());

  // The first window should get minimized but parented by the dock container.
  EXPECT_TRUE(wm::GetWindowState(w1.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(w2.get())->IsNormalShowState());
  EXPECT_TRUE(wm::GetWindowState(w3.get())->IsNormalShowState());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
}

// Docks up to three windows and tests that they split vertical space.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsSplitHeightEvenly) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("600x1000");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 200);

  // The two windows should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // The two windows should be same size vertically and almost 1/2 of work area.
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  EXPECT_NEAR(w1->GetBoundsInScreen().height(),
              w2->GetBoundsInScreen().height(),
              1);
  EXPECT_NEAR(work_area.height() / 2, w1->GetBoundsInScreen().height(),
              min_dock_gap() * 2);

  // Create and dock the third window.
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 220, 204)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 300);

  // All three windows should be docked and snapped to the right edge.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w3->parent()->id());

  // All windows should be near same size vertically and about 1/3 of work area.
  EXPECT_NEAR(w1->GetBoundsInScreen().height(),
              w2->GetBoundsInScreen().height(),
              1);
  EXPECT_NEAR(w2->GetBoundsInScreen().height(),
              w3->GetBoundsInScreen().height(),
              1);
  EXPECT_NEAR(work_area.height() / 3, w1->GetBoundsInScreen().height(),
              min_dock_gap() * 2);
}

// Docks two windows and tests that restrictions on vertical size are honored.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsHeightRestrictions) {
  if (!SupportsHostWindowResize())
    return;

  // The first window is fixed height.
  aura::test::TestWindowDelegate delegate1;
  delegate1.set_minimum_size(gfx::Size(0, 300));
  delegate1.set_maximum_size(gfx::Size(0, 300));
  scoped_ptr<aura::Window> w1(CreateTestWindowWithDelegate(
      gfx::Rect(0, 0, 201, 300), &delegate1));
  // The second window has maximum height.
  aura::test::TestWindowDelegate delegate2;
  delegate2.set_maximum_size(gfx::Size(0, 100));
  scoped_ptr<aura::Window> w2(CreateTestWindowWithDelegate(
      gfx::Rect(0, 0, 280, 90), &delegate2));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 200);

  // The two windows should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // The two windows should have their heights restricted.
  EXPECT_EQ(300, w1->GetBoundsInScreen().height());
  EXPECT_EQ(100, w2->GetBoundsInScreen().height());

  // w1 should be more than half of the work area height (even with a margin).
  // w2 should be less than half of the work area height (even with a margin).
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  EXPECT_GT(w1->GetBoundsInScreen().height(), work_area.height() / 2 + 10);
  EXPECT_LT(w2->GetBoundsInScreen().height(), work_area.height() / 2 - 10);
}

// Tests run twice - on both panels and normal windows
INSTANTIATE_TEST_CASE_P(NormalOrPanel,
                        DockedWindowLayoutManagerTest,
                        testing::Values(aura::client::WINDOW_TYPE_NORMAL,
                                        aura::client::WINDOW_TYPE_PANEL));
}  // namespace internal
}  // namespace ash
