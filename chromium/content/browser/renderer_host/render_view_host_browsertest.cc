// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class RenderViewHostTest : public ContentBrowserTest {
 public:
  RenderViewHostTest() {}
};

class RenderViewHostTestWebContentsObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostTestWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_count_(0) {}
  virtual ~RenderViewHostTestWebContentsObserver() {}

  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) OVERRIDE {
    observed_socket_address_ = params.socket_address;
    base_url_ = params.base_url;
    ++navigation_count_;
  }

  const net::HostPortPair& observed_socket_address() const {
    return observed_socket_address_;
  }

  GURL base_url() const {
    return base_url_;
  }

  int navigation_count() const { return navigation_count_; }

 private:
  net::HostPortPair observed_socket_address_;
  GURL base_url_;
  int navigation_count_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestWebContentsObserver);
};

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, FrameNavigateSocketAddress) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  RenderViewHostTestWebContentsObserver observer(shell()->web_contents());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateToURL(shell(), test_url);

  EXPECT_EQ(net::HostPortPair::FromURL(
                embedded_test_server()->base_url()).ToString(),
            observer.observed_socket_address().ToString());
  EXPECT_EQ(1, observer.navigation_count());
}

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, BaseURLParam) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  RenderViewHostTestWebContentsObserver observer(shell()->web_contents());

  // Base URL is not set if it is the same as the URL.
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateToURL(shell(), test_url);
  EXPECT_TRUE(observer.base_url().is_empty());
  EXPECT_EQ(1, observer.navigation_count());

  // But should be set to the original page when reading MHTML.
  base::FilePath content_test_data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &content_test_data_dir));
  test_url = net::FilePathToFileURL(
      content_test_data_dir.AppendASCII("google.mht"));
  NavigateToURL(shell(), test_url);
  EXPECT_EQ("http://www.google.com/", observer.base_url().spec());
}

// This test ensures a RenderFrameHost object is created for the top level frame
// in each RenderViewHost.
IN_PROC_BROWSER_TEST_F(RenderViewHostTest, BasicRenderFrameHost) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateToURL(shell(), test_url);

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      shell()->web_contents()->GetRenderViewHost());
  EXPECT_TRUE(rvh->main_render_frame_host_.get());

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), "window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();
  RenderViewHostImpl* new_rvh = static_cast<RenderViewHostImpl*>(
      new_shell->web_contents()->GetRenderViewHost());

  EXPECT_TRUE(new_rvh->main_render_frame_host_.get());
  EXPECT_NE(rvh->main_render_frame_host_->routing_id(),
            new_rvh->main_render_frame_host_->routing_id());
}

}  // namespace content
