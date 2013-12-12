// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_test_config.h"
#include "net/base/net_util.h"

namespace content {

// Run the tests with a memory limit of 256MB, and give
// and extra 4MB of wiggle-room for over-allocation.
const char* kMemoryLimitMBSwitch = "256";
const size_t kMemoryLimitMB = 256;
const size_t kSingleTabLimitMB = 128;
const size_t kWiggleRoomMB = 4;

// Observer to report GPU memory usage when requested.
class GpuMemoryBytesAllocatedObserver : public GpuDataManagerObserver {
 public:
  GpuMemoryBytesAllocatedObserver()
      : bytes_allocated_(0) {
  }

  virtual ~GpuMemoryBytesAllocatedObserver() {
  }

  virtual void OnVideoMemoryUsageStatsUpdate(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats) OVERRIDE {
    bytes_allocated_ = video_memory_usage_stats.bytes_allocated;
    message_loop_runner_->Quit();
  }

  size_t GetBytesAllocated() {
    message_loop_runner_ = new MessageLoopRunner;
    GpuDataManager::GetInstance()->AddObserver(this);
    GpuDataManager::GetInstance()->RequestVideoMemoryUsageStatsUpdate();
    message_loop_runner_->Run();
    GpuDataManager::GetInstance()->RemoveObserver(this);
    message_loop_runner_ = NULL;
    return bytes_allocated_;
  }

 private:
  size_t bytes_allocated_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

class GpuMemoryTest : public ContentBrowserTest {
 public:
  GpuMemoryTest()
      : allow_tests_to_run_(false),
        has_used_first_shell_(false) {
  }
  virtual ~GpuMemoryTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableLogging);
    command_line->AppendSwitchASCII(switches::kForceGpuMemAvailableMb,
                                    kMemoryLimitMBSwitch);
    // Only run this on GPU bots for now. These tests should work with
    // any GPU process, but may be slow.
    if (command_line->HasSwitch(switches::kUseGpuInTests)) {
      allow_tests_to_run_ = true;
    }
    // Don't enable these tests on Android just yet (they use lots of memory and
    // may not be stable).
#if defined(OS_ANDROID)
    allow_tests_to_run_ = false;
#endif
  }

  enum PageType {
    PAGE_CSS3D,
    PAGE_WEBGL,
  };

  // Load a page and consume a specified amount of GPU memory.
  void LoadPage(Shell* tab_to_load,
                PageType page_type,
                size_t mb_to_use) {
    base::FilePath url;
    switch (page_type) {
      case PAGE_CSS3D:
        url = gpu_test_dir_.AppendASCII("mem_css3d.html");
        break;
      case PAGE_WEBGL:
        url = gpu_test_dir_.AppendASCII("mem_webgl.html");
        break;
    }

    NavigateToURL(tab_to_load, net::FilePathToFileURL(url));
    std::ostringstream js_call;
    js_call << "useGpuMemory(";
    js_call << mb_to_use;
    js_call << ");";
    std::string message;
    ASSERT_TRUE(ExecuteScriptInFrameAndExtractString(
        tab_to_load->web_contents(), std::string(), js_call.str(), &message));
    EXPECT_EQ("DONE_USE_GPU_MEMORY", message);
  }

  // Create a new tab.
  Shell* CreateNewTab() {
    // The ContentBrowserTest will create one shell by default, use that one
    // first so that we don't confuse the memory manager into thinking there
    // are more windows than there are.
    Shell* new_tab = has_used_first_shell_ ? CreateBrowser() : shell();
    has_used_first_shell_ = true;
    tabs_.insert(new_tab);
    visible_tabs_.insert(new_tab);
    return new_tab;
  }

  void SetTabBackgrounded(Shell* tab_to_background) {
    ASSERT_TRUE(
        visible_tabs_.find(tab_to_background) != visible_tabs_.end());
    visible_tabs_.erase(tab_to_background);
    tab_to_background->web_contents()->WasHidden();
  }

  bool MemoryUsageInRange(size_t low, size_t high) {
    FinishGpuMemoryChanges();
    size_t memory_usage_bytes = GetMemoryUsageMbytes();

    // If it's not immediately the case that low <= usage <= high, then
    // allow
    // Because we haven't implemented the full delay in FinishGpuMemoryChanges,
    // keep re-reading the GPU memory usage for 2 seconds before declaring
    // failure.
    base::Time start_time = base::Time::Now();
    while (low > memory_usage_bytes || memory_usage_bytes > high) {
      memory_usage_bytes = GetMemoryUsageMbytes();
      base::TimeDelta delta = base::Time::Now() - start_time;
      if (delta.InMilliseconds() >= 2000)
        break;
    }

    return (low <= memory_usage_bytes && memory_usage_bytes <= high);
  }

  bool AllowTestsToRun() const {
    return allow_tests_to_run_;
  }

 private:
  void FinishGpuMemoryChanges() {
    // This should wait until all effects of memory management complete.
    // We will need to wait until all
    // 1. pending commits from the main thread to the impl thread in the
    //    compositor complete (for visible compositors).
    // 2. allocations that the renderer's impl thread will make due to the
    //    compositor and WebGL are completed.
    // 3. pending GpuMemoryManager::Manage() calls to manage are made.
    // 4. renderers' OnMemoryAllocationChanged callbacks in response to
    //    manager are made.
    // Each step in this sequence can cause trigger the next (as a 1-2-3-4-1
    // cycle), so we will need to pump this cycle until it stabilizes.

    // Pump the cycle 8 times (in principle it could take an infinite number
    // of iterations to settle).
    for (size_t pump_it = 0; pump_it < 8; ++pump_it) {
      // Wait for a RequestAnimationFrame to complete from all visible tabs
      // for stage 1 of the cycle.
      for (std::set<Shell*>::iterator it = visible_tabs_.begin();
           it != visible_tabs_.end();
           ++it) {
        std::string js_call(
            "window.webkitRequestAnimationFrame(function() {"
            "  domAutomationController.setAutomationId(1);"
            "  domAutomationController.send(\"DONE_RAF\");"
            "})");
        std::string message;
        ASSERT_TRUE(ExecuteScriptInFrameAndExtractString(
            (*it)->web_contents(), std::string(), js_call, &message));
        EXPECT_EQ("DONE_RAF", message);
      }
      // TODO(ccameron): send an IPC from Browser -> Renderer (delay it until
      // painting finishes) -> GPU process (delay it until any pending manages
      // happen) -> All Renderers -> Browser to flush parts 2, 3, and 4.
    }
  }

  size_t GetMemoryUsageMbytes() {
    GpuMemoryBytesAllocatedObserver observer;
    observer.GetBytesAllocated();
    return observer.GetBytesAllocated() / 1048576;
  }

  bool allow_tests_to_run_;
  std::set<Shell*> tabs_;
  std::set<Shell*> visible_tabs_;
  bool has_used_first_shell_;
  base::FilePath gpu_test_dir_;
};

#if defined(OS_LINUX) && !defined(NDEBUG)
// http://crbug.com/254724
#define MAYBE(x) DISABLED_ ## x
#elif defined(OS_WIN) && defined(USE_AURA)
// http://crbug.com/292882
#define MAYBE(x) DISABLED_ ## x
#else
#define MAYBE(x) x
#endif

// When trying to load something that doesn't fit into our total GPU memory
// limit, we shouldn't exceed that limit.
IN_PROC_BROWSER_TEST_F(GpuMemoryTest,
                       MAYBE(SingleWindowDoesNotExceedLimit)) {
  if (!AllowTestsToRun())
    return;

  Shell* tab = CreateNewTab();
  LoadPage(tab, PAGE_CSS3D, kMemoryLimitMB);
  // Make sure that the CSS3D page maxes out a single tab's budget (otherwise
  // the test doesn't test anything) but still stays under the limit.
  EXPECT_TRUE(MemoryUsageInRange(
     kSingleTabLimitMB - kWiggleRoomMB,
     kMemoryLimitMB + kWiggleRoomMB));
}

}  // namespace content
