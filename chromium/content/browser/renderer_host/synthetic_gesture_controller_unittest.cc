// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/port/browser/synthetic_gesture.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#endif

using base::TimeDelta;

namespace content {

namespace {

class MockSyntheticGesture : public SyntheticGesture {
 public:
  MockSyntheticGesture() :
      called_(0) {
  }

  // SmoothScrollGesture implementation:
  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE {
    ++called_;
    return true;
  }

  int called_;

 protected:
  virtual ~MockSyntheticGesture() {
  }
};

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {
  }
  virtual ~MockRenderWidgetHostDelegate() {}
};

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  MockRenderWidgetHost(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int routing_id)
      : RenderWidgetHostImpl(delegate, process, routing_id, false) {
  }
  virtual ~MockRenderWidgetHost() {}
};

class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        mock_gesture_(NULL) {
  }
  virtual ~TestView() {}

  // TestRenderWidgetHostView implementation:
  virtual SyntheticGesture* CreateSmoothScrollGesture(
      bool scroll_down, int pixels_to_scroll, int mouse_event_x,
      int mouse_event_y) OVERRIDE {
    mock_gesture_ = new MockSyntheticGesture();
    return mock_gesture_;
  }

  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE {
    return rwh_;
  }

  MockSyntheticGesture* mock_gesture_;
};

class SyntheticGestureControllerTest : public testing::Test {
 public:
  SyntheticGestureControllerTest() : process_(NULL) {
  }
  virtual ~SyntheticGestureControllerTest() {}

 protected:
  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new MockRenderProcessHost(browser_context_.get());
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
#endif
    host_.reset(
        new MockRenderWidgetHost(delegate_.get(), process_, MSG_ROUTING_NONE));
    view_.reset(new TestView(host_.get()));
    host_->SetView(view_.get());
    host_->Init();
  }

  virtual void TearDown() OVERRIDE {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = NULL;
    browser_context_.reset();

#if defined(USE_AURA)
    aura::Env::DeleteInstance();
    screen_.reset();
#endif

    // Process all pending tasks to avoid leaks.
    base::MessageLoop::current()->RunUntilIdle();
  }

  void PostQuitMessageAndRun() {
    // Allow the message loop to process pending synthetic scrolls, then quit.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitClosure(),
        TimeDelta::FromMilliseconds(
            controller_.GetSyntheticGestureMessageInterval().InMilliseconds() *
            3));
    base::MessageLoop::current()->Run();
  }

  base::MessageLoopForUI message_loop_;

  scoped_ptr<TestBrowserContext> browser_context_;
  MockRenderProcessHost* process_;  // Deleted automatically by the widget.
  scoped_ptr<MockRenderWidgetHostDelegate> delegate_;
  scoped_ptr<MockRenderWidgetHost> host_;
  scoped_ptr<TestView> view_;
#if defined(USE_AURA)
  scoped_ptr<gfx::Screen> screen_;
#endif

  SyntheticGestureController controller_;
};

TEST_F(SyntheticGestureControllerTest, Tick) {
  ViewHostMsg_BeginSmoothScroll_Params params;
  params.scroll_down = true;
  params.pixels_to_scroll = 10;
  params.mouse_event_x = 20;
  params.mouse_event_y = 30;

  // Begin a smooth scroll, |mock_gesture_| won't be |called| until we post
  // message.
  controller_.BeginSmoothScroll(view_.get(), params);
  EXPECT_TRUE(view_->mock_gesture_ != NULL);
  EXPECT_EQ(0, view_->mock_gesture_->called_);

  PostQuitMessageAndRun();
  const int current_ticks = view_->mock_gesture_->called_;
  EXPECT_LT(0, current_ticks);

  // Ensure it won't start another smooth scroll.
  MockSyntheticGesture* original_gesture = view_->mock_gesture_;
  controller_.BeginSmoothScroll(view_.get(), params);
  PostQuitMessageAndRun();
  EXPECT_EQ(original_gesture, view_->mock_gesture_);

  // Ensure the smooth scroll is ticked.
  PostQuitMessageAndRun();
  EXPECT_LT(current_ticks, view_->mock_gesture_->called_);
}

}  // namespace

}  // namespace content
