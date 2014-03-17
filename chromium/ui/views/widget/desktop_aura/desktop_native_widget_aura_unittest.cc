// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase DesktopNativeWidgetAuraTest;

// Verifies creating a Widget with a parent that is not in a RootWindow doesn't
// crash.
TEST_F(DesktopNativeWidgetAuraTest, CreateWithParentNotInRootWindow) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.parent = window.get();
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(params);
}

// Verifies that the AURA windows making up a widget instance have the correct
// bounds after the widget is resized.
TEST_F(DesktopNativeWidgetAuraTest, DesktopAuraWindowSizeTest) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);

  gfx::Rect bounds(0, 0, 100, 100);
  widget.SetBounds(bounds);
  widget.Show();

  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());

  gfx::Rect new_bounds(0, 0, 200, 200);
  widget.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());
}

// Verifies GetNativeView() is initially hidden. If the native view is initially
// shown then animations can not be disabled.
TEST_F(DesktopNativeWidgetAuraTest, NativeViewInitiallyHidden) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  EXPECT_FALSE(widget.GetNativeView()->IsVisible());
}

// Verifies FocusController doesn't attempt to access |content_window_| during
// destruction. Previously the FocusController was destroyed after the window.
// This could be problematic as FocusController references |content_window_| and
// could attempt to use it after |content_window_| was destroyed. This test
// verifies this doesn't happen. Note that this test only failed under ASAN.
TEST_F(DesktopNativeWidgetAuraTest, DontAccessContentWindowDuringDestruction) {
  aura::test::TestWindowDelegate delegate;
  {
    Widget widget;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    DesktopNativeWidgetAura* desktop_native_widget_aura =
        new DesktopNativeWidgetAura(&widget);
    init_params.native_widget = desktop_native_widget_aura;
    widget.Init(init_params);

    // Owned by |widget|.
    aura::Window* window = new aura::Window(&delegate);
    window->Show();
    widget.GetNativeWindow()->parent()->AddChild(window);

    widget.Show();
  }
}

}  // namespace views
