// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_updater.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sequenced_task_runner.h"
#include "base/sha1.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/chrome_extension_policy.pb.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtension2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtension3[] = "cccccccccccccccccccccccccccccccc";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestDownload2[] = "http://example.com/getpolicy?id=456";
const char kTestDownload3[] = "http://example.com/getpolicy?id=789";
const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disabled\""
    "  },"
    "  \"Second\": {"
    "    \"Value\": \"maybe\","
    "    \"Level\": \"Recommended\""
    "  }"
    "}";

class MockComponentCloudPolicyStoreDelegate
    : public ComponentCloudPolicyStore::Delegate {
 public:
  virtual ~MockComponentCloudPolicyStoreDelegate() {}

  MOCK_METHOD0(OnComponentCloudPolicyStoreUpdated, void());
};

}  // namespace

class ComponentCloudPolicyUpdaterTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<em::PolicyFetchResponse> CreateResponse();

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<ResourceCache> cache_;
  scoped_ptr<ComponentCloudPolicyStore> store_;
  MockComponentCloudPolicyStoreDelegate store_delegate_;
  net::TestURLFetcherFactory fetcher_factory_;
  scoped_ptr<ExternalPolicyDataFetcherBackend> fetcher_backend_;
  scoped_ptr<ComponentCloudPolicyUpdater> updater_;
  ComponentPolicyBuilder builder_;
  PolicyBundle expected_bundle_;
};

void ComponentCloudPolicyUpdaterTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  task_runner_ = new base::TestSimpleTaskRunner();
  cache_.reset(new ResourceCache(temp_dir_.path(), task_runner_));
  store_.reset(new ComponentCloudPolicyStore(&store_delegate_, cache_.get()));
  store_->SetCredentials(ComponentPolicyBuilder::kFakeUsername,
                         ComponentPolicyBuilder::kFakeToken);
  fetcher_factory_.set_remove_fetcher_on_delete(true);
  fetcher_backend_.reset(new ExternalPolicyDataFetcherBackend(
      task_runner_,
      scoped_refptr<net::URLRequestContextGetter>()));
  updater_.reset(new ComponentCloudPolicyUpdater(
      task_runner_,
      fetcher_backend_->CreateFrontend(task_runner_),
      store_.get()));
  ASSERT_EQ(store_->policy().end(), store_->policy().begin());

  builder_.policy_data().set_policy_type(
      dm_protocol::kChromeExtensionPolicyType);
  builder_.policy_data().set_settings_entity_id(kTestExtension);
  builder_.payload().set_download_url(kTestDownload);
  builder_.payload().set_secure_hash(base::SHA1HashString(kTestPolicy));

  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  PolicyMap& policy = expected_bundle_.Get(ns);
  policy.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("disabled"), NULL);
  policy.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("maybe"), NULL);
}

void ComponentCloudPolicyUpdaterTest::TearDown() {
  updater_.reset();
  task_runner_->RunUntilIdle();
}

scoped_ptr<em::PolicyFetchResponse>
    ComponentCloudPolicyUpdaterTest::CreateResponse() {
  builder_.Build();
  return make_scoped_ptr(new em::PolicyFetchResponse(builder_.policy()));
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchAndCache) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that a download has been started.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Complete the download.
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseTooLarge) {
  // Submit a policy fetch response that exceeds the allowed maximum size.
  std::string long_download("http://example.com/get?id=");
  long_download.append(20 * 1024, '1');
  builder_.payload().set_download_url(long_download);
  updater_->UpdateExternalPolicy(CreateResponse());

  // Submit two valid policy fetch responses.
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the first policy fetch response has been ignored and downloads
  // have been started for the next two fetch responses instead.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());
  fetcher = fetcher_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload3), fetcher->GetOriginalURL());
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyFetchResponseInvalid) {
  // Submit an invalid policy fetch response.
  builder_.policy_data().set_username("wronguser@example.com");
  updater_->UpdateExternalPolicy(CreateResponse());

  // Submit two valid policy fetch responses.
  builder_.policy_data().set_username(ComponentPolicyBuilder::kFakeUsername);
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the first policy fetch response has been ignored and downloads
  // have been started for the next two fetch responses instead.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());
  fetcher = fetcher_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload3), fetcher->GetOriginalURL());
}

TEST_F(ComponentCloudPolicyUpdaterTest, AlreadyCached) {
  // Cache policy for an extension.
  builder_.Build();
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(ns,
                            builder_.GetBlob(),
                            base::SHA1HashString(kTestPolicy),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Submit a policy fetch response whose extension ID and hash match the
  // already cached policy.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that no download has been started.
  EXPECT_FALSE(fetcher_factory_.GetFetcherByID(0));
}

TEST_F(ComponentCloudPolicyUpdaterTest, PolicyDataInvalid) {
  // Submit three policy fetch responses.
  updater_->UpdateExternalPolicy(CreateResponse());
  builder_.payload().set_download_url(kTestDownload2);
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  updater_->UpdateExternalPolicy(CreateResponse());
  builder_.policy_data().set_settings_entity_id(kTestExtension3);
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the first download has been started.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Verify that the second download has been started.
  fetcher = fetcher_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // Indicate that the policy data size will exceed allowed maximum.
  fetcher->delegate()->OnURLFetchDownloadProgress(fetcher, 6 * 1024 * 1024, -1);
  task_runner_->RunUntilIdle();

  // Verify that the third download has been started.
  fetcher = fetcher_factory_.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload3), fetcher->GetOriginalURL());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedData) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the first download has been started.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Submit a second policy fetch response for the same extension with an
  // updated download URL.
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the first download is no longer running.
  EXPECT_FALSE(fetcher_factory_.GetFetcherByID(0));

  // Verify that the second download has been started.
  fetcher = fetcher_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedDataWithoutPolicy) {
  // Submit a policy fetch response.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the download has been started.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Complete the download.
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Verify that the downloaded policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Submit a second policy fetch response for the same extension with no
  // download URL, meaning that no policy should be provided for this extension.
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  updater_->UpdateExternalPolicy(CreateResponse());
  Mock::VerifyAndClearExpectations(&store_delegate_);
  task_runner_->RunUntilIdle();

  // Verify that no download has been started.
  EXPECT_FALSE(fetcher_factory_.GetFetcherByID(1));

  // Verify that the policy is no longer being served.
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(store_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyUpdaterTest, NoPolicy) {
  // Submit a policy fetch response with a valid download URL.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the download has been started.
  EXPECT_TRUE(fetcher_factory_.GetFetcherByID(0));

  // Update the policy fetch response before the download has finished. The new
  // policy fetch response has no download URL.
  builder_.payload().Clear();
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the download is no longer running.
  EXPECT_FALSE(fetcher_factory_.GetFetcherByID(0));
}

TEST_F(ComponentCloudPolicyUpdaterTest, CancelUpdate) {
  // Submit a policy fetch response with a valid download URL.
  updater_->UpdateExternalPolicy(CreateResponse());
  task_runner_->RunUntilIdle();

  // Verify that the download has been started.
  EXPECT_TRUE(fetcher_factory_.GetFetcherByID(0));

  // Now cancel that update before the download completes.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated()).Times(0);
  updater_->CancelUpdate(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kTestExtension));
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(fetcher_factory_.GetFetcherByID(0));
}

}  // namespace policy
