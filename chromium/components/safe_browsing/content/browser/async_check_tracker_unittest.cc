// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/url_checker_on_sb.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"

namespace safe_browsing {

namespace {

using security_interstitials::UnsafeResource;

class MockUIManager : public BaseUIManager {
 public:
  MockUIManager() : BaseUIManager() {}

  MockUIManager(const MockUIManager&) = delete;
  MockUIManager& operator=(const MockUIManager&) = delete;

  void DisplayBlockingPage(const UnsafeResource& resource) override {
    display_blocking_page_called_times_++;
    displayed_resource_ = resource;
  }

  int DisplayBlockingPageCalledTimes() {
    return display_blocking_page_called_times_;
  }

  UnsafeResource GetDisplayedResource() { return displayed_resource_; }

 protected:
  ~MockUIManager() override {}

 private:
  int display_blocking_page_called_times_ = 0;
  UnsafeResource displayed_resource_;
};

}  // namespace

class AsyncCheckTrackerTest : public content::RenderViewHostTestHarness,
                              public testing::WithParamInterface<bool> {
 protected:
  AsyncCheckTrackerTest()
      : RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    bool sb_on_ui_thread_enabled = GetParam();
    if (sb_on_ui_thread_enabled) {
      feature_list_.InitWithFeatures(
          {kSafeBrowsingAsyncRealTimeCheck, kSafeBrowsingOnUIThread}, {});
    } else {
      feature_list_.InitWithFeatures({kSafeBrowsingAsyncRealTimeCheck},
                                     {kSafeBrowsingOnUIThread});
    }
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    url_ = GURL("https://example.com/");
    EXPECT_CALL(mock_web_contents_getter_, Run())
        .WillRepeatedly(testing::Return(nullptr));
    ui_manager_ = base::MakeRefCounted<MockUIManager>();
    tracker_ = AsyncCheckTracker::GetOrCreateForWebContents(web_contents(),
                                                            ui_manager_.get());
  }

  void TearDown() override {
    tracker_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  void CallDidFinishNavigation(content::MockNavigationHandle& handle,
                               bool has_committed) {
    handle.set_has_committed(has_committed);
    tracker_->DidFinishNavigation(&handle);
    task_environment()->RunUntilIdle();
  }

  void CallPendingCheckerCompleted(int64_t navigation_id,
                                   bool proceed,
                                   bool has_post_commit_interstitial_skipped,
                                   bool all_checks_completed) {
    if (!proceed) {
      // This mocks how BaseUIManager caches unsafe resource if
      // load_post_commit_error_page is false.
      UnsafeResource resource;
      resource.url = url_;
      resource.threat_type = SB_THREAT_TYPE_URL_PHISHING;
      resource.navigation_id = navigation_id;
      ui_manager_->AddUnsafeResource(url_, resource);
    }
    UrlCheckerOnSB::OnCompleteCheckResult result(
        proceed, /*showed_interstitial=*/true,
        has_post_commit_interstitial_skipped,
        SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
        all_checks_completed);
    tracker_->PendingCheckerCompleted(navigation_id, result);
    task_environment()->RunUntilIdle();
  }

  void CallTransferUrlChecker(int64_t navigation_id) {
    auto checker = std::make_unique<UrlCheckerOnSB>(
        /*delegate_getter=*/base::NullCallback(), /*frame_tree_node_id=*/-1,
        navigation_id, mock_web_contents_getter_.Get(),
        /*complete_callback=*/base::NullCallback(),
        /*url_real_time_lookup_enabled=*/false,
        /*can_urt_check_subresource_url=*/false, /*can_check_db=*/true,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/"",
        /*url_lookup_service=*/nullptr,
        /*hash_realtime_service=*/nullptr,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);
    checker->AddUrlInRedirectChainForTesting(url_);
    tracker_->TransferUrlChecker(std::move(checker));
  }

  base::test::ScopedFeatureList feature_list_;
  GURL url_;
  base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
      mock_web_contents_getter_;
  scoped_refptr<MockUIManager> ui_manager_;
  raw_ptr<AsyncCheckTracker> tracker_;
};

INSTANTIATE_TEST_SUITE_P(SafeBrowsingOnUIThreadEnabled,
                         AsyncCheckTrackerTest,
                         testing::Bool());

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PendingCheckNotFound) {
  content::MockNavigationHandle handle(url_, main_rfh());
  // This can happen when the complete callback is scheduled before the checker
  // is scheduled to be deleted on the SB thread. Mock this scenario by not
  // calling CallTransferUrlChecker.
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true,
                              /*all_checks_completed=*/true);
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PendingCheckNotCompleted) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PendingCheckProceed) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/true,
                              /*has_post_commit_interstitial_skipped=*/false,
                              /*all_checks_completed=*/true);
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PostCommitInterstitialNotSkipped) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/false,
                              /*all_checks_completed=*/true);
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_NavigationNotCommitted) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true,
                              /*all_checks_completed=*/true);
  CallDidFinishNavigation(handle, /*has_committed=*/false);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_P(AsyncCheckTrackerTest, DisplayBlockingPageCalled) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true,
                              /*all_checks_completed=*/true);
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 1);
  UnsafeResource resource = ui_manager_->GetDisplayedResource();
  EXPECT_EQ(resource.threat_type, SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_EQ(resource.url, url_);
  EXPECT_EQ(resource.render_process_id, main_rfh()->GetGlobalId().child_id);
  EXPECT_EQ(resource.render_frame_token, main_rfh()->GetFrameToken().value());
}

TEST_P(AsyncCheckTrackerTest,
       DisplayBlockingPageCalled_DidFinishNavigationCalledFirst) {
  content::MockNavigationHandle handle(url_, main_rfh());
  CallTransferUrlChecker(handle.GetNavigationId());
  CallDidFinishNavigation(handle, /*has_committed=*/true);
  // Usually has_post_commit_interstitial_skipped is false if
  // DidFinishNavigation is already called. It can be true if
  // DidFinishNavigation happens to be called between when
  // PendingCheckerCompleted is scheduled and when it is run.
  CallPendingCheckerCompleted(handle.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true,
                              /*all_checks_completed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 1);
  UnsafeResource resource = ui_manager_->GetDisplayedResource();
  EXPECT_EQ(resource.threat_type, SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_EQ(resource.url, url_);
  EXPECT_EQ(resource.render_process_id, main_rfh()->GetGlobalId().child_id);
  EXPECT_EQ(resource.render_frame_token, main_rfh()->GetFrameToken().value());
}

TEST_P(AsyncCheckTrackerTest, IsMainPageLoadPending) {
  base::HistogramTester histograms;
  content::MockNavigationHandle handle(web_contents());
  UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  resource.frame_tree_node_id = main_rfh()->GetFrameTreeNodeId();
  resource.navigation_id = handle.GetNavigationId();

  AsyncCheckTracker* tracker =
      AsyncCheckTracker::FromWebContents(web_contents());
  EXPECT_TRUE(AsyncCheckTracker::IsMainPageLoadPending(resource));

  tracker->DidFinishNavigation(&handle);
  // The navigation is not committed.
  EXPECT_TRUE(AsyncCheckTracker::IsMainPageLoadPending(resource));
  histograms.ExpectUniqueSample(
      "SafeBrowsing.AsyncCheck.CommittedNavigationIdsSize",
      /*sample=*/0,
      /*expected_count=*/1);

  handle.set_has_committed(true);
  tracker->DidFinishNavigation(&handle);
  EXPECT_FALSE(AsyncCheckTracker::IsMainPageLoadPending(resource));
  histograms.ExpectBucketCount(
      "SafeBrowsing.AsyncCheck.CommittedNavigationIdsSize",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_P(AsyncCheckTrackerTest, IsMainPageLoadPending_NoNavigationId) {
  content::MockNavigationHandle handle(web_contents());
  UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  resource.frame_tree_node_id = main_rfh()->GetFrameTreeNodeId();

  EXPECT_TRUE(AsyncCheckTracker::IsMainPageLoadPending(resource));

  // If there is no navigation id associated with the resource, whether the
  // main page load is pending is determined by
  // UnsafeResource::IsMainPageLoadPendingWithSyncCheck.
  resource.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
  EXPECT_FALSE(AsyncCheckTracker::IsMainPageLoadPending(resource));
}

TEST_P(AsyncCheckTrackerTest,
       PendingCheckersManagement_TransferWithSameNavigationId) {
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 0u);
  CallTransferUrlChecker(/*navigation_id=*/1);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 1u);
  CallTransferUrlChecker(/*navigation_id=*/2);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 2u);
  // Transfer a checker with the same navigation id. This scenario can be
  // triggered by HTTP client hints.
  CallTransferUrlChecker(/*navigation_id=*/2);
  // The previous checker should be deleted. The deletion should happen on the
  // SB thread.
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 2u);
}

TEST_P(AsyncCheckTrackerTest,
       PendingCheckersManagement_DeleteOldCheckersAfterDidFinishNavigation) {
  base::HistogramTester histograms;
  content::MockNavigationHandle handle_1(url_, main_rfh());
  content::MockNavigationHandle handle_2(url_, main_rfh());
  content::MockNavigationHandle handle_3(url_, main_rfh());
  CallTransferUrlChecker(handle_1.GetNavigationId());
  histograms.ExpectUniqueSample("SafeBrowsing.AsyncCheck.PendingCheckersSize",
                                /*sample=*/1,
                                /*expected_count=*/1);
  CallTransferUrlChecker(handle_2.GetNavigationId());
  histograms.ExpectBucketCount("SafeBrowsing.AsyncCheck.PendingCheckersSize",
                               /*sample=*/2,
                               /*expected_count=*/1);
  CallTransferUrlChecker(handle_3.GetNavigationId());
  histograms.ExpectBucketCount("SafeBrowsing.AsyncCheck.PendingCheckersSize",
                               /*sample=*/3,
                               /*expected_count=*/1);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 3u);

  // Only the third navigation is committed successfully.
  CallDidFinishNavigation(handle_1, /*has_committed=*/false);
  CallDidFinishNavigation(handle_2, /*has_committed=*/false);
  CallDidFinishNavigation(handle_3, /*has_committed=*/true);
  // Only keep the checker for the committed navigation.
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 1u);

  CallPendingCheckerCompleted(handle_3.GetNavigationId(), /*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true,
                              /*all_checks_completed=*/true);
  // The remaining checker is deleted because proceed is false.
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 0u);
}

TEST_P(AsyncCheckTrackerTest,
       PendingCheckersManagement_CheckerNotDeletedIfAllChecksCompletedFalse) {
  CallTransferUrlChecker(/*navigation_id=*/1);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 1u);

  CallPendingCheckerCompleted(/*navigation_id=*/1, /*proceed=*/true,
                              /*has_post_commit_interstitial_skipped=*/false,
                              /*all_checks_completed=*/false);
  // If all_checks_completed is false, the checker should be kept alive to
  // receive upcoming result from the checker. This scenario can happen if there
  // are server redirects.
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 1u);

  CallPendingCheckerCompleted(/*navigation_id=*/1, /*proceed=*/true,
                              /*has_post_commit_interstitial_skipped=*/false,
                              /*all_checks_completed=*/true);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 0u);
}

TEST_P(AsyncCheckTrackerTest,
       PendingCheckersManagement_DestructWithPendingCheckers) {
  CallTransferUrlChecker(/*navigation_id=*/1);
  CallTransferUrlChecker(/*navigation_id=*/2);
  EXPECT_EQ(tracker_->PendingCheckersSizeForTesting(), 2u);

  tracker_ = nullptr;
  DeleteContents();
  // Tracker is deleted together with the WebContents. pending checkers that the
  // tracker currently owns should also be deleted on the SB thread.
}

}  // namespace safe_browsing
