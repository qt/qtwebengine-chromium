// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_unittest.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace {

// This is the expected return from a current server advertising QUIC.
static const char kQuicAlternateProtocolHttpHeader[] =
    "Alternate-Protocol: 80:quic\r\n\r\n";
static const char kQuicAlternateProtocolHttpsHeader[] =
    "Alternate-Protocol: 443:quic\r\n\r\n";
}  // namespace

namespace net {
namespace test {

class QuicNetworkTransactionTest : public PlatformTest {
 protected:
  QuicNetworkTransactionTest()
      : clock_(new MockClock),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        compressor_(new QuicSpdyCompressor()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        random_generator_(0),
        hanging_data_(NULL, 0, NULL, 0) {
    request_.method = "GET";
    request_.url = GURL("http://www.google.com/");
    request_.load_flags = 0;
  }

  virtual void SetUp() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void TearDown() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
    HttpStreamFactory::set_use_alternate_protocols(false);
    HttpStreamFactory::SetNextProtos(std::vector<NextProto>());
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket(
      QuicPacketSequenceNumber num,
      QuicStreamId stream_id) {
    QuicPacketHeader header;
    header.public_header.guid = random_generator_.RandUint64();
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header.packet_sequence_number = num;
    header.entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicRstStreamFrame rst(stream_id, QUIC_STREAM_NO_ERROR);
    return scoped_ptr<QuicEncryptedPacket>(
        ConstructPacket(header, QuicFrame(&rst)));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructConnectionClosePacket(
      QuicPacketSequenceNumber num) {
    QuicPacketHeader header;
    header.public_header.guid = random_generator_.RandUint64();
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header.packet_sequence_number = num;
    header.entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicConnectionCloseFrame close;
    close.error_code = QUIC_CRYPTO_VERSION_NOT_SUPPORTED;
    close.error_details = "Time to panic!";
    return scoped_ptr<QuicEncryptedPacket>(
        ConstructPacket(header, QuicFrame(&close)));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    QuicPacketHeader header;
    header.public_header.guid = random_generator_.RandUint64();
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header.packet_sequence_number = 2;
    header.entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicAckFrame ack(largest_received, QuicTime::Zero(), least_unacked);

    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 256000;

    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(), false);
    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer.BuildUnsizedDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
        ENCRYPTION_NONE, header.packet_sequence_number, *packet));
  }

  std::string GetRequestString(const std::string& method,
                               const std::string& scheme,
                               const std::string& path) {
    SpdyHeaderBlock headers;
    headers[":method"] = method;
    headers[":host"] = "www.google.com";
    headers[":path"] = path;
    headers[":scheme"] = scheme;
    headers[":version"] = "HTTP/1.1";
    return SerializeHeaderBlock(headers);
  }

  std::string GetResponseString(const std::string& status,
                                const std::string& body) {
    SpdyHeaderBlock headers;
    headers[":status"] = status;
    headers[":version"] = "HTTP/1.1";
    headers["content-type"] = "text/plain";
    return compressor_->CompressHeaders(headers) + body;
  }

  std::string SerializeHeaderBlock(const SpdyHeaderBlock& headers) {
    QuicSpdyCompressor compressor;
    return compressor.CompressHeadersWithPriority(
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY), headers);
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    InitializeHeader(sequence_number, should_include_version);
    QuicStreamFrame frame(stream_id, fin, offset, MakeIOVector(data));
    return ConstructPacket(header_, QuicFrame(&frame)).release();
  }

  scoped_ptr<QuicEncryptedPacket> ConstructPacket(
      const QuicPacketHeader& header,
      const QuicFrame& frame) {
    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(), false);
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer.BuildUnsizedDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
        ENCRYPTION_NONE, header.packet_sequence_number, *packet));
  }

  void InitializeHeader(QuicPacketSequenceNumber sequence_number,
                        bool should_include_version) {
    header_.public_header.guid = random_generator_.RandUint64();
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = should_include_version;
    header_.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header_.packet_sequence_number = sequence_number;
    header_.fec_group = 0;
    header_.entropy_flag = false;
    header_.fec_flag = false;
  }

  void CreateSession() {
    CreateSessionWithFactory(&socket_factory_);
  }

  void CreateSessionWithFactory(ClientSocketFactory* socket_factory) {
    params_.enable_quic = true;
    params_.quic_clock = clock_;
    params_.quic_random = &random_generator_;
    params_.client_socket_factory = socket_factory;
    params_.quic_crypto_client_stream_factory = &crypto_client_stream_factory_;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.transport_security_state = &transport_security_state_;
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = http_server_properties.GetWeakPtr();

    session_ = new HttpNetworkSession(params_);
    session_->quic_stream_factory()->set_require_confirmation(false);
  }

  void CheckWasQuicResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3,
              response->connection_info);
  }

  void CheckWasHttpResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1,
              response->connection_info);
  }

  void CheckResponseData(HttpNetworkTransaction* trans,
                         const std::string& expected) {
    std::string response_data;
    ASSERT_EQ(OK, ReadTransaction(trans, &response_data));
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(HttpNetworkTransaction* trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, callback.WaitForResult());
  }

  void SendRequestAndExpectHttpResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans.get());
    CheckWasHttpResponse(trans);
    CheckResponseData(trans.get(), expected);
  }

  void SendRequestAndExpectQuicResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans.get());
    CheckWasQuicResponse(trans);
    CheckResponseData(trans.get(), expected);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    session_->http_server_properties()->SetAlternateProtocol(
        HostPortPair::FromURL(request_.url), 80, QUIC);
  }

  void ExpectBrokenAlternateProtocolMapping() {
    ASSERT_TRUE(session_->http_server_properties()->HasAlternateProtocol(
        HostPortPair::FromURL(request_.url)));
    const PortAlternateProtocolPair alternate =
        session_->http_server_properties()->GetAlternateProtocol(
            HostPortPair::FromURL(request_.url));
    EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);
  }

  void AddHangingNonAlternateProtocolSocketData() {
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data_.set_connect_data(hanging_connect);
    socket_factory_.AddSocketDataProvider(&hanging_data_);
  }

  QuicPacketHeader header_;
  scoped_refptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockClock* clock_;  // Owned by QuicStreamFactory after CreateSession.
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<QuicSpdyCompressor> compressor_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  MockRandom random_generator_;
  HttpServerPropertiesImpl http_server_properties;
  HttpNetworkSession::Params params_;
  HttpRequestInfo request_;
  CapturingBoundNetLog net_log_;
  StaticSocketDataProvider hanging_data_;
};

TEST_F(QuicNetworkTransactionTest, ForceQuic) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  QuicStreamId stream_id = 3;
  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, stream_id, true, true, 0,
                          GetRequestString("GET", "http", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, stream_id, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED.
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  std::string packet_sequence_number;
  ASSERT_TRUE(entries[pos].GetStringValue(
      "packet_sequence_number", &packet_sequence_number));
  EXPECT_EQ("1", packet_sequence_number);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  int log_stream_id;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &log_stream_id));
  EXPECT_EQ(stream_id, static_cast<QuicStreamId>(log_stream_id));
}

TEST_F(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  CreateSession();

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
}

TEST_F(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider data(http_reads, arraysize(http_reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
}

TEST_F(QuicNetworkTransactionTest, UseAlternateProtocolForQuic) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, 3, true, true, 0,
                          GetRequestString("GET", "http", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, 3, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_F(QuicNetworkTransactionTest, UseAlternateProtocolForQuicForHttps) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");
  params_.enable_quic_https = true;
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpsHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, 3, true, true, 0,
                          GetRequestString("GET", "https", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, 3, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_F(QuicNetworkTransactionTest, HungAlternateProtocol) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
    MockWrite(SYNCHRONOUS, 1, "Host: www.google.com\r\n"),
    MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")
  };

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHttpHeader),
    MockRead(SYNCHRONOUS, 5, "hello world"),
    MockRead(SYNCHRONOUS, OK, 6)
  };

  DeterministicMockClientSocketFactory socket_factory;

  DeterministicSocketData http_data(http_reads, arraysize(http_reads),
                                    http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {
    MockWrite(ASYNC, ERR_IO_PENDING, 0)
  };
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_IO_PENDING, 1),
  };
  DeterministicSocketData quic_data(quic_reads, arraysize(quic_reads),
                                    quic_writes, arraysize(quic_writes));
  socket_factory.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  DeterministicSocketData http_data2(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data2);

  CreateSessionWithFactory(&socket_factory);

  // Run the first request.
  http_data.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");
  ASSERT_TRUE(http_data.at_read_eof());
  ASSERT_TRUE(http_data.at_write_eof());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  http_data2.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");

  ASSERT_TRUE(http_data2.at_read_eof());
  ASSERT_TRUE(http_data2.at_write_eof());
  ASSERT_TRUE(!quic_data.at_read_eof());
  ASSERT_TRUE(!quic_data.at_write_eof());
}

TEST_F(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, 3, true, true, 0,
                          GetRequestString("GET", "http", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, 3, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_F(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, 3, true, true, 0,
                          GetRequestString("GET", "http", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, 3, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         NULL,
                         net_log_.bound());

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_F(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  scoped_ptr<QuicEncryptedPacket> req(
      ConstructDataPacket(1, 3, true, true, 0,
                          GetRequestString("GET", "http", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, req->data(), req->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, 3, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address,
                         CompletionCallback(), NULL, net_log_.bound());

  CreateSession();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_F(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  // Alternate-protocol job
  scoped_ptr<QuicEncryptedPacket> close(ConstructConnectionClosePacket(1));
  MockRead quic_reads[] = {
    MockRead(ASYNC, close->data(), close->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_F(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_F(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  HttpStreamFactory::EnableNpnSpdy3();  // Enables QUIC too.

  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  AddHangingNonAlternateProtocolSocketData();

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.at_read_eof());
  EXPECT_TRUE(quic_data.at_write_eof());
}

}  // namespace test
}  // namespace net
