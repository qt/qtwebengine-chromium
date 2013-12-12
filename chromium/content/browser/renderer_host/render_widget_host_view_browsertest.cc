// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "media/base/video_frame.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "ui/gl/io_surface_support_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/win/dpi.h"
#endif

namespace content {
namespace {

// Convenience macro: Short-circuit a pass for the tests where platform support
// for forced-compositing mode (or disabled-compositing mode) is lacking.
#define SET_UP_SURFACE_OR_PASS_TEST(wait_message)  \
  if (!SetUpSourceSurface(wait_message)) {  \
    LOG(WARNING)  \
        << ("Blindly passing this test: This platform does not support "  \
            "forced compositing (or forced-disabled compositing) mode.");  \
    return;  \
  }

// Convenience macro: Short-circuit a pass for platforms where setting up
// high-DPI fails.
#define PASS_TEST_IF_SCALE_FACTOR_NOT_SUPPORTED(factor) \
  if (ui::GetScaleFactorScale( \
          GetScaleFactorForView(GetRenderWidgetHostViewPort())) != factor) {  \
    LOG(WARNING) << "Blindly passing this test: failed to set up "  \
                    "scale factor: " << factor;  \
    return false;  \
  }

// Common base class for browser tests.  This is subclassed twice: Once to test
// the browser in forced-compositing mode, and once to test with compositing
// mode disabled.
class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest()
      : frame_size_(400, 300),
        callback_invoke_count_(0),
        frames_captured_(0) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  // Attempts to set up the source surface.  Returns false if unsupported on the
  // current platform.
  virtual bool SetUpSourceSurface(const char* wait_message) = 0;

  int callback_invoke_count() const {
    return callback_invoke_count_;
  }

  int frames_captured() const {
    return frames_captured_;
  }

  const gfx::Size& frame_size() const {
    return frame_size_;
  }

  const base::FilePath& test_dir() const {
    return test_dir_;
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh = RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderWidgetHostView()->
            GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewPort* GetRenderWidgetHostViewPort() const {
    RenderWidgetHostViewPort* const view =
        RenderWidgetHostViewPort::FromRWHV(GetRenderViewHost()->GetView());
    CHECK(view);
    return view;
  }

  // Callback when using CopyFromBackingStore() API.
  void FinishCopyFromBackingStore(const base::Closure& quit_closure,
                                  bool frame_captured,
                                  const SkBitmap& bitmap) {
    ++callback_invoke_count_;
    if (frame_captured) {
      ++frames_captured_;
      EXPECT_FALSE(bitmap.empty());
    }
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using CopyFromCompositingSurfaceToVideoFrame() API.
  void FinishCopyFromCompositingSurface(const base::Closure& quit_closure,
                                        bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using frame subscriber API.
  void FrameDelivered(const scoped_refptr<base::MessageLoopProxy>& loop,
                      base::Closure quit_closure,
                      base::Time timestamp,
                      bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      loop->PostTask(FROM_HERE, quit_closure);
  }

  // Copy one frame using the CopyFromBackingStore API.
  void RunBasicCopyFromBackingStoreTest() {
    SET_UP_SURFACE_OR_PASS_TEST(NULL);

    // Repeatedly call CopyFromBackingStore() since, on some platforms (e.g.,
    // Windows), the operation will fail until the first "present" has been
    // made.
    int count_attempts = 0;
    while (true) {
      ++count_attempts;
      base::RunLoop run_loop;
      GetRenderViewHost()->CopyFromBackingStore(
          gfx::Rect(),
          frame_size(),
          base::Bind(
              &RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
              base::Unretained(this),
              run_loop.QuitClosure()));
      run_loop.Run();

      if (frames_captured())
        break;
      else
        GiveItSomeTime();
    }

    EXPECT_EQ(count_attempts, callback_invoke_count());
    EXPECT_EQ(1, frames_captured());
  }

 protected:
  // Waits until the source is available for copying.
  void WaitForCopySourceReady() {
    while (!GetRenderWidgetHostViewPort()->IsSurfaceAvailableForCopy())
      GiveItSomeTime();
  }

  // Run the current message loop for a short time without unwinding the current
  // call stack.
  static void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }

 private:
  const gfx::Size frame_size_;
  base::FilePath test_dir_;
  int callback_invoke_count_;
  int frames_captured_;
};

class CompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest {
 public:
  virtual void SetUp() OVERRIDE {
    // We expect real pixel output for these tests.
    UseRealGLContexts();

    // On legacy windows, these tests need real GL bindings to pass.
#if defined(OS_WIN) && !defined(USE_AURA)
    UseRealGLBindings();
#endif

    RenderWidgetHostViewBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Note: Not appending kForceCompositingMode switch here, since not all bots
    // support compositing.  Some bots will run with compositing on, and others
    // won't.  Therefore, the call to SetUpSourceSurface() later on will detect
    // whether compositing mode is actually on or not.  If not, the tests will
    // pass blindly, logging a warning message, since we cannot test what the
    // platform/implementation does not support.
    RenderWidgetHostViewBrowserTest::SetUpCommandLine(command_line);
  }

  virtual GURL TestUrl() {
    return net::FilePathToFileURL(
        test_dir().AppendASCII("rwhv_compositing_animation.html"));
  }

  virtual bool SetUpSourceSurface(const char* wait_message) OVERRIDE {
    if (!IsForceCompositingModeEnabled())
      return false;  // See comment in SetUpCommandLine().
#if defined(OS_MACOSX)
    CHECK(IOSurfaceSupport::Initialize());
#endif

    content::DOMMessageQueue message_queue;
    NavigateToURL(shell(), TestUrl());
    if (wait_message != NULL) {
      std::string result(wait_message);
      if (!message_queue.WaitForMessage(&result)) {
        EXPECT_TRUE(false) << "WaitForMessage " << result << " failed.";
        return false;
      }
    }

#if !defined(USE_AURA)
    if (!GetRenderWidgetHost()->is_accelerated_compositing_active())
      return false;  // Renderer did not turn on accelerated compositing.
#endif

    // Using accelerated compositing, but a compositing surface might not be
    // available yet.  So, wait for it.
    WaitForCopySourceReady();
    return true;
  }
};

class NonCompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Note: Appending the kDisableAcceleratedCompositing switch here, but there
    // are some builds that only use compositing and will ignore this switch.
    // Therefore, the call to SetUpSourceSurface() later on will detect whether
    // compositing mode is actually off.  If it's on, the tests will pass
    // blindly, logging a warning message, since we cannot test what the
    // platform/implementation does not support.
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
    RenderWidgetHostViewBrowserTest::SetUpCommandLine(command_line);
  }

  virtual GURL TestUrl() {
    return GURL(kAboutBlankURL);
  }

  virtual bool SetUpSourceSurface(const char* wait_message) OVERRIDE {
    if (IsForceCompositingModeEnabled())
      return false;  // See comment in SetUpCommandLine().

    content::DOMMessageQueue message_queue;
    NavigateToURL(shell(), TestUrl());
    if (wait_message != NULL) {
      std::string result(wait_message);
      if (!message_queue.WaitForMessage(&result)) {
        EXPECT_TRUE(false) << "WaitForMessage " << result << " failed.";
        return false;
      }
    }

    WaitForCopySourceReady();
    // Return whether the renderer left accelerated compositing turned off.
    return !GetRenderWidgetHost()->is_accelerated_compositing_active();
  }
};

class FakeFrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FakeFrameSubscriber(
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback)
      : callback_(callback) {
  }

  virtual bool ShouldCaptureFrame(
      base::Time present_time,
      scoped_refptr<media::VideoFrame>* storage,
      DeliverFrameCallback* callback) OVERRIDE {
    // Only allow one frame capture to be made.  Otherwise, the compositor could
    // start multiple captures, unbounded, and eventually its own limiter logic
    // will begin invoking |callback| with a |false| result.  This flakes out
    // the unit tests, since they receive a "failed" callback before the later
    // "success" callbacks.
    if (callback_.is_null())
      return false;
    *storage = media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
    *callback = callback_;
    callback_.Reset();
    return true;
  }

 private:
  DeliverFrameCallback callback_;
};

// Disable tests for Android and IOS as these platforms have incomplete
// implementation.
#if !defined(OS_ANDROID) && !defined(OS_IOS)

// The CopyFromBackingStore() API should work on all platforms when compositing
// is enabled.
IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore) {
  // Disable the test for WinXP.  See http://crbug/294116.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << "Test disabled due to unknown bug on WinXP.";
    return;
  }
#endif

  RunBasicCopyFromBackingStoreTest();
}

// The CopyFromBackingStore() API should work on all platforms when compositing
// is disabled.
IN_PROC_BROWSER_TEST_F(NonCompositingRenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore) {
  RunBasicCopyFromBackingStoreTest();
}

// Tests that the callback passed to CopyFromBackingStore is always called,
// even when the RenderWidgetHost is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);

  base::RunLoop run_loop;
  GetRenderViewHost()->CopyFromBackingStore(
      gfx::Rect(),
      frame_size(),
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), run_loop.QuitClosure()));
  // Delete the surface before the callback is run.
  GetRenderWidgetHostViewPort()->AcceleratedSurfaceRelease();
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// Tests that the callback passed to CopyFromCompositingSurfaceToVideoFrame is
// always called, even when the RenderWidgetHost is deleting in the middle of
// an async copy.
//
// Test is flaky on Win Aura. http://crbug.com/276783
#if (defined(OS_WIN) && defined(USE_AURA)) || \
    (defined(OS_CHROMEOS) && !defined(NDEBUG))
#define MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete \
  DISABLED_CopyFromCompositingSurface_CallbackDespiteDelete
#else
#define MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete \
  CopyFromCompositingSurface_CallbackDespiteDelete
#endif
IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTest,
                       MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewPort* const view = GetRenderWidgetHostViewPort();
  if (!view->CanCopyToVideoFrame()) {
    LOG(WARNING) <<
        ("Blindly passing this test: CopyFromCompositingSurfaceToVideoFrame() "
         "not supported on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_refptr<media::VideoFrame> dest =
      media::VideoFrame::CreateBlackFrame(frame_size());
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()), dest, base::Bind(
          &RenderWidgetHostViewBrowserTest::FinishCopyFromCompositingSurface,
          base::Unretained(this), run_loop.QuitClosure()));
  // Delete the surface before the callback is run.
  view->AcceleratedSurfaceRelease();
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// With compositing turned off, no platforms should support the
// CopyFromCompositingSurfaceToVideoFrame() API.
IN_PROC_BROWSER_TEST_F(NonCompositingRenderWidgetHostViewBrowserTest,
                       CopyFromCompositingSurfaceToVideoFrameCallbackTest) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  EXPECT_FALSE(GetRenderWidgetHostViewPort()->CanCopyToVideoFrame());
}

// Test basic frame subscription functionality.  We subscribe, and then run
// until at least one DeliverFrameCallback has been invoked.
IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTest,
                       FrameSubscriberTest) {
  // Disable the test for WinXP.  See http://crbug/294116.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << "Test disabled due to unknown bug on WinXP.";
    return;
  }
#endif

  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewPort* const view = GetRenderWidgetHostViewPort();
  if (!view->CanSubscribeFrame()) {
    LOG(WARNING) << ("Blindly passing this test: Frame subscription not "
                     "supported on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
      new FakeFrameSubscriber(
          base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                     base::Unretained(this),
                     base::MessageLoopProxy::current(),
                     run_loop.QuitClosure())));
  view->BeginFrameSubscription(subscriber.Pass());
  run_loop.Run();
  view->EndFrameSubscription();

  EXPECT_LE(1, callback_invoke_count());
  EXPECT_LE(1, frames_captured());
}

// Test that we can copy twice from an accelerated composited page.
IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTest, CopyTwice) {
  // Disable the test for WinXP.  See http://crbug/294116.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << "Test disabled due to unknown bug on WinXP.";
    return;
  }
#endif

  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewPort* const view = GetRenderWidgetHostViewPort();
  if (!view->CanCopyToVideoFrame()) {
    LOG(WARNING) << ("Blindly passing this test: "
                     "CopyFromCompositingSurfaceToVideoFrame() not supported "
                     "on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_refptr<media::VideoFrame> first_output =
      media::VideoFrame::CreateBlackFrame(frame_size());
  ASSERT_TRUE(first_output.get());
  scoped_refptr<media::VideoFrame> second_output =
      media::VideoFrame::CreateBlackFrame(frame_size());
  ASSERT_TRUE(second_output.get());
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()),
      first_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this),
                 base::MessageLoopProxy::current(),
                 base::Closure(),
                 base::Time::Now()));
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()), second_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this),
                 base::MessageLoopProxy::current(),
                 run_loop.QuitClosure(),
                 base::Time::Now()));
  run_loop.Run();

  EXPECT_EQ(2, callback_invoke_count());
  EXPECT_EQ(2, frames_captured());
}

class CompositingRenderWidgetHostViewBrowserTestTabCapture
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCapture()
      : expected_copy_from_compositing_surface_result_(false),
        allowable_error_(0),
        test_url_("data:text/html,<!doctype html>") {}

  void CopyFromCompositingSurfaceCallback(base::Closure quit_callback,
                                          bool result,
                                          const SkBitmap& bitmap) {
    EXPECT_EQ(expected_copy_from_compositing_surface_result_, result);
    if (!result) {
      quit_callback.Run();
      return;
    }

    const SkBitmap& expected_bitmap =
        expected_copy_from_compositing_surface_bitmap_;
    EXPECT_EQ(expected_bitmap.width(), bitmap.width());
    EXPECT_EQ(expected_bitmap.height(), bitmap.height());
    EXPECT_EQ(expected_bitmap.config(), bitmap.config());
    SkAutoLockPixels expected_bitmap_lock(expected_bitmap);
    SkAutoLockPixels bitmap_lock(bitmap);
    int fails = 0;
    for (int i = 0; i < bitmap.width() && fails < 10; ++i) {
      for (int j = 0; j < bitmap.height() && fails < 10; ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        SkColor expected_color = expected_bitmap.getColor(i, j);
        SkColor color = bitmap.getColor(i, j);
        int expected_alpha = SkColorGetA(expected_color);
        int alpha = SkColorGetA(color);
        int expected_red = SkColorGetR(expected_color);
        int red = SkColorGetR(color);
        int expected_green = SkColorGetG(expected_color);
        int green = SkColorGetG(color);
        int expected_blue = SkColorGetB(expected_color);
        int blue = SkColorGetB(color);
        EXPECT_NEAR(expected_alpha, alpha, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_red, red, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_green, green, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_blue, blue, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
      }
    }
    EXPECT_LT(fails, 10);

    quit_callback.Run();
  }

  void CopyFromCompositingSurfaceCallbackForVideo(
      scoped_refptr<media::VideoFrame> video_frame,
      base::Closure quit_callback,
      bool result) {
    EXPECT_EQ(expected_copy_from_compositing_surface_result_, result);
    if (!result) {
      quit_callback.Run();
      return;
    }

    media::SkCanvasVideoRenderer video_renderer;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     video_frame->visible_rect().width(),
                     video_frame->visible_rect().height());
    bitmap.allocPixels();
    bitmap.setIsOpaque(true);

    SkBitmapDevice device(bitmap);
    SkCanvas canvas(&device);

    video_renderer.Paint(video_frame.get(),
                         &canvas,
                         video_frame->visible_rect(),
                         0xff);

    CopyFromCompositingSurfaceCallback(quit_callback,
                                       result,
                                       bitmap);
  }

  void SetExpectedCopyFromCompositingSurfaceResult(bool result,
                                                   const SkBitmap& bitmap) {
    expected_copy_from_compositing_surface_result_ = result;
    expected_copy_from_compositing_surface_bitmap_ = bitmap;
  }

  void SetAllowableError(int amount) { allowable_error_ = amount; }
  void SetExcludeRect(gfx::Rect exclude) { exclude_rect_ = exclude; }

  virtual GURL TestUrl() OVERRIDE {
    return GURL(test_url_);
  }

  void SetTestUrl(std::string url) { test_url_ = url; }

  // Loads a page two boxes side-by-side, each half the width of
  // |html_rect_size|, and with different background colors. The test then
  // copies from |copy_rect| region of the page into a bitmap of size
  // |output_size|, and compares that with a bitmap of size
  // |expected_bitmap_size|.
  // Note that |output_size| may not have the same size as |copy_rect| (e.g.
  // when the output is scaled). Also note that |expected_bitmap_size| may not
  // be the same as |output_size| (e.g. when the device scale factor is not 1).
  void PerformTestWithLeftRightRects(const gfx::Size& html_rect_size,
                                     const gfx::Rect& copy_rect,
                                     const gfx::Size& output_size,
                                     const gfx::Size& expected_bitmap_size,
                                     bool video_frame) {
    const gfx::Size box_size(html_rect_size.width() / 2,
                             html_rect_size.height());
    SetTestUrl(base::StringPrintf(
        "data:text/html,<!doctype html>"
        "<div class='left'>"
        "  <div class='right'></div>"
        "</div>"
        "<style>"
        "body { padding: 0; margin: 0; }"
        ".left { position: absolute;"
        "        background: #0ff;"
        "        width: %dpx;"
        "        height: %dpx;"
        "}"
        ".right { position: absolute;"
        "         left: %dpx;"
        "         background: #ff0;"
        "         width: %dpx;"
        "         height: %dpx;"
        "}"
        "</style>"
        "<script>"
        "  domAutomationController.setAutomationId(0);"
        "  domAutomationController.send(\"DONE\");"
        "</script>",
        box_size.width(),
        box_size.height(),
        box_size.width(),
        box_size.width(),
        box_size.height()));

    SET_UP_SURFACE_OR_PASS_TEST("\"DONE\"");
    if (!ShouldContinueAfterTestURLLoad())
      return;

    RenderWidgetHostViewPort* rwhvp = GetRenderWidgetHostViewPort();

    // The page is loaded in the renderer, wait for a new frame to arrive.
    uint32 frame = rwhvp->RendererFrameNumber();
    while (!GetRenderWidgetHost()->ScheduleComposite())
      GiveItSomeTime();
    while (rwhvp->RendererFrameNumber() == frame)
      GiveItSomeTime();

    SkBitmap expected_bitmap;
    SetupLeftRightBitmap(expected_bitmap_size, &expected_bitmap);
    SetExpectedCopyFromCompositingSurfaceResult(true, expected_bitmap);

    base::RunLoop run_loop;
    if (video_frame) {
      // Allow pixel differences as long as we have the right idea.
      SetAllowableError(0x10);
      // Exclude the middle two columns which are blended between the two sides.
      SetExcludeRect(
          gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));

      scoped_refptr<media::VideoFrame> video_frame =
          media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                         expected_bitmap_size,
                                         gfx::Rect(expected_bitmap_size),
                                         expected_bitmap_size,
                                         base::TimeDelta());

      base::Callback<void(bool success)> callback =
          base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                         CopyFromCompositingSurfaceCallbackForVideo,
                     base::Unretained(this),
                     video_frame,
                     run_loop.QuitClosure());
      rwhvp->CopyFromCompositingSurfaceToVideoFrame(copy_rect,
                                                    video_frame,
                                                    callback);
    } else {
      base::Callback<void(bool, const SkBitmap&)> callback =
          base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                       CopyFromCompositingSurfaceCallback,
                   base::Unretained(this),
                   run_loop.QuitClosure());
      rwhvp->CopyFromCompositingSurface(copy_rect, output_size, callback);
    }
    run_loop.Run();
  }

  // Sets up |bitmap| to have size |copy_size|. It floods the left half with
  // #0ff and the right half with #ff0.
  void SetupLeftRightBitmap(const gfx::Size& copy_size, SkBitmap* bitmap) {
    bitmap->setConfig(
        SkBitmap::kARGB_8888_Config, copy_size.width(), copy_size.height());
    bitmap->allocPixels();
    // Left half is #0ff.
    bitmap->eraseARGB(255, 0, 255, 255);
    // Right half is #ff0.
    {
      SkAutoLockPixels lock(*bitmap);
      for (int i = 0; i < copy_size.width() / 2; ++i) {
        for (int j = 0; j < copy_size.height(); ++j) {
          *(bitmap->getAddr32(copy_size.width() / 2 + i, j)) =
              SkColorSetARGB(255, 255, 255, 0);
        }
      }
    }
  }

 protected:
  virtual bool ShouldContinueAfterTestURLLoad() {
    return true;
  }

 private:
  bool expected_copy_from_compositing_surface_result_;
  SkBitmap expected_copy_from_compositing_surface_bitmap_;
  int allowable_error_;
  gfx::Rect exclude_rect_;
  std::string test_url_;
};

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Origin_Unscaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Origin_Scaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size(200, 100);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Cropped_Unscaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Cropped_Scaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size(20, 10);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_ForVideoFrame) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_ForVideoFrame_Scaled) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  // Scale to 30 x 20 (preserve aspect ratio).
  gfx::Size output_size(30, 20);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

class CompositingRenderWidgetHostViewTabCaptureHighDPI
    : public CompositingRenderWidgetHostViewBrowserTestTabCapture {
 public:
  CompositingRenderWidgetHostViewTabCaptureHighDPI()
      : kScale(2.f) {
  }

  virtual void SetUpCommandLine(CommandLine* cmd) OVERRIDE {
    CompositingRenderWidgetHostViewBrowserTestTabCapture::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                           base::StringPrintf("%f", scale()));
#if defined(OS_WIN)
    cmd->AppendSwitchASCII(switches::kHighDPISupport, "1");
    gfx::EnableHighDPISupport();
#endif
  }

  float scale() const { return kScale; }

 private:
  virtual bool ShouldContinueAfterTestURLLoad() OVERRIDE {
    PASS_TEST_IF_SCALE_FACTOR_NOT_SUPPORTED(scale());
    return true;
  }

  const float kScale;

  DISALLOW_COPY_AND_ASSIGN(CompositingRenderWidgetHostViewTabCaptureHighDPI);
};

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewTabCaptureHighDPI,
                       CopyFromCompositingSurface) {
  gfx::Rect copy_rect(200, 150);
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size =
      gfx::ToFlooredSize(gfx::ScaleSize(output_size, scale(), scale()));
  gfx::Size html_rect_size(200, 150);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_F(CompositingRenderWidgetHostViewTabCaptureHighDPI,
                       CopyFromCompositingSurfaceVideoFrame) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size =
      gfx::ToFlooredSize(gfx::ScaleSize(output_size, scale(), scale()));
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace
}  // namespace content
