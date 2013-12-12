// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_handler.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* const kCookieHeaders[] = {
  "cookie", "cookie2"
};

const char* const kSetCookieHeaders[] = {
  "set-cookie", "set-cookie2"
};

// A test fixture to simplify tests for GetKeyNumber().
class WebSocketHandshakeGetKeyNumberTest : public ::testing::Test {
 protected:
  static const char kExampleFromDraftKey1[];

  // The object is default-initialised with an empty challenge and the example
  // key from draft-ietf-hybi-thewebsocketprotocol-00. These can be changed
  // using set_challenge() and set_key().
  WebSocketHandshakeGetKeyNumberTest()
      : challenge_(), key_(kExampleFromDraftKey1) {}

  // A convenience wrapper for the function under test which automatically
  // passes in the arguments stored in the object.
  void GetKeyNumber() { ::net::internal::GetKeyNumber(key_, &challenge_); }

  // Read current challenge.
  const std::string& challenge() const { return challenge_; }

  // Overwrite challenge.
  void set_challenge(const std::string& challenge) { challenge_ = challenge; }

  // Reset the challenge to be empty.
  void reset_challenge() { challenge_.clear(); }

  // Change key.
  void set_key(const std::string& key) { key_ = key; }

 private:
  std::string challenge_;
  std::string key_;
};

const char WebSocketHandshakeGetKeyNumberTest::kExampleFromDraftKey1[] =
    "3e6b263  4 17 80";

// A version of the above fixture for death tests.
class WebSocketHandshakeGetKeyNumberDeathTest
    : public WebSocketHandshakeGetKeyNumberTest {
};

}  // namespace

namespace net {

TEST(WebSocketHandshakeRequestHandlerTest, SimpleRequest) {
  WebSocketHandshakeRequestHandler handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(0, handler.protocol_version());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeRequestMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeRequestHandlerTest, SimpleRequestHybi06Handshake) {
  WebSocketHandshakeRequestHandler handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Version: 6\r\n"
      "\r\n";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(6, handler.protocol_version());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeRequestMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeRequestHandlerTest, ReplaceRequestCookies) {
  WebSocketHandshakeRequestHandler handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-websocket-test=1\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(0, handler.protocol_version());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  handler.AppendHeaderIfMissing("Cookie",
                                "WK-websocket-test=1; "
                                "WK-websocket-test-httponly=1");

  static const char kHandshakeRequestExpectedMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-websocket-test=1; WK-websocket-test-httponly=1\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_EQ(kHandshakeRequestExpectedMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeRequestHandlerTest,
     ReplaceRequestCookiesHybi06Handshake) {
  WebSocketHandshakeRequestHandler handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Version: 6\r\n"
      "Cookie: WK-websocket-test=1\r\n"
      "\r\n";

  EXPECT_TRUE(handler.ParseRequest(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(6, handler.protocol_version());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  handler.AppendHeaderIfMissing("Cookie",
                                "WK-websocket-test=1; "
                                "WK-websocket-test-httponly=1");

  static const char kHandshakeRequestExpectedMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Version: 6\r\n"
      "Cookie: WK-websocket-test=1; WK-websocket-test-httponly=1\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeRequestExpectedMessage, handler.GetRawRequest());
}

TEST(WebSocketHandshakeResponseHandlerTest, SimpleResponse) {
  WebSocketHandshakeResponseHandler handler;
  EXPECT_EQ(0, handler.protocol_version());

  static const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(strlen(kHandshakeResponseMessage),
            handler.ParseRawResponse(kHandshakeResponseMessage,
                                     strlen(kHandshakeResponseMessage)));
  EXPECT_TRUE(handler.HasResponse());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeResponseMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest, SimpleResponseHybi06Handshake) {
  WebSocketHandshakeResponseHandler handler;
  handler.set_protocol_version(6);
  EXPECT_EQ(6, handler.protocol_version());

  static const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n";

  EXPECT_EQ(strlen(kHandshakeResponseMessage),
            handler.ParseRawResponse(kHandshakeResponseMessage,
                                     strlen(kHandshakeResponseMessage)));
  EXPECT_TRUE(handler.HasResponse());

  handler.RemoveHeaders(kCookieHeaders, arraysize(kCookieHeaders));

  EXPECT_EQ(kHandshakeResponseMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest, ReplaceResponseCookies) {
  WebSocketHandshakeResponseHandler handler;
  EXPECT_EQ(0, handler.protocol_version());

  static const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Set-Cookie: WK-websocket-test-1\r\n"
      "Set-Cookie: WK-websocket-test-httponly=1; HttpOnly\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(strlen(kHandshakeResponseMessage),
            handler.ParseRawResponse(kHandshakeResponseMessage,
                                     strlen(kHandshakeResponseMessage)));
  EXPECT_TRUE(handler.HasResponse());
  std::vector<std::string> cookies;
  handler.GetHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders), &cookies);
  ASSERT_EQ(2U, cookies.size());
  EXPECT_EQ("WK-websocket-test-1", cookies[0]);
  EXPECT_EQ("WK-websocket-test-httponly=1; HttpOnly", cookies[1]);
  handler.RemoveHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders));

  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest,
     ReplaceResponseCookiesHybi06Handshake) {
  WebSocketHandshakeResponseHandler handler;
  handler.set_protocol_version(6);
  EXPECT_EQ(6, handler.protocol_version());

  static const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Set-Cookie: WK-websocket-test-1\r\n"
      "Set-Cookie: WK-websocket-test-httponly=1; HttpOnly\r\n"
      "\r\n";

  EXPECT_EQ(strlen(kHandshakeResponseMessage),
            handler.ParseRawResponse(kHandshakeResponseMessage,
                                     strlen(kHandshakeResponseMessage)));
  EXPECT_TRUE(handler.HasResponse());
  std::vector<std::string> cookies;
  handler.GetHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders), &cookies);
  ASSERT_EQ(2U, cookies.size());
  EXPECT_EQ("WK-websocket-test-1", cookies[0]);
  EXPECT_EQ("WK-websocket-test-httponly=1; HttpOnly", cookies[1]);
  handler.RemoveHeaders(kSetCookieHeaders, arraysize(kSetCookieHeaders));

  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest, BadResponse) {
  WebSocketHandshakeResponseHandler handler;

  static const char kBadMessage[] = "\n\n\r\net-Location: w";
  EXPECT_EQ(strlen(kBadMessage),
            handler.ParseRawResponse(kBadMessage, strlen(kBadMessage)));
  EXPECT_TRUE(handler.HasResponse());
  EXPECT_EQ(kBadMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeResponseHandlerTest, BadResponse2) {
  WebSocketHandshakeResponseHandler handler;

  static const char kBadMessage[] = "\n\r\n\r\net-Location: w";
  EXPECT_EQ(strlen(kBadMessage),
            handler.ParseRawResponse(kBadMessage, strlen(kBadMessage)));
  EXPECT_TRUE(handler.HasResponse());
  EXPECT_EQ(kBadMessage, handler.GetResponse());
}

TEST(WebSocketHandshakeHandlerTest, HttpRequestResponse) {
  WebSocketHandshakeRequestHandler request_handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(0, request_handler.protocol_version());

  GURL url("ws://example.com/demo");
  std::string challenge;
  const HttpRequestInfo& request_info =
      request_handler.GetRequestInfo(url, &challenge);

  EXPECT_EQ(url, request_info.url);
  EXPECT_EQ("GET", request_info.method);
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Upgrade"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Connection"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Sec-WebSocket-Key1"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Sec-WebSocket-Key2"));
  std::string value;
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Host", &value));
  EXPECT_EQ("example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Origin", &value));
  EXPECT_EQ("http://example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Sec-WebSocket-Protocol",
                                                   &value));
  EXPECT_EQ("sample", value);

  const char expected_challenge[] = "\x31\x6e\x41\x13\x0f\x7e\xd6\x3c^n:ds[4U";

  EXPECT_EQ(expected_challenge, challenge);

  static const char kHandshakeResponseHeader[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n";

  std::string raw_headers =
      HttpUtil::AssembleRawHeaders(kHandshakeResponseHeader,
                                   strlen(kHandshakeResponseHeader));
  HttpResponseInfo response_info;
  response_info.headers = new HttpResponseHeaders(raw_headers);

  EXPECT_TRUE(StartsWithASCII(response_info.headers->GetStatusLine(),
                              "HTTP/1.1 101 ", false));
  EXPECT_FALSE(response_info.headers->HasHeader("Upgrade"));
  EXPECT_FALSE(response_info.headers->HasHeader("Connection"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Origin",
                                                    "http://example.com"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Location",
                                                    "ws://example.com/demo"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Protocol",
                                                    "sample"));

  WebSocketHandshakeResponseHandler response_handler;
  EXPECT_EQ(0, response_handler.protocol_version());
  EXPECT_TRUE(response_handler.ParseResponseInfo(response_info, challenge));
  EXPECT_TRUE(response_handler.HasResponse());

  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

TEST(WebSocketHandshakeHandlerTest, HttpRequestResponseHybi06Handshake) {
  WebSocketHandshakeRequestHandler request_handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Version: 6\r\n"
      "\r\n";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(6, request_handler.protocol_version());

  GURL url("ws://example.com/demo");
  std::string challenge;
  const HttpRequestInfo& request_info =
      request_handler.GetRequestInfo(url, &challenge);

  EXPECT_EQ(url, request_info.url);
  EXPECT_EQ("GET", request_info.method);
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Upgrade"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Connection"));
  EXPECT_FALSE(request_info.extra_headers.HasHeader("Sec-WebSocket-Key"));
  std::string value;
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Host", &value));
  EXPECT_EQ("example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Sec-WebSocket-Origin",
                                                   &value));
  EXPECT_EQ("http://example.com", value);
  EXPECT_TRUE(request_info.extra_headers.GetHeader("Sec-WebSocket-Protocol",
                                                   &value));
  EXPECT_EQ("sample", value);

  EXPECT_EQ("dGhlIHNhbXBsZSBub25jZQ==", challenge);

  static const char kHandshakeResponseHeader[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Sec-WebSocket-Protocol: sample\r\n";

  std::string raw_headers =
      HttpUtil::AssembleRawHeaders(kHandshakeResponseHeader,
                                   strlen(kHandshakeResponseHeader));
  HttpResponseInfo response_info;
  response_info.headers = new HttpResponseHeaders(raw_headers);

  EXPECT_TRUE(StartsWithASCII(response_info.headers->GetStatusLine(),
                              "HTTP/1.1 101 ", false));
  EXPECT_FALSE(response_info.headers->HasHeader("Upgrade"));
  EXPECT_FALSE(response_info.headers->HasHeader("Connection"));
  EXPECT_FALSE(response_info.headers->HasHeader("Sec-WebSocket-Accept"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Sec-WebSocket-Protocol",
                                                    "sample"));

  WebSocketHandshakeResponseHandler response_handler;
  response_handler.set_protocol_version(request_handler.protocol_version());
  EXPECT_EQ(6, response_handler.protocol_version());

  EXPECT_TRUE(response_handler.ParseResponseInfo(response_info, challenge));
  EXPECT_TRUE(response_handler.HasResponse());

  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, AppendsToString) {
  set_challenge("hello");
  GetKeyNumber();
  EXPECT_EQ("hello", challenge().substr(0, 5));
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, AppendsFourBytes) {
  set_challenge("hello");
  set_key("1 1");
  GetKeyNumber();
  EXPECT_EQ(9u, challenge().length());
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, IsBigEndian) {
  set_key(base::StringPrintf("%u ", 0x61626364));
  GetKeyNumber();
  EXPECT_EQ("abcd", challenge());
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, IgnoresLetters) {
  set_key("1b 1");
  GetKeyNumber();
  char expected_response[] = { 0, 0, 0, 11 };
  EXPECT_EQ(std::string(expected_response, 4), challenge());
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, DividesBySpaces) {
  set_key("1 2");
  GetKeyNumber();
  EXPECT_EQ(12, challenge()[3]);
  reset_challenge();
  set_key("1  2");
  GetKeyNumber();
  EXPECT_EQ(6, challenge()[3]);
  reset_challenge();
  set_key(" 1  2");
  GetKeyNumber();
  EXPECT_EQ(4, challenge()[3]);
  reset_challenge();
  set_key(" 1  2 ");
  GetKeyNumber();
  EXPECT_EQ(3, challenge()[3]);
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, MatchesExampleFromDraft) {
  set_key("3e6b263  4 17 80");
  GetKeyNumber();
  char expected_response[] = { 0x36, 0x09, 0x65, 0x65 };
  EXPECT_EQ(std::string(expected_response, 4), challenge());
}

TEST_F(WebSocketHandshakeGetKeyNumberTest, Maximum32bitInteger) {
  set_key("4294967295 ");
  GetKeyNumber();
  char expected_response[] = { '\xFF', '\xFF', '\xFF', '\xFF' };
  EXPECT_EQ(std::string(expected_response, 4), challenge());
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST_F(WebSocketHandshakeGetKeyNumberDeathTest, ThirtyThreeBitIntegerNoGood) {
  set_key(" 4294967296");
  EXPECT_DEBUG_DEATH(GetKeyNumber(), "overflow");
}

TEST_F(WebSocketHandshakeGetKeyNumberDeathTest, NoSpacesNoGood) {
  set_key("11");
  EXPECT_DEBUG_DEATH(GetKeyNumber(), "space");
}

TEST_F(WebSocketHandshakeGetKeyNumberDeathTest, MustBeIntegralMultiple) {
  set_key("1  1");
  EXPECT_DEBUG_DEATH(GetKeyNumber(), "spaces");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

}  // namespace net
