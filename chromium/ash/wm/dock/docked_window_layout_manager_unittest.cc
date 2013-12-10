// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
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
        ash::switches::kAshEnableStickyEdges);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedWindows);
    AshTestBase::SetUp();
    UpdateDisplay("600x600");
    ASSERT_TRUE(test::TestLauncherDelegate::instance());

    launcher_view_test_.reset(new test::LauncherViewTestAPI(
        test::LauncherTestAPI(Launcher::ForPrimaryDisplay()).launcher_view()));
    launcher_view_test_->SetAnimationDuration(1);
  }

 protected:
  enum DockedEdge {
    DOCKED_EDGE_NONE,
    DOCKED_EDGE_LEFT,
    DOCKED_EDGE_RIGHT,
  };

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL,
        window_type_,
        0,
        bounds);
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      test::TestLauncherDelegate* launcher_delegate =
          test::TestLauncherDelegate::instance();
      launcher_delegate->AddLauncherItem(window);
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
  scoped_ptr<test::LauncherViewTestAPI> launcher_view_test_;
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

  // The window should be attached and snapped to the right side of the screen.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
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
      manager->docked_width_ + DockedWindowLayoutManager::kMinDockGap +
      (desktop_area.width() - manager->docked_width_ -
       DockedWindowLayoutManager::kMinDockGap - window1->bounds().width()) / 2)+
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
      base::IntToString(
          manager->docked_width_ + DockedWindowLayoutManager::kMinDockGap) +
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
      (desktop_area.width() - manager->docked_width_ -
       DockedWindowLayoutManager::kMinDockGap - window1->bounds().width()) / 2)+
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
          manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap) +
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
      600 + (desktop_area.width() - manager->docked_width_ -
       DockedWindowLayoutManager::kMinDockGap - window1->bounds().width()) / 2)+
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
          manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap) +
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
  int gap1 = w1->GetBoundsInScreen().y();
  int gap2 = w2->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  int gap3 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w1.get()).bottom() -
      w2->GetBoundsInScreen().bottom();
  EXPECT_LE(abs(gap1 - gap2), 1);
  EXPECT_LE(abs(gap2 - gap3), 1);
  EXPECT_LE(abs(gap3 - gap1), 1);
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
  DragMove(0, w1->bounds().y() - w2->bounds().y() + 20);
  DragEnd();

  // Test the new windows order and that the gaps differ at most by a pixel.
  int gap1 = w2->GetBoundsInScreen().y();
  int gap2 = w1->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  int gap3 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w1.get()).bottom() -
      w1->GetBoundsInScreen().bottom();
  EXPECT_LE(abs(gap1 - gap2), 1);
  EXPECT_LE(abs(gap2 - gap3), 1);
  EXPECT_LE(abs(gap3 - gap1), 1);
}

// Adds three overlapping windows and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsDragging) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 200);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 220, 204)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 300);

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
  // that the overlaps between the windows differ at most by a pixel.
  int overlap1 = w1->GetBoundsInScreen().y();
  int overlap2 = w1->GetBoundsInScreen().bottom() - w2->GetBoundsInScreen().y();
  int overlap3 = w2->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  int overlap4 =
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom() -
      w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);

  // Drag w1 below the point where w1 and w2 would swap places. This point is
  // half way between the tops of those two windows. Add 1 because w2 is taller.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 0, 20));
  DragMove(0, (w2->bounds().y() - w1->bounds().y()) / 2 + 1);

  // During the drag the windows get rearranged and the top and the bottom
  // should be clamped by the work area.
  EXPECT_EQ(0, w2->GetBoundsInScreen().y());
  EXPECT_GT(w1->GetBoundsInScreen().y(), w2->GetBoundsInScreen().y());
  EXPECT_EQ(ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom(),
            w3->GetBoundsInScreen().bottom());
  DragEnd();

  // Test the new windows order and that the overlaps differ at most by a pixel.
  overlap1 = w2->GetBoundsInScreen().y();
  overlap2 = w2->GetBoundsInScreen().bottom() - w1->GetBoundsInScreen().y();
  overlap3 = w1->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  overlap4 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom() -
      w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);
}

// Adds three windows in bottom display and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsDraggingSecondScreen) {
  if (!SupportsMultipleDisplays() || !SupportsHostWindowResize())
    return;

  // Create two screen vertical layout.
  UpdateDisplay("600x600,600x600");
  // Layout the secondary display to the bottom of the primary.
  DisplayLayout layout(DisplayLayout::BOTTOM, 0);
  ASSERT_GT(Shell::GetScreen()->GetNumDisplays(), 1);
  Shell::GetInstance()->display_controller()->
      SetLayoutForCurrentDisplays(layout);

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 600, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 600 + 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 600, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 600 + 200);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 600, 220, 204)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 600 + 300);

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
  int overlap1 = w1->GetBoundsInScreen().y() - work_area.y();
  int overlap2 = w1->GetBoundsInScreen().bottom() - w2->GetBoundsInScreen().y();
  int overlap3 = w2->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  int overlap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);

  // Drag w1 below the point where w1 and w2 would swap places. This point is
  // half way between the tops of those two windows. Add 1 because w2 is taller.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 0, 20));
  DragMove(0, (w2->bounds().y() - w1->bounds().y()) / 2 + 1);

  // During the drag the windows get rearranged and the top and the bottom
  // should be clamped by the work area.
  EXPECT_EQ(work_area.y(), w2->GetBoundsInScreen().y());
  EXPECT_GT(w1->GetBoundsInScreen().y(), w2->GetBoundsInScreen().y());
  EXPECT_EQ(work_area.bottom(), w3->GetBoundsInScreen().bottom());
  DragEnd();

  // Test the new windows order and that the overlaps differ at most by a pixel.
  overlap1 = w2->GetBoundsInScreen().y() - work_area.y();
  overlap2 = w2->GetBoundsInScreen().bottom() - w1->GetBoundsInScreen().y();
  overlap3 = w1->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  overlap4 = work_area.bottom() - w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);
}

// Tests run twice - on both panels and normal windows
INSTANTIATE_TEST_CASE_P(NormalOrPanel,
                        DockedWindowLayoutManagerTest,
                        testing::Values(aura::client::WINDOW_TYPE_NORMAL,
                                        aura::client::WINDOW_TYPE_PANEL));
}  // namespace internal
}  // namespace ash
