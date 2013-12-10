// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_handler.h"

#include <limits>

#include "base/base64.h"
#include "base/md5.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "url/gurl.h"

namespace net {
namespace {

const size_t kRequestKey3Size = 8U;
const size_t kResponseKeySize = 16U;

// First version that introduced new WebSocket handshake which does not
// require sending "key3" or "response key" data after headers.
const int kMinVersionOfHybiNewHandshake = 4;

void ParseHandshakeHeader(
    const char* handshake_message, int len,
    std::string* status_line,
    std::string* headers) {
  size_t i = base::StringPiece(handshake_message, len).find_first_of("\r\n");
  if (i == base::StringPiece::npos) {
    *status_line = std::string(handshake_message, len);
    *headers = "";
    return;
  }
  // |status_line| includes \r\n.
  *status_line = std::string(handshake_message, i + 2);

  int header_len = len - (i + 2) - 2;
  if (header_len > 0) {
    // |handshake_message| includes trailing \r\n\r\n.
    // |headers| doesn't include 2nd \r\n.
    *headers = std::string(handshake_message + i + 2, header_len);
  } else {
    *headers = "";
  }
}

void FetchHeaders(const std::string& headers,
                  const char* const headers_to_get[],
                  size_t headers_to_get_len,
                  std::vector<std::string>* values) {
  net::HttpUtil::HeadersIterator iter(headers.begin(), headers.end(), "\r\n");
  while (iter.GetNext()) {
    for (size_t i = 0; i < headers_to_get_len; i++) {
      if (LowerCaseEqualsASCII(iter.name_begin(), iter.name_end(),
                               headers_to_get[i])) {
        values->push_back(iter.values());
      }
    }
  }
}

bool GetHeaderName(std::string::const_iterator line_begin,
                   std::string::const_iterator line_end,
                   std::string::const_iterator* name_begin,
                   std::string::const_iterator* name_end) {
  std::string::const_iterator colon = std::find(line_begin, line_end, ':');
  if (colon == line_end) {
    return false;
  }
  *name_begin = line_begin;
  *name_end = colon;
  if (*name_begin == *name_end || net::HttpUtil::IsLWS(**name_begin))
    return false;
  net::HttpUtil::TrimLWS(name_begin, name_end);
  return true;
}

// Similar to HttpUtil::StripHeaders, but it preserves malformed headers, that
// is, lines that are not formatted as "<name>: <value>\r\n".
std::string FilterHeaders(
    const std::string& headers,
    const char* const headers_to_remove[],
    size_t headers_to_remove_len) {
  std::string filtered_headers;

  base::StringTokenizer lines(headers.begin(), headers.end(), "\r\n");
  while (lines.GetNext()) {
    std::string::const_iterator line_begin = lines.token_begin();
    std::string::const_iterator line_end = lines.token_end();
    std::string::const_iterator name_begin;
    std::string::const_iterator name_end;
    bool should_remove = false;
    if (GetHeaderName(line_begin, line_end, &name_begin, &name_end)) {
      for (size_t i = 0; i < headers_to_remove_len; ++i) {
        if (LowerCaseEqualsASCII(name_begin, name_end, headers_to_remove[i])) {
          should_remove = true;
          break;
        }
      }
    }
    if (!should_remove) {
      filtered_headers.append(line_begin, line_end);
      filtered_headers.append("\r\n");
    }
  }
  return filtered_headers;
}

int GetVersionFromRequest(const std::string& request_headers) {
  std::vector<std::string> values;
  const char* const headers_to_get[2] = {
      websockets::kSecWebSocketVersionLowercase, "sec-websocket-draft"};
  FetchHeaders(request_headers, headers_to_get, 2, &values);
  DCHECK_LE(values.size(), 1U);
  if (values.empty())
    return 0;
  int version;
  bool conversion_success = base::StringToInt(values[0], &version);
  DCHECK(conversion_success);
  DCHECK_GE(version, 1);
  return version;
}

// Append a header to a string. Equivalent to
//   response_message += header + ": " + value + "\r\n"
// but avoids unnecessary allocations and copies.
void AppendHeader(const base::StringPiece& header,
                  const base::StringPiece& value,
                  std::string* response_message) {
  static const char kColonSpace[] = ": ";
  const size_t kColonSpaceSize = sizeof(kColonSpace) - 1;
  static const char kCrNl[] = "\r\n";
  const size_t kCrNlSize = sizeof(kCrNl) - 1;

  size_t extra_size =
      header.size() + kColonSpaceSize + value.size() + kCrNlSize;
  response_message->reserve(response_message->size() + extra_size);
  response_message->append(header.begin(), header.end());
  response_message->append(kColonSpace, kColonSpace + kColonSpaceSize);
  response_message->append(value.begin(), value.end());
  response_message->append(kCrNl, kCrNl + kCrNlSize);
}

}  // namespace

namespace internal {

void GetKeyNumber(const std::string& key, std::string* challenge) {
  uint32 key_number = 0;
  uint32 spaces = 0;
  for (size_t i = 0; i < key.size(); ++i) {
    if (isdigit(key[i])) {
      // key_number should not overflow. (it comes from
      // WebCore/websockets/WebSocketHandshake.cpp).
      // Trust, but verify.
      DCHECK_GE((std::numeric_limits<uint32>::max() - (key[i] - '0')) / 10,
                key_number) << "Supplied key would overflow";
      key_number = key_number * 10 + key[i] - '0';
    } else if (key[i] == ' ') {
      ++spaces;
    }
  }
  DCHECK_NE(0u, spaces) << "Key must contain at least one space";
  if (spaces == 0)
    return;
  DCHECK_EQ(0u, key_number % spaces) << "Key number must be an integral "
                                     << "multiple of the number of spaces";
  key_number /= spaces;

  char part[4];
  for (int i = 0; i < 4; i++) {
    part[3 - i] = key_number & 0xFF;
    key_number >>= 8;
  }
  challenge->append(part, 4);
}

}  // namespace internal

WebSocketHandshakeRequestHandler::WebSocketHandshakeRequestHandler()
    : original_length_(0),
      raw_length_(0),
      protocol_version_(-1) {}

bool WebSocketHandshakeRequestHandler::ParseRequest(
    const char* data, int length) {
  DCHECK_GT(length, 0);
  std::string input(data, length);
  int input_header_length =
      HttpUtil::LocateEndOfHeaders(input.data(), input.size(), 0);
  if (input_header_length <= 0)
    return false;

  ParseHandshakeHeader(input.data(),
                       input_header_length,
                       &status_line_,
                       &headers_);

  // WebSocket protocol drafts hixie-76 (hybi-00), hybi-01, 02 and 03 require
  // the clients to send key3 after the handshake request header fields.
  // Hybi-04 and later drafts, on the other hand, no longer have key3
  // in the handshake format.
  protocol_version_ = GetVersionFromRequest(headers_);
  DCHECK_GE(protocol_version_, 0);
  if (protocol_version_ >= kMinVersionOfHybiNewHandshake) {
    key3_ = "";
    original_length_ = input_header_length;
    return true;
  }

  if (input_header_length + kRequestKey3Size > input.size())
    return false;

  // Assumes WebKit doesn't send any data after handshake request message
  // until handshake is finished.
  // Thus, |key3_| is part of handshake message, and not in part
  // of WebSocket frame stream.
  DCHECK_EQ(kRequestKey3Size, input.size() - input_header_length);
  key3_ = std::string(input.data() + input_header_length,
                      input.size() - input_header_length);
  original_length_ = input.size();
  return true;
}

size_t WebSocketHandshakeRequestHandler::original_length() const {
  return original_length_;
}

void WebSocketHandshakeRequestHandler::AppendHeaderIfMissing(
    const std::string& name, const std::string& value) {
  DCHECK(!headers_.empty());
  HttpUtil::AppendHeaderIfMissing(name.c_str(), value, &headers_);
}

void WebSocketHandshakeRequestHandler::RemoveHeaders(
    const char* const headers_to_remove[],
    size_t headers_to_remove_len) {
  DCHECK(!headers_.empty());
  headers_ = FilterHeaders(
      headers_, headers_to_remove, headers_to_remove_len);
}

HttpRequestInfo WebSocketHandshakeRequestHandler::GetRequestInfo(
    const GURL& url, std::string* challenge) {
  HttpRequestInfo request_info;
  request_info.url = url;
  size_t method_end = base::StringPiece(status_line_).find_first_of(" ");
  if (method_end != base::StringPiece::npos)
    request_info.method = std::string(status_line_.data(), method_end);

  request_info.extra_headers.Clear();
  request_info.extra_headers.AddHeadersFromString(headers_);

  request_info.extra_headers.RemoveHeader(websockets::kUpgrade);
  request_info.extra_headers.RemoveHeader(HttpRequestHeaders::kConnection);

  if (protocol_version_ >= kMinVersionOfHybiNewHandshake) {
    std::string key;
    bool header_present = request_info.extra_headers.GetHeader(
        websockets::kSecWebSocketKey, &key);
    DCHECK(header_present);
    request_info.extra_headers.RemoveHeader(websockets::kSecWebSocketKey);
    *challenge = key;
  } else {
    challenge->clear();
    std::string key;
    bool header_present =
        request_info.extra_headers.GetHeader("Sec-WebSocket-Key1", &key);
    DCHECK(header_present);
    request_info.extra_headers.RemoveHeader("Sec-WebSocket-Key1");
    internal::GetKeyNumber(key, challenge);

    header_present =
        request_info.extra_headers.GetHeader("Sec-WebSocket-Key2", &key);
    DCHECK(header_present);
    request_info.extra_headers.RemoveHeader("Sec-WebSocket-Key2");
    internal::GetKeyNumber(key, challenge);

    challenge->append(key3_);
  }

  return request_info;
}

bool WebSocketHandshakeRequestHandler::GetRequestHeaderBlock(
    const GURL& url,
    SpdyHeaderBlock* headers,
    std::string* challenge,
    int spdy_protocol_version) {
  // Construct opening handshake request headers as a SPDY header block.
  // For details, see WebSocket Layering over SPDY/3 Draft 8.
  if (spdy_protocol_version <= 2) {
    (*headers)["path"] = url.path();
    (*headers)["version"] =
      base::StringPrintf("%s%d", "WebSocket/", protocol_version_);
    (*headers)["scheme"] = url.scheme();
  } else {
    (*headers)[":path"] = url.path();
    (*headers)[":version"] =
      base::StringPrintf("%s%d", "WebSocket/", protocol_version_);
    (*headers)[":scheme"] = url.scheme();
  }

  HttpUtil::HeadersIterator iter(headers_.begin(), headers_.end(), "\r\n");
  while (iter.GetNext()) {
    if (LowerCaseEqualsASCII(iter.name_begin(),
                             iter.name_end(),
                             websockets::kUpgradeLowercase) ||
        LowerCaseEqualsASCII(
            iter.name_begin(), iter.name_end(), "connection") ||
        LowerCaseEqualsASCII(iter.name_begin(),
                             iter.name_end(),
                             websockets::kSecWebSocketVersionLowercase)) {
      // These headers must be ignored.
      continue;
    } else if (LowerCaseEqualsASCII(iter.name_begin(),
                                    iter.name_end(),
                                    websockets::kSecWebSocketKeyLowercase)) {
      *challenge = iter.values();
      // Sec-WebSocket-Key is not sent to the server.
      continue;
    } else if (LowerCaseEqualsASCII(
                   iter.name_begin(), iter.name_end(), "host") ||
               LowerCaseEqualsASCII(
                   iter.name_begin(), iter.name_end(), "origin") ||
               LowerCaseEqualsASCII(
                   iter.name_begin(),
                   iter.name_end(),
                   websockets::kSecWebSocketProtocolLowercase) ||
               LowerCaseEqualsASCII(
                   iter.name_begin(),
                   iter.name_end(),
                   websockets::kSecWebSocketExtensionsLowercase)) {
      // TODO(toyoshim): Some WebSocket extensions may not be compatible with
      // SPDY. We should omit them from a Sec-WebSocket-Extension header.
      std::string name;
      if (spdy_protocol_version <= 2)
        name = StringToLowerASCII(iter.name());
      else
        name = ":" + StringToLowerASCII(iter.name());
      (*headers)[name] = iter.values();
      continue;
    }
    // Others should be sent out to |headers|.
    std::string name = StringToLowerASCII(iter.name());
    SpdyHeaderBlock::iterator found = headers->find(name);
    if (found == headers->end()) {
      (*headers)[name] = iter.values();
    } else {
      // For now, websocket doesn't use multiple headers, but follows to http.
      found->second.append(1, '\0');  // +=() doesn't append 0's
      found->second.append(iter.values());
    }
  }

  return true;
}

std::string WebSocketHandshakeRequestHandler::GetRawRequest() {
  DCHECK(!status_line_.empty());
  DCHECK(!headers_.empty());
  // The following works on both hybi-04 and older handshake,
  // because |key3_| is guaranteed to be empty if the handshake was hybi-04's.
  std::string raw_request = status_line_ + headers_ + "\r\n" + key3_;
  raw_length_ = raw_request.size();
  return raw_request;
}

size_t WebSocketHandshakeRequestHandler::raw_length() const {
  DCHECK_GT(raw_length_, 0);
  return raw_length_;
}

int WebSocketHandshakeRequestHandler::protocol_version() const {
  DCHECK_GE(protocol_version_, 0);
  return protocol_version_;
}

WebSocketHandshakeResponseHandler::WebSocketHandshakeResponseHandler()
    : original_header_length_(0),
      protocol_version_(0) {}

WebSocketHandshakeResponseHandler::~WebSocketHandshakeResponseHandler() {}

int WebSocketHandshakeResponseHandler::protocol_version() const {
  DCHECK_GE(protocol_version_, 0);
  return protocol_version_;
}

void WebSocketHandshakeResponseHandler::set_protocol_version(
    int protocol_version) {
  DCHECK_GE(protocol_version, 0);
  protocol_version_ = protocol_version;
}

size_t WebSocketHandshakeResponseHandler::ParseRawResponse(
    const char* data, int length) {
  DCHECK_GT(length, 0);
  if (HasResponse()) {
    DCHECK(!status_line_.empty());
    // headers_ might be empty for wrong response from server.
    return 0;
  }

  size_t old_original_length = original_.size();

  original_.append(data, length);
  // TODO(ukai): fail fast when response gives wrong status code.
  original_header_length_ = HttpUtil::LocateEndOfHeaders(
      original_.data(), original_.size(), 0);
  if (!HasResponse())
    return length;

  ParseHandshakeHeader(original_.data(),
                       original_header_length_,
                       &status_line_,
                       &headers_);
  int header_size = status_line_.size() + headers_.size();
  DCHECK_GE(original_header_length_, header_size);
  header_separator_ = std::string(original_.data() + header_size,
                                  original_header_length_ - header_size);
  key_ = std::string(original_.data() + original_header_length_,
                     GetResponseKeySize());
  return original_header_length_ + GetResponseKeySize() - old_original_length;
}

bool WebSocketHandshakeResponseHandler::HasResponse() const {
  return original_header_length_ > 0 &&
      original_header_length_ + GetResponseKeySize() <= original_.size();
}

bool WebSocketHandshakeResponseHandler::ParseResponseInfo(
    const HttpResponseInfo& response_info,
    const std::string& challenge) {
  if (!response_info.headers.get())
    return false;

  // TODO(ricea): Eliminate all the reallocations and string copies.
  std::string response_message;
  response_message = response_info.headers->GetStatusLine();
  response_message += "\r\n";
  if (protocol_version_ >= kMinVersionOfHybiNewHandshake)
    AppendHeader(websockets::kUpgrade,
                 websockets::kWebSocketLowercase,
                 &response_message);
  else
    AppendHeader(websockets::kUpgrade, "WebSocket", &response_message);
  AppendHeader(
      HttpRequestHeaders::kConnection, websockets::kUpgrade, &response_message);

  if (protocol_version_ >= kMinVersionOfHybiNewHandshake) {
    std::string hash =
        base::SHA1HashString(challenge + websockets::kWebSocketGuid);
    std::string websocket_accept;
    bool encode_success = base::Base64Encode(hash, &websocket_accept);
    DCHECK(encode_success);
    AppendHeader(
        websockets::kSecWebSocketAccept, websocket_accept, &response_message);
  }

  void* iter = NULL;
  std::string name;
  std::string value;
  while (response_info.headers->EnumerateHeaderLines(&iter, &name, &value)) {
    AppendHeader(name, value, &response_message);
  }
  response_message += "\r\n";

  if (protocol_version_ < kMinVersionOfHybiNewHandshake) {
    base::MD5Digest digest;
    base::MD5Sum(challenge.data(), challenge.size(), &digest);

    const char* digest_data = reinterpret_cast<char*>(digest.a);
    response_message.append(digest_data, sizeof(digest.a));
  }

  return ParseRawResponse(response_message.data(),
                          response_message.size()) == response_message.size();
}

bool WebSocketHandshakeResponseHandler::ParseResponseHeaderBlock(
    const SpdyHeaderBlock& headers,
    const std::string& challenge,
    int spdy_protocol_version) {
  SpdyHeaderBlock::const_iterator status;
  if (spdy_protocol_version <= 2)
    status = headers.find("status");
  else
    status = headers.find(":status");
  if (status == headers.end())
    return false;

  std::string hash =
      base::SHA1HashString(challenge + websockets::kWebSocketGuid);
  std::string websocket_accept;
  bool encode_success = base::Base64Encode(hash, &websocket_accept);
  DCHECK(encode_success);

  std::string response_message = base::StringPrintf(
      "%s %s\r\n", websockets::kHttpProtocolVersion, status->second.c_str());

  AppendHeader(
      websockets::kUpgrade, websockets::kWebSocketLowercase, &response_message);
  AppendHeader(
      HttpRequestHeaders::kConnection, websockets::kUpgrade, &response_message);
  AppendHeader(
      websockets::kSecWebSocketAccept, websocket_accept, &response_message);

  for (SpdyHeaderBlock::const_iterator iter = headers.begin();
       iter != headers.end();
       ++iter) {
    // For each value, if the server sends a NUL-separated list of values,
    // we separate that back out into individual headers for each value
    // in the list.
    if ((spdy_protocol_version <= 2 &&
         LowerCaseEqualsASCII(iter->first, "status")) ||
        (spdy_protocol_version >= 3 &&
         LowerCaseEqualsASCII(iter->first, ":status"))) {
      // The status value is already handled as the first line of
      // |response_message|. Just skip here.
      continue;
    }
    const std::string& value = iter->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != std::string::npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      if (spdy_protocol_version >= 3 &&
          (LowerCaseEqualsASCII(iter->first,
                                websockets::kSecWebSocketProtocolSpdy3) ||
           LowerCaseEqualsASCII(iter->first,
                                websockets::kSecWebSocketExtensionsSpdy3)))
        AppendHeader(iter->first.substr(1), tval, &response_message);
      else
        AppendHeader(iter->first, tval, &response_message);
      start = end + 1;
    } while (end != std::string::npos);
  }
  response_message += "\r\n";

  return ParseRawResponse(response_message.data(),
                          response_message.size()) == response_message.size();
}

void WebSocketHandshakeResponseHandler::GetHeaders(
    const char* const headers_to_get[],
    size_t headers_to_get_len,
    std::vector<std::string>* values) {
  DCHECK(HasResponse());
  DCHECK(!status_line_.empty());
  // headers_ might be empty for wrong response from server.
  if (headers_.empty())
    return;

  FetchHeaders(headers_, headers_to_get, headers_to_get_len, values);
}

void WebSocketHandshakeResponseHandler::RemoveHeaders(
    const char* const headers_to_remove[],
    size_t headers_to_remove_len) {
  DCHECK(HasResponse());
  DCHECK(!status_line_.empty());
  // headers_ might be empty for wrong response from server.
  if (headers_.empty())
    return;

  headers_ = FilterHeaders(headers_, headers_to_remove, headers_to_remove_len);
}

std::string WebSocketHandshakeResponseHandler::GetRawResponse() const {
  DCHECK(HasResponse());
  return std::string(original_.data(),
                     original_header_length_ + GetResponseKeySize());
}

std::string WebSocketHandshakeResponseHandler::GetResponse() {
  DCHECK(HasResponse());
  DCHECK(!status_line_.empty());
  // headers_ might be empty for wrong response from server.
  DCHECK_EQ(GetResponseKeySize(), key_.size());

  return status_line_ + headers_ + header_separator_ + key_;
}

size_t WebSocketHandshakeResponseHandler::GetResponseKeySize() const {
  if (protocol_version_ >= kMinVersionOfHybiNewHandshake)
    return 0;
  return kResponseKeySize;
}

}  // namespace net
