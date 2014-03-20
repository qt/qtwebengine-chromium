// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

#if defined(USE_X11)
#include "base/message_loop/message_pump_x11.h"
#endif

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return true;
  }
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}
  virtual void DidRecreateLayer(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE {}

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit DemoWindowTreeClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~DemoWindowTreeClient() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowTreeClient);
};

int DemoMain() {
  // Create the message-loop here before creating the root window.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);

  // The ContextFactory must exist before any Compositors are created.
  bool allow_test_contexts = false;
  ui::InitializeContextFactoryForTests(allow_test_contexts);

  aura::Env::CreateInstance();
  scoped_ptr<aura::TestScreen> test_screen(aura::TestScreen::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen.get());
  scoped_ptr<aura::RootWindow> root_window(
      test_screen->CreateRootWindowForPrimaryDisplay());
  scoped_ptr<DemoWindowTreeClient> window_tree_client(new DemoWindowTreeClient(
      root_window->window()));
  aura::test::TestFocusClient focus_client;
  aura::client::SetFocusClient(root_window->window(), &focus_client);

  // Create a hierarchy of test windows.
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window window1(&window_delegate1);
  window1.set_id(1);
  window1.Init(ui::LAYER_TEXTURED);
  window1.SetBounds(gfx::Rect(100, 100, 400, 400));
  window1.Show();
  aura::client::ParentWindowWithContext(
      &window1, root_window->window(), gfx::Rect());

  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window window2(&window_delegate2);
  window2.set_id(2);
  window2.Init(ui::LAYER_TEXTURED);
  window2.SetBounds(gfx::Rect(200, 200, 350, 350));
  window2.Show();
  aura::client::ParentWindowWithContext(
      &window2, root_window->window(), gfx::Rect());

  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window window3(&window_delegate3);
  window3.set_id(3);
  window3.Init(ui::LAYER_TEXTURED);
  window3.SetBounds(gfx::Rect(10, 10, 50, 50));
  window3.Show();
  window2.AddChild(&window3);

  root_window->host()->Show();
  base::MessageLoopForUI::current()->Run();

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  base::i18n::InitializeICU();
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

  return DemoMain();
}
