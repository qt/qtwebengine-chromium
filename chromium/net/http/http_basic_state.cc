// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_handle.h"
#include "url/gurl.h"

namespace net {

HttpBasicState::HttpBasicState(ClientSocketHandle* connection, bool using_proxy)
    : read_buf_(new GrowableIOBuffer()),
      connection_(connection),
      using_proxy_(using_proxy),
      request_info_(NULL) {}

HttpBasicState::~HttpBasicState() {}

int HttpBasicState::Initialize(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) {
  DCHECK(!parser_.get());
  request_info_ = request_info;
  parser_.reset(new HttpStreamParser(
      connection_.get(), request_info, read_buf_.get(), net_log));
  return OK;
}

scoped_ptr<ClientSocketHandle> HttpBasicState::ReleaseConnection() {
  return connection_.Pass();
}

scoped_refptr<GrowableIOBuffer> HttpBasicState::read_buf() const {
  return read_buf_;
}

void HttpBasicState::DeleteParser() { parser_.reset(); }

std::string HttpBasicState::GenerateRequestLine() const {
  static const char kSuffix[] = " HTTP/1.1\r\n";
  const size_t kSuffixLen = arraysize(kSuffix) - 1;
  DCHECK(request_info_);
  const GURL& url = request_info_->url;
  const std::string path = using_proxy_ ? HttpUtil::SpecForRequest(url)
                                        : HttpUtil::PathForRequest(url);
  // Don't use StringPrintf for concatenation because it is very inefficient.
  std::string request_line;
  const size_t expected_size = request_info_->method.size() + 1 + path.size() +
      kSuffixLen;
  request_line.reserve(expected_size);
  request_line.append(request_info_->method);
  request_line.append(1, ' ');
  request_line.append(path);
  request_line.append(kSuffix, kSuffixLen);
  DCHECK_EQ(expected_size, request_line.size());
  return request_line;
}

}  // namespace net
