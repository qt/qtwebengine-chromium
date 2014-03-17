// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_connection_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/http/http_pipelined_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_version.h"
#include "net/socket/client_socket_handle.h"

namespace net {

namespace {

base::Value* NetLogReceivedHeadersCallback(const NetLog::Source& source,
                                           const std::string* feedback,
                                           NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  source.AddToEventParameters(dict);
  dict->SetString("feedback", *feedback);
  return dict;
}

base::Value* NetLogStreamClosedCallback(const NetLog::Source& source,
                                        bool not_reusable,
                                        NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  source.AddToEventParameters(dict);
  dict->SetBoolean("not_reusable", not_reusable);
  return dict;
}

base::Value* NetLogHostPortPairCallback(const HostPortPair* host_port_pair,
                                        NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  dict->SetString("host_and_port", host_port_pair->ToString());
  return dict;
}

}  // anonymous namespace

HttpPipelinedConnection*
HttpPipelinedConnectionImpl::Factory::CreateNewPipeline(
    ClientSocketHandle* connection,
    HttpPipelinedConnection::Delegate* delegate,
    const HostPortPair& origin,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated,
    NextProto protocol_negotiated) {
  return new HttpPipelinedConnectionImpl(connection, delegate, origin,
                                         used_ssl_config, used_proxy_info,
                                         net_log, was_npn_negotiated,
                                         protocol_negotiated);
}

HttpPipelinedConnectionImpl::HttpPipelinedConnectionImpl(
    ClientSocketHandle* connection,
    HttpPipelinedConnection::Delegate* delegate,
    const HostPortPair& origin,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated,
    NextProto protocol_negotiated)
    : delegate_(delegate),
      connection_(connection),
      used_ssl_config_(used_ssl_config),
      used_proxy_info_(used_proxy_info),
      net_log_(BoundNetLog::Make(net_log.net_log(),
                                 NetLog::SOURCE_HTTP_PIPELINED_CONNECTION)),
      was_npn_negotiated_(was_npn_negotiated),
      protocol_negotiated_(protocol_negotiated),
      read_buf_(new GrowableIOBuffer()),
      next_pipeline_id_(1),
      active_(false),
      usable_(true),
      completed_one_request_(false),
      weak_factory_(this),
      send_next_state_(SEND_STATE_NONE),
      send_still_on_call_stack_(false),
      read_next_state_(READ_STATE_NONE),
      active_read_id_(0),
      read_still_on_call_stack_(false) {
  CHECK(connection_.get());
  net_log_.BeginEvent(
      NetLog::TYPE_HTTP_PIPELINED_CONNECTION,
      base::Bind(&NetLogHostPortPairCallback, &origin));
}

HttpPipelinedConnectionImpl::~HttpPipelinedConnectionImpl() {
  CHECK_EQ(depth(), 0);
  CHECK(stream_info_map_.empty());
  CHECK(pending_send_request_queue_.empty());
  CHECK(request_order_.empty());
  CHECK_EQ(send_next_state_, SEND_STATE_NONE);
  CHECK_EQ(read_next_state_, READ_STATE_NONE);
  CHECK(!active_send_request_.get());
  CHECK(!active_read_id_);
  if (!usable_) {
    connection_->socket()->Disconnect();
  }
  connection_->Reset();
  net_log_.EndEvent(NetLog::TYPE_HTTP_PIPELINED_CONNECTION);
}

HttpPipelinedStream* HttpPipelinedConnectionImpl::CreateNewStream() {
  int pipeline_id = next_pipeline_id_++;
  CHECK(pipeline_id);
  HttpPipelinedStream* stream = new HttpPipelinedStream(this, pipeline_id);
  stream_info_map_.insert(std::make_pair(pipeline_id, StreamInfo()));
  return stream;
}

void HttpPipelinedConnectionImpl::InitializeParser(
    int pipeline_id,
    const HttpRequestInfo* request,
    const BoundNetLog& net_log) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(!stream_info_map_[pipeline_id].parser.get());
  stream_info_map_[pipeline_id].state = STREAM_BOUND;
  stream_info_map_[pipeline_id].parser.reset(new HttpStreamParser(
      connection_.get(), request, read_buf_.get(), net_log));
  stream_info_map_[pipeline_id].source = net_log.source();

  // In case our first stream doesn't SendRequest() immediately, we should still
  // allow others to use this pipeline.
  if (pipeline_id == 1) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpPipelinedConnectionImpl::ActivatePipeline,
                   weak_factory_.GetWeakPtr()));
  }
}

void HttpPipelinedConnectionImpl::ActivatePipeline() {
  if (!active_) {
    active_ = true;
    delegate_->OnPipelineHasCapacity(this);
  }
}

void HttpPipelinedConnectionImpl::OnStreamDeleted(int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  Close(pipeline_id, false);

  if (stream_info_map_[pipeline_id].state != STREAM_CREATED &&
      stream_info_map_[pipeline_id].state != STREAM_UNUSED) {
    CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_CLOSED);
    CHECK(stream_info_map_[pipeline_id].parser.get());
    stream_info_map_[pipeline_id].parser.reset();
  }
  CHECK(!stream_info_map_[pipeline_id].parser.get());
  stream_info_map_.erase(pipeline_id);

  delegate_->OnPipelineHasCapacity(this);
}

int HttpPipelinedConnectionImpl::SendRequest(
    int pipeline_id,
    const std::string& request_line,
    const HttpRequestHeaders& headers,
    HttpResponseInfo* response,
    const CompletionCallback& callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_BOUND);
  if (!usable_) {
    return ERR_PIPELINE_EVICTION;
  }

  PendingSendRequest* send_request = new PendingSendRequest;
  send_request->pipeline_id = pipeline_id;
  send_request->request_line = request_line;
  send_request->headers = headers;
  send_request->response = response;
  send_request->callback = callback;
  pending_send_request_queue_.push(send_request);

  int rv;
  if (send_next_state_ == SEND_STATE_NONE) {
    send_next_state_ = SEND_STATE_START_IMMEDIATELY;
    rv = DoSendRequestLoop(OK);
  } else {
    rv = ERR_IO_PENDING;
  }
  ActivatePipeline();
  return rv;
}

int HttpPipelinedConnectionImpl::DoSendRequestLoop(int result) {
  int rv = result;
  do {
    SendRequestState state = send_next_state_;
    send_next_state_ = SEND_STATE_NONE;
    switch (state) {
      case SEND_STATE_START_IMMEDIATELY:
        rv = DoStartRequestImmediately(rv);
        break;
      case SEND_STATE_START_NEXT_DEFERRED_REQUEST:
        rv = DoStartNextDeferredRequest(rv);
        break;
      case SEND_STATE_SEND_ACTIVE_REQUEST:
        rv = DoSendActiveRequest(rv);
        break;
      case SEND_STATE_COMPLETE:
        rv = DoSendComplete(rv);
        break;
      case SEND_STATE_EVICT_PENDING_REQUESTS:
        rv = DoEvictPendingSendRequests(rv);
        break;
      default:
        CHECK(false) << "bad send state: " << state;
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && send_next_state_ != SEND_STATE_NONE);
  send_still_on_call_stack_ = false;
  return rv;
}

void HttpPipelinedConnectionImpl::OnSendIOCallback(int result) {
  CHECK(active_send_request_.get());
  DoSendRequestLoop(result);
}

int HttpPipelinedConnectionImpl::DoStartRequestImmediately(int result) {
  CHECK(!active_send_request_.get());
  CHECK_EQ(static_cast<size_t>(1), pending_send_request_queue_.size());
  // If SendRequest() completes synchronously, then we need to return the value
  // directly to the caller. |send_still_on_call_stack_| will track this.
  // Otherwise, asynchronous completions will notify the caller via callback.
  send_still_on_call_stack_ = true;
  active_send_request_.reset(pending_send_request_queue_.front());
  pending_send_request_queue_.pop();
  send_next_state_ = SEND_STATE_SEND_ACTIVE_REQUEST;
  return OK;
}

int HttpPipelinedConnectionImpl::DoStartNextDeferredRequest(int result) {
  CHECK(!send_still_on_call_stack_);
  CHECK(!active_send_request_.get());

  while (!pending_send_request_queue_.empty()) {
    scoped_ptr<PendingSendRequest> next_request(
        pending_send_request_queue_.front());
    pending_send_request_queue_.pop();
    CHECK(ContainsKey(stream_info_map_, next_request->pipeline_id));
    if (stream_info_map_[next_request->pipeline_id].state != STREAM_CLOSED) {
      active_send_request_.reset(next_request.release());
      send_next_state_ = SEND_STATE_SEND_ACTIVE_REQUEST;
      return OK;
    }
  }

  send_next_state_ = SEND_STATE_NONE;
  return OK;
}

int HttpPipelinedConnectionImpl::DoSendActiveRequest(int result) {
  CHECK(stream_info_map_[active_send_request_->pipeline_id].parser.get());
  int rv = stream_info_map_[active_send_request_->pipeline_id].parser->
      SendRequest(active_send_request_->request_line,
                  active_send_request_->headers,
                  active_send_request_->response,
                  base::Bind(&HttpPipelinedConnectionImpl::OnSendIOCallback,
                             base::Unretained(this)));
  stream_info_map_[active_send_request_->pipeline_id].state = STREAM_SENDING;
  send_next_state_ = SEND_STATE_COMPLETE;
  return rv;
}

int HttpPipelinedConnectionImpl::DoSendComplete(int result) {
  CHECK(active_send_request_.get());
  CHECK_EQ(STREAM_SENDING,
           stream_info_map_[active_send_request_->pipeline_id].state);

  request_order_.push(active_send_request_->pipeline_id);
  stream_info_map_[active_send_request_->pipeline_id].state = STREAM_SENT;
  net_log_.AddEvent(
      NetLog::TYPE_HTTP_PIPELINED_CONNECTION_SENT_REQUEST,
      stream_info_map_[active_send_request_->pipeline_id].source.
          ToEventParametersCallback());

  if (result == ERR_SOCKET_NOT_CONNECTED && completed_one_request_) {
    result = ERR_PIPELINE_EVICTION;
  }
  if (result < OK) {
    usable_ = false;
  }

  if (!send_still_on_call_stack_) {
    QueueUserCallback(active_send_request_->pipeline_id,
                      active_send_request_->callback, result, FROM_HERE);
  }

  active_send_request_.reset();

  if (send_still_on_call_stack_) {
    // It should be impossible for another request to appear on the queue while
    // this send was on the call stack.
    CHECK(pending_send_request_queue_.empty());
    send_next_state_ = SEND_STATE_NONE;
  } else if (!usable_) {
    send_next_state_ = SEND_STATE_EVICT_PENDING_REQUESTS;
  } else {
    send_next_state_ = SEND_STATE_START_NEXT_DEFERRED_REQUEST;
  }

  return result;
}

int HttpPipelinedConnectionImpl::DoEvictPendingSendRequests(int result) {
  while (!pending_send_request_queue_.empty()) {
    scoped_ptr<PendingSendRequest> evicted_send(
        pending_send_request_queue_.front());
    pending_send_request_queue_.pop();
    if (ContainsKey(stream_info_map_, evicted_send->pipeline_id) &&
        stream_info_map_[evicted_send->pipeline_id].state != STREAM_CLOSED) {
      evicted_send->callback.Run(ERR_PIPELINE_EVICTION);
    }
  }
  send_next_state_ = SEND_STATE_NONE;
  return result;
}

int HttpPipelinedConnectionImpl::ReadResponseHeaders(
    int pipeline_id, const CompletionCallback& callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(STREAM_SENT, stream_info_map_[pipeline_id].state);
  CHECK(stream_info_map_[pipeline_id].read_headers_callback.is_null());

  if (!usable_)
    return ERR_PIPELINE_EVICTION;

  stream_info_map_[pipeline_id].state = STREAM_READ_PENDING;
  stream_info_map_[pipeline_id].read_headers_callback = callback;
  if (read_next_state_ == READ_STATE_NONE &&
      pipeline_id == request_order_.front()) {
    read_next_state_ = READ_STATE_START_IMMEDIATELY;
    return DoReadHeadersLoop(OK);
  }
  return ERR_IO_PENDING;
}

void HttpPipelinedConnectionImpl::StartNextDeferredRead() {
  if (read_next_state_ == READ_STATE_NONE) {
    read_next_state_ = READ_STATE_START_NEXT_DEFERRED_READ;
    DoReadHeadersLoop(OK);
  }
}

int HttpPipelinedConnectionImpl::DoReadHeadersLoop(int result) {
  int rv = result;
  do {
    ReadHeadersState state = read_next_state_;
    read_next_state_ = READ_STATE_NONE;
    switch (state) {
      case READ_STATE_START_IMMEDIATELY:
        rv = DoStartReadImmediately(rv);
        break;
      case READ_STATE_START_NEXT_DEFERRED_READ:
        rv = DoStartNextDeferredRead(rv);
        break;
      case READ_STATE_READ_HEADERS:
        rv = DoReadHeaders(rv);
        break;
      case READ_STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      case READ_STATE_WAITING_FOR_CLOSE:
        // This is a holding state. We return instead of continuing to run hte
        // loop. The state will advance when the stream calls Close().
        rv = DoReadWaitForClose(rv);
        read_still_on_call_stack_ = false;
        return rv;
      case READ_STATE_STREAM_CLOSED:
        rv = DoReadStreamClosed();
        break;
      case READ_STATE_EVICT_PENDING_READS:
        rv = DoEvictPendingReadHeaders(rv);
        break;
      case READ_STATE_NONE:
        break;
      default:
        CHECK(false) << "bad read state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && read_next_state_ != READ_STATE_NONE);
  read_still_on_call_stack_ = false;
  return rv;
}

void HttpPipelinedConnectionImpl::OnReadIOCallback(int result) {
  DoReadHeadersLoop(result);
}

int HttpPipelinedConnectionImpl::DoStartReadImmediately(int result) {
  CHECK(!active_read_id_);
  CHECK(!read_still_on_call_stack_);
  CHECK(!request_order_.empty());
  // If ReadResponseHeaders() completes synchronously, then we need to return
  // the value directly to the caller. |read_still_on_call_stack_| will track
  // this. Otherwise, asynchronous completions will notify the caller via
  // callback.
  read_still_on_call_stack_ = true;
  read_next_state_ = READ_STATE_READ_HEADERS;
  active_read_id_ = request_order_.front();
  request_order_.pop();
  return OK;
}

int HttpPipelinedConnectionImpl::DoStartNextDeferredRead(int result) {
  CHECK(!active_read_id_);
  CHECK(!read_still_on_call_stack_);

  if (request_order_.empty()) {
    read_next_state_ = READ_STATE_NONE;
    return OK;
  }

  int next_id = request_order_.front();
  CHECK(ContainsKey(stream_info_map_, next_id));
  switch (stream_info_map_[next_id].state) {
    case STREAM_READ_PENDING:
      read_next_state_ = READ_STATE_READ_HEADERS;
      active_read_id_ = next_id;
      request_order_.pop();
      break;

    case STREAM_CLOSED:
      // Since nobody will read whatever data is on the pipeline associated with
      // this closed request, we must shut down the rest of the pipeline.
      read_next_state_ = READ_STATE_EVICT_PENDING_READS;
      break;

    case STREAM_SENT:
      read_next_state_ = READ_STATE_NONE;
      break;

    default:
      CHECK(false) << "Unexpected read state: "
                   << stream_info_map_[next_id].state;
  }

  return OK;
}

int HttpPipelinedConnectionImpl::DoReadHeaders(int result) {
  CHECK(active_read_id_);
  CHECK(ContainsKey(stream_info_map_, active_read_id_));
  CHECK_EQ(STREAM_READ_PENDING, stream_info_map_[active_read_id_].state);
  stream_info_map_[active_read_id_].state = STREAM_ACTIVE;
  int rv = stream_info_map_[active_read_id_].parser->ReadResponseHeaders(
      base::Bind(&HttpPipelinedConnectionImpl::OnReadIOCallback,
                 base::Unretained(this)));
  read_next_state_ = READ_STATE_READ_HEADERS_COMPLETE;
  return rv;
}

int HttpPipelinedConnectionImpl::DoReadHeadersComplete(int result) {
  CHECK(active_read_id_);
  CHECK(ContainsKey(stream_info_map_, active_read_id_));
  CHECK_EQ(STREAM_ACTIVE, stream_info_map_[active_read_id_].state);

  read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
  if (result < OK) {
    if (completed_one_request_ &&
        (result == ERR_CONNECTION_CLOSED ||
         result == ERR_EMPTY_RESPONSE ||
         result == ERR_SOCKET_NOT_CONNECTED)) {
      // These usually indicate that pipelining failed on the server side. In
      // that case, we should retry without pipelining.
      result = ERR_PIPELINE_EVICTION;
    }
    usable_ = false;
  }

  CheckHeadersForPipelineCompatibility(active_read_id_, result);

  if (!read_still_on_call_stack_) {
    QueueUserCallback(active_read_id_,
                      stream_info_map_[active_read_id_].read_headers_callback,
                      result, FROM_HERE);
  }

  return result;
}

int HttpPipelinedConnectionImpl::DoReadWaitForClose(int result) {
  read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
  return result;
}

int HttpPipelinedConnectionImpl::DoReadStreamClosed() {
  CHECK(active_read_id_);
  CHECK(ContainsKey(stream_info_map_, active_read_id_));
  CHECK_EQ(stream_info_map_[active_read_id_].state, STREAM_CLOSED);
  active_read_id_ = 0;
  if (!usable_) {
    // TODO(simonjam): Don't wait this long to evict.
    read_next_state_ = READ_STATE_EVICT_PENDING_READS;
    return OK;
  }
  completed_one_request_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&HttpPipelinedConnectionImpl::StartNextDeferredRead,
                 weak_factory_.GetWeakPtr()));
  read_next_state_ = READ_STATE_NONE;
  return OK;
}

int HttpPipelinedConnectionImpl::DoEvictPendingReadHeaders(int result) {
  while (!request_order_.empty()) {
    int evicted_id = request_order_.front();
    request_order_.pop();
    if (!ContainsKey(stream_info_map_, evicted_id)) {
      continue;
    }
    if (stream_info_map_[evicted_id].state == STREAM_READ_PENDING) {
      stream_info_map_[evicted_id].state = STREAM_READ_EVICTED;
      stream_info_map_[evicted_id].read_headers_callback.Run(
          ERR_PIPELINE_EVICTION);
    }
  }
  read_next_state_ = READ_STATE_NONE;
  return result;
}

void HttpPipelinedConnectionImpl::Close(int pipeline_id,
                                        bool not_reusable) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  net_log_.AddEvent(
      NetLog::TYPE_HTTP_PIPELINED_CONNECTION_STREAM_CLOSED,
      base::Bind(&NetLogStreamClosedCallback,
                 stream_info_map_[pipeline_id].source, not_reusable));
  switch (stream_info_map_[pipeline_id].state) {
    case STREAM_CREATED:
      stream_info_map_[pipeline_id].state = STREAM_UNUSED;
      break;

    case STREAM_BOUND:
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      break;

    case STREAM_SENDING:
      usable_ = false;
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      active_send_request_.reset();
      send_next_state_ = SEND_STATE_EVICT_PENDING_REQUESTS;
      DoSendRequestLoop(OK);
      break;

    case STREAM_SENT:
    case STREAM_READ_PENDING:
      usable_ = false;
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      if (!request_order_.empty() &&
          pipeline_id == request_order_.front() &&
          read_next_state_ == READ_STATE_NONE) {
        read_next_state_ = READ_STATE_EVICT_PENDING_READS;
        DoReadHeadersLoop(OK);
      }
      break;

    case STREAM_ACTIVE:
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      if (not_reusable) {
        usable_ = false;
      }
      read_next_state_ = READ_STATE_STREAM_CLOSED;
      DoReadHeadersLoop(OK);
      break;

    case STREAM_READ_EVICTED:
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      break;

    case STREAM_CLOSED:
    case STREAM_UNUSED:
      // TODO(simonjam): Why is Close() sometimes called twice?
      break;

    default:
      CHECK(false);
      break;
  }
}

int HttpPipelinedConnectionImpl::ReadResponseBody(
    int pipeline_id, IOBuffer* buf, int buf_len,
    const CompletionCallback& callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(active_read_id_, pipeline_id);
  CHECK(stream_info_map_[pipeline_id].parser.get());
  return stream_info_map_[pipeline_id].parser->ReadResponseBody(
      buf, buf_len, callback);
}

UploadProgress HttpPipelinedConnectionImpl::GetUploadProgress(
    int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->GetUploadProgress();
}

HttpResponseInfo* HttpPipelinedConnectionImpl::GetResponseInfo(
    int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->GetResponseInfo();
}

bool HttpPipelinedConnectionImpl::IsResponseBodyComplete(
    int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->
      IsResponseBodyComplete();
}

bool HttpPipelinedConnectionImpl::CanFindEndOfResponse(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->
      CanFindEndOfResponse();
}

bool HttpPipelinedConnectionImpl::IsConnectionReused(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  if (pipeline_id > 1) {
    return true;
  }
  ClientSocketHandle::SocketReuseType reuse_type = connection_->reuse_type();
  return connection_->is_reused() ||
         reuse_type == ClientSocketHandle::UNUSED_IDLE;
}

void HttpPipelinedConnectionImpl::SetConnectionReused(int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  connection_->set_is_reused(true);
}

int64 HttpPipelinedConnectionImpl::GetTotalReceivedBytes(
    int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->received_bytes();
}

bool HttpPipelinedConnectionImpl::GetLoadTimingInfo(
    int pipeline_id, LoadTimingInfo* load_timing_info) const {
  return connection_->GetLoadTimingInfo(IsConnectionReused(pipeline_id),
                                        load_timing_info);
}

void HttpPipelinedConnectionImpl::GetSSLInfo(int pipeline_id,
                                             SSLInfo* ssl_info) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_[pipeline_id].parser.get());
  stream_info_map_[pipeline_id].parser->GetSSLInfo(ssl_info);
}

void HttpPipelinedConnectionImpl::GetSSLCertRequestInfo(
    int pipeline_id,
    SSLCertRequestInfo* cert_request_info) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_[pipeline_id].parser.get());
  stream_info_map_[pipeline_id].parser->GetSSLCertRequestInfo(
      cert_request_info);
}

void HttpPipelinedConnectionImpl::Drain(HttpPipelinedStream* stream,
                                        HttpNetworkSession* session) {
  HttpResponseHeaders* headers = stream->GetResponseInfo()->headers.get();
  if (!stream->CanFindEndOfResponse() || headers->IsChunkEncoded() ||
      !usable_) {
    // TODO(simonjam): Drain chunk-encoded responses if they're relatively
    // common.
    stream->Close(true);
    delete stream;
    return;
  }
  HttpResponseBodyDrainer* drainer = new HttpResponseBodyDrainer(stream);
  drainer->StartWithSize(session, headers->GetContentLength());
  // |drainer| will delete itself when done.
}

void HttpPipelinedConnectionImpl::CheckHeadersForPipelineCompatibility(
    int pipeline_id,
    int result) {
  if (result < OK) {
    switch (result) {
      // TODO(simonjam): Ignoring specific errors like this may not work.
      // Collect metrics to see if this code is useful.
      case ERR_ABORTED:
      case ERR_INTERNET_DISCONNECTED:
      case ERR_NETWORK_CHANGED:
        // These errors are no fault of the server.
        break;

      default:
        ReportPipelineFeedback(pipeline_id, PIPELINE_SOCKET_ERROR);
        break;
    }
    return;
  }
  HttpResponseInfo* info = GetResponseInfo(pipeline_id);
  const HttpVersion required_version(1, 1);
  if (info->headers->GetParsedHttpVersion() < required_version) {
    ReportPipelineFeedback(pipeline_id, OLD_HTTP_VERSION);
    return;
  }
  if (!info->headers->IsKeepAlive() || !CanFindEndOfResponse(pipeline_id)) {
    usable_ = false;
    ReportPipelineFeedback(pipeline_id, MUST_CLOSE_CONNECTION);
    return;
  }
  if (info->headers->HasHeader(
      HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_SERVER))) {
    ReportPipelineFeedback(pipeline_id, AUTHENTICATION_REQUIRED);
    return;
  }
  ReportPipelineFeedback(pipeline_id, OK);
}

void HttpPipelinedConnectionImpl::ReportPipelineFeedback(int pipeline_id,
                                                         Feedback feedback) {
  std::string feedback_str;
  switch (feedback) {
    case OK:
      feedback_str = "OK";
      break;

    case PIPELINE_SOCKET_ERROR:
      feedback_str = "PIPELINE_SOCKET_ERROR";
      break;

    case OLD_HTTP_VERSION:
      feedback_str = "OLD_HTTP_VERSION";
      break;

    case MUST_CLOSE_CONNECTION:
      feedback_str = "MUST_CLOSE_CONNECTION";
      break;

    case AUTHENTICATION_REQUIRED:
      feedback_str = "AUTHENTICATION_REQUIRED";
      break;

    default:
      NOTREACHED();
      feedback_str = "UNKNOWN";
      break;
  }
  net_log_.AddEvent(
      NetLog::TYPE_HTTP_PIPELINED_CONNECTION_RECEIVED_HEADERS,
      base::Bind(&NetLogReceivedHeadersCallback,
                 stream_info_map_[pipeline_id].source, &feedback_str));
  delegate_->OnPipelineFeedback(this, feedback);
}

void HttpPipelinedConnectionImpl::QueueUserCallback(
    int pipeline_id, const CompletionCallback& callback, int rv,
    const tracked_objects::Location& from_here) {
  CHECK(stream_info_map_[pipeline_id].pending_user_callback.is_null());
  stream_info_map_[pipeline_id].pending_user_callback = callback;
  base::MessageLoop::current()->PostTask(
      from_here,
      base::Bind(&HttpPipelinedConnectionImpl::FireUserCallback,
                 weak_factory_.GetWeakPtr(), pipeline_id, rv));
}

void HttpPipelinedConnectionImpl::FireUserCallback(int pipeline_id,
                                                   int result) {
  if (ContainsKey(stream_info_map_, pipeline_id)) {
    CHECK(!stream_info_map_[pipeline_id].pending_user_callback.is_null());
    CompletionCallback callback =
        stream_info_map_[pipeline_id].pending_user_callback;
    stream_info_map_[pipeline_id].pending_user_callback.Reset();
    callback.Run(result);
  }
}

int HttpPipelinedConnectionImpl::depth() const {
  return stream_info_map_.size();
}

bool HttpPipelinedConnectionImpl::usable() const {
  return usable_;
}

bool HttpPipelinedConnectionImpl::active() const {
  return active_;
}

const SSLConfig& HttpPipelinedConnectionImpl::used_ssl_config() const {
  return used_ssl_config_;
}

const ProxyInfo& HttpPipelinedConnectionImpl::used_proxy_info() const {
  return used_proxy_info_;
}

const BoundNetLog& HttpPipelinedConnectionImpl::net_log() const {
  return net_log_;
}

bool HttpPipelinedConnectionImpl::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

NextProto HttpPipelinedConnectionImpl::protocol_negotiated()
    const {
  return protocol_negotiated_;
}

HttpPipelinedConnectionImpl::PendingSendRequest::PendingSendRequest()
    : pipeline_id(0),
      response(NULL) {
}

HttpPipelinedConnectionImpl::PendingSendRequest::~PendingSendRequest() {
}

HttpPipelinedConnectionImpl::StreamInfo::StreamInfo()
    : state(STREAM_CREATED) {
}

HttpPipelinedConnectionImpl::StreamInfo::~StreamInfo() {
}

}  // namespace net
