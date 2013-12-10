// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "content/child/request_extra_data.h"
#include "content/child/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/common/appcache/appcache_interfaces.h"

using webkit_glue::ResourceLoaderBridge;
using webkit_glue::ResourceResponseInfo;

namespace content {

static const char test_page_url[] = "http://www.google.com/";
static const char test_page_headers[] =
  "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static const char test_page_mime_type[] = "text/html";
static const char test_page_charset[] = "";
static const char test_page_contents[] =
  "<html><head><title>Google</title></head><body><h1>Google</h1></body></html>";
static const uint32 test_page_contents_len = arraysize(test_page_contents) - 1;

static const char kShmemSegmentName[] = "DeferredResourceLoaderTest";

// Listens for request response data and stores it so that it can be compared
// to the reference data.
class TestRequestCallback : public ResourceLoaderBridge::Peer {
 public:
  TestRequestCallback() : complete_(false) {
  }

  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE {
  }

  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies) OVERRIDE {
    *has_new_first_party_for_cookies = false;
    return true;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
  }

  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {
  }

  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
    EXPECT_FALSE(complete_);
    data_.append(data, data_length);
    total_encoded_data_length_ += encoded_data_length;
  }

  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      const std::string& security_info,
      const base::TimeTicks& completion_time) OVERRIDE {
    EXPECT_FALSE(complete_);
    complete_ = true;
  }

  bool complete() const {
    return complete_;
  }
  const std::string& data() const {
    return data_;
  }
  int total_encoded_data_length() const {
    return total_encoded_data_length_;
  }

 private:
  bool complete_;
  std::string data_;
  int total_encoded_data_length_;
};


// Sets up the message sender override for the unit test
class ResourceDispatcherTest : public testing::Test, public IPC::Sender {
 public:
  // Emulates IPC send operations (IPC::Sender) by adding
  // pending messages to the queue.
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    message_queue_.push_back(IPC::Message(*msg));
    delete msg;
    return true;
  }

  // Emulates the browser process and processes the pending IPC messages,
  // returning the hardcoded file contents.
  void ProcessMessages() {
    while (!message_queue_.empty()) {
      int request_id;
      ResourceHostMsg_Request request;
      ASSERT_TRUE(ResourceHostMsg_RequestResource::Read(
          &message_queue_[0], &request_id, &request));

      // check values
      EXPECT_EQ(test_page_url, request.url.spec());

      // received response message
      ResourceResponseHead response;
      std::string raw_headers(test_page_headers);
      std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
      response.headers = new net::HttpResponseHeaders(raw_headers);
      response.mime_type = test_page_mime_type;
      response.charset = test_page_charset;
      dispatcher_->OnReceivedResponse(request_id, response);

      // received data message with the test contents
      base::SharedMemory shared_mem;
      EXPECT_TRUE(shared_mem.CreateAndMapAnonymous(test_page_contents_len));
      char* put_data_here = static_cast<char*>(shared_mem.memory());
      memcpy(put_data_here, test_page_contents, test_page_contents_len);
      base::SharedMemoryHandle dup_handle;
      EXPECT_TRUE(shared_mem.GiveToProcess(
          base::Process::Current().handle(), &dup_handle));
      dispatcher_->OnSetDataBuffer(request_id, dup_handle,
                                   test_page_contents_len, 0);
      dispatcher_->OnReceivedData(request_id, 0, test_page_contents_len,
                                  test_page_contents_len);

      message_queue_.erase(message_queue_.begin());

      // read the ack message.
      Tuple1<int> request_ack;
      ASSERT_TRUE(ResourceHostMsg_DataReceived_ACK::Read(
          &message_queue_[0], &request_ack));

      ASSERT_EQ(request_ack.a, request_id);

      message_queue_.erase(message_queue_.begin());
    }
  }

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE {
    dispatcher_.reset(new ResourceDispatcher(this));
  }
  virtual void TearDown() OVERRIDE {
    dispatcher_.reset();
  }

  ResourceLoaderBridge* CreateBridge() {
    webkit_glue::ResourceLoaderBridge::RequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL(test_page_url);
    request_info.first_party_for_cookies = GURL(test_page_url);
    request_info.referrer = GURL();
    request_info.headers = std::string();
    request_info.load_flags = 0;
    request_info.requestor_pid = 0;
    request_info.request_type = ResourceType::SUB_RESOURCE;
    request_info.appcache_host_id = appcache::kNoHostId;
    request_info.routing_id = 0;
    RequestExtraData extra_data(WebKit::WebReferrerPolicyDefault,
                                WebKit::WebString(),
                                false, true, 0, GURL(),
                                false, -1, true,
                                PAGE_TRANSITION_LINK, -1, -1);
    request_info.extra_data = &extra_data;

    return dispatcher_->CreateBridge(request_info);
  }

  std::vector<IPC::Message> message_queue_;
  static scoped_ptr<ResourceDispatcher> dispatcher_;
};

/*static*/
scoped_ptr<ResourceDispatcher> ResourceDispatcherTest::dispatcher_;

// Does a simple request and tests that the correct data is received.
TEST_F(ResourceDispatcherTest, RoundTrip) {
  TestRequestCallback callback;
  ResourceLoaderBridge* bridge = CreateBridge();

  bridge->Start(&callback);

  ProcessMessages();

  // FIXME(brettw) when the request complete messages are actually handledo
  // and dispatched, uncomment this.
  //EXPECT_TRUE(callback.complete());
  //EXPECT_STREQ(test_page_contents, callback.data().c_str());
  //EXPECT_EQ(test_page_contents_len, callback.total_encoded_data_length());

  delete bridge;
}

// Tests that the request IDs are straight when there are multiple requests.
TEST_F(ResourceDispatcherTest, MultipleRequests) {
  // FIXME
}

// Tests that the cancel method prevents other messages from being received
TEST_F(ResourceDispatcherTest, Cancel) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, Cookies) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, SerializedPostData) {
  // FIXME
}

// This class provides functionality to validate whether the ResourceDispatcher
// object honors the deferred loading contract correctly, i.e. if deferred
// loading is enabled it should queue up any responses received. If deferred
// loading is enabled/disabled in the context of a dispatched message, other
// queued messages should not be dispatched until deferred load is turned off.
class DeferredResourceLoadingTest : public ResourceDispatcherTest,
                                    public ResourceLoaderBridge::Peer {
 public:
  DeferredResourceLoadingTest()
      : defer_loading_(false) {
  }

  virtual bool Send(IPC::Message* msg) OVERRIDE {
    delete msg;
    return true;
  }

  void InitMessages() {
    set_defer_loading(true);

    ResourceResponseHead response_head;
    response_head.error_code = net::OK;

    dispatcher_->OnMessageReceived(
        ResourceMsg_ReceivedResponse(0, response_head));

    // Duplicate the shared memory handle so both the test and the callee can
    // close their copy.
    base::SharedMemoryHandle duplicated_handle;
    EXPECT_TRUE(shared_handle_.ShareToProcess(base::GetCurrentProcessHandle(),
                                              &duplicated_handle));

    dispatcher_->OnMessageReceived(
        ResourceMsg_SetDataBuffer(0, duplicated_handle, 100, 0));
    dispatcher_->OnMessageReceived(ResourceMsg_DataReceived(0, 0, 100, 100));

    set_defer_loading(false);
  }

  // ResourceLoaderBridge::Peer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE {
  }

  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies) OVERRIDE {
    *has_new_first_party_for_cookies = false;
    return true;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
    EXPECT_EQ(defer_loading_, false);
    set_defer_loading(true);
  }

  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {
  }

  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
    EXPECT_EQ(defer_loading_, false);
    set_defer_loading(false);
  }

  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      const std::string& security_info,
      const base::TimeTicks& completion_time) OVERRIDE {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    ResourceDispatcherTest::SetUp();
    shared_handle_.Delete(kShmemSegmentName);
    EXPECT_TRUE(shared_handle_.CreateNamed(kShmemSegmentName, false, 100));
  }

  virtual void TearDown() OVERRIDE {
    shared_handle_.Close();
    EXPECT_TRUE(shared_handle_.Delete(kShmemSegmentName));
    ResourceDispatcherTest::TearDown();
  }

 private:
  void set_defer_loading(bool defer) {
    defer_loading_ = defer;
    dispatcher_->SetDefersLoading(0, defer);
  }

  bool defer_loading() const {
    return defer_loading_;
  }

  bool defer_loading_;
  base::SharedMemory shared_handle_;
};

TEST_F(DeferredResourceLoadingTest, DeferredLoadTest) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);

  ResourceLoaderBridge* bridge = CreateBridge();

  bridge->Start(this);
  InitMessages();

  // Dispatch deferred messages.
  message_loop.RunUntilIdle();
  delete bridge;
}

class TimeConversionTest : public ResourceDispatcherTest,
                           public ResourceLoaderBridge::Peer {
 public:
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    delete msg;
    return true;
  }

  void PerformTest(const ResourceResponseHead& response_head) {
    scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
    bridge->Start(this);

    dispatcher_->OnMessageReceived(
        ResourceMsg_ReceivedResponse(0, response_head));
  }

  // ResourceLoaderBridge::Peer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE {
  }

  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies) OVERRIDE {
    return true;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
    response_info_ = info;
  }

  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {
  }

  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
  }

  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      const std::string& security_info,
      const base::TimeTicks& completion_time) OVERRIDE {
  }

  const ResourceResponseInfo& response_info() const { return response_info_; }

 private:
  ResourceResponseInfo response_info_;
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);
  response_head.load_timing.request_start_time = base::Time::Now();
  response_head.load_timing.request_start =
      base::TimeTicks::FromInternalValue(10);
  response_head.load_timing.connect_timing.connect_start =
      base::TimeTicks::FromInternalValue(13);

  PerformTest(response_head);

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
  EXPECT_LE(response_head.load_timing.request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

}  // namespace content
