// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/net_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace content {

class RenderViewHostManagerTest : public ContentBrowserTest {
 public:
  RenderViewHostManagerTest() {}

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::SpawnedTestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::SpawnedTestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }
};

// Web pages should not have script access to the swapped out page.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_NoScriptAccessAfterSwapOut) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Open a same-site link in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // We should no longer have script access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_FALSE(success);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetVisibleURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// As of crbug.com/69267, we create a new BrowsingInstance (and SiteInstance)
// for rel=noreferrer links in new windows, even to same site pages and named
// targets.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessWithSameSiteRelNoreferrer) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetVisibleURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontSwapProcessWithOnlyTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

namespace {

class WebContentsDestroyedObserver : public WebContentsObserver {
 public:
  WebContentsDestroyedObserver(WebContents* web_contents,
                               const base::Closure& callback)
      : WebContentsObserver(web_contents),
        callback_(callback) {
  }

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    callback_.Run();
  }

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedObserver);
};

}  // namespace

// Test for crbug.com/116192.  Targeted links should still work after the
// named target window has swapped processes.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       AllowTargetedNavigationsAfterSwap) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  TestNavigationObserver navigation_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new window again.
  scoped_refptr<SiteInstance> revisit_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  EXPECT_EQ(new_site_instance,
            new_shell->web_contents()->GetSiteInstance());
  scoped_refptr<MessageLoopRunner> loop_runner(new MessageLoopRunner);
  WebContentsDestroyedObserver close_observer(new_shell->web_contents(),
                                              loop_runner->QuitClosure());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  loop_runner->Run();
}

// Test that setting the opener to null in a window affects cross-process
// navigations, including those to existing entries.  http://crbug.com/156669.
// Flaky on windows: http://crbug.com/291249
#if defined(OS_WIN)
#define MAYBE_DisownOpener DISABLED_DisownOpener
#else
#define MAYBE_DisownOpener DisownOpener
#endif
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, MAYBE_DisownOpener) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=_blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Now disown the opener.
  EXPECT_TRUE(ExecuteScript(new_shell->web_contents(),
                            "window.opener = null;"));

  // Go back and ensure the opener is still null.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Now navigate forward again (creating a new process) and check opener.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
}

// Test that subframes can disown their openers.  http://crbug.com/225528.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, DisownSubframeOpener) {
  const GURL frame_url("data:text/html,<iframe name=\"foo\"></iframe>");
  NavigateToURL(shell(), frame_url);

  // Give the frame an opener using window.open.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('about:blank','foo');"));

  // Now disown the frame's opener.  Shouldn't crash.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.frames[0].opener = null;"));
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
// 5) Post a message from "foo" to a subframe of opener, which replies back.
// 6) Post a message from _blank to a subframe of "foo".
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SupportCrossProcessPostMessage) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);
  RenderViewHostManager* opener_manager = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/files/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell, https_server.GetURL("files/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  Shell* new_shell2 = new_shell_observer2.GetShell();
  WebContents* new_contents = new_shell2->web_contents();
  WaitForLoadStop(new_contents);
  EXPECT_EQ("/files/title2.html", new_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell2, test_server()->GetURL("files/post_message.html"));
  EXPECT_EQ(orig_site_instance, new_contents->GetSiteInstance());
  RenderViewHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));
  EXPECT_FALSE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg',"
      "    'http://google.com'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  WindowedNotificationObserver title_observer(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(foo_contents));
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  title_observer.Wait();

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  WindowedNotificationObserver title_observer2(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(foo_contents));
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_contents,
      "window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  title_observer2.Wait();
  EXPECT_EQ(ASCIIToUTF16("msg2"), foo_contents->GetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // TODO(nasko): Test subframe targeting of postMessage once
  // http://crbug.com/153701 is fixed.
}

// Test for crbug.com/116192.  Navigations to a window's opener should
// still work after a process swap.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       AllowTargetedNavigationsInOpenerAfterSwap) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original tab and SiteInstance for later comparison.
  WebContents* orig_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      orig_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  TestNavigationObserver navigation_observer(orig_contents);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back into this process.
  scoped_refptr<SiteInstance> revisit_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);
}

// Test that opening a new window in the same SiteInstance and then navigating
// both windows to a different SiteInstance allows the first process to exit.
// See http://crbug.com/126333.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       ProcessExitWithSwappedOutViews) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened window to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first window.
  RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->HasConnection());

  // Navigate the first window to a different site as well.  The original
  // process should exit, since all of its views are now swapped out.
  WindowedNotificationObserver exit_observer(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      Source<RenderProcessHost>(orig_process));
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(new_site_instance, new_site_instance2);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, ClickLinkAfter204Error) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  // The links will point to the HTTPS server.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Load a cross-site page that fails with a 204 error.
  NavigateToURL(shell(), https_server.GetURL("nocontent"));

  // We should still be looking at the normal page.  The typed URL will
  // still be visible until the user clears it manually, but the last
  // committed URL will be the previous page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/nocontent",
            shell()->web_contents()->GetVisibleURL().path());
  EXPECT_EQ("/files/click-noreferrer-links.html",
            shell()->web_contents()->GetController().
                GetLastCommittedEntry()->GetVirtualURL().path());

  // Renderer-initiated navigations should work.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same tab.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/9682.  We should show the URL for a pending renderer-
// initiated navigation in a new tab, until the content of the initial
// about:blank page is modified by another window.  At that point, we should
// revert to showing about:blank to prevent a URL spoof.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, ShowLoadingURLUntilSpoof) {
  ASSERT_TRUE(test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(
      shell(), test_server()->GetURL("files/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickNoContentTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  WindowedNotificationObserver title_observer(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(contents));
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(modifyNewWindow());",
      &success));
  EXPECT_TRUE(success);
  title_observer.Wait();
  EXPECT_EQ(ASCIIToUTF16("Modified Title"), contents->GetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Test for crbug.com/9682.  We should not show the URL for a pending renderer-
// initiated navigation in a new tab if it is not the initial navigation.  In
// this case, the renderer will not notify us of a modification, so we cannot
// show the pending URL without allowing a spoof.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontShowLoadingURLIfNotInitialNav) {
  ASSERT_TRUE(test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(
      shell(), test_server()->GetURL("files/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  // By using an onclick handler that first creates the window, the slow
  // navigation is not considered an initial navigation.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send("
      "clickNoContentScriptedTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is not visible, because it is not the initial
  // navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_FALSE(contents->GetController().IsInitialNavigation());
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, BackForwardNotStale) {
  NavigateToURL(shell(), GURL(kAboutBlankURL));

  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Visit a page on first site.
  std::string replacement_path_a1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      test_server()->host_port_pair(),
      &replacement_path_a1));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path_a1));

  // Visit three pages on second site.
  std::string replacement_path_b1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      https_server.host_port_pair(),
      &replacement_path_b1));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b1));
  std::string replacement_path_b2;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title2.html",
      https_server.host_port_pair(),
      &replacement_path_b2));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b2));
  std::string replacement_path_b3;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title3.html",
      https_server.host_port_pair(),
      &replacement_path_b3));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b3));

  // History is now [blank, A1, B1, B2, *B3].
  WebContents* contents = shell()->web_contents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another window in same process to keep this process alive.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell, https_server.GetURL(replacement_path_b1));

  // Go back three times to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// Test for http://crbug.com/130016.
// Swapping out a render view should update its visiblity state.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwappedOutViewHasCorrectVisibilityState) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Open a same-site link in a new widnow.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  RenderViewHost* rvh = new_shell->web_contents()->GetRenderViewHost();

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.webkitVisibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site. This should swap out the
  // tab's existing RenderView, causing it become hidden.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.webkitVisibilityState == 'hidden');",
      &success));
  EXPECT_TRUE(success);

  // Going back should make the previously swapped-out view to become visible
  // again.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  EXPECT_EQ(rvh, new_shell->web_contents()->GetRenderViewHost());

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.webkitVisibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);
}

// This class holds onto RenderViewHostObservers for as long as their observed
// RenderViewHosts are alive. This allows us to confirm that all hosts have
// properly been shutdown.
class RenderViewHostObserverArray {
 public:
  ~RenderViewHostObserverArray() {
    // In case some would be left in there with a dead pointer to us.
    for (std::list<RVHObserver*>::iterator iter = observers_.begin();
         iter != observers_.end(); ++iter) {
      (*iter)->ClearParent();
    }
  }
  void AddObserverToRVH(RenderViewHost* rvh) {
    observers_.push_back(new RVHObserver(this, rvh));
  }
  size_t GetNumObservers() const {
    return observers_.size();
  }

 private:
  friend class RVHObserver;
  class RVHObserver : public RenderViewHostObserver {
   public:
    RVHObserver(RenderViewHostObserverArray* parent, RenderViewHost* rvh)
        : RenderViewHostObserver(rvh),
          parent_(parent) {
    }
    virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE {
      if (parent_)
        parent_->RemoveObserver(this);
      RenderViewHostObserver::RenderViewHostDestroyed(rvh);
    };
    void ClearParent() {
      parent_ = NULL;
    }
   private:
    RenderViewHostObserverArray* parent_;
  };

  void RemoveObserver(RVHObserver* observer) {
    observers_.remove(observer);
  }

  std::list<RVHObserver*> observers_;
};

// Test for crbug.com/90867. Make sure we don't leak render view hosts since
// they may cause crashes or memory corruptions when trying to call dead
// delegate_. This test also verifies crbug.com/117420 and crbug.com/143255 to
// ensure that a separate SiteInstance is created when navigating to view-source
// URLs, regardless of current URL.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, LeakingRenderViewHosts) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Observe the created render_view_host's to make sure they will not leak.
  RenderViewHostObserverArray rvh_observers;

  GURL navigated_url(test_server()->GetURL("files/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       navigated_url.spec());

  // Let's ensure that when we start with a blank window, navigating away to a
  // view-source URL, we create a new SiteInstance.
  RenderViewHost* blank_rvh = shell()->web_contents()->GetRenderViewHost();
  SiteInstance* blank_site_instance = blank_rvh->GetSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL::EmptyGURL());
  EXPECT_EQ(blank_site_instance->GetSiteURL(), GURL::EmptyGURL());
  rvh_observers.AddObserverToRVH(blank_rvh);

  // Now navigate to the view-source URL and ensure we got a different
  // SiteInstance and RenderViewHost.
  NavigateToURL(shell(), view_source_url);
  EXPECT_NE(blank_rvh, shell()->web_contents()->GetRenderViewHost());
  EXPECT_NE(blank_site_instance, shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance());
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  NavigateToURL(shell(), navigated_url);
  SiteInstance* site_instance1 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  NavigateToURL(shell(), view_source_url);
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());
  SiteInstance* site_instance2 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  NavigateToURL(shell(), https_server.GetURL("files/title2.html"));
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  // This used to leak a render view host.
  shell()->Close();

  RunAllPendingInMessageLoop();  // Needed on ChromeOS.

  EXPECT_EQ(0U, rvh_observers.GetNumObservers());
}

// Test for crbug.com/143155.  Frame tree updates during unload should not
// interrupt the intended navigation and show swappedout:// instead.
// Specifically:
// 1) Open 2 tabs in an HTTP SiteInstance, with a subframe in the opener.
// 2) Send the second tab to a different HTTPS SiteInstance.
//    This creates a swapped out opener for the first tab in the HTTPS process.
// 3) Navigate the first tab to the HTTPS SiteInstance, and have the first
//    tab's unload handler remove its frame.
// This used to cause an update to the frame tree of the swapped out RV,
// just as it was navigating to a real page.  That pre-empted the real
// navigation and visibly sent the tab to swappedout://.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontPreemptNavigationWithFrameTreeUpdate) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // 1. Load a page that deletes its iframe during unload.
  NavigateToURL(shell(),
                test_server()->GetURL("files/remove_frame_on_unload.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a same-site page in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(openWindow());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title1.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // 2. Send the second tab to a different process.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // 3. Send the first tab to the second tab's process.
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));

  // Make sure it ends up at the right page.
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(https_server.GetURL("files/title1.html"),
            shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(new_site_instance, shell()->web_contents()->GetSiteInstance());
}

}  // namespace content
