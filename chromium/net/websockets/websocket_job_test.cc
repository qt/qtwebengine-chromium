// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket_stream/socket_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_websocket_test_util.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_throttle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace net {

namespace {

class MockSocketStream : public SocketStream {
 public:
  MockSocketStream(const GURL& url, SocketStream::Delegate* delegate)
      : SocketStream(url, delegate) {}

  virtual void Connect() OVERRIDE {}
  virtual bool SendData(const char* data, int len) OVERRIDE {
    sent_data_ += std::string(data, len);
    return true;
  }

  virtual void Close() OVERRIDE {}
  virtual void RestartWithAuth(
      const AuthCredentials& credentials) OVERRIDE {
  }

  virtual void DetachDelegate() OVERRIDE {
    delegate_ = NULL;
  }

  const std::string& sent_data() const {
    return sent_data_;
  }

 protected:
  virtual ~MockSocketStream() {}

 private:
  std::string sent_data_;
};

class MockSocketStreamDelegate : public SocketStream::Delegate {
 public:
  MockSocketStreamDelegate()
      : amount_sent_(0), allow_all_cookies_(true) {}
  void set_allow_all_cookies(bool allow_all_cookies) {
    allow_all_cookies_ = allow_all_cookies;
  }
  virtual ~MockSocketStreamDelegate() {}

  void SetOnStartOpenConnection(const base::Closure& callback) {
    on_start_open_connection_ = callback;
  }
  void SetOnConnected(const base::Closure& callback) {
    on_connected_ = callback;
  }
  void SetOnSentData(const base::Closure& callback) {
    on_sent_data_ = callback;
  }
  void SetOnReceivedData(const base::Closure& callback) {
    on_received_data_ = callback;
  }
  void SetOnClose(const base::Closure& callback) {
    on_close_ = callback;
  }

  virtual int OnStartOpenConnection(
      SocketStream* socket,
      const CompletionCallback& callback) OVERRIDE {
    if (!on_start_open_connection_.is_null())
      on_start_open_connection_.Run();
    return OK;
  }
  virtual void OnConnected(SocketStream* socket,
                           int max_pending_send_allowed) OVERRIDE {
    if (!on_connected_.is_null())
      on_connected_.Run();
  }
  virtual void OnSentData(SocketStream* socket,
                          int amount_sent) OVERRIDE {
    amount_sent_ += amount_sent;
    if (!on_sent_data_.is_null())
      on_sent_data_.Run();
  }
  virtual void OnReceivedData(SocketStream* socket,
                              const char* data, int len) OVERRIDE {
    received_data_ += std::string(data, len);
    if (!on_received_data_.is_null())
      on_received_data_.Run();
  }
  virtual void OnClose(SocketStream* socket) OVERRIDE {
    if (!on_close_.is_null())
      on_close_.Run();
  }
  virtual bool CanGetCookies(SocketStream* socket,
                             const GURL& url) OVERRIDE {
    return allow_all_cookies_;
  }
  virtual bool CanSetCookie(SocketStream* request,
                            const GURL& url,
                            const std::string& cookie_line,
                            CookieOptions* options) OVERRIDE {
    return allow_all_cookies_;
  }

  size_t amount_sent() const { return amount_sent_; }
  const std::string& received_data() const { return received_data_; }

 private:
  int amount_sent_;
  bool allow_all_cookies_;
  std::string received_data_;
  base::Closure on_start_open_connection_;
  base::Closure on_connected_;
  base::Closure on_sent_data_;
  base::Closure on_received_data_;
  base::Closure on_close_;
};

class MockCookieStore : public CookieStore {
 public:
  struct Entry {
    GURL url;
    std::string cookie_line;
    CookieOptions options;
  };

  MockCookieStore() {}

  bool SetCookieWithOptions(const GURL& url,
                            const std::string& cookie_line,
                            const CookieOptions& options) {
    Entry entry;
    entry.url = url;
    entry.cookie_line = cookie_line;
    entry.options = options;
    entries_.push_back(entry);
    return true;
  }

  std::string GetCookiesWithOptions(const GURL& url,
                                    const CookieOptions& options) {
    std::string result;
    for (size_t i = 0; i < entries_.size(); i++) {
      Entry& entry = entries_[i];
      if (url == entry.url) {
        if (!result.empty()) {
          result += "; ";
        }
        result += entry.cookie_line;
      }
    }
    return result;
  }

  // CookieStore:
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE {
    bool result = SetCookieWithOptions(url, cookie_line, options);
    if (!callback.is_null())
      callback.Run(result);
  }

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE {
    if (!callback.is_null())
      callback.Run(GetCookiesWithOptions(url, options));
  }

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void DeleteAllCreatedBetweenAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const DeleteCallback& callback) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) OVERRIDE {
    ADD_FAILURE();
  }

  virtual CookieMonster* GetCookieMonster() OVERRIDE { return NULL; }

  const std::vector<Entry>& entries() const { return entries_; }

 private:
  friend class base::RefCountedThreadSafe<MockCookieStore>;
  virtual ~MockCookieStore() {}

  std::vector<Entry> entries_;
};

class MockSSLConfigService : public SSLConfigService {
 public:
  virtual void GetSSLConfig(SSLConfig* config) OVERRIDE {}

 protected:
  virtual ~MockSSLConfigService() {}
};

class MockURLRequestContext : public URLRequestContext {
 public:
  explicit MockURLRequestContext(CookieStore* cookie_store)
      : transport_security_state_() {
    set_cookie_store(cookie_store);
    set_transport_security_state(&transport_security_state_);
    base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
    bool include_subdomains = false;
    transport_security_state_.AddHSTS("upgrademe.com", expiry,
                                      include_subdomains);
  }

  virtual ~MockURLRequestContext() {}

 private:
  TransportSecurityState transport_security_state_;
};

class MockHttpTransactionFactory : public HttpTransactionFactory {
 public:
  MockHttpTransactionFactory(NextProto next_proto, OrderedSocketData* data) {
    data_ = data;
    MockConnect connect_data(SYNCHRONOUS, OK);
    data_->set_connect_data(connect_data);
    session_deps_.reset(new SpdySessionDependencies(next_proto));
    session_deps_->socket_factory->AddSocketDataProvider(data_);
    http_session_ =
        SpdySessionDependencies::SpdyCreateSession(session_deps_.get());
    host_port_pair_.set_host("example.com");
    host_port_pair_.set_port(80);
    spdy_session_key_ = SpdySessionKey(host_port_pair_,
                                            ProxyServer::Direct(),
                                            kPrivacyModeDisabled);
    session_ = CreateInsecureSpdySession(
        http_session_, spdy_session_key_, BoundNetLog());
  }

  virtual int CreateTransaction(
      RequestPriority priority,
      scoped_ptr<HttpTransaction>* trans,
      HttpTransactionDelegate* delegate) OVERRIDE {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  virtual HttpCache* GetCache() OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual HttpNetworkSession* GetSession() OVERRIDE {
    return http_session_.get();
  }

 private:
  OrderedSocketData* data_;
  scoped_ptr<SpdySessionDependencies> session_deps_;
  scoped_refptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;
  HostPortPair host_port_pair_;
  SpdySessionKey spdy_session_key_;
};

}  // namespace

class WebSocketJobTest : public PlatformTest,
                         public ::testing::WithParamInterface<NextProto> {
 public:
  WebSocketJobTest() : spdy_util_(GetParam()) {}

  virtual void SetUp() OVERRIDE {
    stream_type_ = STREAM_INVALID;
    cookie_store_ = new MockCookieStore;
    context_.reset(new MockURLRequestContext(cookie_store_.get()));
  }
  virtual void TearDown() OVERRIDE {
    cookie_store_ = NULL;
    context_.reset();
    websocket_ = NULL;
    socket_ = NULL;
  }
  void DoSendRequest() {
    EXPECT_TRUE(websocket_->SendData(kHandshakeRequestWithoutCookie,
                                     kHandshakeRequestWithoutCookieLength));
  }
  void DoSendData() {
    if (received_data().size() == kHandshakeResponseWithoutCookieLength)
      websocket_->SendData(kDataHello, kDataHelloLength);
  }
  void DoSync() {
    sync_test_callback_.callback().Run(OK);
  }
  int WaitForResult() {
    return sync_test_callback_.WaitForResult();
  }
 protected:
  enum StreamType {
    STREAM_INVALID,
    STREAM_MOCK_SOCKET,
    STREAM_SOCKET,
    STREAM_SPDY_WEBSOCKET,
  };
  enum ThrottlingOption {
    THROTTLING_OFF,
    THROTTLING_ON,
  };
  enum SpdyOption {
    SPDY_OFF,
    SPDY_ON,
  };
  void InitWebSocketJob(const GURL& url,
                        MockSocketStreamDelegate* delegate,
                        StreamType stream_type) {
    DCHECK_NE(STREAM_INVALID, stream_type);
    stream_type_ = stream_type;
    websocket_ = new WebSocketJob(delegate);

    if (stream_type == STREAM_MOCK_SOCKET)
      socket_ = new MockSocketStream(url, websocket_.get());

    if (stream_type == STREAM_SOCKET || stream_type == STREAM_SPDY_WEBSOCKET) {
      if (stream_type == STREAM_SPDY_WEBSOCKET) {
        http_factory_.reset(
            new MockHttpTransactionFactory(GetParam(), data_.get()));
        context_->set_http_transaction_factory(http_factory_.get());
      }

      ssl_config_service_ = new MockSSLConfigService();
      context_->set_ssl_config_service(ssl_config_service_.get());
      proxy_service_.reset(ProxyService::CreateDirect());
      context_->set_proxy_service(proxy_service_.get());
      host_resolver_.reset(new MockHostResolver);
      context_->set_host_resolver(host_resolver_.get());

      socket_ = new SocketStream(url, websocket_.get());
      socket_factory_.reset(new MockClientSocketFactory);
      DCHECK(data_.get());
      socket_factory_->AddSocketDataProvider(data_.get());
      socket_->SetClientSocketFactory(socket_factory_.get());
    }

    websocket_->InitSocketStream(socket_.get());
    websocket_->set_context(context_.get());
    // MockHostResolver resolves all hosts to 127.0.0.1; however, when we create
    // a WebSocketJob purely to block another one in a throttling test, we don't
    // perform a real connect. In that case, the following address is used
    // instead.
    IPAddressNumber ip;
    ParseIPLiteralToNumber("127.0.0.1", &ip);
    websocket_->addresses_ = AddressList::CreateFromIPAddress(ip, 80);
  }
  void SkipToConnecting() {
    websocket_->state_ = WebSocketJob::CONNECTING;
    ASSERT_TRUE(WebSocketThrottle::GetInstance()->PutInQueue(websocket_.get()));
  }
  WebSocketJob::State GetWebSocketJobState() {
    return websocket_->state_;
  }
  void CloseWebSocketJob() {
    if (websocket_->socket_.get()) {
      websocket_->socket_->DetachDelegate();
      WebSocketThrottle::GetInstance()->RemoveFromQueue(websocket_.get());
    }
    websocket_->state_ = WebSocketJob::CLOSED;
    websocket_->delegate_ = NULL;
    websocket_->socket_ = NULL;
  }
  SocketStream* GetSocket(SocketStreamJob* job) {
    return job->socket_.get();
  }
  const std::string& sent_data() const {
    DCHECK_EQ(STREAM_MOCK_SOCKET, stream_type_);
    MockSocketStream* socket =
        static_cast<MockSocketStream*>(socket_.get());
    DCHECK(socket);
    return socket->sent_data();
  }
  const std::string& received_data() const {
    DCHECK_NE(STREAM_INVALID, stream_type_);
    MockSocketStreamDelegate* delegate =
        static_cast<MockSocketStreamDelegate*>(websocket_->delegate_);
    DCHECK(delegate);
    return delegate->received_data();
  }

  void TestSimpleHandshake();
  void TestSlowHandshake();
  void TestHandshakeWithCookie();
  void TestHandshakeWithCookieButNotAllowed();
  void TestHSTSUpgrade();
  void TestInvalidSendData();
  void TestConnectByWebSocket(ThrottlingOption throttling);
  void TestConnectBySpdy(SpdyOption spdy, ThrottlingOption throttling);
  void TestThrottlingLimit();

  SpdyWebSocketTestUtil spdy_util_;
  StreamType stream_type_;
  scoped_refptr<MockCookieStore> cookie_store_;
  scoped_ptr<MockURLRequestContext> context_;
  scoped_refptr<WebSocketJob> websocket_;
  scoped_refptr<SocketStream> socket_;
  scoped_ptr<MockClientSocketFactory> socket_factory_;
  scoped_ptr<OrderedSocketData> data_;
  TestCompletionCallback sync_test_callback_;
  scoped_refptr<MockSSLConfigService> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<MockHostResolver> host_resolver_;
  scoped_ptr<MockHttpTransactionFactory> http_factory_;

  static const char kHandshakeRequestWithoutCookie[];
  static const char kHandshakeRequestWithCookie[];
  static const char kHandshakeRequestWithFilteredCookie[];
  static const char kHandshakeResponseWithoutCookie[];
  static const char kHandshakeResponseWithCookie[];
  static const char kDataHello[];
  static const char kDataWorld[];
  static const char* const kHandshakeRequestForSpdy[];
  static const char* const kHandshakeResponseForSpdy[];
  static const size_t kHandshakeRequestWithoutCookieLength;
  static const size_t kHandshakeRequestWithCookieLength;
  static const size_t kHandshakeRequestWithFilteredCookieLength;
  static const size_t kHandshakeResponseWithoutCookieLength;
  static const size_t kHandshakeResponseWithCookieLength;
  static const size_t kDataHelloLength;
  static const size_t kDataWorldLength;
};

const char WebSocketJobTest::kHandshakeRequestWithoutCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

const char WebSocketJobTest::kHandshakeRequestWithCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "Cookie: WK-test=1\r\n"
    "\r\n";

const char WebSocketJobTest::kHandshakeRequestWithFilteredCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "Cookie: CR-test=1; CR-test-httponly=1\r\n"
    "\r\n";

const char WebSocketJobTest::kHandshakeResponseWithoutCookie[] =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "\r\n";

const char WebSocketJobTest::kHandshakeResponseWithCookie[] =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Set-Cookie: CR-set-test=1\r\n"
    "\r\n";

const char WebSocketJobTest::kDataHello[] = "Hello, ";

const char WebSocketJobTest::kDataWorld[] = "World!\n";

const size_t WebSocketJobTest::kHandshakeRequestWithoutCookieLength =
    arraysize(kHandshakeRequestWithoutCookie) - 1;
const size_t WebSocketJobTest::kHandshakeRequestWithCookieLength =
    arraysize(kHandshakeRequestWithCookie) - 1;
const size_t WebSocketJobTest::kHandshakeRequestWithFilteredCookieLength =
    arraysize(kHandshakeRequestWithFilteredCookie) - 1;
const size_t WebSocketJobTest::kHandshakeResponseWithoutCookieLength =
    arraysize(kHandshakeResponseWithoutCookie) - 1;
const size_t WebSocketJobTest::kHandshakeResponseWithCookieLength =
    arraysize(kHandshakeResponseWithCookie) - 1;
const size_t WebSocketJobTest::kDataHelloLength =
    arraysize(kDataHello) - 1;
const size_t WebSocketJobTest::kDataWorldLength =
    arraysize(kDataWorld) - 1;

void WebSocketJobTest::TestSimpleHandshake() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithoutCookie,
                             kHandshakeResponseWithoutCookieLength);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

void WebSocketJobTest::TestSlowHandshake() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  // We assume request is sent in one data chunk (from WebKit)
  // We don't support streaming request.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  std::vector<std::string> lines;
  base::SplitString(kHandshakeResponseWithoutCookie, '\n', &lines);
  for (size_t i = 0; i < lines.size() - 2; i++) {
    std::string line = lines[i] + "\r\n";
    SCOPED_TRACE("Line: " + line);
    websocket_->OnReceivedData(socket_.get(), line.c_str(), line.size());
    base::MessageLoop::current()->RunUntilIdle();
    EXPECT_TRUE(delegate.received_data().empty());
    EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  }
  websocket_->OnReceivedData(socket_.get(), "\r\n", 2);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(delegate.received_data().empty());
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

INSTANTIATE_TEST_CASE_P(
    NextProto,
    WebSocketJobTest,
    testing::Values(kProtoSPDY2, kProtoSPDY3, kProtoSPDY31, kProtoSPDY4a2,
                    kProtoHTTP2Draft04));

TEST_P(WebSocketJobTest, DelayedCookies) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  scoped_refptr<DelayedCookieMonster> cookie_store = new DelayedCookieMonster();
  context_->set_cookie_store(cookie_store.get());
  cookie_store->SetCookieWithOptionsAsync(cookieUrl,
                                          "CR-test=1",
                                          cookie_options,
                                          CookieMonster::SetCookiesCallback());
  cookie_options.set_include_httponly();
  cookie_store->SetCookieWithOptionsAsync(
      cookieUrl, "CR-test-httponly=1", cookie_options,
      CookieMonster::SetCookiesCallback());

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithFilteredCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithFilteredCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength,
            delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  CloseWebSocketJob();
}

void WebSocketJobTest::TestHandshakeWithCookie() {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithFilteredCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithFilteredCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength,
            delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(3U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[2].url);
  EXPECT_EQ("CR-set-test=1", cookie_store_->entries()[2].cookie_line);

  CloseWebSocketJob();
}

void WebSocketJobTest::TestHandshakeWithCookieButNotAllowed() {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);

  MockSocketStreamDelegate delegate;
  delegate.set_allow_all_cookies(false);
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(), kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength, delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(2U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);

  CloseWebSocketJob();
}

void WebSocketJobTest::TestHSTSUpgrade() {
  GURL url("ws://upgrademe.com/");
  MockSocketStreamDelegate delegate;
  scoped_refptr<SocketStreamJob> job =
      SocketStreamJob::CreateSocketStreamJob(
          url, &delegate, context_->transport_security_state(),
          context_->ssl_config_service());
  EXPECT_TRUE(GetSocket(job.get())->is_secure());
  job->DetachDelegate();

  url = GURL("ws://donotupgrademe.com/");
  job = SocketStreamJob::CreateSocketStreamJob(
      url, &delegate, context_->transport_security_state(),
      context_->ssl_config_service());
  EXPECT_FALSE(GetSocket(job.get())->is_secure());
  job->DetachDelegate();
}

void WebSocketJobTest::TestInvalidSendData() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  // We assume request is sent in one data chunk (from WebKit)
  // We don't support streaming request.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  // We could not send any data until connection is established.
  bool sent = websocket_->SendData(kHandshakeRequestWithoutCookie,
                                   kHandshakeRequestWithoutCookieLength);
  EXPECT_FALSE(sent);
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  CloseWebSocketJob();
}

// Following tests verify cooperation between WebSocketJob and SocketStream.
// Other former tests use MockSocketStream as SocketStream, so we could not
// check SocketStream behavior.
// OrderedSocketData provide socket level verifiation by checking out-going
// packets in comparison with the MockWrite array and emulating in-coming
// packets with MockRead array.

void WebSocketJobTest::TestConnectByWebSocket(
    ThrottlingOption throttling) {
  // This is a test for verifying cooperation between WebSocketJob and
  // SocketStream. If |throttling| was |THROTTLING_OFF|, it test basic
  // situation. If |throttling| was |THROTTLING_ON|, throttling limits the
  // latter connection.
  MockWrite writes[] = {
    MockWrite(ASYNC,
              kHandshakeRequestWithoutCookie,
              kHandshakeRequestWithoutCookieLength,
              1),
    MockWrite(ASYNC,
              kDataHello,
              kDataHelloLength,
              3)
  };
  MockRead reads[] = {
    MockRead(ASYNC,
             kHandshakeResponseWithoutCookie,
             kHandshakeResponseWithoutCookieLength,
             2),
    MockRead(ASYNC,
             kDataWorld,
             kDataWorldLength,
             4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };
  data_.reset(new OrderedSocketData(
      reads, arraysize(reads), writes, arraysize(writes)));

  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  WebSocketJobTest* test = this;
  if (throttling == THROTTLING_ON)
    delegate.SetOnStartOpenConnection(
        base::Bind(&WebSocketJobTest::DoSync, base::Unretained(test)));
  delegate.SetOnConnected(
      base::Bind(&WebSocketJobTest::DoSendRequest,
                 base::Unretained(test)));
  delegate.SetOnReceivedData(
      base::Bind(&WebSocketJobTest::DoSendData, base::Unretained(test)));
  delegate.SetOnClose(
      base::Bind(&WebSocketJobTest::DoSync, base::Unretained(test)));
  InitWebSocketJob(url, &delegate, STREAM_SOCKET);

  scoped_refptr<WebSocketJob> block_websocket;
  if (throttling == THROTTLING_ON) {
    // Create former WebSocket object which obstructs the latter one.
    block_websocket = new WebSocketJob(NULL);
    block_websocket->addresses_ = AddressList(websocket_->address_list());
    ASSERT_TRUE(
        WebSocketThrottle::GetInstance()->PutInQueue(block_websocket.get()));
  }

  websocket_->Connect();

  if (throttling == THROTTLING_ON) {
    EXPECT_EQ(OK, WaitForResult());
    EXPECT_TRUE(websocket_->IsWaiting());

    // Remove the former WebSocket object from throttling queue to unblock the
    // latter.
    block_websocket->state_ = WebSocketJob::CLOSED;
    WebSocketThrottle::GetInstance()->RemoveFromQueue(block_websocket.get());
    block_websocket = NULL;
  }

  EXPECT_EQ(OK, WaitForResult());
  EXPECT_TRUE(data_->at_read_eof());
  EXPECT_TRUE(data_->at_write_eof());
  EXPECT_EQ(WebSocketJob::CLOSED, GetWebSocketJobState());
}

void WebSocketJobTest::TestConnectBySpdy(
    SpdyOption spdy, ThrottlingOption throttling) {
  // This is a test for verifying cooperation between WebSocketJob and
  // SocketStream in the situation we have SPDY session to the server. If
  // |throttling| was |THROTTLING_ON|, throttling limits the latter connection.
  // If you enabled spdy, you should specify |spdy| as |SPDY_ON|. Expected
  // results depend on its configuration.
  MockWrite writes_websocket[] = {
    MockWrite(ASYNC,
              kHandshakeRequestWithoutCookie,
              kHandshakeRequestWithoutCookieLength,
              1),
    MockWrite(ASYNC,
              kDataHello,
              kDataHelloLength,
              3)
  };
  MockRead reads_websocket[] = {
    MockRead(ASYNC,
             kHandshakeResponseWithoutCookie,
             kHandshakeResponseWithoutCookieLength,
             2),
    MockRead(ASYNC,
             kDataWorld,
             kDataWorldLength,
             4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };

  scoped_ptr<SpdyHeaderBlock> request_headers(new SpdyHeaderBlock());
  spdy_util_.SetHeader("path", "/demo", request_headers.get());
  spdy_util_.SetHeader("version", "WebSocket/13", request_headers.get());
  spdy_util_.SetHeader("scheme", "ws", request_headers.get());
  spdy_util_.SetHeader("host", "example.com", request_headers.get());
  spdy_util_.SetHeader("origin", "http://example.com", request_headers.get());
  spdy_util_.SetHeader("sec-websocket-protocol", "sample",
                       request_headers.get());

  scoped_ptr<SpdyHeaderBlock> response_headers(new SpdyHeaderBlock());
  spdy_util_.SetHeader("status", "101 Switching Protocols",
                       response_headers.get());
  spdy_util_.SetHeader("sec-websocket-protocol", "sample",
                       response_headers.get());

  const SpdyStreamId kStreamId = 1;
  scoped_ptr<SpdyFrame> request_frame(
      spdy_util_.ConstructSpdyWebSocketHandshakeRequestFrame(
          request_headers.Pass(),
          kStreamId,
          MEDIUM));
  scoped_ptr<SpdyFrame> response_frame(
      spdy_util_.ConstructSpdyWebSocketHandshakeResponseFrame(
          response_headers.Pass(),
          kStreamId,
          MEDIUM));
  scoped_ptr<SpdyFrame> data_hello_frame(
      spdy_util_.ConstructSpdyWebSocketDataFrame(
          kDataHello,
          kDataHelloLength,
          kStreamId,
          false));
  scoped_ptr<SpdyFrame> data_world_frame(
      spdy_util_.ConstructSpdyWebSocketDataFrame(
          kDataWorld,
          kDataWorldLength,
          kStreamId,
          false));
  MockWrite writes_spdy[] = {
    CreateMockWrite(*request_frame.get(), 1),
    CreateMockWrite(*data_hello_frame.get(), 3),
  };
  MockRead reads_spdy[] = {
    CreateMockRead(*response_frame.get(), 2),
    CreateMockRead(*data_world_frame.get(), 4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };

  if (spdy == SPDY_ON)
    data_.reset(new OrderedSocketData(
        reads_spdy, arraysize(reads_spdy),
        writes_spdy, arraysize(writes_spdy)));
  else
    data_.reset(new OrderedSocketData(
        reads_websocket, arraysize(reads_websocket),
        writes_websocket, arraysize(writes_websocket)));

  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  WebSocketJobTest* test = this;
  if (throttling == THROTTLING_ON)
    delegate.SetOnStartOpenConnection(
        base::Bind(&WebSocketJobTest::DoSync, base::Unretained(test)));
  delegate.SetOnConnected(
      base::Bind(&WebSocketJobTest::DoSendRequest,
                 base::Unretained(test)));
  delegate.SetOnReceivedData(
      base::Bind(&WebSocketJobTest::DoSendData, base::Unretained(test)));
  delegate.SetOnClose(
      base::Bind(&WebSocketJobTest::DoSync, base::Unretained(test)));
  InitWebSocketJob(url, &delegate, STREAM_SPDY_WEBSOCKET);

  scoped_refptr<WebSocketJob> block_websocket;
  if (throttling == THROTTLING_ON) {
    // Create former WebSocket object which obstructs the latter one.
    block_websocket = new WebSocketJob(NULL);
    block_websocket->addresses_ = AddressList(websocket_->address_list());
    ASSERT_TRUE(
        WebSocketThrottle::GetInstance()->PutInQueue(block_websocket.get()));
  }

  websocket_->Connect();

  if (throttling == THROTTLING_ON) {
    EXPECT_EQ(OK, WaitForResult());
    EXPECT_TRUE(websocket_->IsWaiting());

    // Remove the former WebSocket object from throttling queue to unblock the
    // latter.
    block_websocket->state_ = WebSocketJob::CLOSED;
    WebSocketThrottle::GetInstance()->RemoveFromQueue(block_websocket.get());
    block_websocket = NULL;
  }

  EXPECT_EQ(OK, WaitForResult());
  EXPECT_TRUE(data_->at_read_eof());
  EXPECT_TRUE(data_->at_write_eof());
  EXPECT_EQ(WebSocketJob::CLOSED, GetWebSocketJobState());
}

void WebSocketJobTest::TestThrottlingLimit() {
  std::vector<scoped_refptr<WebSocketJob> > jobs;
  const int kMaxWebSocketJobsThrottled = 1024;
  IPAddressNumber ip;
  ParseIPLiteralToNumber("127.0.0.1", &ip);
  for (int i = 0; i < kMaxWebSocketJobsThrottled + 1; ++i) {
    scoped_refptr<WebSocketJob> job = new WebSocketJob(NULL);
    job->addresses_ = AddressList(AddressList::CreateFromIPAddress(ip, 80));
    if (i >= kMaxWebSocketJobsThrottled)
      EXPECT_FALSE(WebSocketThrottle::GetInstance()->PutInQueue(job));
    else
      EXPECT_TRUE(WebSocketThrottle::GetInstance()->PutInQueue(job));
    jobs.push_back(job);
  }

  // Close the jobs in reverse order. Otherwise, We need to make them prepared
  // for Wakeup call.
  for (std::vector<scoped_refptr<WebSocketJob> >::reverse_iterator iter =
           jobs.rbegin();
       iter != jobs.rend();
       ++iter) {
    WebSocketJob* job = (*iter).get();
    job->state_ = WebSocketJob::CLOSED;
    WebSocketThrottle::GetInstance()->RemoveFromQueue(job);
  }
}

// Execute tests in both spdy-disabled mode and spdy-enabled mode.
TEST_P(WebSocketJobTest, SimpleHandshake) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestSimpleHandshake();
}

TEST_P(WebSocketJobTest, SlowHandshake) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestSlowHandshake();
}

TEST_P(WebSocketJobTest, HandshakeWithCookie) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHandshakeWithCookie();
}

TEST_P(WebSocketJobTest, HandshakeWithCookieButNotAllowed) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHandshakeWithCookieButNotAllowed();
}

TEST_P(WebSocketJobTest, HSTSUpgrade) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHSTSUpgrade();
}

TEST_P(WebSocketJobTest, InvalidSendData) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestInvalidSendData();
}

TEST_P(WebSocketJobTest, SimpleHandshakeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestSimpleHandshake();
}

TEST_P(WebSocketJobTest, SlowHandshakeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestSlowHandshake();
}

TEST_P(WebSocketJobTest, HandshakeWithCookieSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHandshakeWithCookie();
}

TEST_P(WebSocketJobTest, HandshakeWithCookieButNotAllowedSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHandshakeWithCookieButNotAllowed();
}

TEST_P(WebSocketJobTest, HSTSUpgradeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHSTSUpgrade();
}

TEST_P(WebSocketJobTest, InvalidSendDataSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestInvalidSendData();
}

TEST_P(WebSocketJobTest, ConnectByWebSocket) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectByWebSocket(THROTTLING_OFF);
}

TEST_P(WebSocketJobTest, ConnectByWebSocketSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectByWebSocket(THROTTLING_OFF);
}

TEST_P(WebSocketJobTest, ConnectBySpdy) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectBySpdy(SPDY_OFF, THROTTLING_OFF);
}

TEST_P(WebSocketJobTest, ConnectBySpdySpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectBySpdy(SPDY_ON, THROTTLING_OFF);
}

TEST_P(WebSocketJobTest, ThrottlingWebSocket) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectByWebSocket(THROTTLING_ON);
}

TEST_P(WebSocketJobTest, ThrottlingMaxNumberOfThrottledJobLimit) {
  TestThrottlingLimit();
}

TEST_P(WebSocketJobTest, ThrottlingWebSocketSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectByWebSocket(THROTTLING_ON);
}

TEST_P(WebSocketJobTest, ThrottlingSpdy) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectBySpdy(SPDY_OFF, THROTTLING_ON);
}

TEST_P(WebSocketJobTest, ThrottlingSpdySpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectBySpdy(SPDY_ON, THROTTLING_ON);
}

// TODO(toyoshim): Add tests to verify throttling, SPDY stream limitation.
// TODO(toyoshim,yutak): Add tests to verify closing handshake.
}  // namespace net
