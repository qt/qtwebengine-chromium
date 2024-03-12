// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

const std::string kTestDictionaryData = "HelloHallo你好こんにちは";
// The hex of sha256 of `kTestDictionaryData`.
const std::string kTestDictionarySha256 =
    "c19728aed36503cfc81a0f5359e6f472e121f77bf20a2faac7994191293c0623";
const std::string kTestData =
    "HelloこんにちはHallo你好HelloこんにちはHallo你好";
// The brotli encoded data of `kTestData` using `kTestDictionaryData` as a
// dictionary.
// kBrotliEncodedData is generated using the following commands:
// $ echo -n "HelloHallo你好こんにちは" > /tmp/dict
// $ echo -n "HelloこんにちはHallo你好HelloこんにちはHallo你好" > /tmp/data
// $ brotli -o /tmp/out.sbr -D /tmp/dict /tmp/data
// $ xxd -i /tmp/out.sbr
const uint8_t kBrotliEncodedData[] = {0xa1, 0xe8, 0x01, 0x00, 0x22, 0x8d, 0x54,
                                      0xc6, 0xf6, 0x26, 0x81, 0x69, 0x46, 0x9d,
                                      0xb2, 0x60, 0x0e, 0x6b, 0xf5, 0x07, 0x02};
const std::string kBrotliEncodedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliEncodedData),
                sizeof(kBrotliEncodedData));

// The zstd encoded data of `kTestData` using `kTestDictionaryData` as a
// dictionary.
// kZstdEncodedData is generated using the following commands:
// $ echo -n "HelloHallo你好こんにちは" > /tmp/dict
// $ echo -n "HelloこんにちはHallo你好HelloこんにちはHallo你好" > /tmp/data
// $ zstd -o /tmp/out.szst -D /tmp/dict /tmp/data
// $ xxd -i /tmp/out.szst
const uint8_t kZstdEncodedData[] = {
    0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x3e, 0x85, 0x00, 0x00, 0x28,
    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x03, 0x00, 0x42, 0x35, 0x88,
    0x6a, 0x03, 0x87, 0x4c, 0x2d, 0xcd, 0x1e, 0xde, 0x25};
const std::string kZstdEncodedDataString =
    std::string(reinterpret_cast<const char*>(kZstdEncodedData),
                sizeof(kZstdEncodedData));

const size_t kDefaultBufferSize = 1023;

class DummySyncDictionary : public SharedDictionary {
 public:
  explicit DummySyncDictionary(const std::string& data_string)
      : data_(base::MakeRefCounted<net::StringIOBuffer>(data_string)),
        size_(data_string.size()) {
    std::unique_ptr<crypto::SecureHash> secure_hash =
        crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    secure_hash->Update(data_->data(), size_);
    secure_hash->Finish(hash_.data, sizeof(hash_.data));
  }
  ~DummySyncDictionary() override = default;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    return net::OK;
  }
  scoped_refptr<net::IOBuffer> data() const override { return data_; }
  size_t size() const override { return size_; }
  const net::SHA256HashValue& hash() const override { return hash_; }

 private:
  const scoped_refptr<net::IOBuffer> data_;
  const size_t size_;
  net::SHA256HashValue hash_;
};

class DummyAsyncDictionary : public DummySyncDictionary {
 public:
  explicit DummyAsyncDictionary(const std::string& data_string)
      : DummySyncDictionary(data_string) {}
  ~DummyAsyncDictionary() override = default;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    read_all_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }
  base::OnceCallback<void(int)> TakeReadAllCallback() {
    return std::move(read_all_callback_);
  }

 private:
  base::OnceCallback<void(int)> read_all_callback_;
};

class DummySharedDictionaryStorage : public SharedDictionaryStorage {
 public:
  explicit DummySharedDictionaryStorage(
      std::unique_ptr<SharedDictionary> dictionary)
      : dictionary_(std::move(dictionary)) {}

  // SharedDictionaryStorage
  std::unique_ptr<SharedDictionary> GetDictionarySync(
      const GURL& url) override {
    return std::move(dictionary_);
  }
  void GetDictionary(const GURL& url,
                     base::OnceCallback<void(std::unique_ptr<SharedDictionary>)>
                         callback) override {}
  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match) override {
    return nullptr;
  }
  bool IsAlreadyRegistered(const GURL& url,
                           base::Time response_time,
                           base::TimeDelta expiration,
                           const std::string& match) override {
    return false;
  }

  void set_on_deleted_closure_runner(base::ScopedClosureRunner closure_runner) {
    on_deleted_closure_runner_ = std::move(closure_runner);
  }

 private:
  ~DummySharedDictionaryStorage() override = default;

  std::unique_ptr<SharedDictionary> dictionary_;
  base::ScopedClosureRunner on_deleted_closure_runner_;
};

class DummySharedDictionaryManager : public SharedDictionaryManager {
 public:
  explicit DummySharedDictionaryManager(
      scoped_refptr<DummySharedDictionaryStorage> storage)
      : storage_(std::move(storage)) {}
  ~DummySharedDictionaryManager() override = default;

  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) override {
    create_storage_called_ = true;
    if (storage_) {
      storage_->set_on_deleted_closure_runner(base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
    }
    return storage_;
  }
  void SetCacheMaxSize(uint64_t cache_max_size) override {}
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override {}
  void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) override {}
  void GetUsageInfo(base::OnceCallback<
                    void(const std::vector<net::SharedDictionaryUsageInfo>&)>
                        callback) override {}
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<
          void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback)
      override {}
  void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback)
      override {}

  bool create_storage_called() const { return create_storage_called_; }

 private:
  scoped_refptr<DummySharedDictionaryStorage> storage_;
  bool create_storage_called_ = false;
};

net::TransportInfo TestSpdyTransportInfo() {
  return net::TransportInfo(
      net::TransportType::kDirect,
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 80),
      /*accept_ch_frame_arg=*/"",
      /*cert_is_issued_by_known_root=*/false, net::kProtoHTTP2);
}

static void BrotliTestTransactionHandler(const net::HttpRequestInfo* request,
                                         std::string* response_status,
                                         std::string* response_headers,
                                         std::string* response_data) {
  std::string sec_available_dictionary_header;
  EXPECT_TRUE(request->extra_headers.GetHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName,
      &sec_available_dictionary_header));
  EXPECT_EQ(kTestDictionarySha256, sec_available_dictionary_header);
  *response_data = kBrotliEncodedDataString;
}

static void ZstdTestTransactionHandler(const net::HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data) {
  std::string sec_available_dictionary_header;
  EXPECT_TRUE(request->extra_headers.GetHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName,
      &sec_available_dictionary_header));
  EXPECT_EQ(kTestDictionarySha256, sec_available_dictionary_header);
  *response_data = kZstdEncodedDataString;
}

static const auto kTestTransactionHandlerWithoutAvailableDictionary =
    base::BindRepeating([](const net::HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data) {
      EXPECT_FALSE(request->extra_headers.HasHeader(
          network::shared_dictionary::kSecAvailableDictionaryHeaderName));
      *response_data = kTestData;
    });

const net::MockTransaction kBrotliDictionaryTestTransactionV1 = {
    .url = "https://test.example/test",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: sbr\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = absl::nullopt,
    .browser_run_id = absl::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&BrotliTestTransactionHandler),
    .read_handler = net::MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

const net::MockTransaction kBrotliDictionaryTestTransactionV2 = {
    .url = "https://test.example/test",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: br-d\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = absl::nullopt,
    .browser_run_id = absl::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&BrotliTestTransactionHandler),
    .read_handler = net::MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

const net::MockTransaction kZstdDictionaryTestTransaction = {
    .url = "https://test.example/test",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: zstd-d\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = absl::nullopt,
    .browser_run_id = absl::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&ZstdTestTransactionHandler),
    .read_handler = net::MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

class SharedDictionaryNetworkTransactionTestBase : public ::testing::Test {
 public:
  explicit SharedDictionaryNetworkTransactionTestBase(
      network::features::CompressionDictionaryTransportBackendVersion version)
      : version_(version),
        network_layer_(std::make_unique<net::MockNetworkLayer>()) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {base::test::FeatureRefAndParams(
            network::features::kCompressionDictionaryTransportBackend,
            {{network::features::kCompressionDictionaryTransportBackendVersion
                  .name,
              network::features::kCompressionDictionaryTransportBackendVersion
                  .GetName(GetVersion())}})},
        /*disabled_features=*/{});
    net::AddMockTransaction(&GetBrotliDictionaryTestTransaction());
  }
  ~SharedDictionaryNetworkTransactionTestBase() override = default;

  SharedDictionaryNetworkTransactionTestBase(
      const SharedDictionaryNetworkTransactionTestBase&) = delete;
  SharedDictionaryNetworkTransactionTestBase& operator=(
      const SharedDictionaryNetworkTransactionTestBase&) = delete;

 protected:
  network::features::CompressionDictionaryTransportBackendVersion GetVersion() {
    return version_;
  }
  const net::MockTransaction& GetBrotliDictionaryTestTransaction() {
    switch (GetVersion()) {
      case network::features::CompressionDictionaryTransportBackendVersion::kV1:
        return kBrotliDictionaryTestTransactionV1;
      case network::features::CompressionDictionaryTransportBackendVersion::kV2:
        return kBrotliDictionaryTestTransactionV2;
    }
  }
  std::unique_ptr<net::HttpTransaction> CreateNetworkTransaction() {
    std::unique_ptr<net::HttpTransaction> network_transaction;
    network_layer_->CreateTransaction(net::DEFAULT_PRIORITY,
                                      &network_transaction);
    return network_transaction;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  net::MockNetworkLayer& network_layer() { return *network_layer_.get(); }

 private:
  const network::features::CompressionDictionaryTransportBackendVersion
      version_;
  std::unique_ptr<net::MockNetworkLayer> network_layer_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SharedDictionaryNetworkTransactionTest
    : public SharedDictionaryNetworkTransactionTestBase,
      public ::testing::WithParamInterface<
          network::features::CompressionDictionaryTransportBackendVersion> {
 public:
  SharedDictionaryNetworkTransactionTest()
      : SharedDictionaryNetworkTransactionTestBase(GetVersion()) {}
  ~SharedDictionaryNetworkTransactionTest() override = default;

  SharedDictionaryNetworkTransactionTest(
      const SharedDictionaryNetworkTransactionTest&) = delete;
  SharedDictionaryNetworkTransactionTest& operator=(
      const SharedDictionaryNetworkTransactionTest&) = delete;

 protected:
  network::features::CompressionDictionaryTransportBackendVersion GetVersion() {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedDictionaryNetworkTransactionTest,
    testing::Values(
        network::features::CompressionDictionaryTransportBackendVersion::kV1,
        network::features::CompressionDictionaryTransportBackendVersion::kV2),
    [](const testing::TestParamInfo<
        network::features::CompressionDictionaryTransportBackendVersion>&
           info) {
      switch (info.param) {
        case network::features::CompressionDictionaryTransportBackendVersion::
            kV1:
          return "V1";
        case network::features::CompressionDictionaryTransportBackendVersion::
            kV2:
          return "V2";
      }
    });

TEST_P(SharedDictionaryNetworkTransactionTest, SyncDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, NotAllowedToUseDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return false; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  new_mock_transaction.transport_info.cert_is_issued_by_known_root = false;

  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // The BrotliTestTransactionHandler `new_mock_transaction.handler` will check
  // that the there is a correct sec-available-dictionary request header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.transport_info.cert_is_issued_by_known_root = true;

  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccessForLocalhost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // The BrotliTestTransactionHandler `new_mock_transaction.handler` will check
  // that the there is a correct sec-available-dictionary request header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.url = "http:///localhost:1234/test";
  new_mock_transaction.transport_info.cert_is_issued_by_known_root = false;

  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, NoMatchingDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, OpaqueFrameOrigin) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  request.frame_origin = url::Origin();
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, WithoutValidLoadFlag) {
  DummySharedDictionaryManager manager(/*storage=*/nullptr);

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  CHECK_EQ(net::LOAD_CAN_USE_SHARED_DICTIONARY, request.load_flags);
  // Change load_flags not to trigger the shared dictionary logic.
  request.load_flags = net::LOAD_NORMAL;

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));

  // SharedDictionaryManager::CreateStorage() must not be called when
  // LOAD_CAN_USE_SHARED_DICTIONARY is not set.
  EXPECT_FALSE(manager.create_storage_called());
}

TEST_P(SharedDictionaryNetworkTransactionTest, NoSbrContentEncoding) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to remove `content-encoding: sbr`.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.response_headers = "";
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: sbr" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, MultipleContentEncodingWithSbr) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to set `content-encoding: sbr, deflate`.
  net::MockTransaction new_mock_transaction =
      GetBrotliDictionaryTestTransaction();
  new_mock_transaction.response_headers = "content-encoding: sbr, deflate\n";
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is Content-Encoding header which value is other than "sbr",
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessBeforeStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(net::OK);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(net::OK);

  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterTransactionDestroy) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  std::unique_ptr<SharedDictionaryNetworkTransaction> transaction =
      std::make_unique<SharedDictionaryNetworkTransaction>(
          manager, CreateNetworkTransaction());
  transaction->SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  transaction.reset();

  std::move(dictionary_read_all_callback).Run(net::OK);

  EXPECT_FALSE(read_callback.have_result());
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureBeforeStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(net::ERR_FAILED);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_DICTIONARY_LOAD_FAILED));
}

TEST_P(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureAfterStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(GetBrotliDictionaryTestTransaction());
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(net::ERR_FAILED);

  EXPECT_EQ(net::ERR_DICTIONARY_LOAD_FAILED, read_callback.WaitForResult());
}

TEST_P(SharedDictionaryNetworkTransactionTest, Restart) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  net::MockTransaction mock_transaction(net::kSimpleGET_Transaction);
  mock_transaction.start_return_code = net::ERR_FAILED;
  net::AddMockTransaction(&mock_transaction);
  net::MockHttpRequest request(mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(),
              net::test::IsError(net::ERR_FAILED));

  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartIgnoringLastError(restart_callback.callback()),
        net::test::IsError(net::ERR_FAILED));
  }
  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartWithCertificate(
            /*client_cert=*/nullptr,
            /*client_private_key=*/nullptr, restart_callback.callback()),
        net::test::IsError(net::ERR_FAILED));
  }
  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(transaction.RestartWithAuth(net::AuthCredentials(),
                                            restart_callback.callback()),
                net::test::IsError(net::ERR_FAILED));
  }
  ASSERT_FALSE(transaction.IsReadyToRestartForAuth());
}

TEST_P(SharedDictionaryNetworkTransactionTest, StopCaching) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  EXPECT_FALSE(network_layer().stop_caching_called());
  transaction.StopCaching();
  EXPECT_TRUE(network_layer().stop_caching_called());
}

TEST_P(SharedDictionaryNetworkTransactionTest, DoneReading) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  EXPECT_FALSE(network_layer().done_reading_called());
  transaction.DoneReading();
  EXPECT_TRUE(network_layer().done_reading_called());
}

TEST_P(SharedDictionaryNetworkTransactionTest, GetLoadState) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  net::AddMockTransaction(&net::kSimpleGET_Transaction);
  net::MockHttpRequest request(net::kSimpleGET_Transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  EXPECT_EQ(net::LOAD_STATE_IDLE, transaction.GetLoadState());

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(1);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, 1);

  EXPECT_EQ(net::LOAD_STATE_READING_RESPONSE, transaction.GetLoadState());
}

TEST_P(SharedDictionaryNetworkTransactionTest, SharedZstd) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(network::features::kSharedZstd);

  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to use `content-encoding: zstd-d`.
  net::MockTransaction new_mock_transaction = kZstdDictionaryTestTransaction;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_P(SharedDictionaryNetworkTransactionTest, NoZstdDContentEncoding) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(network::features::kSharedZstd);

  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to remove `content-encoding: zstd-d`.
  net::MockTransaction new_mock_transaction = kZstdDictionaryTestTransaction;
  new_mock_transaction.response_headers = "";
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: zstd-d" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kZstdEncodedDataString.size());
  EXPECT_EQ(kZstdEncodedDataString, std::string(buf->data(), read_result));
}

enum class ProtocolCheckProtocolTestCase {
  kHttp1,
  kHttp2,
  kHttp3,
};
std::string ToString(ProtocolCheckProtocolTestCase protocol) {
  switch (protocol) {
    case ProtocolCheckProtocolTestCase::kHttp1:
      return "Http1";
    case ProtocolCheckProtocolTestCase::kHttp2:
      return "Http2";
    case ProtocolCheckProtocolTestCase::kHttp3:
      return "Http3";
  }
}

enum class ProtocolCheckFeatureTestCase {
  kAllowHttp1,
  kDoNotAllowHttp1,
};
std::string ToString(ProtocolCheckFeatureTestCase feature) {
  switch (feature) {
    case ProtocolCheckFeatureTestCase::kAllowHttp1:
      return "AllowHttp1";
    case ProtocolCheckFeatureTestCase::kDoNotAllowHttp1:
      return "DoNotAllowHttp1";
  }
}

enum class ProtocolCheckHostTestCase {
  kLocalHost,
  kNonLocalhost,
};
std::string ToString(ProtocolCheckHostTestCase host_type) {
  switch (host_type) {
    case ProtocolCheckHostTestCase::kLocalHost:
      return "LocalHost";
    case ProtocolCheckHostTestCase::kNonLocalhost:
      return "NonLocalhost";
  }
}

class SharedDictionaryNetworkTransactionProtocolCheckTest
    : public SharedDictionaryNetworkTransactionTestBase,
      public testing::WithParamInterface<
          std::tuple<ProtocolCheckFeatureTestCase,
                     ProtocolCheckProtocolTestCase,
                     ProtocolCheckHostTestCase>> {
 public:
  SharedDictionaryNetworkTransactionProtocolCheckTest()
      : SharedDictionaryNetworkTransactionTestBase(
            // Protocol check logic doesn't depend on versions. So we just check
            // the V2 behavior.
            network::features::CompressionDictionaryTransportBackendVersion::
                kV2) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (AllowHttp1()) {
      enabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp1);
    } else {
      disabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp1);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  SharedDictionaryNetworkTransactionProtocolCheckTest(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  SharedDictionaryNetworkTransactionProtocolCheckTest& operator=(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  ~SharedDictionaryNetworkTransactionProtocolCheckTest() override = default;

 protected:
  net::MockTransaction CreateMockTransaction() {
    net::MockTransaction mock_transaction =
        GetBrotliDictionaryTestTransaction();
    if (IsLocalHost()) {
      mock_transaction.url = "http://localhost/test";
    }
    if (!ShuoldUseDictionary()) {
      // Override MockTransaction to check that there is no
      // sec-available-dictionary header.
      mock_transaction.handler =
          kTestTransactionHandlerWithoutAvailableDictionary;
    }
    if (IsHttp2()) {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoHTTP2;
    } else if (IsHttp3()) {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoQUIC;
    } else {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoHTTP11;
    }
    return mock_transaction;
  }

 private:
  bool AllowHttp1() const {
    return std::get<0>(GetParam()) == ProtocolCheckFeatureTestCase::kAllowHttp1;
  }
  bool IsHttp2() const {
    return std::get<1>(GetParam()) == ProtocolCheckProtocolTestCase::kHttp2;
  }
  bool IsHttp3() const {
    return std::get<1>(GetParam()) == ProtocolCheckProtocolTestCase::kHttp3;
  }
  bool IsLocalHost() const {
    return std::get<2>(GetParam()) == ProtocolCheckHostTestCase::kLocalHost;
  }
  bool ShuoldUseDictionary() const {
    return AllowHttp1() || IsLocalHost() || IsHttp2() || IsHttp3();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedDictionaryNetworkTransactionProtocolCheckTest,
    ::testing::Combine(
        ::testing::Values(ProtocolCheckFeatureTestCase::kAllowHttp1,
                          ProtocolCheckFeatureTestCase::kDoNotAllowHttp1),
        ::testing::Values(ProtocolCheckProtocolTestCase::kHttp1,
                          ProtocolCheckProtocolTestCase::kHttp2,
                          ProtocolCheckProtocolTestCase::kHttp3),
        ::testing::Values(ProtocolCheckHostTestCase::kLocalHost,
                          ProtocolCheckHostTestCase::kNonLocalhost)),
    [](const testing::TestParamInfo<std::tuple<ProtocolCheckFeatureTestCase,
                                               ProtocolCheckProtocolTestCase,
                                               ProtocolCheckHostTestCase>>&
           info) {
      return ToString(std::get<0>(info.param)) + "_" +
             ToString(std::get<1>(info.param)) + "_" +
             ToString(std::get<2>(info.param));
    });

TEST_P(SharedDictionaryNetworkTransactionProtocolCheckTest, Basic) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  net::MockTransaction new_mock_transaction = CreateMockTransaction();

  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

}  // namespace

}  // namespace network
