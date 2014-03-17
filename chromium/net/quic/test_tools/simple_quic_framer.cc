// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/simple_quic_framer.h"

#include "base/stl_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"

using base::StringPiece;
using std::string;
using std::vector;

namespace net {
namespace test {

class SimpleFramerVisitor : public QuicFramerVisitorInterface {
 public:
  SimpleFramerVisitor()
      : error_(QUIC_NO_ERROR) {
  }

  virtual ~SimpleFramerVisitor() {
    STLDeleteElements(&stream_data_);
  }

  virtual void OnError(QuicFramer* framer) OVERRIDE {
    error_ = framer->error();
  }

  virtual bool OnProtocolVersionMismatch(QuicVersion version) OVERRIDE {
    return false;
  }

  virtual void OnPacket() OVERRIDE {}
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE {}
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE {}
  virtual void OnRevivedPacket() OVERRIDE {}

  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE {
    has_header_ = true;
    header_ = header;
    return true;
  }

  virtual bool OnUnauthenticatedHeader(
      const QuicPacketHeader& header) OVERRIDE {
    return true;
  }

  virtual void OnFecProtectedPayload(StringPiece payload) OVERRIDE {}

  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    // Save a copy of the data so it is valid after the packet is processed.
    stream_data_.push_back(frame.GetDataAsString());
    QuicStreamFrame stream_frame(frame);
    // Make sure that the stream frame points to this data.
    stream_frame.data.Clear();
    stream_frame.data.Append(const_cast<char*>(stream_data_.back()->data()),
                             stream_data_.back()->size());
    stream_frames_.push_back(stream_frame);
    return true;
  }

  virtual bool OnAckFrame(const QuicAckFrame& frame) OVERRIDE {
    ack_frames_.push_back(frame);
    return true;
  }

  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE {
    feedback_frames_.push_back(frame);
    return true;
  }

  virtual void OnFecData(const QuicFecData& fec) OVERRIDE {
    fec_data_ = fec;
    fec_redundancy_ = fec_data_.redundancy.as_string();
    fec_data_.redundancy = fec_redundancy_;
  }

  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE {
    rst_stream_frames_.push_back(frame);
    return true;
  }

  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE {
    connection_close_frames_.push_back(frame);
    return true;
  }

  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE {
    goaway_frames_.push_back(frame);
    return true;
  }

  virtual void OnPacketComplete() OVERRIDE {}

  const QuicPacketHeader& header() const { return header_; }
  const vector<QuicAckFrame>& ack_frames() const { return ack_frames_; }
  const vector<QuicConnectionCloseFrame>& connection_close_frames() const {
    return connection_close_frames_;
  }
  const vector<QuicCongestionFeedbackFrame>& feedback_frames() const {
    return feedback_frames_;
  }
  const vector<QuicGoAwayFrame>& goaway_frames() const {
    return goaway_frames_;
  }
  const vector<QuicRstStreamFrame>& rst_stream_frames() const {
    return rst_stream_frames_;
  }
  const vector<QuicStreamFrame>& stream_frames() const {
    return stream_frames_;
  }
  const QuicFecData& fec_data() const {
    return fec_data_;
  }

 private:
  QuicErrorCode error_;
  bool has_header_;
  QuicPacketHeader header_;
  QuicFecData fec_data_;
  string fec_redundancy_;
  vector<QuicAckFrame> ack_frames_;
  vector<QuicCongestionFeedbackFrame> feedback_frames_;
  vector<QuicStreamFrame> stream_frames_;
  vector<QuicRstStreamFrame> rst_stream_frames_;
  vector<QuicGoAwayFrame> goaway_frames_;
  vector<QuicConnectionCloseFrame> connection_close_frames_;
  vector<string*> stream_data_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFramerVisitor);
};

SimpleQuicFramer::SimpleQuicFramer()
    : framer_(QuicSupportedVersions(), QuicTime::Zero(), true) {
}

SimpleQuicFramer::~SimpleQuicFramer() {
}

bool SimpleQuicFramer::ProcessPacket(const QuicPacket& packet) {
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(
      ENCRYPTION_NONE, 0, packet));
  return ProcessPacket(*encrypted);
}

bool SimpleQuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  visitor_.reset(new SimpleFramerVisitor);
  framer_.set_visitor(visitor_.get());
  return framer_.ProcessPacket(packet);
}

const QuicPacketHeader& SimpleQuicFramer::header() const {
  return visitor_->header();
}

const QuicFecData& SimpleQuicFramer::fec_data() const {
  return visitor_->fec_data();
}

QuicFramer* SimpleQuicFramer::framer() {
  return &framer_;
}

size_t SimpleQuicFramer::num_frames() const {
  return ack_frames().size() +
      stream_frames().size() +
      feedback_frames().size() +
      rst_stream_frames().size() +
      goaway_frames().size() +
      connection_close_frames().size();
}

const vector<QuicAckFrame>& SimpleQuicFramer::ack_frames() const {
  return visitor_->ack_frames();
}

const vector<QuicStreamFrame>& SimpleQuicFramer::stream_frames() const {
  return visitor_->stream_frames();
}

const vector<QuicRstStreamFrame>& SimpleQuicFramer::rst_stream_frames() const {
  return visitor_->rst_stream_frames();
}

const vector<QuicCongestionFeedbackFrame>&
SimpleQuicFramer::feedback_frames() const {
  return visitor_->feedback_frames();
}

const vector<QuicGoAwayFrame>&
SimpleQuicFramer::goaway_frames() const {
  return visitor_->goaway_frames();
}

const vector<QuicConnectionCloseFrame>&
SimpleQuicFramer::connection_close_frames() const {
  return visitor_->connection_close_frames();
}

}  // namespace test
}  // namespace net
