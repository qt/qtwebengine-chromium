// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/fetch/http_listen_socket.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/tools/fetch/http_server_request_info.h"
#include "net/tools/fetch/http_server_response_info.h"

HttpListenSocket::HttpListenSocket(net::SocketDescriptor s,
                                   HttpListenSocket::Delegate* delegate)
    : net::TCPListenSocket(s, this),
      delegate_(delegate) {
}

HttpListenSocket::~HttpListenSocket() {
  STLDeleteElements(&connections_);
}

void HttpListenSocket::Accept() {
  net::SocketDescriptor conn = net::TCPListenSocket::AcceptSocket();
  DCHECK_NE(conn, net::kInvalidSocket);
  if (conn == net::kInvalidSocket) {
    // TODO
  } else {
    scoped_ptr<StreamListenSocket> sock(
        new HttpListenSocket(conn, delegate_));
    DidAccept(this, sock.Pass());
  }
}

// static
scoped_ptr<HttpListenSocket> HttpListenSocket::CreateAndListen(
    const std::string& ip,
    int port,
    HttpListenSocket::Delegate* delegate) {
  net::SocketDescriptor s = net::TCPListenSocket::CreateAndBind(ip, port);
  if (s == net::kInvalidSocket) {
    // TODO (ibrar): error handling.
  } else {
    scoped_ptr<HttpListenSocket> serv(new HttpListenSocket(s, delegate));
    serv->Listen();
    return serv.Pass();
  }
  return scoped_ptr<HttpListenSocket>();
}

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_SPACE,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method.
  ST_URL,        // Receiving the URL.
  ST_PROTO,      // Receiving the protocol.
  ST_HEADER,     // Starting a Request Header.
  ST_NAME,       // Receiving a request header name.
  ST_SEPARATOR,  // Receiving the separator between header name and value.
  ST_VALUE,      // Receiving a request header value.
  ST_DONE,       // Parsing is complete and successful.
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table.
int parser_state[MAX_STATES][MAX_INPUTS] = {
/* METHOD    */ { ST_URL,       ST_ERR,     ST_ERR,  ST_ERR,       ST_METHOD },
/* URL       */ { ST_PROTO,     ST_ERR,     ST_ERR,  ST_URL,       ST_URL },
/* PROTOCOL  */ { ST_ERR,       ST_HEADER,  ST_NAME, ST_ERR,       ST_PROTO },
/* HEADER    */ { ST_ERR,       ST_ERR,     ST_NAME, ST_ERR,       ST_ERR },
/* NAME      */ { ST_SEPARATOR, ST_DONE,    ST_ERR,  ST_SEPARATOR, ST_NAME },
/* SEPARATOR */ { ST_SEPARATOR, ST_ERR,     ST_ERR,  ST_SEPARATOR, ST_VALUE },
/* VALUE     */ { ST_VALUE,     ST_HEADER,  ST_NAME, ST_VALUE,     ST_VALUE },
/* DONE      */ { ST_DONE,      ST_DONE,    ST_DONE, ST_DONE,      ST_DONE },
/* ERR       */ { ST_ERR,       ST_ERR,     ST_ERR,  ST_ERR,       ST_ERR }
};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch(ch) {
    case ' ':
      return INPUT_SPACE;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
  }
  return INPUT_DEFAULT;
}

HttpServerRequestInfo* HttpListenSocket::ParseHeaders() {
  int pos = 0;
  int data_len = recv_data_.length();
  int state = ST_METHOD;
  HttpServerRequestInfo* info = new HttpServerRequestInfo();
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = recv_data_[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->url = GURL(buffer);
          buffer.clear();
          break;
        case ST_PROTO:
          // TODO(mbelshe): Deal better with parsing protocol.
          DCHECK(buffer == "HTTP/1.1");
          buffer.clear();
          break;
        case ST_NAME:
          header_name = buffer;
          buffer.clear();
          break;
        case ST_VALUE:
          header_value = buffer;
          // TODO(mbelshe): Deal better with duplicate headers.
          DCHECK(info->headers.find(header_name) == info->headers.end());
          info->headers[header_name] = header_value;
          buffer.clear();
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state.
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          recv_data_ = recv_data_.substr(pos);
          return info;
        case ST_ERR:
          delete info;
          return NULL;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  delete info;
  return NULL;
}

void HttpListenSocket::DidAccept(
    net::StreamListenSocket* server,
    scoped_ptr<net::StreamListenSocket> connection) {
  connections_.insert(connection.release());
}

void HttpListenSocket::DidRead(net::StreamListenSocket* connection,
                               const char* data,
                               int len) {
  recv_data_.append(data, len);
  while (recv_data_.length()) {
    HttpServerRequestInfo* request = ParseHeaders();
    if (!request)
      break;
    delegate_->OnRequest(this, request);
    delete request;
  }
}

void HttpListenSocket::DidClose(net::StreamListenSocket* sock) {
  size_t count = connections_.erase(sock);
  DCHECK_EQ(1u, count);
  delete sock;
}

// Convert the numeric status code to a string.
// e.g.  200 -> "200 OK".
std::string ServerStatus(int code) {
  switch(code) {
    case 200:
      return std::string("200 OK");
    // TODO(mbelshe): handle other codes.
  }
  NOTREACHED();
  return std::string();
}

void HttpListenSocket::Respond(HttpServerResponseInfo* info,
                               std::string& data) {
  std::string response;

  // Status line.
  response = info->protocol + " ";
  response += ServerStatus(info->status);
  response += "\r\n";

  // Standard headers.
  if (info->content_type.length())
    response += "Content-type: " + info->content_type + "\r\n";

  if (info->content_length > 0)
    response += "Content-length: " + base::IntToString(info->content_length) +
        "\r\n";

  if (info->connection_close)
    response += "Connection: close\r\n";

  // TODO(mbelshe): support additional headers.

  // End of headers.
  response += "\r\n";

  // Add data.
  response += data;

  // Write it all out.
  this->Send(response, false);
}
