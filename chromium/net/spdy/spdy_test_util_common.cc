// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_common.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"

namespace net {

namespace {

bool next_proto_is_spdy(NextProto next_proto) {
  // TODO(akalin): Change this to kProtoSPDYMinimumVersion once we
  // stop supporting SPDY/1.
  return next_proto >= kProtoSPDY2 && next_proto <= kProtoSPDYMaximumVersion;
}

// Parses a URL into the scheme, host, and path components required for a
// SPDY request.
void ParseUrl(base::StringPiece url, std::string* scheme, std::string* host,
              std::string* path) {
  GURL gurl(url.as_string());
  path->assign(gurl.PathForRequest());
  scheme->assign(gurl.scheme());
  host->assign(gurl.host());
  if (gurl.has_port()) {
    host->append(":");
    host->append(gurl.port());
  }
}

}  // namespace

std::vector<NextProto> SpdyNextProtos() {
  std::vector<NextProto> next_protos;
  for (int i = kProtoMinimumVersion; i <= kProtoMaximumVersion; ++i) {
    NextProto proto = static_cast<NextProto>(i);
    if (proto != kProtoSPDY1 && proto != kProtoSPDY21)
      next_protos.push_back(proto);
  }
  return next_protos;
}

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const char* data, int length, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(ASYNC, ptr, chunk_size);
  }
  return chunks;
}

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks) {
  return ChopWriteFrame(frame.data(), frame.size(), num_chunks);
}

// Chop a frame into an array of MockReads.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const char* data, int length, int num_chunks) {
  MockRead* chunks = new MockRead[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockRead(ASYNC, ptr, chunk_size);
  }
  return chunks;
}

// Chop a SpdyFrame into an array of MockReads.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const SpdyFrame& frame, int num_chunks) {
  return ChopReadFrame(frame.data(), frame.size(), num_chunks);
}

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendToHeaderBlock(const char* const extra_headers[],
                         int extra_header_count,
                         SpdyHeaderBlock* headers) {
  std::string this_header;
  std::string this_value;

  if (!extra_header_count)
    return;

  // Sanity check: Non-NULL header list.
  DCHECK(NULL != extra_headers) << "NULL header value pair list";
  // Sanity check: Non-NULL header map.
  DCHECK(NULL != headers) << "NULL header map";
  // Copy in the headers.
  for (int i = 0; i < extra_header_count; i++) {
    // Sanity check: Non-empty header.
    DCHECK_NE('\0', *extra_headers[i * 2]) << "Empty header value pair";
    this_header = extra_headers[i * 2];
    std::string::size_type header_len = this_header.length();
    if (!header_len)
      continue;
    this_value = extra_headers[1 + (i * 2)];
    std::string new_value;
    if (headers->find(this_header) != headers->end()) {
      // More than one entry in the header.
      // Don't add the header again, just the append to the value,
      // separated by a NULL character.

      // Adjust the value.
      new_value = (*headers)[this_header];
      // Put in a NULL separator.
      new_value.append(1, '\0');
      // Append the new value.
      new_value += this_value;
    } else {
      // Not a duplicate, just write the value.
      new_value = this_value;
    }
    (*headers)[this_header] = new_value;
  }
}

// Create a MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req) {
  return MockWrite(ASYNC, req.data(), req.size());
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq) {
  return CreateMockWrite(req, seq, ASYNC);
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode) {
  return MockWrite(mode, req.data(), req.size(), seq);
}

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp) {
  return MockRead(ASYNC, resp.data(), resp.size());
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq) {
  return CreateMockRead(resp, seq, ASYNC);
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode) {
  return MockRead(mode, resp.data(), resp.size(), seq);
}

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames, int num_frames,
                  char* buff, int buff_len) {
  int total_len = 0;
  for (int i = 0; i < num_frames; ++i) {
    total_len += frames[i]->size();
  }
  DCHECK_LE(total_len, buff_len);
  char* ptr = buff;
  for (int i = 0; i < num_frames; ++i) {
    int len = frames[i]->size();
    memcpy(ptr, frames[i]->data(), len);
    ptr += len;
  }
  return total_len;
}

namespace {

class PriorityGetter : public BufferedSpdyFramerVisitorInterface {
 public:
  PriorityGetter() : priority_(0) {}
  virtual ~PriorityGetter() {}

  SpdyPriority priority() const {
    return priority_;
  }

  virtual void OnError(SpdyFramer::SpdyError error_code) OVERRIDE {}
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) OVERRIDE {}
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) OVERRIDE {
    priority_ = priority;
  }
  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) OVERRIDE {}
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) OVERRIDE {}
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {}
  virtual void OnSettings(bool clear_persisted) OVERRIDE {}
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE {}
  virtual void OnPing(uint32 unique_id) OVERRIDE {}
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {}
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {}
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {}
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id) OVERRIDE {}

 private:
  SpdyPriority priority_;
};

}  // namespace

bool GetSpdyPriority(SpdyMajorVersion version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority) {
  BufferedSpdyFramer framer(version, false);
  PriorityGetter priority_getter;
  framer.set_visitor(&priority_getter);
  size_t frame_size = frame.size();
  if (framer.ProcessInput(frame.data(), frame_size) != frame_size) {
    return false;
  }
  *priority = priority_getter.priority();
  return true;
}

base::WeakPtr<SpdyStream> CreateStreamSynchronously(
    SpdyStreamType type,
    const base::WeakPtr<SpdySession>& session,
    const GURL& url,
    RequestPriority priority,
    const BoundNetLog& net_log) {
  SpdyStreamRequest stream_request;
  int rv = stream_request.StartRequest(type, session, url, priority, net_log,
                                       CompletionCallback());
  return
      (rv == OK) ? stream_request.ReleaseStream() : base::WeakPtr<SpdyStream>();
}

StreamReleaserCallback::StreamReleaserCallback() {}

StreamReleaserCallback::~StreamReleaserCallback() {}

CompletionCallback StreamReleaserCallback::MakeCallback(
    SpdyStreamRequest* request) {
  return base::Bind(&StreamReleaserCallback::OnComplete,
                    base::Unretained(this),
                    request);
}

void StreamReleaserCallback::OnComplete(
    SpdyStreamRequest* request, int result) {
  if (result == OK)
    request->ReleaseStream()->Cancel();
  SetResult(result);
}

MockECSignatureCreator::MockECSignatureCreator(crypto::ECPrivateKey* key)
    : key_(key) {
}

bool MockECSignatureCreator::Sign(const uint8* data,
                                  int data_len,
                                  std::vector<uint8>* signature) {
  std::vector<uint8> private_key_value;
  key_->ExportValue(&private_key_value);
  std::string head = "fakesignature";
  std::string tail = "/fakesignature";

  signature->clear();
  signature->insert(signature->end(), head.begin(), head.end());
  signature->insert(signature->end(), private_key_value.begin(),
                    private_key_value.end());
  signature->insert(signature->end(), '-');
  signature->insert(signature->end(), data, data + data_len);
  signature->insert(signature->end(), tail.begin(), tail.end());
  return true;
}

bool MockECSignatureCreator::DecodeSignature(
    const std::vector<uint8>& signature,
    std::vector<uint8>* out_raw_sig) {
  *out_raw_sig = signature;
  return true;
}

MockECSignatureCreatorFactory::MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(this);
}

MockECSignatureCreatorFactory::~MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(NULL);
}

crypto::ECSignatureCreator* MockECSignatureCreatorFactory::Create(
    crypto::ECPrivateKey* key) {
  return new MockECSignatureCreator(key);
}

SpdySessionDependencies::SpdySessionDependencies(NextProto protocol)
    : host_resolver(new MockCachingHostResolver),
      cert_verifier(new MockCertVerifier),
      transport_security_state(new TransportSecurityState),
      proxy_service(ProxyService::CreateDirect()),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      deterministic_socket_factory(new DeterministicMockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      enable_user_alternate_protocol_ports(false),
      protocol(protocol),
      stream_initial_recv_window_size(kSpdyStreamInitialWindowSize),
      time_func(&base::TimeTicks::Now),
      net_log(NULL) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;

  // Note: The CancelledTransaction test does cleanup by running all
  // tasks in the message loop (RunAllPending).  Unfortunately, that
  // doesn't clean up tasks on the host resolver thread; and
  // TCPConnectJob is currently not cancellable.  Using synchronous
  // lookups allows the test to shutdown cleanly.  Until we have
  // cancellable TCPConnectJobs, use synchronous lookups.
  host_resolver->set_synchronous_mode(true);
}

SpdySessionDependencies::SpdySessionDependencies(
    NextProto protocol, ProxyService* proxy_service)
    : host_resolver(new MockHostResolver),
      cert_verifier(new MockCertVerifier),
      transport_security_state(new TransportSecurityState),
      proxy_service(proxy_service),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      deterministic_socket_factory(new DeterministicMockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      enable_user_alternate_protocol_ports(false),
      protocol(protocol),
      stream_initial_recv_window_size(kSpdyStreamInitialWindowSize),
      time_func(&base::TimeTicks::Now),
      net_log(NULL) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;
}

SpdySessionDependencies::~SpdySessionDependencies() {}

// static
HttpNetworkSession* SpdySessionDependencies::SpdyCreateSession(
    SpdySessionDependencies* session_deps) {
  net::HttpNetworkSession::Params params = CreateSessionParams(session_deps);
  params.client_socket_factory = session_deps->socket_factory.get();
  HttpNetworkSession* http_session = new HttpNetworkSession(params);
  SpdySessionPoolPeer pool_peer(http_session->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  return http_session;
}

// static
HttpNetworkSession* SpdySessionDependencies::SpdyCreateSessionDeterministic(
    SpdySessionDependencies* session_deps) {
  net::HttpNetworkSession::Params params = CreateSessionParams(session_deps);
  params.client_socket_factory =
      session_deps->deterministic_socket_factory.get();
  HttpNetworkSession* http_session = new HttpNetworkSession(params);
  SpdySessionPoolPeer pool_peer(http_session->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  return http_session;
}

// static
net::HttpNetworkSession::Params SpdySessionDependencies::CreateSessionParams(
    SpdySessionDependencies* session_deps) {
  DCHECK(next_proto_is_spdy(session_deps->protocol)) <<
      "Invalid protocol: " << session_deps->protocol;

  net::HttpNetworkSession::Params params;
  params.host_resolver = session_deps->host_resolver.get();
  params.cert_verifier = session_deps->cert_verifier.get();
  params.transport_security_state =
      session_deps->transport_security_state.get();
  params.proxy_service = session_deps->proxy_service.get();
  params.ssl_config_service = session_deps->ssl_config_service.get();
  params.http_auth_handler_factory =
      session_deps->http_auth_handler_factory.get();
  params.http_server_properties =
      session_deps->http_server_properties.GetWeakPtr();
  params.enable_spdy_compression = session_deps->enable_compression;
  params.enable_spdy_ping_based_connection_checking = session_deps->enable_ping;
  params.enable_user_alternate_protocol_ports =
      session_deps->enable_user_alternate_protocol_ports;
  params.spdy_default_protocol = session_deps->protocol;
  params.spdy_stream_initial_recv_window_size =
      session_deps->stream_initial_recv_window_size;
  params.time_func = session_deps->time_func;
  params.trusted_spdy_proxy = session_deps->trusted_spdy_proxy;
  params.net_log = session_deps->net_log;
  return params;
}

SpdyURLRequestContext::SpdyURLRequestContext(NextProto protocol)
    : storage_(this) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;

  storage_.set_host_resolver(scoped_ptr<HostResolver>(new MockHostResolver));
  storage_.set_cert_verifier(new MockCertVerifier);
  storage_.set_transport_security_state(new TransportSecurityState);
  storage_.set_proxy_service(ProxyService::CreateDirect());
  storage_.set_ssl_config_service(new SSLConfigServiceDefaults);
  storage_.set_http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault(
      host_resolver()));
  storage_.set_http_server_properties(
      scoped_ptr<HttpServerProperties>(new HttpServerPropertiesImpl()));
  net::HttpNetworkSession::Params params;
  params.client_socket_factory = &socket_factory_;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.transport_security_state = transport_security_state();
  params.proxy_service = proxy_service();
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = network_delegate();
  params.enable_spdy_compression = false;
  params.enable_spdy_ping_based_connection_checking = false;
  params.spdy_default_protocol = protocol;
  params.http_server_properties = http_server_properties();
  scoped_refptr<HttpNetworkSession> network_session(
      new HttpNetworkSession(params));
  SpdySessionPoolPeer pool_peer(network_session->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  storage_.set_http_transaction_factory(new HttpCache(
      network_session.get(), HttpCache::DefaultBackend::InMemory(0)));
}

SpdyURLRequestContext::~SpdyURLRequestContext() {
}

bool HasSpdySession(SpdySessionPool* pool, const SpdySessionKey& key) {
  return pool->FindAvailableSession(key, BoundNetLog()) != NULL;
}

namespace {

base::WeakPtr<SpdySession> CreateSpdySessionHelper(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log,
    Error expected_status,
    bool is_secure) {
  EXPECT_FALSE(HasSpdySession(http_session->spdy_session_pool(), key));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(
          key.host_port_pair(), MEDIUM, false, false,
          OnHostResolutionCallback()));

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  TestCompletionCallback callback;

  int rv = ERR_UNEXPECTED;
  if (is_secure) {
    SSLConfig ssl_config;
    scoped_refptr<SOCKSSocketParams> socks_params;
    scoped_refptr<HttpProxySocketParams> http_proxy_params;
    scoped_refptr<SSLSocketParams> ssl_params(
        new SSLSocketParams(transport_params,
                            socks_params,
                            http_proxy_params,
                            ProxyServer::SCHEME_DIRECT,
                            key.host_port_pair(),
                            ssl_config,
                            key.privacy_mode(),
                            0,
                            false,
                            false));
    rv = connection->Init(key.host_port_pair().ToString(),
                          ssl_params,
                          MEDIUM,
                          callback.callback(),
                          http_session->GetSSLSocketPool(
                              HttpNetworkSession::NORMAL_SOCKET_POOL),
                          net_log);
  } else {
    rv = connection->Init(key.host_port_pair().ToString(),
                          transport_params,
                          MEDIUM,
                          callback.callback(),
                          http_session->GetTransportSocketPool(
                              HttpNetworkSession::NORMAL_SOCKET_POOL),
                          net_log);
  }

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);

  base::WeakPtr<SpdySession> spdy_session;
  EXPECT_EQ(
      expected_status,
      http_session->spdy_session_pool()->CreateAvailableSessionFromSocket(
          key, connection.Pass(), net_log, OK, &spdy_session,
          is_secure));
  EXPECT_EQ(expected_status == OK, spdy_session != NULL);
  EXPECT_EQ(expected_status == OK,
            HasSpdySession(http_session->spdy_session_pool(), key));
  return spdy_session;
}

}  // namespace

base::WeakPtr<SpdySession> CreateInsecureSpdySession(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log) {
  return CreateSpdySessionHelper(http_session, key, net_log,
                                 OK, false /* is_secure */);
}

void TryCreateInsecureSpdySessionExpectingFailure(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    Error expected_error,
    const BoundNetLog& net_log) {
  DCHECK_LT(expected_error, ERR_IO_PENDING);
  CreateSpdySessionHelper(http_session, key, net_log,
                          expected_error, false /* is_secure */);
}

base::WeakPtr<SpdySession> CreateSecureSpdySession(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log) {
  return CreateSpdySessionHelper(http_session, key, net_log,
                                 OK, true /* is_secure */);
}

namespace {

// A ClientSocket used for CreateFakeSpdySession() below.
class FakeSpdySessionClientSocket : public MockClientSocket {
 public:
  FakeSpdySessionClientSocket(int read_result)
      : MockClientSocket(BoundNetLog()),
        read_result_(read_result) {}

  virtual ~FakeSpdySessionClientSocket() {}

  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return read_result_;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  // Return kProtoUnknown to use the pool's default protocol.
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }

  // The functions below are not expected to be called.

  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual bool WasEverUsed() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual bool UsingTCPFastOpen() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual bool WasNpnNegotiated() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    ADD_FAILURE();
    return false;
  }

 private:
  int read_result_;
};

base::WeakPtr<SpdySession> CreateFakeSpdySessionHelper(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    Error expected_status) {
  EXPECT_NE(expected_status, ERR_IO_PENDING);
  EXPECT_FALSE(HasSpdySession(pool, key));
  base::WeakPtr<SpdySession> spdy_session;
  scoped_ptr<ClientSocketHandle> handle(new ClientSocketHandle());
  handle->set_socket(new FakeSpdySessionClientSocket(
      expected_status == OK ? ERR_IO_PENDING : expected_status));
  EXPECT_EQ(
      expected_status,
      pool->CreateAvailableSessionFromSocket(
          key, handle.Pass(), BoundNetLog(), OK, &spdy_session,
          true /* is_secure */));
  EXPECT_EQ(expected_status == OK, spdy_session != NULL);
  EXPECT_EQ(expected_status == OK, HasSpdySession(pool, key));
  return spdy_session;
}

}  // namespace

base::WeakPtr<SpdySession> CreateFakeSpdySession(SpdySessionPool* pool,
                                                 const SpdySessionKey& key) {
  return CreateFakeSpdySessionHelper(pool, key, OK);
}

void TryCreateFakeSpdySessionExpectingFailure(SpdySessionPool* pool,
                                              const SpdySessionKey& key,
                                              Error expected_error) {
  DCHECK_LT(expected_error, ERR_IO_PENDING);
  CreateFakeSpdySessionHelper(pool, key, expected_error);
}

SpdySessionPoolPeer::SpdySessionPoolPeer(SpdySessionPool* pool) : pool_(pool) {
}

void SpdySessionPoolPeer::RemoveAliases(const SpdySessionKey& key) {
  pool_->RemoveAliases(key);
}

void SpdySessionPoolPeer::DisableDomainAuthenticationVerification() {
  pool_->verify_domain_authentication_ = false;
}

void SpdySessionPoolPeer::SetEnableSendingInitialData(bool enabled) {
  pool_->enable_sending_initial_data_ = enabled;
}

SpdyTestUtil::SpdyTestUtil(NextProto protocol)
    : protocol_(protocol),
      spdy_version_(NextProtoToSpdyMajorVersion(protocol)) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;
}

void SpdyTestUtil::AddUrlToHeaderBlock(base::StringPiece url,
                                       SpdyHeaderBlock* headers) const {
  if (is_spdy2()) {
    (*headers)["url"] = url.as_string();
  } else {
    std::string scheme, host, path;
    ParseUrl(url, &scheme, &host, &path);
    (*headers)[GetSchemeKey()] = scheme;
    (*headers)[GetHostKey()] = host;
    (*headers)[GetPathKey()] = path;
  }
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructGetHeaderBlock(
    base::StringPiece url) const {
  return ConstructHeaderBlock("GET", url, NULL);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructGetHeaderBlockForProxy(
    base::StringPiece url) const {
  scoped_ptr<SpdyHeaderBlock> headers(ConstructGetHeaderBlock(url));
  if (is_spdy2())
    (*headers)[GetPathKey()] = url.data();
  return headers.Pass();
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructHeadHeaderBlock(
    base::StringPiece url,
    int64 content_length) const {
  return ConstructHeaderBlock("HEAD", url, &content_length);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructPostHeaderBlock(
    base::StringPiece url,
    int64 content_length) const {
  return ConstructHeaderBlock("POST", url, &content_length);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructPutHeaderBlock(
    base::StringPiece url,
    int64 content_length) const {
  return ConstructHeaderBlock("PUT", url, &content_length);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyFrame(
    const SpdyHeaderInfo& header_info,
    scoped_ptr<SpdyHeaderBlock> headers) const {
  BufferedSpdyFramer framer(spdy_version_, header_info.compressed);
  SpdyFrame* frame = NULL;
  switch (header_info.kind) {
    case DATA:
      frame = framer.CreateDataFrame(header_info.id, header_info.data,
                                     header_info.data_length,
                                     header_info.data_flags);
      break;
    case SYN_STREAM:
      {
        size_t credential_slot = is_spdy2() ? 0 : header_info.credential_slot;
        frame = framer.CreateSynStream(header_info.id, header_info.assoc_id,
                                       header_info.priority,
                                       credential_slot,
                                       header_info.control_flags,
                                       header_info.compressed, headers.get());
      }
      break;
    case SYN_REPLY:
      frame = framer.CreateSynReply(header_info.id, header_info.control_flags,
                                    header_info.compressed, headers.get());
      break;
    case RST_STREAM:
      frame = framer.CreateRstStream(header_info.id, header_info.status);
      break;
    case HEADERS:
      frame = framer.CreateHeaders(header_info.id, header_info.control_flags,
                                   header_info.compressed, headers.get());
      break;
    default:
      ADD_FAILURE();
      break;
  }
  return frame;
}

SpdyFrame* SpdyTestUtil::ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                                            const char* const extra_headers[],
                                            int extra_header_count,
                                            const char* const tail_headers[],
                                            int tail_header_count) const {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  if (tail_headers && tail_header_count)
    AppendToHeaderBlock(tail_headers, tail_header_count, headers.get());
  return ConstructSpdyFrame(header_info, headers.Pass());
}

SpdyFrame* SpdyTestUtil::ConstructSpdyControlFrame(
    scoped_ptr<SpdyHeaderBlock> headers,
    bool compressed,
    SpdyStreamId stream_id,
    RequestPriority request_priority,
    SpdyFrameType type,
    SpdyControlFlags flags,
    SpdyStreamId associated_stream_id) const {
  EXPECT_GE(type, FIRST_CONTROL_TYPE);
  EXPECT_LE(type, LAST_CONTROL_TYPE);
  const SpdyHeaderInfo header_info = {
    type,
    stream_id,
    associated_stream_id,
    ConvertRequestPriorityToSpdyPriority(request_priority, spdy_version_),
    0,  // credential slot
    flags,
    compressed,
    RST_STREAM_INVALID,  // status
    NULL,  // data
    0,  // length
    DATA_FLAG_NONE
  };
  return ConstructSpdyFrame(header_info, headers.Pass());
}

SpdyFrame* SpdyTestUtil::ConstructSpdyControlFrame(
    const char* const extra_headers[],
    int extra_header_count,
    bool compressed,
    SpdyStreamId stream_id,
    RequestPriority request_priority,
    SpdyFrameType type,
    SpdyControlFlags flags,
    const char* const* tail_headers,
    int tail_header_size,
    SpdyStreamId associated_stream_id) const {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  if (tail_headers && tail_header_size)
    AppendToHeaderBlock(tail_headers, tail_header_size / 2, headers.get());
  return ConstructSpdyControlFrame(
      headers.Pass(), compressed, stream_id,
      request_priority, type, flags, associated_stream_id);
}

std::string SpdyTestUtil::ConstructSpdyReplyString(
    const SpdyHeaderBlock& headers) const {
  std::string reply_string;
  for (SpdyHeaderBlock::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string key = it->first;
    // Remove leading colon from "special" headers (for SPDY3 and
    // above).
    if (spdy_version() >= SPDY3 && key[0] == ':')
      key = key.substr(1);
    std::vector<std::string> values;
    base::SplitString(it->second, '\0', &values);
    for (std::vector<std::string>::const_iterator it2 = values.begin();
         it2 != values.end(); ++it2) {
      reply_string += key + ": " + *it2 + "\n";
    }
  }
  return reply_string;
}

SpdyFrame* SpdyTestUtil::ConstructSpdySettings(
    const SettingsMap& settings) const {
  return CreateFramer()->CreateSettings(settings);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyCredential(
    const SpdyCredential& credential) const {
  return CreateFramer()->CreateCredentialFrame(credential);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPing(uint32 ping_id) const {
  return CreateFramer()->CreatePingFrame(ping_id);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGoAway() const {
  return ConstructSpdyGoAway(0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGoAway(
    SpdyStreamId last_good_stream_id) const {
  return CreateFramer()->CreateGoAway(last_good_stream_id, GOAWAY_OK);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyWindowUpdate(
    const SpdyStreamId stream_id, uint32 delta_window_size) const {
  return CreateFramer()->CreateWindowUpdate(stream_id, delta_window_size);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyRstStream(
    SpdyStreamId stream_id,
    SpdyRstStreamStatus status) const {
  return CreateFramer()->CreateRstStream(stream_id, status);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGet(
    const char* const url,
    bool compressed,
    SpdyStreamId stream_id,
    RequestPriority request_priority) const {
  const SpdyHeaderInfo header_info = {
    SYN_STREAM,
    stream_id,
    0,                   // associated stream ID
    ConvertRequestPriorityToSpdyPriority(request_priority, spdy_version_),
    0,                   // credential slot
    CONTROL_FLAG_FIN,
    compressed,
    RST_STREAM_INVALID,  // status
    NULL,                // data
    0,                   // length
    DATA_FLAG_NONE
  };
  return ConstructSpdyFrame(header_info, ConstructGetHeaderBlock(url));
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGet(const char* const extra_headers[],
                                          int extra_header_count,
                                          bool compressed,
                                          int stream_id,
                                          RequestPriority request_priority,
                                          bool direct) const {
  const bool spdy2 = is_spdy2();
  const char* url = (spdy2 && !direct) ? "http://www.google.com/" : "/";
  const char* const kStandardGetHeaders[] = {
    GetMethodKey(),  "GET",
    GetHostKey(),    "www.google.com",
    GetSchemeKey(),  "http",
    GetVersionKey(), "HTTP/1.1",
    GetPathKey(),    url
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   compressed,
                                   stream_id,
                                   request_priority,
                                   SYN_STREAM,
                                   CONTROL_FLAG_FIN,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyConnect(
    const char* const extra_headers[],
    int extra_header_count,
    int stream_id) const {
  const char* const kConnectHeaders[] = {
    GetMethodKey(),  "CONNECT",
    GetPathKey(),    "www.google.com:443",
    GetHostKey(),    "www.google.com",
    GetVersionKey(), "HTTP/1.1",
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   /*compressed*/ false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kConnectHeaders,
                                   arraysize(kConnectHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPush(const char* const extra_headers[],
                                           int extra_header_count,
                                           int stream_id,
                                           int associated_stream_id,
                                           const char* url) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  (*headers)["hello"] = "bye";
  (*headers)[GetStatusKey()] = "200 OK";
  (*headers)[GetVersionKey()] = "HTTP/1.1";
  AddUrlToHeaderBlock(url, headers.get());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  return ConstructSpdyControlFrame(headers.Pass(),
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   associated_stream_id);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPush(const char* const extra_headers[],
                                           int extra_header_count,
                                           int stream_id,
                                           int associated_stream_id,
                                           const char* url,
                                           const char* status,
                                           const char* location) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  (*headers)["hello"] = "bye";
  (*headers)[GetStatusKey()] = status;
  (*headers)[GetVersionKey()] = "HTTP/1.1";
  (*headers)["location"] = location;
  AddUrlToHeaderBlock(url, headers.get());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  return ConstructSpdyControlFrame(headers.Pass(),
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   associated_stream_id);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPushHeaders(
    int stream_id,
    const char* const extra_headers[],
    int extra_header_count) {
  const char* const kStandardGetHeaders[] = {
    GetStatusKey(), "200 OK",
    GetVersionKey(), "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   HEADERS,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdySynReplyError(
    const char* const status,
    const char* const* const extra_headers,
    int extra_header_count,
    int stream_id) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    GetStatusKey(), status,
    GetVersionKey(), "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGetSynReplyRedirect(int stream_id) {
  static const char* const kExtraHeaders[] = {
    "location", "http://www.foo.com/index.php",
  };
  return ConstructSpdySynReplyError("301 Moved Permanently", kExtraHeaders,
                                    arraysize(kExtraHeaders)/2, stream_id);
}

SpdyFrame* SpdyTestUtil::ConstructSpdySynReplyError(int stream_id) {
  return ConstructSpdySynReplyError("500 Internal Server Error", NULL, 0, 1);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGetSynReply(
    const char* const extra_headers[],
    int extra_header_count,
    int stream_id) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    GetStatusKey(), "200",
    GetVersionKey(), "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPost(const char* url,
                                           SpdyStreamId stream_id,
                                           int64 content_length,
                                           RequestPriority priority,
                                           const char* const extra_headers[],
                                           int extra_header_count) {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    stream_id,
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(priority, spdy_version_),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,
    false,                  // Compressed
    RST_STREAM_INVALID,
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE
  };
  return ConstructSpdyFrame(
      kSynStartHeader, ConstructPostHeaderBlock(url, content_length));
}

SpdyFrame* SpdyTestUtil::ConstructChunkedSpdyPost(
    const char* const extra_headers[],
    int extra_header_count) {
  const char* post_headers[] = {
    GetMethodKey(), "POST",
    GetPathKey(), "/",
    GetHostKey(), "www.google.com",
    GetSchemeKey(), "http",
    GetVersionKey(), "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   post_headers,
                                   arraysize(post_headers),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPostSynReply(
    const char* const extra_headers[],
    int extra_header_count) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    GetStatusKey(), "200",
    GetPathKey(), "/index.php",
    GetVersionKey(), "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyBodyFrame(int stream_id, bool fin) {
  SpdyFramer framer(spdy_version_);
  return framer.CreateDataFrame(
      stream_id, kUploadData, kUploadDataSize,
      fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyBodyFrame(int stream_id,
                                                const char* data,
                                                uint32 len,
                                                bool fin) {
  SpdyFramer framer(spdy_version_);
  return framer.CreateDataFrame(
      stream_id, data, len, fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

SpdyFrame* SpdyTestUtil::ConstructWrappedSpdyFrame(
    const scoped_ptr<SpdyFrame>& frame,
    int stream_id) {
  return ConstructSpdyBodyFrame(stream_id, frame->data(),
                                frame->size(), false);
}

const SpdyHeaderInfo SpdyTestUtil::MakeSpdyHeader(SpdyFrameType type) {
  const SpdyHeaderInfo kHeader = {
    type,
    1,                            // Stream ID
    0,                            // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(LOWEST, spdy_version_),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_FIN,             // Control Flags
    false,                        // Compressed
    RST_STREAM_INVALID,
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE
  };
  return kHeader;
}

scoped_ptr<SpdyFramer> SpdyTestUtil::CreateFramer() const {
  return scoped_ptr<SpdyFramer>(new SpdyFramer(spdy_version_));
}

const char* SpdyTestUtil::GetMethodKey() const {
  return is_spdy2() ? "method" : ":method";
}

const char* SpdyTestUtil::GetStatusKey() const {
  return is_spdy2() ? "status" : ":status";
}

const char* SpdyTestUtil::GetHostKey() const {
  return is_spdy2() ? "host" : ":host";
}

const char* SpdyTestUtil::GetSchemeKey() const {
  return is_spdy2() ? "scheme" : ":scheme";
}

const char* SpdyTestUtil::GetVersionKey() const {
  return is_spdy2() ? "version" : ":version";
}

const char* SpdyTestUtil::GetPathKey() const {
  return is_spdy2() ? "url" : ":path";
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructHeaderBlock(
    base::StringPiece method,
    base::StringPiece url,
    int64* content_length) const {
  std::string scheme, host, path;
  ParseUrl(url.data(), &scheme, &host, &path);
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  (*headers)[GetMethodKey()] = method.as_string();
  (*headers)[GetPathKey()] = path.c_str();
  (*headers)[GetHostKey()] = host.c_str();
  (*headers)[GetSchemeKey()] = scheme.c_str();
  (*headers)[GetVersionKey()] = "HTTP/1.1";
  if (content_length) {
    std::string length_str = base::Int64ToString(*content_length);
    (*headers)["content-length"] = length_str;
  }
  return headers.Pass();
}

}  // namespace net
