// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

namespace {
  void VerifyGPUProcessLaunch(bool* result) {
    GpuProcessHost* host =
        GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                            content::CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH);
    *result = !!host;
  }
}

class GpuFunctionalTest : public ContentBrowserTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kDisableGpuProcessPrelaunch);
  }

  void VerifyHardwareAccelerated(const std::string& feature) {
    NavigateToURL(shell(),
                  GURL(std::string(chrome::kChromeUIScheme).
                       append("://").
                       append(kChromeUIGpuHost)));

    {
      // Verify that the given feature is hardware accelerated..
      std::string javascript =
          "function VerifyHardwareAccelerated(feature) {"
          "  var list = document.querySelector(\".feature-status-list\");"
          "  for (var i=0; i < list.childElementCount; i++) {"
          "    var span_list = list.children[i].getElementsByTagName('span');"
          "    var feature_str = span_list[0].textContent;"
          "    var value_str = span_list[1].textContent;"
          "    if ((feature_str == feature) &&"
          "        (value_str == 'Hardware accelerated')) {"
          "      domAutomationController.send(\"success\");"
          "    }"
          "  }"
          "};";
      javascript.append("VerifyHardwareAccelerated(\"");
      javascript.append(feature);
      javascript.append("\");");
      std::string result;
      EXPECT_TRUE(ExecuteScriptAndExtractString(shell()->web_contents(),
                                                javascript,
                                                &result));
      EXPECT_EQ(result, "success");
    }
  }

  void VerifyGPUProcessOnPage(std::string filename, bool wait) {
    ASSERT_TRUE(test_server()->Start());
    DOMMessageQueue message_queue;

    std::string url("files/gpu/");
    GURL full_url = test_server()->GetURL(url.append(filename));
    NavigateToURL(shell(), full_url);

    if (wait) {
      std::string result_string;
      ASSERT_TRUE(message_queue.WaitForMessage(&result_string));
    }

    bool result = false;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VerifyGPUProcessLaunch, &result),
        base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
    EXPECT_TRUE(result);
  }

  base::FilePath gpu_test_dir_;
};

#if defined(OS_LINUX) && !defined(NDEBUG)
// http://crbug.com/254724
#define IF_NOT_DEBUG_LINUX(x) DISABLED_ ## x
#else
#define IF_NOT_DEBUG_LINUX(x) x
#endif

IN_PROC_BROWSER_TEST_F(
    GpuFunctionalTest,
    IF_NOT_DEBUG_LINUX(MANUAL_TestFeatureHardwareAccelerated)) {
  VerifyHardwareAccelerated("WebGL: ");
  VerifyHardwareAccelerated("Canvas: ");
  VerifyHardwareAccelerated("3D CSS: ");
}

// Verify that gpu process is spawned in webgl example.
IN_PROC_BROWSER_TEST_F(GpuFunctionalTest,
                       IF_NOT_DEBUG_LINUX(MANUAL_TestWebGL)) {
  VerifyGPUProcessOnPage("functional_webgl.html", false);
}

// Verify that gpu process is spawned when viewing a 2D canvas.
IN_PROC_BROWSER_TEST_F(GpuFunctionalTest,
                       IF_NOT_DEBUG_LINUX(MANUAL_Test2dCanvas)) {
  VerifyGPUProcessOnPage("functional_canvas_demo.html", false);
}

// Verify that gpu process is spawned when viewing a 3D CSS page.
IN_PROC_BROWSER_TEST_F(GpuFunctionalTest,
                       IF_NOT_DEBUG_LINUX(MANUAL_Test3dCss)) {
  VerifyGPUProcessOnPage("functional_3d_css.html", false);
}

#if defined(OS_LINUX)
// crbug.com/257109
#define MANUAL_TestGpuWithVideo DISABLED_TestGpuWithVideo
#endif

// Verify that gpu process is started when viewing video.
IN_PROC_BROWSER_TEST_F(GpuFunctionalTest,
                       MANUAL_TestGpuWithVideo) {
  VerifyGPUProcessOnPage("functional_video.html", true);
}

} // namespace content
