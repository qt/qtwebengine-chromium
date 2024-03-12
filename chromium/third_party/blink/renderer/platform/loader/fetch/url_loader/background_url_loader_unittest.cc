// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_url_loader.h"
#include <deque>

#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/background_code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/gurl.h"

namespace blink {
namespace {

constexpr char kTestURL[] = "http://example.com/";
constexpr char kRedirectedURL[] = "http://example.com/redirected";
constexpr int kMaxBufferedBytesPerProcess = 1000;
constexpr std::string kTestBodyString = "test data.";

using LoadStartCallback = base::OnceCallback<void(
    mojo::PendingReceiver<network::mojom::URLLoader>,
    mojo::PendingRemote<network::mojom::URLLoaderClient>)>;

mojo::ScopedDataPipeConsumerHandle CreateDataPipeConsumerHandleFilledWithString(
    const std::string& string) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
           MOJO_RESULT_OK);
  CHECK(mojo::BlockingCopyFromString(string, producer_handle));
  return consumer_handle;
}

mojo::ScopedDataPipeConsumerHandle CreateTestBody() {
  return CreateDataPipeConsumerHandleFilledWithString(kTestBodyString);
}

mojo_base::BigBuffer CreateTestCachedMetaData() {
  return mojo_base::BigBuffer(std::vector<uint8_t>({1, 2, 3, 4, 5}));
}

std::unique_ptr<network::ResourceRequest> CreateTestRequest() {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kTestURL);
  return request;
}

network::mojom::URLResponseHeadPtr CreateTestResponse() {
  auto response = network::mojom::URLResponseHead::New();
  response->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response->mime_type = "text/html";
  return response;
}

class FakeBackForwardCacheLoaderHelper : public BackForwardCacheLoaderHelper {
 public:
  FakeBackForwardCacheLoaderHelper() = default;
  ~FakeBackForwardCacheLoaderHelper() = default;

  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason reason) override {
    evicted_reason_ = reason;
  }
  void DidBufferLoadWhileInBackForwardCache(bool update_process_wide_count,
                                            size_t num_bytes) override {
    EXPECT_FALSE(update_process_wide_count);
    total_bytes_buffered_while_in_back_forward_cache_ += num_bytes;
  }
  void Detach() override {}
  void Trace(Visitor* visitor) const override {
    BackForwardCacheLoaderHelper::Trace(visitor);
  }

  const absl::optional<mojom::blink::RendererEvictionReason>& evicted_reason()
      const {
    return evicted_reason_;
  }
  size_t total_bytes_buffered_while_in_back_forward_cache() const {
    return total_bytes_buffered_while_in_back_forward_cache_;
  }

 private:
  absl::optional<mojom::blink::RendererEvictionReason> evicted_reason_;
  size_t total_bytes_buffered_while_in_back_forward_cache_ = 0;
};

class FakeURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  // This SharedURLLoaderFactory is cloned and passed to the background thread
  // via PendingFactory. `load_start_callback` will be called in the background
  // thread.
  explicit FakeURLLoaderFactory(LoadStartCallback load_start_callback)
      : load_start_callback_(std::move(load_start_callback)) {}
  FakeURLLoaderFactory(const FakeURLLoaderFactory&) = delete;
  FakeURLLoaderFactory& operator=(const FakeURLLoaderFactory&) = delete;
  ~FakeURLLoaderFactory() override = default;

  // network::SharedURLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    CHECK(load_start_callback_);
    std::move(load_start_callback_).Run(std::move(loader), std::move(client));
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    // Pass |this| as the receiver context to make sure this object stays alive
    // while it still has receivers.
    receivers_.Add(this, std::move(receiver), this);
  }
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    CHECK(load_start_callback_);
    return std::make_unique<PendingFactory>(std::move(load_start_callback_));
  }

 private:
  class PendingFactory : public network::PendingSharedURLLoaderFactory {
   public:
    explicit PendingFactory(LoadStartCallback load_start_callback)
        : load_start_callback_(std::move(load_start_callback)) {}
    PendingFactory(const PendingFactory&) = delete;
    PendingFactory& operator=(const PendingFactory&) = delete;
    ~PendingFactory() override = default;

   protected:
    scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
      CHECK(load_start_callback_);
      return base::MakeRefCounted<FakeURLLoaderFactory>(
          std::move(load_start_callback_));
    }

   private:
    LoadStartCallback load_start_callback_;
  };

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    scoped_refptr<FakeURLLoaderFactory>>
      receivers_;
  LoadStartCallback load_start_callback_;
};

class FakeBackgroundResourceFetchAssets
    : public WebBackgroundResourceFetchAssets {
 public:
  explicit FakeBackgroundResourceFetchAssets(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      LoadStartCallback load_start_callback)
      : background_task_runner_(std::move(background_task_runner)),
        pending_loader_factory_(base::MakeRefCounted<FakeURLLoaderFactory>(
                                    std::move(load_start_callback))
                                    ->Clone()) {}
  FakeBackgroundResourceFetchAssets(const FakeBackgroundResourceFetchAssets&) =
      delete;
  FakeBackgroundResourceFetchAssets& operator=(
      const FakeBackgroundResourceFetchAssets&) = delete;
  ~FakeBackgroundResourceFetchAssets() override {
    if (url_loader_factory_) {
      // `url_loader_factory_` must be released in the background thread.
      background_task_runner_->ReleaseSoon(FROM_HERE,
                                           std::move(url_loader_factory_));
    }
  }

  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner() override {
    return background_task_runner_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetLoaderFactory() override {
    if (!url_loader_factory_) {
      url_loader_factory_ = network::SharedURLLoaderFactory::Create(
          std::move(pending_loader_factory_));
    }
    return url_loader_factory_;
  }

  URLLoaderThrottleProvider* GetThrottleProvider() override { return nullptr; }
  const blink::LocalFrameToken& GetLocalFrameToken() override {
    return local_frame_token_;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const blink::LocalFrameToken local_frame_token_;
};

class FakeURLLoaderClient : public URLLoaderClient {
 public:
  explicit FakeURLLoaderClient(
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner)
      : unfreezable_task_runner_(std::move(unfreezable_task_runner)) {}

  FakeURLLoaderClient(const FakeURLLoaderClient&) = delete;
  FakeURLLoaderClient& operator=(const FakeURLLoaderClient&) = delete;

  ~FakeURLLoaderClient() override = default;

  using WillFollowRedirectCallback =
      base::OnceCallback<bool(const WebURL& new_url)>;
  void AddWillFollowRedirectCallback(
      WillFollowRedirectCallback will_follow_callback) {
    will_follow_callbacks_.push_back(std::move(will_follow_callback));
  }

  // URLLoaderClient implementation:
  bool WillFollowRedirect(const WebURL& new_url,
                          const net::SiteForCookies& new_site_for_cookies,
                          const WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const WebString& new_method,
                          const WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers,
                          std::vector<std::string>*,
                          net::HttpRequestHeaders&,
                          bool insecure_scheme_was_upgraded) override {
    DCHECK(unfreezable_task_runner_->BelongsToCurrentThread());
    DCHECK(!will_follow_callbacks_.empty());
    WillFollowRedirectCallback will_follow_callback =
        std::move(will_follow_callbacks_.front());
    will_follow_callbacks_.pop_front();
    return std::move(will_follow_callback).Run(new_url);
  }
  void DidSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override {
    NOTREACHED();
  }
  void DidReceiveResponse(
      const WebURLResponse& response,
      mojo::ScopedDataPipeConsumerHandle response_body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override {
    DCHECK(unfreezable_task_runner_->BelongsToCurrentThread());
    DCHECK(!response_);
    DCHECK(!response_body_);
    response_ = response;
    cached_metadata_ = std::move(cached_metadata);
    response_body_ = std::move(response_body);
  }
  void DidReceiveData(const char* data, size_t dataLength) override {
    NOTREACHED();
  }
  void DidReceiveTransferSizeUpdate(int transfer_size_diff) override {
    DCHECK(unfreezable_task_runner_->BelongsToCurrentThread());
    transfer_size_diffs_.push_back(transfer_size_diff);
  }
  void DidFinishLoading(base::TimeTicks finishTime,
                        int64_t totalEncodedDataLength,
                        uint64_t totalEncodedBodyLength,
                        int64_t totalDecodedBodyLength) override {
    DCHECK(unfreezable_task_runner_->BelongsToCurrentThread());
    did_finish_ = true;
  }
  void DidFail(const WebURLError& error,
               base::TimeTicks finishTime,
               int64_t totalEncodedDataLength,
               uint64_t totalEncodedBodyLength,
               int64_t totalDecodedBodyLength) override {
    DCHECK(unfreezable_task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(did_finish_);
    error_ = error;
  }

  const absl::optional<WebURLResponse>& response() const { return response_; }
  const absl::optional<mojo_base::BigBuffer>& cached_metadata() const {
    return cached_metadata_;
  }
  const mojo::ScopedDataPipeConsumerHandle& response_body() const {
    return response_body_;
  }
  const std::vector<int>& transfer_size_diffs() const {
    return transfer_size_diffs_;
  }
  bool did_finish() const { return did_finish_; }
  const absl::optional<WebURLError>& error() const { return error_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner_;

  std::deque<WillFollowRedirectCallback> will_follow_callbacks_;

  absl::optional<WebURLResponse> response_;
  absl::optional<mojo_base::BigBuffer> cached_metadata_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  std::vector<int> transfer_size_diffs_;
  bool did_finish_ = false;
  absl::optional<WebURLError> error_;
};

struct PriorityInfo {
  net::RequestPriority priority;
  int32_t intra_priority_value;
};

class FakeURLLoader : public network::mojom::URLLoader {
 public:
  explicit FakeURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  FakeURLLoader(const FakeURLLoader&) = delete;
  FakeURLLoader& operator=(const FakeURLLoader&) = delete;
  ~FakeURLLoader() override = default;

  // network::mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {
    follow_redirect_called_ = true;
  }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    set_priority_log_.push_back(PriorityInfo{
        .priority = priority, .intra_priority_value = intra_priority_value});
  }
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  bool follow_redirect_called() const { return follow_redirect_called_; }
  const std::vector<PriorityInfo>& set_priority_log() const {
    return set_priority_log_;
  }

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

 private:
  bool follow_redirect_called_ = false;
  std::vector<PriorityInfo> set_priority_log_;
  mojo::Receiver<network::mojom::URLLoader> receiver_;
};

// Sets up the message sender override for the unit test.
class BackgroundResourceFecherTest : public testing::Test {
 public:
  explicit BackgroundResourceFecherTest()
      : unfreezable_task_runner_(
            base::MakeRefCounted<scheduler::FakeTaskRunner>()) {}

  BackgroundResourceFecherTest(const BackgroundResourceFecherTest&) = delete;
  BackgroundResourceFecherTest& operator=(const BackgroundResourceFecherTest&) =
      delete;
  ~BackgroundResourceFecherTest() override = default;

  // testing::Test implementation:
  void SetUp() override {
    background_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({});
    WebRuntimeFeatures::EnableBackForwardCache(true);
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLoadingTasksUnfreezable,
          {{"max_buffered_bytes_per_process",
            base::NumberToString(kMaxBufferedBytesPerProcess)}}}},
        {});
    bfcache_loader_helper_ =
        MakeGarbageCollected<FakeBackForwardCacheLoaderHelper>();
  }
  void TearDown() override {
    // Need to run tasks to avoid memory leak.
    task_environment_.RunUntilIdle();
    unfreezable_task_runner_->RunUntilIdle();
  }

 protected:
  std::unique_ptr<BackgroundURLLoader> CreateBackgroundURLLoaderAndStart(
      std::unique_ptr<network::ResourceRequest> request,
      URLLoaderClient* url_loader_client) {
    base::RunLoop run_loop;
    scoped_refptr<WebBackgroundResourceFetchAssets>
        background_resource_fetch_assets =
            base::MakeRefCounted<FakeBackgroundResourceFetchAssets>(
                background_task_runner_,
                base::BindLambdaForTesting(
                    [&](mojo::PendingReceiver<network::mojom::URLLoader> loader,
                        mojo::PendingRemote<network::mojom::URLLoaderClient>
                            client) {
                      DCHECK(background_task_runner_
                                 ->RunsTasksInCurrentSequence());
                      loader_pending_receiver_ = std::move(loader);
                      loader_client_pending_remote_ = std::move(client);
                      run_loop.Quit();
                    }));
    std::unique_ptr<BackgroundURLLoader> background_url_loader =
        std::make_unique<BackgroundURLLoader>(
            std::move(background_resource_fetch_assets),
            /*cors_exempt_header_list=*/Vector<String>(),
            unfreezable_task_runner_, bfcache_loader_helper_,
            /*background_code_cache_host=*/nullptr);
    background_url_loader->LoadAsynchronously(
        std::move(request), SecurityOrigin::Create(KURL(kTestURL)),
        /*no_mime_sniffing=*/false,
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        /*code_cache_host=*/nullptr, url_loader_client);
    run_loop.Run();
    return background_url_loader;
  }

  mojo::PendingReceiver<network::mojom::URLLoader> loader_pending_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderClient>
      loader_client_pending_remote_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<scheduler::FakeTaskRunner> unfreezable_task_runner_;
  base::test::TaskEnvironment task_environment_;
  Persistent<FakeBackForwardCacheLoaderHelper> bfcache_loader_helper_;

 private:
  class TestPlatformForRedirects final : public TestingPlatformSupport {
   public:
    bool IsRedirectSafe(const GURL& from_url, const GURL& to_url) override {
      return true;
    }
  };

  ScopedTestingPlatformSupport<TestPlatformForRedirects> platform_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackgroundResourceFecherTest, SimpleRequest) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(
      CreateTestResponse(), CreateTestBody(), CreateTestCachedMetaData());

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(client.response());
  EXPECT_FALSE(client.cached_metadata());
  EXPECT_FALSE(client.response_body());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.cached_metadata());
  EXPECT_TRUE(client.response_body());

  loader_client_remote->OnTransferSizeUpdated(10);
  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(client.transfer_size_diffs().empty());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_THAT(client.transfer_size_diffs(), testing::ElementsAreArray({10}));

  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client.did_finish());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.did_finish());

  EXPECT_FALSE(client.error());
}

TEST_F(BackgroundResourceFecherTest, FailedRequest) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));

  loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(client.error());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.error());
}

TEST_F(BackgroundResourceFecherTest, Redirect) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  KURL redirected_url;
  client.AddWillFollowRedirectCallback(
      base::BindLambdaForTesting([&](const WebURL& new_url) {
        redirected_url = new_url;
        return true;
      }));
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedURL);

  loader_client_remote->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(redirected_url.IsEmpty());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_EQ(KURL(kRedirectedURL), redirected_url);

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(loader.follow_redirect_called());

  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  task_environment_.RunUntilIdle();
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.did_finish());
}

TEST_F(BackgroundResourceFecherTest, RedirectDoNotFollow) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  KURL redirected_url;
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  client.AddWillFollowRedirectCallback(
      base::BindLambdaForTesting([&](const WebURL& new_url) {
        redirected_url = new_url;
        background_url_loader.reset();
        return false;
      }));

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedURL);

  loader_client_remote->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(redirected_url.IsEmpty());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_EQ(KURL(kRedirectedURL), redirected_url);
}

TEST_F(BackgroundResourceFecherTest, RedirectAndCancelDoNotCrash) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  KURL redirected_url;
  client.AddWillFollowRedirectCallback(
      base::BindLambdaForTesting([&](const WebURL& new_url) {
        redirected_url = new_url;
        return true;
      }));
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedURL);

  loader_client_remote->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(redirected_url.IsEmpty());
  // Cancel the request before Context::OnReceivedRedirect() is called in
  // `unfreezable_task_runner_`.
  background_url_loader.reset();
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(redirected_url.IsEmpty());
}

TEST_F(BackgroundResourceFecherTest, AbortWhileHandlingRedirectDoNotCrash) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  KURL redirected_url;
  client.AddWillFollowRedirectCallback(
      base::BindLambdaForTesting([&](const WebURL& new_url) {
        redirected_url = new_url;
        return true;
      }));
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedURL);

  loader_client_remote->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());
  loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(redirected_url.IsEmpty());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_FALSE(redirected_url.IsEmpty());
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceFecherTest, CancelSoonAfterStart) {
  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  background_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        waitable_event.Wait();
      }));

  scoped_refptr<WebBackgroundResourceFetchAssets>
      background_resource_fetch_assets =
          base::MakeRefCounted<FakeBackgroundResourceFetchAssets>(
              background_task_runner_,
              base::BindLambdaForTesting(
                  [&](mojo::PendingReceiver<network::mojom::URLLoader> loader,
                      mojo::PendingRemote<network::mojom::URLLoaderClient>
                          client) {
                    // CreateLoaderAndStart should not be called.
                    CHECK(false);
                  }));
  std::unique_ptr<BackgroundURLLoader> background_url_loader =
      std::make_unique<BackgroundURLLoader>(
          std::move(background_resource_fetch_assets),
          /*cors_exempt_header_list=*/Vector<String>(),
          unfreezable_task_runner_,
          /*back_forward_cache_loader_helper=*/nullptr,
          /*background_code_cache_host*/ nullptr);
  FakeURLLoaderClient client(unfreezable_task_runner_);
  background_url_loader->LoadAsynchronously(
      CreateTestRequest(), SecurityOrigin::Create(KURL(kTestURL)),
      /*no_mime_sniffing=*/false,
      std::make_unique<ResourceLoadInfoNotifierWrapper>(
          /*resource_load_info_notifier=*/nullptr),
      /*code_cache_host=*/nullptr, &client);

  background_url_loader.reset();
  waitable_event.Signal();
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceFecherTest, CancelAfterStart) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  bool url_loader_client_dissconnected = false;
  bool url_loader_dissconnected = false;
  loader_client_remote.set_disconnect_handler(base::BindLambdaForTesting(
      [&]() { url_loader_client_dissconnected = true; }));
  loader.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { url_loader_dissconnected = true; }));

  background_url_loader.reset();

  // Call RunUntilIdle() to call Mojo's disconnect handler.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_loader_client_dissconnected);
  EXPECT_TRUE(url_loader_dissconnected);
}

TEST_F(BackgroundResourceFecherTest, CancelAfterReceiveResponse) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  bool url_loader_client_dissconnected = false;
  bool url_loader_dissconnected = false;
  loader_client_remote.set_disconnect_handler(base::BindLambdaForTesting(
      [&]() { url_loader_client_dissconnected = true; }));
  loader.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { url_loader_dissconnected = true; }));

  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);

  // Call RunUntilIdle() to call Mojo's disconnect handler.
  task_environment_.RunUntilIdle();

  background_url_loader.reset();

  // Call RunUntilIdle() to call Mojo's disconnect handler.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(url_loader_client_dissconnected);
  EXPECT_TRUE(url_loader_dissconnected);

  // Flush all tasks posted to avoid memory leak.
  unfreezable_task_runner_->RunUntilIdle();
}

TEST_F(BackgroundResourceFecherTest, FreezeThenUnfreeze) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(
      CreateTestResponse(), CreateTestBody(), CreateTestCachedMetaData());
  loader_client_remote->OnTransferSizeUpdated(10);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  background_url_loader->Freeze(LoaderFreezeMode::kStrict);

  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_FALSE(client.response());
  EXPECT_FALSE(client.cached_metadata());
  EXPECT_FALSE(client.response_body());
  EXPECT_FALSE(client.did_finish());

  background_url_loader->Freeze(LoaderFreezeMode::kNone);

  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.cached_metadata());
  EXPECT_TRUE(client.response_body());
  EXPECT_TRUE(client.did_finish());
}

TEST_F(BackgroundResourceFecherTest, FreezeCancelThenUnfreeze) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnTransferSizeUpdated(10);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  background_url_loader->Freeze(LoaderFreezeMode::kStrict);

  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_FALSE(client.response());
  EXPECT_FALSE(client.cached_metadata());
  EXPECT_FALSE(client.response_body());
  EXPECT_FALSE(client.did_finish());

  background_url_loader->Freeze(LoaderFreezeMode::kNone);

  // Cancel the request.
  background_url_loader.reset();

  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_FALSE(client.response());
  EXPECT_FALSE(client.cached_metadata());
  EXPECT_FALSE(client.response_body());
  EXPECT_FALSE(client.did_finish());
}

TEST_F(BackgroundResourceFecherTest, BufferIncomingFreezeAndResume) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  background_url_loader->Freeze(LoaderFreezeMode::kBufferIncoming);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnTransferSizeUpdated(10);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kTestBodyString.size(), BackForwardCacheBufferLimitTracker::Get()
                                        .total_bytes_buffered_for_testing());
  EXPECT_TRUE(
      BackForwardCacheBufferLimitTracker::Get().IsUnderPerProcessBufferLimit());

  // Methods of `bfcache_loader_helper_` must called at
  // `unfreezable_task_runner_`.
  EXPECT_FALSE(bfcache_loader_helper_->evicted_reason());
  EXPECT_EQ(0u, bfcache_loader_helper_
                    ->total_bytes_buffered_while_in_back_forward_cache());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_FALSE(bfcache_loader_helper_->evicted_reason());
  EXPECT_EQ(kTestBodyString.size(),
            bfcache_loader_helper_
                ->total_bytes_buffered_while_in_back_forward_cache());

  // Restore from BFCache.
  BackForwardCacheBufferLimitTracker::Get()
      .DidRemoveFrameOrWorkerFromBackForwardCache(
          bfcache_loader_helper_
              ->total_bytes_buffered_while_in_back_forward_cache());
  background_url_loader->Freeze(LoaderFreezeMode::kNone);
  task_environment_.RunUntilIdle();
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.response_body());
  EXPECT_THAT(client.transfer_size_diffs(), testing::ElementsAreArray({10}));
  EXPECT_TRUE(client.did_finish());
  EXPECT_FALSE(client.error());
}

TEST_F(BackgroundResourceFecherTest,
       BufferIncomingFreezeExceedMaxBufferedBytesPerProcess) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  background_url_loader->Freeze(LoaderFreezeMode::kBufferIncoming);
  constexpr size_t kBodySize = kMaxBufferedBytesPerProcess + 1;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(
      CreateTestResponse(),
      CreateDataPipeConsumerHandleFilledWithString(std::string(kBodySize, '*')),
      /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnTransferSizeUpdated(kBodySize);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      BackForwardCacheBufferLimitTracker::Get().IsUnderPerProcessBufferLimit());

  EXPECT_EQ(kBodySize, BackForwardCacheBufferLimitTracker::Get()
                           .total_bytes_buffered_for_testing());

  // Methods of `bfcache_loader_helper_` must called at
  // `unfreezable_task_runner_`.
  EXPECT_FALSE(bfcache_loader_helper_->evicted_reason());
  EXPECT_EQ(0u, bfcache_loader_helper_
                    ->total_bytes_buffered_while_in_back_forward_cache());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_THAT(bfcache_loader_helper_->evicted_reason(),
              mojom::blink::RendererEvictionReason::kNetworkExceedsBufferLimit);
  EXPECT_EQ(kBodySize,
            bfcache_loader_helper_
                ->total_bytes_buffered_while_in_back_forward_cache());

  // Reset BackForwardCacheBufferLimitTracker not to interfere other tests.
  BackForwardCacheBufferLimitTracker::Get()
      .DidRemoveFrameOrWorkerFromBackForwardCache(
          bfcache_loader_helper_
              ->total_bytes_buffered_while_in_back_forward_cache());
}

TEST_F(BackgroundResourceFecherTest,
       BufferIncomingFreezeAndResumeBeforeExecutingUnfreezableTask) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  background_url_loader->Freeze(LoaderFreezeMode::kBufferIncoming);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnTransferSizeUpdated(10);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();

  // Restore from BFCache before running tasks in `unfreezable_task_runner_`.
  background_url_loader->Freeze(LoaderFreezeMode::kNone);

  // The BackForwardCacheBufferLimitTracker must be updated by the task in
  // `unfreezable_task_runner_`.
  EXPECT_EQ(kTestBodyString.size(), BackForwardCacheBufferLimitTracker::Get()
                                        .total_bytes_buffered_for_testing());
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, BackForwardCacheBufferLimitTracker::Get()
                    .total_bytes_buffered_for_testing());

  // Methods of `bfcache_loader_helper_` must not be called.
  EXPECT_FALSE(bfcache_loader_helper_->evicted_reason());
  EXPECT_EQ(0u, bfcache_loader_helper_
                    ->total_bytes_buffered_while_in_back_forward_cache());

  task_environment_.RunUntilIdle();
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.response_body());
  EXPECT_THAT(client.transfer_size_diffs(), testing::ElementsAreArray({10}));
  EXPECT_TRUE(client.did_finish());
  EXPECT_FALSE(client.error());
}

TEST_F(BackgroundResourceFecherTest, ChangePriority) {
  FakeURLLoaderClient client(unfreezable_task_runner_);
  auto background_url_loader =
      CreateBackgroundURLLoaderAndStart(CreateTestRequest(), &client);

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote_));
  FakeURLLoader loader(std::move(loader_pending_receiver_));

  background_url_loader->DidChangePriority(WebURLRequest::Priority::kVeryHigh,
                                           100);

  // Call RunUntilIdle() to receive Mojo IPC.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1u, loader.set_priority_log().size());
  EXPECT_EQ(net::RequestPriority::HIGHEST,
            loader.set_priority_log()[0].priority);
  EXPECT_EQ(100, loader.set_priority_log()[0].intra_priority_value);

  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestBody(),
                                          /*cached_metadata=*/absl::nullopt);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  task_environment_.RunUntilIdle();
  unfreezable_task_runner_->RunUntilIdle();
  EXPECT_TRUE(client.response());
  EXPECT_TRUE(client.did_finish());
}

}  // namespace
}  // namespace blink
