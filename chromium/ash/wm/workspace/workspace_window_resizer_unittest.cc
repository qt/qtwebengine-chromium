// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace gfx {

// Class to provide access to SlideAnimation internals for testing.
// TODO: this should be next to SlideAnimation, not here.
class SlideAnimation::TestApi {
 public:
  explicit TestApi(SlideAnimation* animation) : animation_(animation) {}

  void SetStartTime(base::TimeTicks ticks) {
    animation_->SetStartTime(ticks);
  }

  void Step(base::TimeTicks ticks) {
    animation_->Step(ticks);
  }

  void RunTillComplete() {
    SetStartTime(base::TimeTicks());
    Step(base::TimeTicks() +
         base::TimeDelta::FromMilliseconds(animation_->GetSlideDuration()));
    EXPECT_EQ(1.0, animation_->GetCurrentValue());
  }

 private:
  SlideAnimation* animation_;

  DISALLOW_COPY_AND_ASSIGN(TestApi);
};

}

namespace ash {
namespace internal {
namespace {

const int kRootHeight = 600;

// A simple window delegate that returns the specified min size.
class TestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestWindowDelegate() {
  }
  virtual ~TestWindowDelegate() {}

  void set_min_size(const gfx::Size& size) {
    min_size_ = size;
  }

  void set_max_size(const gfx::Size& size) {
    max_size_ = size;
  }

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return min_size_;
  }

  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return max_size_;
  }

  gfx::Size min_size_;
  gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

}  // namespace

class WorkspaceWindowResizerTest : public test::AshTestBase {
 public:
  WorkspaceWindowResizerTest() : workspace_resizer_(NULL) {}
  virtual ~WorkspaceWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay(base::StringPrintf("800x%d", kRootHeight));

    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Rect root_bounds(root->bounds());
#if defined(OS_WIN)
    // RootWindow and Display can't resize on Windows Ash.
    // http://crbug.com/165962
    EXPECT_EQ(kRootHeight, root_bounds.height());
#endif
    EXPECT_EQ(800, root_bounds.width());
    Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());
    window_.reset(new aura::Window(&delegate_));
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->set_id(1);

    window2_.reset(new aura::Window(&delegate2_));
    window2_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window2_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window2_.get());
    window2_->set_id(2);

    window3_.reset(new aura::Window(&delegate3_));
    window3_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window3_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window3_.get());
    window3_->set_id(3);

    window4_.reset(new aura::Window(&delegate4_));
    window4_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window4_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window4_.get());
    window4_->set_id(4);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    window2_.reset();
    window3_.reset();
    window4_.reset();
    touch_resize_window_.reset();
    AshTestBase::TearDown();
  }

  // Returns a string identifying the z-order of each of the known child windows
  // of |parent|.  The returned string constains the id of the known windows and
  // is ordered from topmost to bottomost windows.
  std::string WindowOrderAsString(aura::Window* parent) const {
    std::string result;
    const aura::Window::Windows& windows = parent->children();
    for (aura::Window::Windows::const_reverse_iterator i = windows.rbegin();
         i != windows.rend(); ++i) {
      if (*i == window_ || *i == window2_ || *i == window3_) {
        if (!result.empty())
          result += " ";
        result += base::IntToString((*i)->id());
      }
    }
    return result;
  }

 protected:
  WindowResizer* CreateResizerForTest(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    WindowResizer* resizer = CreateWindowResizer(
        window,
        point_in_parent,
        window_component,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE).release();
    workspace_resizer_ = WorkspaceWindowResizer::instance_;
    return resizer;
  }

  PhantomWindowController* snap_phantom_window_controller() const {
    return workspace_resizer_->snap_phantom_window_controller_.get();
  }

  gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  std::vector<aura::Window*> empty_windows() const {
    return std::vector<aura::Window*>();
  }

  internal::ShelfLayoutManager* shelf_layout_manager() {
    return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  }

  void InitTouchResizeWindow(const gfx::Rect& bounds, int window_component) {
    touch_resize_delegate_.set_window_component(window_component);
    touch_resize_window_.reset(
        CreateTestWindowInShellWithDelegate(&touch_resize_delegate_, 0,
                                            bounds));
    gfx::Insets mouse_outer_insets(-ash::kResizeOutsideBoundsSize,
                                   -ash::kResizeOutsideBoundsSize,
                                   -ash::kResizeOutsideBoundsSize,
                                   -ash::kResizeOutsideBoundsSize);
    gfx::Insets touch_outer_insets = mouse_outer_insets.Scale(
        ash::kResizeOutsideBoundsScaleForTouch);
    touch_resize_window_->SetHitTestBoundsOverrideOuter(mouse_outer_insets,
                                                        touch_outer_insets);
  }

  // Simulate running the animation.
  void RunAnimationTillComplete(gfx::SlideAnimation* animation) {
    gfx::SlideAnimation::TestApi test_api(animation);
    test_api.RunTillComplete();
  }

  TestWindowDelegate delegate_;
  TestWindowDelegate delegate2_;
  TestWindowDelegate delegate3_;
  TestWindowDelegate delegate4_;
  scoped_ptr<aura::Window> window_;
  scoped_ptr<aura::Window> window2_;
  scoped_ptr<aura::Window> window3_;
  scoped_ptr<aura::Window> window4_;

  TestWindowDelegate touch_resize_delegate_;
  scoped_ptr<aura::Window> touch_resize_window_;
  WorkspaceWindowResizer* workspace_resizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizerTest);
};

class WorkspaceWindowResizerTestSticky : public WorkspaceWindowResizerTest {
 public:
  WorkspaceWindowResizerTestSticky() {}
  virtual ~WorkspaceWindowResizerTestSticky() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableStickyEdges);
    WorkspaceWindowResizerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizerTestSticky);
};

// Assertions around attached window resize dragging from the right with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_2) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  EXPECT_EQ("0,300 780x300", window_->bounds().ToString());
  EXPECT_EQ("780,200 20x200", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,300 400x300", window_->bounds().ToString());
  EXPECT_EQ("400,200 100x200", window2_->bounds().ToString());
}

// Assertions around collapsing and expanding.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_Compress) {
  window_->SetBounds(gfx::Rect(   0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the left, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 10), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, -800, 20), 0);
  EXPECT_EQ("0,300 20x300", window_->bounds().ToString());
  EXPECT_EQ("20,200 480x200", window2_->bounds().ToString());

  // Move 100 to the left.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 20), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the right with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3) {
  window_->SetBounds(gfx::Rect( 100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 150, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());

  // Move it 300, things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, 300, -10), 0);
  EXPECT_EQ("100,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("600,300 120x200", window2_->bounds().ToString());
  EXPECT_EQ("720,300 80x200", window3_->bounds().ToString());

  // Move it so much the last two end up at their min.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 50), 0);
  EXPECT_EQ("100,300 610x300", window_->bounds().ToString());
  EXPECT_EQ("710,300 52x200", window2_->bounds().ToString());
  EXPECT_EQ("762,300 38x200", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("100,300 200x300", window_->bounds().ToString());
  EXPECT_EQ("300,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("450,300 100x200", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3_Compress) {
  window_->SetBounds(gfx::Rect( 100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it -100 to the right, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 266x200", window2_->bounds().ToString());
  EXPECT_EQ("466,300 134x200", window3_->bounds().ToString());

  // Move it 100 to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("600,300 100x200", window3_->bounds().ToString());

  // 100 to the left again.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 266x200", window2_->bounds().ToString());
  EXPECT_EQ("466,300 134x200", window3_->bounds().ToString());
}

// Assertions around collapsing and expanding from the bottom.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_Compress) {
  window_->SetBounds(gfx::Rect(   0, 100, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 400, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it up 100, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, 10, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 20, -800), 0);
  EXPECT_EQ("0,100 400x20", window_->bounds().ToString());
  EXPECT_EQ("400,120 100x480", window2_->bounds().ToString());

  // Move 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,100 400x400", window_->bounds().ToString());
  EXPECT_EQ("400,500 100x100", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, 20, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the bottom with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_2) {
  window_->SetBounds(gfx::Rect( 0,  50, 400, 200));
  window2_->SetBounds(gfx::Rect(0, 250, 200, 100));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the bottom, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 50, 820), 0);
  EXPECT_EQ("0,50 400x530", window_->bounds().ToString());
  EXPECT_EQ("0,580 200x20", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,50 400x200", window_->bounds().ToString());
  EXPECT_EQ("0,250 200x100", window2_->bounds().ToString());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_AttachedResize_BOTTOM_3 DISABLED_AttachedResize_BOTTOM_3
#else
#define MAYBE_AttachedResize_BOTTOM_3 AttachedResize_BOTTOM_3
#endif

// Assertions around attached window resize dragging from the bottom with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, MAYBE_AttachedResize_BOTTOM_3) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  window_->SetBounds(gfx::Rect( 300, 100, 300, 200));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 150));
  window3_->SetBounds(gfx::Rect(300, 450, 200, 100));
  delegate2_.set_min_size(gfx::Size(50, 52));
  delegate3_.set_min_size(gfx::Size(50, 38));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 down, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 100), 0);
  EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());

  // Move it 296 things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 296), 0);
  EXPECT_EQ("300,100 300x496", window_->bounds().ToString());
  EXPECT_EQ("300,596 200x123", window2_->bounds().ToString());
  EXPECT_EQ("300,719 200x81", window3_->bounds().ToString());

  // Move it so much everything ends up at its min.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 798), 0);
  EXPECT_EQ("300,100 300x610", window_->bounds().ToString());
  EXPECT_EQ("300,710 200x52", window2_->bounds().ToString());
  EXPECT_EQ("300,762 200x38", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("300,100 300x200", window_->bounds().ToString());
  EXPECT_EQ("300,300 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,450 200x100", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3_Compress) {
  window_->SetBounds(gfx::Rect(  0,   0, 200, 200));
  window2_->SetBounds(gfx::Rect(10, 200, 200, 200));
  window3_->SetBounds(gfx::Rect(20, 400, 100, 100));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 up, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x266", window2_->bounds().ToString());
  EXPECT_EQ("20,366 100x134", window3_->bounds().ToString());

  // Move it 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,0 200x300", window_->bounds().ToString());
  EXPECT_EQ("10,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("20,500 100x100", window3_->bounds().ToString());

  // 100 up again.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x266", window2_->bounds().ToString());
  EXPECT_EQ("20,366 100x134", window3_->bounds().ToString());
}

// Tests that touch-dragging a window does not lock the mouse cursor
// and therefore shows the cursor on a mousemove.
TEST_F(WorkspaceWindowResizerTest, MouseMoveWithTouchDrag) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  Shell* shell = Shell::GetInstance();
  aura::test::EventGenerator generator(window_->GetRootWindow());

  // The cursor should not be locked initially.
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_TOUCH, windows));
  ASSERT_TRUE(resizer.get());

  // Creating a WorkspaceWindowResizer should not lock the cursor.
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  // The cursor should be hidden after touching the screen and
  // starting a drag.
  EXPECT_TRUE(shell->cursor_manager()->IsCursorVisible());
  generator.PressTouch();
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_FALSE(shell->cursor_manager()->IsCursorVisible());
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  // Moving the mouse should show the cursor.
  generator.MoveMouseBy(1, 1);
  EXPECT_TRUE(shell->cursor_manager()->IsCursorVisible());
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  resizer->RevertDrag();
}

// Assertions around dragging to the left/right edge of the screen.
TEST_F(WorkspaceWindowResizerTest, Edge) {
  if (!SupportsHostWindowResize())
    return;

  // Resize host window to force insets update.
  UpdateDisplay("800x700");
  // TODO(varkha): Insets are reset after every drag because of
  // http://crbug.com/292238.
  // Window is wide enough not to get docked right away.
  window_->SetBounds(gfx::Rect(20, 30, 400, 60));
  wm::WindowState* window_state = wm::GetWindowState(window_.get());

  {
    internal::SnapSizer snap_sizer(window_state, gfx::Point(),
        internal::SnapSizer::LEFT_EDGE, internal::SnapSizer::OTHER_INPUT);
    gfx::Rect expected_bounds(snap_sizer.target_bounds());

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    resizer->CompleteDrag(0);

    EXPECT_EQ(expected_bounds.ToString(), window_->bounds().ToString());
    ASSERT_TRUE(window_state->HasRestoreBounds());
    EXPECT_EQ("20,30 400x60",
              window_state->GetRestoreBoundsInScreen().ToString());
  }
  // Try the same with the right side.
  {
    internal::SnapSizer snap_sizer(window_state, gfx::Point(),
        internal::SnapSizer::RIGHT_EDGE, internal::SnapSizer::OTHER_INPUT);
    gfx::Rect expected_bounds(snap_sizer.target_bounds());

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag(0);
    EXPECT_EQ(expected_bounds.ToString(), window_->bounds().ToString());
    ASSERT_TRUE(window_state->HasRestoreBounds());
    EXPECT_EQ("20,30 400x60",
              window_state->GetRestoreBoundsInScreen().ToString());
  }

  // Test if the restore bounds is correct in multiple displays.
  if (!SupportsMultipleDisplays())
    return;

  // Restore the window to clear snapped state.
  window_state->Restore();

  UpdateDisplay("800x600,500x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  // Window is wide enough not to get docked right away.
  window_->SetBoundsInScreen(gfx::Rect(800, 10, 400, 60),
                             ScreenAsh::GetSecondaryDisplay());
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());
  {
    EXPECT_EQ("800,10 400x60", window_->GetBoundsInScreen().ToString());

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 499, 0), 0);
    int bottom =
        ScreenAsh::GetDisplayWorkAreaBoundsInParent(window_.get()).bottom();
    resizer->CompleteDrag(0);
    // With the resolution of 500x600 we will hit in this case the 50% screen
    // size setting.
    // TODO(varkha): Insets are updated because of http://crbug.com/292238
    EXPECT_EQ("250,0 250x" + base::IntToString(bottom),
              window_->bounds().ToString());
    EXPECT_EQ("800,10 400x60",
              window_state->GetRestoreBoundsInScreen().ToString());
  }
}

// Check that non resizable windows will not get resized.
TEST_F(WorkspaceWindowResizerTest, NonResizableWindows) {
  window_->SetBounds(gfx::Rect(20, 30, 50, 60));
  window_->SetProperty(aura::client::kCanResizeKey, false);

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -20, 0), 0);
  resizer->CompleteDrag(0);
  EXPECT_EQ("0,30 50x60", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CancelSnapPhantom) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    EXPECT_FALSE(snap_phantom_window_controller());

    // The pointer is on the edge but not shared. The snap phantom window
    // controller should be non-NULL.
    resizer->Drag(CalculateDragPoint(*resizer, 799, 0), 0);
    EXPECT_TRUE(snap_phantom_window_controller());

    // Move the cursor across the edge. Now the snap phantom window controller
    // should be canceled.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);
    EXPECT_FALSE(snap_phantom_window_controller());
  }
}

// Verifies windows are correctly restacked when reordering multiple windows.
TEST_F(WorkspaceWindowResizerTest, RestackAttached) {
  window_->SetBounds(gfx::Rect(   0, 0, 200, 300));
  window2_->SetBounds(gfx::Rect(200, 0, 100, 200));
  window3_->SetBounds(gfx::Rect(300, 0, 100, 100));

  {
    std::vector<aura::Window*> windows;
    windows.push_back(window2_.get());
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    EXPECT_EQ("2 1 3", WindowOrderAsString(window_->parent()));
  }

  {
    std::vector<aura::Window*> windows;
    windows.push_back(window3_.get());
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window2_.get(), gfx::Point(), HTRIGHT,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    EXPECT_EQ("2 3 1", WindowOrderAsString(window_->parent()));
  }
}

// Makes sure we don't allow dragging below the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffBottom) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 10, 0));

  ASSERT_EQ(1, Shell::GetScreen()->GetNumDisplays());

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600), 0);
  int expected_y =
      kRootHeight - WorkspaceWindowResizer::kMinOnscreenHeight - 10;
  EXPECT_EQ("100," + base::IntToString(expected_y) + " 300x400",
            window_->bounds().ToString());
}

// Makes sure we don't allow dragging on the work area with multidisplay.
TEST_F(WorkspaceWindowResizerTest, DontDragOffBottomWithMultiDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());

  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 10, 0));

  // Positions the secondary display at the bottom the primary display.
  Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      ash::DisplayLayout(ash::DisplayLayout::BOTTOM, 0));

  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 20));
    DCHECK_LT(window_->bounds().height(),
              WorkspaceWindowResizer::kMinOnscreenHeight);
    // Drag down avoiding dragging along the edge as that would side-snap.
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(10, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 400), 0);
    int expected_y = kRootHeight - window_->bounds().height() - 10;
    // When the mouse cursor is in the primary display, the window cannot move
    // on non-work area but can get all the way towards the bottom,
    // restricted only by the window height.
    EXPECT_EQ("100," + base::IntToString(expected_y) + " 300x20",
              window_->bounds().ToString());
    // Revert the drag in order to not remember the restore bounds.
    resizer->RevertDrag();
  }

  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 10, 0));
  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 400));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(10, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag down avoiding dragging along the edge as that would side-snap.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 400), 0);
    int expected_y =
        kRootHeight - WorkspaceWindowResizer::kMinOnscreenHeight - 10;
    // When the mouse cursor is in the primary display, the window cannot move
    // on non-work area with kMinOnscreenHeight margin.
    EXPECT_EQ("100," + base::IntToString(expected_y) + " 300x400",
              window_->bounds().ToString());
    resizer->CompleteDrag(0);
  }

  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 400));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), window_->bounds().origin(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag down avoiding getting stuck against the shelf on the bottom screen.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 500), 0);
    // The window can move to the secondary display beyond non-work area of
    // the primary display.
    EXPECT_EQ("100,700 300x400", window_->bounds().ToString());
    resizer->CompleteDrag(0);
  }
}

// Makes sure we don't allow dragging off the top of the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffTop) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(10, 0, 0, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -600), 0);
  EXPECT_EQ("100,10 300x400", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeBottomOutsideWorkArea) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 380));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOP));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 8, 0), 0);
  EXPECT_EQ("100,200 300x380", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideLeftWorkArea) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));
  int left = ScreenAsh::GetDisplayWorkAreaBoundsInParent(window_.get()).x();
  int pixels_to_left_border = 50;
  int window_width = 300;
  int window_x = left - window_width + pixels_to_left_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(pixels_to_left_border, 0), HTRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -window_width, 0), 0);
  EXPECT_EQ(base::IntToString(window_x) + ",100 " +
            base::IntToString(kMinimumOnScreenArea - window_x) +
            "x380", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideRightWorkArea) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));
  int right = ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      window_.get()).right();
  int pixels_to_right_border = 50;
  int window_width = 300;
  int window_x = right - pixels_to_right_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(window_x, 0), HTLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(base::IntToString(right - kMinimumOnScreenArea) +
            ",100 " +
            base::IntToString(window_width - pixels_to_right_border +
                              kMinimumOnScreenArea) +
            "x380", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideBottomWorkArea) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));
  int bottom = ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      window_.get()).bottom();
  int delta_to_bottom = 50;
  int height = 380;
  window_->SetBounds(gfx::Rect(100, bottom - delta_to_bottom, 300, height));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(0, bottom - delta_to_bottom), HTTOP));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, bottom), 0);
  EXPECT_EQ("100," +
            base::IntToString(bottom - kMinimumOnScreenArea) +
            " 300x" +
            base::IntToString(height - (delta_to_bottom -
                                        kMinimumOnScreenArea)),
            window_->bounds().ToString());
}

// Verifies that 'outside' check of the resizer take into account the extended
// desktop in case of repositions.
TEST_F(WorkspaceWindowResizerTest, DragWindowOutsideRightToSecondaryDisplay) {
  // Only primary display.  Changes the window position to fit within the
  // display.
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));
  int right = ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      window_.get()).right();
  int pixels_to_right_border = 50;
  int window_width = 300;
  int window_x = right - pixels_to_right_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(window_x, 0), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(base::IntToString(right - kMinimumOnScreenArea) +
            ",100 " +
            base::IntToString(window_width) +
            "x380", window_->bounds().ToString());

  if (!SupportsMultipleDisplays())
    return;

  // With secondary display.  Operation itself is same but doesn't change
  // the position because the window is still within the secondary display.
  UpdateDisplay("1000x600,600x400");
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(base::IntToString(window_x + window_width) +
            ",100 " +
            base::IntToString(window_width) +
            "x380", window_->bounds().ToString());
}

// Verifies snapping to edges works.
TEST_F(WorkspaceWindowResizerTest, SnapToEdge) {
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  // Click 50px to the right so that the mouse pointer does not leave the
  // workspace ensuring sticky behavior.
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(),
      window_->bounds().origin() + gfx::Vector2d(50, 0),
      HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Move to an x-coordinate of 15, which should not snap.
  resizer->Drag(CalculateDragPoint(*resizer, 15 - 96, 0), 0);
  // An x-coordinate of 7 should snap.
  resizer->Drag(CalculateDragPoint(*resizer, 7 - 96, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // Move to -15, should still snap to 0.
  resizer->Drag(CalculateDragPoint(*resizer, -15 - 96, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // At -32 should move past snap points.
  resizer->Drag(CalculateDragPoint(*resizer, -32 - 96, 0), 0);
  EXPECT_EQ("-32,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, -33 - 96, 0), 0);
  EXPECT_EQ("-33,112 320x160", window_->bounds().ToString());

  // Right side should similarly snap.
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 - 15, 0), 0);
  EXPECT_EQ("465,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 - 7, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 15, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 32, 0), 0);
  EXPECT_EQ("512,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 33, 0), 0);
  EXPECT_EQ("513,112 320x160", window_->bounds().ToString());

  // And the bottom should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 3 - 7), 0);
  EXPECT_EQ("96,437 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 3 + 15), 0);
  EXPECT_EQ("96,437 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 2 + 32), 0);
  EXPECT_EQ("96,470 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 2 + 33), 0);
  EXPECT_EQ("96,471 320x160", window_->bounds().ToString());

  // And the top should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -112 + 20), 0);
  EXPECT_EQ("96,20 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -112 + 7), 0);
  EXPECT_EQ("96,0 320x160", window_->bounds().ToString());

  // And bottom/left should snap too.
  resizer->Drag(
      CalculateDragPoint(*resizer, 7 - 96, 600 - 160 - 112 - 3 - 7), 0);
  EXPECT_EQ("0,437 320x160", window_->bounds().ToString());
  resizer->Drag(
      CalculateDragPoint(*resizer, -15 - 96, 600 - 160 - 112 - 3 + 15), 0);
  EXPECT_EQ("0,437 320x160", window_->bounds().ToString());
  // should move past snap points.
  resizer->Drag(
      CalculateDragPoint(*resizer, -32 - 96, 600 - 160 - 112 - 2 + 32), 0);
  EXPECT_EQ("-32,470 320x160", window_->bounds().ToString());
  resizer->Drag(
      CalculateDragPoint(*resizer, -33 - 96, 600 - 160 - 112 - 2 + 33), 0);
  EXPECT_EQ("-33,471 320x160", window_->bounds().ToString());

  // No need to test dragging < 0 as we force that to 0.
}

// Verifies a resize snap when dragging TOPLEFT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_TOPLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOPLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -98, -199), 0);
  EXPECT_EQ("0,0 120x230", window_->bounds().ToString());
}

// Verifies a resize snap when dragging TOPRIGHT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_TOPRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOPRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(
      CalculateDragPoint(*resizer, work_area.right() - 120 - 1, -199), 0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(work_area.y(), window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(230, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMRIGHT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_BOTTOMRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(
      CalculateDragPoint(*resizer, work_area.right() - 120 - 1,
                         work_area.bottom() - 220 - 2), 0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMLEFT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_BOTTOMLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(
      CalculateDragPoint(*resizer, -98, work_area.bottom() - 220 - 2), 0);
  EXPECT_EQ(0, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(120, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

// Verifies sticking to edges works.
TEST_F(WorkspaceWindowResizerTestSticky, StickToEdge) {
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  // Click 50px to the right so that the mouse pointer does not leave the
  // workspace ensuring sticky behavior.
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(),
      window_->bounds().origin() + gfx::Vector2d(50, 0),
      HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Move to an x-coordinate of 15, which should not stick.
  resizer->Drag(CalculateDragPoint(*resizer, 15 - 96, 0), 0);
  // Move to -15, should still stick to 0.
  resizer->Drag(CalculateDragPoint(*resizer, -15 - 96, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // At -100 should move past edge.
  resizer->Drag(CalculateDragPoint(*resizer, -100 - 96, 0), 0);
  EXPECT_EQ("-100,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, -101 - 96, 0), 0);
  EXPECT_EQ("-101,112 320x160", window_->bounds().ToString());

  // Right side should similarly stick.
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 - 15, 0), 0);
  EXPECT_EQ("465,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 15, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 100, 0), 0);
  EXPECT_EQ("580,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 101, 0), 0);
  EXPECT_EQ("581,112 320x160", window_->bounds().ToString());

  // And the bottom should stick too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 3 + 15), 0);
  EXPECT_EQ("96,437 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 2 + 100), 0);
  EXPECT_EQ("96,538 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 2 + 101), 0);
  EXPECT_EQ("96,539 320x160", window_->bounds().ToString());

  // No need to test dragging < 0 as we force that to 0.
}

// Verifies not sticking to edges when a mouse pointer is outside of work area.
TEST_F(WorkspaceWindowResizerTestSticky, NoStickToEdgeWhenOutside) {
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Move to an x-coordinate of 15, which should not stick.
  resizer->Drag(CalculateDragPoint(*resizer, 15 - 96, 0), 0);
  // Move to -15, should still stick to 0.
  resizer->Drag(CalculateDragPoint(*resizer, -15 - 96, 0), 0);
  EXPECT_EQ("-15,112 320x160", window_->bounds().ToString());
}

// Verifies a resize sticks when dragging TOPLEFT.
TEST_F(WorkspaceWindowResizerTestSticky, StickToWorkArea_TOPLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOPLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -15 - 100, -15 -200), 0);
  EXPECT_EQ("0,0 120x230", window_->bounds().ToString());
}

// Verifies a resize sticks when dragging TOPRIGHT.
TEST_F(WorkspaceWindowResizerTestSticky, StickToWorkArea_TOPRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOPRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, work_area.right() - 100 + 20,
                                   -200 - 15), 0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(work_area.y(), window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(230, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMRIGHT.
TEST_F(WorkspaceWindowResizerTestSticky, StickToWorkArea_BOTTOMRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, work_area.right() - 100 - 20 + 15,
                                   work_area.bottom() - 200 - 30 + 15), 0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMLEFT.
TEST_F(WorkspaceWindowResizerTestSticky, StickToWorkArea_BOTTOMLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                          window_.get()));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -15 - 100,
                                   work_area.bottom() - 200 - 30 + 15), 0);
  EXPECT_EQ(0, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(120, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

TEST_F(WorkspaceWindowResizerTest, CtrlDragResizeToExactPosition) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  ASSERT_TRUE(resizer.get());
  // Resize the right bottom to add 10 in width, 12 in height.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), ui::EF_CONTROL_DOWN);
  // Both bottom and right sides to resize to exact size requested.
  EXPECT_EQ("96,112 330x172", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CtrlCompleteDragMoveToExactPosition) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Ctrl + drag the window to new poistion by adding (10, 12) to its origin,
  // the window should move to the exact position.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), 0);
  resizer->CompleteDrag(ui::EF_CONTROL_DOWN);
  EXPECT_EQ("106,124 320x160", window_->bounds().ToString());
}

// Verifies that a dragged window will restore to its pre-maximized size.
TEST_F(WorkspaceWindowResizerTest, RestoreToPreMaximizeCoordinates) {
  window_->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  wm::WindowState* window_state = wm::GetWindowState(window_.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(96, 112, 320, 160));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Drag the window to new position by adding (10, 10) to original point,
  // the window should get restored.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
  resizer->CompleteDrag(0);
  EXPECT_EQ("10,10 320x160", window_->bounds().ToString());
  // The restore rectangle should get cleared as well.
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies that a dragged window will restore to its pre-maximized size.
TEST_F(WorkspaceWindowResizerTest, RevertResizeOperation) {
  const gfx::Rect initial_bounds(0, 0, 200, 400);
  window_->SetBounds(initial_bounds);

  wm::WindowState* window_state = wm::GetWindowState(window_.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(96, 112, 320, 160));
  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Drag the window to new poistion by adding (180, 16) to original point,
  // the window should get restored.
  resizer->Drag(CalculateDragPoint(*resizer, 180, 16), 0);
  resizer->RevertDrag();
  EXPECT_EQ(initial_bounds.ToString(), window_->bounds().ToString());
  EXPECT_EQ("96,112 320x160",
            window_state->GetRestoreBoundsInScreen().ToString());
}

// Check that only usable sizes get returned by the resizer.
TEST_F(WorkspaceWindowResizerTest, MagneticallyAttach) {
  window_->SetBounds(gfx::Rect(10, 10, 20, 30));
  window2_->SetBounds(gfx::Rect(150, 160, 25, 20));
  window2_->Show();

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  // Move |window| one pixel to the left of |window2|. Should snap to right and
  // top.
  resizer->Drag(CalculateDragPoint(*resizer, 119, 145), 0);
  EXPECT_EQ("130,160 20x30", window_->bounds().ToString());

  // Move |window| one pixel to the right of |window2|. Should snap to left and
  // top.
  resizer->Drag(CalculateDragPoint(*resizer, 164, 145), 0);
  EXPECT_EQ("175,160 20x30", window_->bounds().ToString());

  // Move |window| one pixel above |window2|. Should snap to top and left.
  resizer->Drag(CalculateDragPoint(*resizer, 142, 119), 0);
  EXPECT_EQ("150,130 20x30", window_->bounds().ToString());

  // Move |window| one pixel above the bottom of |window2|. Should snap to
  // bottom and left.
  resizer->Drag(CalculateDragPoint(*resizer, 142, 169), 0);
  EXPECT_EQ("150,180 20x30", window_->bounds().ToString());
}

// The following variants verify magnetic snapping during resize when dragging a
// particular edge.
TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOP) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(99, 179, 10, 20));
  window2_->Show();

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTTOP));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,199 20x31", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOPLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(99, 179, 10, 20));
  window2_->Show();

  {
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTTOPLEFT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(88, 201, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTTOPLEFT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("98,201 22x29", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOPRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(111, 179, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTTOPRIGHT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(121, 199, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTTOPRIGHT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_RIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(121, 199, 10, 20));
  window2_->Show();

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTRIGHT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,200 21x30", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOMRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(122, 212, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTBOTTOMRIGHT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,200 22x32", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(111, 233, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTBOTTOMRIGHT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,200 21x33", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOM) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(111, 233, 10, 20));
  window2_->Show();

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOM));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,200 20x33", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOMLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(99, 231, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTBOTTOMLEFT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,200 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(89, 209, 10, 20));
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTBOTTOMLEFT));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,200 21x29", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_LEFT) {
  window2_->SetBounds(gfx::Rect(89, 209, 10, 20));
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTLEFT));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("99,200 21x30", window_->bounds().ToString());
}

// Test that the user user moved window flag is getting properly set.
TEST_F(WorkspaceWindowResizerTest, CheckUserWindowManagedFlags) {
  window_->SetBounds(gfx::Rect( 0,  50, 400, 200));

  std::vector<aura::Window*> no_attached_windows;
  // Check that an abort doesn't change anything.
  {
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the bottom.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 100), 0);
    EXPECT_EQ("0,150 400x200", window_->bounds().ToString());
    resizer->RevertDrag();

    EXPECT_FALSE(wm::GetWindowState(window_.get())->bounds_changed_by_user());
  }

  // Check that a completed move / size does change the user coordinates.
  {
    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the bottom.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 100), 0);
    EXPECT_EQ("0,150 400x200", window_->bounds().ToString());
    resizer->CompleteDrag(0);
    EXPECT_TRUE(wm::GetWindowState(window_.get())->bounds_changed_by_user());
  }
}

// Test that a window with a specified max size doesn't exceed it when dragged.
TEST_F(WorkspaceWindowResizerTest, TestMaxSizeEnforced) {
  window_->SetBounds(gfx::Rect(0, 0, 400, 300));
  delegate_.set_max_size(gfx::Size(401, 301));

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  resizer->Drag(CalculateDragPoint(*resizer, 2, 2), 0);
  EXPECT_EQ(401, window_->bounds().width());
  EXPECT_EQ(301, window_->bounds().height());
}

// Test that a window with a specified max width doesn't restrict its height.
TEST_F(WorkspaceWindowResizerTest, TestPartialMaxSizeEnforced) {
  window_->SetBounds(gfx::Rect(0, 0, 400, 300));
  delegate_.set_max_size(gfx::Size(401, 0));

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  resizer->Drag(CalculateDragPoint(*resizer, 2, 2), 0);
  EXPECT_EQ(401, window_->bounds().width());
  EXPECT_EQ(302, window_->bounds().height());
}

// Test that a window with a specified max size can't be snapped.
TEST_F(WorkspaceWindowResizerTest, PhantomSnapMaxSize) {
  {
    // With max size not set we get a phantom window controller for dragging off
    // the right hand side.
    // Make the window wider than maximum docked width.
    window_->SetBounds(gfx::Rect(0, 0, 400, 200));

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_FALSE(snap_phantom_window_controller());
    resizer->Drag(CalculateDragPoint(*resizer, 801, 0), 0);
    EXPECT_TRUE(snap_phantom_window_controller());
    resizer->RevertDrag();
  }
  {
    // With max size defined, we get no phantom window for snapping but we still
    // get a phantom window (docking guide).
    window_->SetBounds(gfx::Rect(0, 0, 400, 200));
    delegate_.set_max_size(gfx::Size(400, 200));

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    resizer->Drag(CalculateDragPoint(*resizer, 801, 0), 0);
    if (switches::UseDockedWindows())
      EXPECT_TRUE(snap_phantom_window_controller());
    else
      EXPECT_FALSE(snap_phantom_window_controller());
    resizer->RevertDrag();
  }
  {
    // With max size defined, we get no phantom window for snapping.
    window_->SetBounds(gfx::Rect(0, 0, 400, 200));
    delegate_.set_max_size(gfx::Size(400, 200));
    // With min size defined, we get no phantom window for docking.
    delegate_.set_min_size(gfx::Size(400, 200));

    scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
        window_.get(), gfx::Point(), HTCAPTION));
    resizer->Drag(CalculateDragPoint(*resizer, 801, 0), 0);
    EXPECT_FALSE(snap_phantom_window_controller());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, DontRewardRightmostWindowForOverflows) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  window4_->SetBounds(gfx::Rect(400, 100, 100, 100));
  delegate2_.set_max_size(gfx::Size(101, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 51 to the left, which should contract w1 and expand w2-4.
  // w2 will hit its max size straight away, and in doing so will leave extra
  // pixels that a naive implementation may award to the rightmost window. A
  // fair implementation will give 25 pixels to each of the other windows.
  resizer->Drag(CalculateDragPoint(*resizer, -51, 0), 0);
  EXPECT_EQ("100,100 49x100", window_->bounds().ToString());
  EXPECT_EQ("149,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("250,100 125x100", window3_->bounds().ToString());
  EXPECT_EQ("375,100 125x100", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExceedMaxWidth) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  window4_->SetBounds(gfx::Rect(400, 100, 100, 100));
  delegate2_.set_max_size(gfx::Size(101, 0));
  delegate3_.set_max_size(gfx::Size(101, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 52 to the left, which should contract w1 and expand w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, -52, 0), 0);
  EXPECT_EQ("100,100 48x100", window_->bounds().ToString());
  EXPECT_EQ("148,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("249,100 101x100", window3_->bounds().ToString());
  EXPECT_EQ("350,100 150x100", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExceedMaxHeight) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(100, 200, 100, 100));
  window3_->SetBounds(gfx::Rect(100, 300, 100, 100));
  window4_->SetBounds(gfx::Rect(100, 400, 100, 100));
  delegate2_.set_max_size(gfx::Size(0, 101));
  delegate3_.set_max_size(gfx::Size(0, 101));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 52 up, which should contract w1 and expand w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -52), 0);
  EXPECT_EQ("100,100 100x48", window_->bounds().ToString());
  EXPECT_EQ("100,148 100x101", window2_->bounds().ToString());
  EXPECT_EQ("100,249 100x101", window3_->bounds().ToString());
  EXPECT_EQ("100,350 100x150", window4_->bounds().ToString());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_DontExceedMinHeight DISABLED_DontExceedMinHeight
#else
#define MAYBE_DontExceedMinHeight DontExceedMinHeight
#endif

TEST_F(WorkspaceWindowResizerTest, MAYBE_DontExceedMinHeight) {
  UpdateDisplay("600x500");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(100, 200, 100, 100));
  window3_->SetBounds(gfx::Rect(100, 300, 100, 100));
  window4_->SetBounds(gfx::Rect(100, 400, 100, 100));
  delegate2_.set_min_size(gfx::Size(0, 99));
  delegate3_.set_min_size(gfx::Size(0, 99));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 52 down, which should expand w1 and contract w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 52), 0);
  EXPECT_EQ("100,100 100x152", window_->bounds().ToString());
  EXPECT_EQ("100,252 100x99", window2_->bounds().ToString());
  EXPECT_EQ("100,351 100x99", window3_->bounds().ToString());
  EXPECT_EQ("100,450 100x50", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExpandRightmostPastMaxWidth) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate3_.set_max_size(gfx::Size(101, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 51 to the left, which should contract w1 and expand w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -51, 0), 0);
  EXPECT_EQ("100,100 49x100", window_->bounds().ToString());
  EXPECT_EQ("149,100 150x100", window2_->bounds().ToString());
  EXPECT_EQ("299,100 101x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MoveAttachedWhenGrownToMaxSize) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate2_.set_max_size(gfx::Size(101, 0));
  delegate3_.set_max_size(gfx::Size(101, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 52 to the left, which should contract w1 and expand and move w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -52, 0), 0);
  EXPECT_EQ("100,100 48x100", window_->bounds().ToString());
  EXPECT_EQ("148,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("249,100 101x100", window3_->bounds().ToString());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_MainWindowHonoursMaxWidth DISABLED_MainWindowHonoursMaxWidth
#else
#define MAYBE_MainWindowHonoursMaxWidth MainWindowHonoursMaxWidth
#endif

TEST_F(WorkspaceWindowResizerTest, MAYBE_MainWindowHonoursMaxWidth) {
  UpdateDisplay("400x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate_.set_max_size(gfx::Size(102, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 50 to the right, which should expand w1 and contract w2-3, as they
  // won't fit in the root window in their original sizes.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 0), 0);
  EXPECT_EQ("100,100 102x100", window_->bounds().ToString());
  EXPECT_EQ("202,100 99x100", window2_->bounds().ToString());
  EXPECT_EQ("301,100 99x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MainWindowHonoursMinWidth) {
  UpdateDisplay("400x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect( 100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate_.set_min_size(gfx::Size(98, 0));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT,
      aura::client::WINDOW_MOVE_SOURCE_MOUSE, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 50 to the left, which should contract w1 and expand w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -50, 0), 0);
  EXPECT_EQ("100,100 98x100", window_->bounds().ToString());
  EXPECT_EQ("198,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("299,100 101x100", window3_->bounds().ToString());
}

// The following variants test that windows are resized correctly to the edges
// of the screen using touch, when touch point is off of the window border.
TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_RIGHT) {
  shelf_layout_manager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTRIGHT);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       touch_resize_window_.get());

  // Drag out of the right border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(715, kRootHeight / 2),
                                  gfx::Point(725, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 625, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(725, kRootHeight / 2),
                                  gfx::Point(760, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 660, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(760, kRootHeight / 2),
                                  gfx::Point(775, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 700, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_LEFT) {
  shelf_layout_manager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTLEFT);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       touch_resize_window_.get());

  // Drag out of the left border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(85, kRootHeight / 2),
                                  gfx::Point(75, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(75, 100, 625, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(75, kRootHeight / 2),
                                  gfx::Point(40, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(40, 100, 660, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(40, kRootHeight / 2),
                                  gfx::Point(25, kRootHeight / 2),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(0, 100, 700, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_TOP) {
  shelf_layout_manager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTTOP);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       touch_resize_window_.get());

  // Drag out of the top border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(400, 85),
                                  gfx::Point(400, 75),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 75, 600, kRootHeight - 175).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(400, 75),
                                  gfx::Point(400, 40),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 40, 600, kRootHeight - 140).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(400, 40),
                                  gfx::Point(400, 25),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 0, 600, kRootHeight - 100).ToString(),
            touch_resize_window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_BOTTOM) {
  shelf_layout_manager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTBOTTOM);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200).ToString(),
            touch_resize_window_->bounds().ToString());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       touch_resize_window_.get());

  // Drag out of the bottom border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 85),
                                  gfx::Point(400, kRootHeight - 75),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 175).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 75),
                                  gfx::Point(400, kRootHeight - 40),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 140).ToString(),
            touch_resize_window_->bounds().ToString());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 40),
                                  gfx::Point(400, kRootHeight - 25),
                                  base::TimeDelta::FromMilliseconds(10),
                                  5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 100).ToString(),
            touch_resize_window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, PhantomWindowShow) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x400,500x400");
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());

  scoped_ptr<WindowResizer> resizer(CreateResizerForTest(
      window_.get(), gfx::Point(), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(snap_phantom_window_controller());

  // The pointer is on the edge but not shared. The snap phantom window
  // controller should be non-NULL.
  resizer->Drag(CalculateDragPoint(*resizer, -1, 0), 0);
  EXPECT_TRUE(snap_phantom_window_controller());
  PhantomWindowController* phantom_controller(snap_phantom_window_controller());

  // phantom widget only in the left screen.
  phantom_controller->Show(gfx::Rect(100, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_FALSE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());

  // Move phantom widget into the right screen. Test that 2 widgets got created.
  phantom_controller->Show(gfx::Rect(600, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_TRUE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[1],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_start_->GetNativeWindow()->
          GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Move phantom widget only in the right screen. Start widget should close.
  phantom_controller->Show(gfx::Rect(700, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_FALSE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[1],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Move phantom widget into the left screen. Start widget should open.
  phantom_controller->Show(gfx::Rect(100, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_TRUE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(
      root_windows[1],
      phantom_controller->phantom_widget_start_->GetNativeWindow()->
          GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Move phantom widget while in the left screen. Start widget should close.
  phantom_controller->Show(gfx::Rect(200, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_FALSE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Move phantom widget spanning both screens with most of the window in the
  // right screen. Two widgets are created.
  phantom_controller->Show(gfx::Rect(495, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_TRUE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[1],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_start_->GetNativeWindow()->
          GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Move phantom widget back into the left screen. Phantom widgets should swap.
  phantom_controller->Show(gfx::Rect(200, 100, 50, 60));
  EXPECT_TRUE(phantom_controller->phantom_widget_);
  EXPECT_TRUE(phantom_controller->phantom_widget_start_);
  EXPECT_EQ(
      root_windows[0],
      phantom_controller->phantom_widget_->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(
      root_windows[1],
      phantom_controller->phantom_widget_start_->GetNativeWindow()->
          GetRootWindow());
  RunAnimationTillComplete(phantom_controller->animation_.get());

  // Hide phantom controller. Both widgets should close.
  phantom_controller->Hide();
  EXPECT_FALSE(phantom_controller->phantom_widget_);
  EXPECT_FALSE(phantom_controller->phantom_widget_start_);
}

}  // namespace internal
}  // namespace ash
