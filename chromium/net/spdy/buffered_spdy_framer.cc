// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include "base/logging.h"

namespace net {

SpdyMajorVersion NextProtoToSpdyMajorVersion(NextProto next_proto) {
  switch (next_proto) {
    case kProtoDeprecatedSPDY2:
      return SPDY2;
    case kProtoSPDY3:
    case kProtoSPDY31:
      return SPDY3;
    // SPDY/4 and HTTP/2 share the same framing for now.
    case kProtoSPDY4a2:
    case kProtoHTTP2Draft04:
      return SPDY4;
    case kProtoUnknown:
    case kProtoHTTP11:
    case kProtoQUIC1SPDY3:
      break;
  }
  NOTREACHED();
  return SPDY2;
}

BufferedSpdyFramer::BufferedSpdyFramer(SpdyMajorVersion version,
                                       bool enable_compression)
    : spdy_framer_(version),
      visitor_(NULL),
      header_buffer_used_(0),
      header_buffer_valid_(false),
      header_stream_id_(SpdyFramer::kInvalidStream),
      frames_received_(0) {
  spdy_framer_.set_enable_compression(enable_compression);
  memset(header_buffer_, 0, sizeof(header_buffer_));
}

BufferedSpdyFramer::~BufferedSpdyFramer() {
}

void BufferedSpdyFramer::set_visitor(
    BufferedSpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
  spdy_framer_.set_visitor(this);
}

void BufferedSpdyFramer::set_debug_visitor(
    SpdyFramerDebugVisitorInterface* debug_visitor) {
  spdy_framer_.set_debug_visitor(debug_visitor);
}

void BufferedSpdyFramer::OnError(SpdyFramer* spdy_framer) {
  DCHECK(spdy_framer);
  visitor_->OnError(spdy_framer->error_code());
}

void BufferedSpdyFramer::OnSynStream(SpdyStreamId stream_id,
                                     SpdyStreamId associated_stream_id,
                                     SpdyPriority priority,
                                     uint8 credential_slot,
                                     bool fin,
                                     bool unidirectional) {
  frames_received_++;
  DCHECK(!control_frame_fields_.get());
  control_frame_fields_.reset(new ControlFrameFields());
  control_frame_fields_->type = SYN_STREAM;
  control_frame_fields_->stream_id = stream_id;
  control_frame_fields_->associated_stream_id = associated_stream_id;
  control_frame_fields_->priority = priority;
  control_frame_fields_->credential_slot = credential_slot;
  control_frame_fields_->fin = fin;
  control_frame_fields_->unidirectional = unidirectional;

  InitHeaderStreaming(stream_id);
}

void BufferedSpdyFramer::OnHeaders(SpdyStreamId stream_id,
                                   bool fin) {
  frames_received_++;
  DCHECK(!control_frame_fields_.get());
  control_frame_fields_.reset(new ControlFrameFields());
  control_frame_fields_->type = HEADERS;
  control_frame_fields_->stream_id = stream_id;
  control_frame_fields_->fin = fin;

  InitHeaderStreaming(stream_id);
}

void BufferedSpdyFramer::OnSynReply(SpdyStreamId stream_id,
                                    bool fin) {
  frames_received_++;
  DCHECK(!control_frame_fields_.get());
  control_frame_fields_.reset(new ControlFrameFields());
  control_frame_fields_->type = SYN_REPLY;
  control_frame_fields_->stream_id = stream_id;
  control_frame_fields_->fin = fin;

  InitHeaderStreaming(stream_id);
}

bool BufferedSpdyFramer::OnCredentialFrameData(const char* frame_data,
                                               size_t len) {
  DCHECK(false);
  return false;
}

bool BufferedSpdyFramer::OnControlFrameHeaderData(SpdyStreamId stream_id,
                                                  const char* header_data,
                                                  size_t len) {
  CHECK_EQ(header_stream_id_, stream_id);

  if (len == 0) {
    // Indicates end-of-header-block.
    CHECK(header_buffer_valid_);

    SpdyHeaderBlock headers;
    size_t parsed_len = spdy_framer_.ParseHeaderBlockInBuffer(
        header_buffer_, header_buffer_used_, &headers);
    // TODO(rch): this really should be checking parsed_len != len,
    // but a bunch of tests fail.  Need to figure out why.
    if (parsed_len == 0) {
      visitor_->OnStreamError(
          stream_id, "Could not parse Spdy Control Frame Header.");
      return false;
    }
    DCHECK(control_frame_fields_.get());
    switch (control_frame_fields_->type) {
      case SYN_STREAM:
        visitor_->OnSynStream(control_frame_fields_->stream_id,
                              control_frame_fields_->associated_stream_id,
                              control_frame_fields_->priority,
                              control_frame_fields_->credential_slot,
                              control_frame_fields_->fin,
                              control_frame_fields_->unidirectional,
                              headers);
        break;
      case SYN_REPLY:
        visitor_->OnSynReply(control_frame_fields_->stream_id,
                             control_frame_fields_->fin,
                             headers);
        break;
      case HEADERS:
        visitor_->OnHeaders(control_frame_fields_->stream_id,
                            control_frame_fields_->fin,
                            headers);
        break;
      default:
        DCHECK(false) << "Unexpect control frame type: "
                      << control_frame_fields_->type;
        break;
    }
    control_frame_fields_.reset(NULL);
    return true;
  }

  const size_t available = kHeaderBufferSize - header_buffer_used_;
  if (len > available) {
    header_buffer_valid_ = false;
    visitor_->OnStreamError(
        stream_id, "Received more data than the allocated size.");
    return false;
  }
  memcpy(header_buffer_ + header_buffer_used_, header_data, len);
  header_buffer_used_ += len;
  return true;
}

void BufferedSpdyFramer::OnDataFrameHeader(SpdyStreamId stream_id,
                                           size_t length,
                                           bool fin) {
  frames_received_++;
  header_stream_id_ = stream_id;
  visitor_->OnDataFrameHeader(stream_id, length, fin);
}

void BufferedSpdyFramer::OnStreamFrameData(SpdyStreamId stream_id,
                                           const char* data,
                                           size_t len,
                                           bool fin) {
  visitor_->OnStreamFrameData(stream_id, data, len, fin);
}

void BufferedSpdyFramer::OnSettings(bool clear_persisted) {
  visitor_->OnSettings(clear_persisted);
}

void BufferedSpdyFramer::OnSetting(SpdySettingsIds id,
                                   uint8 flags,
                                   uint32 value) {
  visitor_->OnSetting(id, flags, value);
}

void BufferedSpdyFramer::OnPing(uint32 unique_id) {
  visitor_->OnPing(unique_id);
}

void BufferedSpdyFramer::OnRstStream(SpdyStreamId stream_id,
                                     SpdyRstStreamStatus status) {
  visitor_->OnRstStream(stream_id, status);
}
void BufferedSpdyFramer::OnGoAway(SpdyStreamId last_accepted_stream_id,
                                  SpdyGoAwayStatus status) {
  visitor_->OnGoAway(last_accepted_stream_id, status);
}

void BufferedSpdyFramer::OnWindowUpdate(SpdyStreamId stream_id,
                                        uint32 delta_window_size) {
  visitor_->OnWindowUpdate(stream_id, delta_window_size);
}

void BufferedSpdyFramer::OnPushPromise(SpdyStreamId stream_id,
                                       SpdyStreamId promised_stream_id) {
  visitor_->OnPushPromise(stream_id, promised_stream_id);
}

SpdyMajorVersion BufferedSpdyFramer::protocol_version() {
  return spdy_framer_.protocol_version();
}

size_t BufferedSpdyFramer::ProcessInput(const char* data, size_t len) {
  return spdy_framer_.ProcessInput(data, len);
}

void BufferedSpdyFramer::Reset() {
  spdy_framer_.Reset();
}

SpdyFramer::SpdyError BufferedSpdyFramer::error_code() const {
  return spdy_framer_.error_code();
}

SpdyFramer::SpdyState BufferedSpdyFramer::state() const {
  return spdy_framer_.state();
}

bool BufferedSpdyFramer::MessageFullyRead() {
  return state() == SpdyFramer::SPDY_AUTO_RESET;
}

bool BufferedSpdyFramer::HasError() {
  return spdy_framer_.HasError();
}

SpdyFrame* BufferedSpdyFramer::CreateSynStream(
    SpdyStreamId stream_id,
    SpdyStreamId associated_stream_id,
    SpdyPriority priority,
    uint8 credential_slot,
    SpdyControlFlags flags,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynStream(stream_id, associated_stream_id, priority,
                                      credential_slot, flags, headers);
}

SpdyFrame* BufferedSpdyFramer::CreateSynReply(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynReply(stream_id, flags, headers);
}

SpdyFrame* BufferedSpdyFramer::CreateRstStream(
    SpdyStreamId stream_id,
    SpdyRstStreamStatus status) const {
  return spdy_framer_.CreateRstStream(stream_id, status);
}

SpdyFrame* BufferedSpdyFramer::CreateSettings(
    const SettingsMap& values) const {
  return spdy_framer_.CreateSettings(values);
}

SpdyFrame* BufferedSpdyFramer::CreatePingFrame(
    uint32 unique_id) const {
  return spdy_framer_.CreatePingFrame(unique_id);
}

SpdyFrame* BufferedSpdyFramer::CreateGoAway(
    SpdyStreamId last_accepted_stream_id,
    SpdyGoAwayStatus status) const {
  return spdy_framer_.CreateGoAway(last_accepted_stream_id, status);
}

SpdyFrame* BufferedSpdyFramer::CreateHeaders(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateHeaders(stream_id, flags, headers);
}

SpdyFrame* BufferedSpdyFramer::CreateWindowUpdate(
    SpdyStreamId stream_id,
    uint32 delta_window_size) const {
  return spdy_framer_.CreateWindowUpdate(stream_id, delta_window_size);
}

SpdyFrame* BufferedSpdyFramer::CreateDataFrame(SpdyStreamId stream_id,
                                               const char* data,
                                               uint32 len,
                                               SpdyDataFlags flags) {
  return spdy_framer_.CreateDataFrame(stream_id, data, len, flags);
}

SpdyPriority BufferedSpdyFramer::GetHighestPriority() const {
  return spdy_framer_.GetHighestPriority();
}

void BufferedSpdyFramer::InitHeaderStreaming(SpdyStreamId stream_id) {
  memset(header_buffer_, 0, kHeaderBufferSize);
  header_buffer_used_ = 0;
  header_buffer_valid_ = true;
  header_stream_id_ = stream_id;
  DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
}

}  // namespace net
