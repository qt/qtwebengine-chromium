// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

void ResizeWebContentsView(Shell* shell, const gfx::Size& size,
                           bool set_start_page) {
  // Shell::SizeTo is not implemented on Aura; WebContentsView::SizeContents
  // works on Win and ChromeOS but not Linux - we need to resize the shell
  // window on Linux because if we don't, the next layout of the unchanged shell
  // window will resize WebContentsView back to the previous size.
  // The cleaner and shorter SizeContents is preferred as more platforms convert
  // to Aura.
#if defined(TOOLKIT_GTK) || defined(OS_MACOSX)
  shell->SizeTo(size.width(), size.height());
  // If |set_start_page| is true, start with blank page to make sure resize
  // takes effect.
  if (set_start_page)
    NavigateToURL(shell, GURL("about://blank"));
#else
  shell->web_contents()->GetView()->SizeContents(size);
#endif  // defined(TOOLKIT_GTK) || defined(OS_MACOSX)
}

class WebContentsImplBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImplBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsImplBrowserTest);
};

// Keeps track of data from LoadNotificationDetails so we can later verify that
// they are correct, after the LoadNotificationDetails object is deleted.
class LoadStopNotificationObserver : public WindowedNotificationObserver {
 public:
  LoadStopNotificationObserver(NavigationController* controller)
      : WindowedNotificationObserver(NOTIFICATION_LOAD_STOP,
                                     Source<NavigationController>(controller)),
        session_index_(-1),
        controller_(NULL) {
  }
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    if (type == NOTIFICATION_LOAD_STOP) {
      const Details<LoadNotificationDetails> load_details(details);
      url_ = load_details->url;
      session_index_ = load_details->session_index;
      controller_ = load_details->controller;
    }
    WindowedNotificationObserver::Observe(type, source, details);
  }

  GURL url_;
  int session_index_;
  NavigationController* controller_;
};

// Starts a new navigation as soon as the current one commits, but does not
// wait for it to complete.  This allows us to observe DidStopLoading while
// a pending entry is present.
class NavigateOnCommitObserver : public WebContentsObserver {
 public:
  NavigateOnCommitObserver(Shell* shell, GURL url)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        url_(url),
        done_(false) {
  }

  // WebContentsObserver:
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) OVERRIDE {
    if (!done_) {
      done_ = true;
      shell_->LoadURL(url_);
    }
  }

  Shell* shell_;
  GURL url_;
  bool done_;
};

class RenderViewSizeDelegate : public WebContentsDelegate {
 public:
  void set_size_insets(const gfx::Size& size_insets) {
    size_insets_ = size_insets;
  }

  // WebContentsDelegate:
  virtual gfx::Size GetSizeForNewRenderView(
      const WebContents* web_contents) const OVERRIDE {
    gfx::Size size(web_contents->GetView()->GetContainerSize());
    size.Enlarge(size_insets_.width(), size_insets_.height());
    return size;
  }

 private:
  gfx::Size size_insets_;
};

class RenderViewSizeObserver : public WebContentsObserver {
 public:
  RenderViewSizeObserver(Shell* shell, const gfx::Size& wcv_new_size)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        wcv_new_size_(wcv_new_size) {
  }

  // WebContentsObserver:
  virtual void RenderViewCreated(RenderViewHost* rvh) OVERRIDE {
    rwhv_create_size_ = rvh->GetView()->GetViewBounds().size();
  }

  virtual void NavigateToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) OVERRIDE {
    ResizeWebContentsView(shell_, wcv_new_size_, false);
  }

  gfx::Size rwhv_create_size() const { return rwhv_create_size_; }

 private:
  Shell* shell_;  // Weak ptr.
  gfx::Size wcv_new_size_;
  gfx::Size rwhv_create_size_;
};

// Test that DidStopLoading includes the correct URL in the details.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DidStopLoadingDetails) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  load_observer.Wait();

  EXPECT_EQ("/title1.html", load_observer.url_.path());
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

// Test that DidStopLoading includes the correct URL in the details when a
// pending entry is present.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidStopLoadingDetailsWithPending) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Listen for the first load to stop.
  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  // Start a new pending navigation as soon as the first load commits.
  // We will hear a DidStopLoading from the first load as the new load
  // is started.
  NavigateOnCommitObserver commit_observer(
      shell(), embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  load_observer.Wait();

  EXPECT_EQ("/title1.html", load_observer.url_.path());
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}
// Test that a renderer-initiated navigation to an invalid URL does not leave
// around a pending entry that could be used in a URL spoof.  We test this in
// a browser test because our unit test framework incorrectly calls
// DidStartProvisionalLoadForFrame for in-page navigations.
// See http://crbug.com/280512.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ClearNonVisiblePendingOnFail) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Navigate to an invalid URL and make sure it doesn't leave a pending entry.
  LoadStopNotificationObserver load_observer1(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href=\"nonexistent:12121\";"));
  load_observer1.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

  LoadStopNotificationObserver load_observer2(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href=\"#foo\";"));
  load_observer2.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
            shell()->web_contents()->GetVisibleURL());
}

// Test that the browser receives the proper frame attach/detach messages from
// the renderer and builds proper frame tree.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FrameTree) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      wc->GetRenderViewHost());
  FrameTreeNode* root = wc->GetFrameTreeRootForTesting();

  // Check that the root node is properly created with the frame id of the
  // initial navigation.
  EXPECT_EQ(3UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());

  EXPECT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_STREQ("1-1-name", root->child_at(0)->frame_name().c_str());

  // Verify the deepest node exists and has the right name.
  EXPECT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ("3-1-id",
      root->child_at(2)->child_at(1)->child_at(0)->frame_name().c_str());

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  root = wc->GetFrameTreeRootForTesting();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());
}

// TODO(sail): enable this for MAC when auto resizing of WebContentsViewCocoa is
// fixed.
// TODO(shrikant): enable this for Windows when issue with
// force-compositing-mode is resolved (http://crbug.com/281726).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_ANDROID)
#define MAYBE_GetSizeForNewRenderView DISABLED_GetSizeForNewRenderView
#else
#define MAYBE_GetSizeForNewRenderView GetSizeForNewRenderView
#endif
// Test that RenderViewHost is created and updated at the size specified by
// WebContentsDelegate::GetSizeForNewRenderView().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_GetSizeForNewRenderView) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  // Create a new server with a different site.
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  scoped_ptr<RenderViewSizeDelegate> delegate(new RenderViewSizeDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  // When no size is set, RenderWidgetHostView adopts the size of
  // WebContenntsView.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(shell()->web_contents()->GetView()->GetContainerSize(),
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(-10, -15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  NavigateToURL(shell(), https_server.GetURL("/"));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  EXPECT_EQ(size, shell()->web_contents()->GetView()->GetContainerSize());

  // If WebContentsView is resized after RenderWidgetHostView is created but
  // before pending navigation entry is committed, both RenderWidgetHostView and
  // WebContentsView use the new size of WebContentsView.
  gfx::Size init_size(200, 200);
  gfx::Size new_size(100, 100);
  size_insets = gfx::Size(-20, -30);
  ResizeWebContentsView(shell(), init_size, true);
  delegate->set_size_insets(size_insets);
  RenderViewSizeObserver observer(shell(), new_size);
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  // RenderWidgetHostView is created at specified size.
  init_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(init_size, observer.rwhv_create_size());
  // RenderViewSizeObserver resizes WebContentsView in NavigateToPendingEntry,
  // so both WebContentsView and RenderWidgetHostView adopt this new size.
  new_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(new_size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  EXPECT_EQ(new_size, shell()->web_contents()->GetView()->GetContainerSize());
}

}  // namespace content
