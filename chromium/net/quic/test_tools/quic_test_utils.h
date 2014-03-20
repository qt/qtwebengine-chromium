// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

namespace test {

static const QuicGuid kTestGuid = 42;
static const int kTestPort = 123;

// Upper limit on versions we support.
QuicVersion QuicVersionMax();

// Lower limit on versions we support.
QuicVersion QuicVersionMin();

// Returns an address for 127.0.0.1.
IPAddressNumber Loopback4();

void CompareCharArraysWithHexError(const std::string& description,
                                   const char* actual,
                                   const int actual_len,
                                   const char* expected,
                                   const int expected_len);

bool DecodeHexString(const base::StringPiece& hex, std::string* bytes);

// Returns the length of a QuicPacket that is capable of holding either a
// stream frame or a minimal ack frame.  Sets |*payload_length| to the number
// of bytes of stream data that will fit in such a packet.
size_t GetPacketLengthForOneStream(
    QuicVersion version,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group,
    size_t* payload_length);

// Size in bytes of the stream frame fields for an arbitrary StreamID and
// offset and the last frame in a packet.
size_t GetMinStreamFrameSize(QuicVersion version);

// Returns QuicConfig set to default values.
QuicConfig DefaultQuicConfig();

template<typename SaveType>
class ValueRestore {
 public:
  ValueRestore(SaveType* name, SaveType value)
      : name_(name),
        value_(*name) {
    *name_ = value;
  }
  ~ValueRestore() {
    *name_ = value_;
  }

 private:
  SaveType* name_;
  SaveType value_;

  DISALLOW_COPY_AND_ASSIGN(ValueRestore);
};

class MockFramerVisitor : public QuicFramerVisitorInterface {
 public:
  MockFramerVisitor();
  ~MockFramerVisitor();

  MOCK_METHOD1(OnError, void(QuicFramer* framer));
  // The constructor sets this up to return false by default.
  MOCK_METHOD1(OnProtocolVersionMismatch, bool(QuicVersion version));
  MOCK_METHOD0(OnPacket, void());
  MOCK_METHOD1(OnPublicResetPacket, void(const QuicPublicResetPacket& header));
  MOCK_METHOD1(OnVersionNegotiationPacket,
               void(const QuicVersionNegotiationPacket& packet));
  MOCK_METHOD0(OnRevivedPacket, void());
  // The constructor sets this up to return true by default.
  MOCK_METHOD1(OnUnauthenticatedHeader, bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnPacketHeader, bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnFecProtectedPayload, void(base::StringPiece payload));
  MOCK_METHOD1(OnStreamFrame, bool(const QuicStreamFrame& frame));
  MOCK_METHOD1(OnAckFrame, bool(const QuicAckFrame& frame));
  MOCK_METHOD1(OnCongestionFeedbackFrame,
               bool(const QuicCongestionFeedbackFrame& frame));
  MOCK_METHOD1(OnFecData, void(const QuicFecData& fec));
  MOCK_METHOD1(OnRstStreamFrame, bool(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnConnectionCloseFrame,
               bool(const QuicConnectionCloseFrame& frame));
  MOCK_METHOD1(OnGoAwayFrame, bool(const QuicGoAwayFrame& frame));
  MOCK_METHOD0(OnPacketComplete, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFramerVisitor);
};

class NoOpFramerVisitor : public QuicFramerVisitorInterface {
 public:
  NoOpFramerVisitor() {}

  virtual void OnError(QuicFramer* framer) OVERRIDE {}
  virtual void OnPacket() OVERRIDE {}
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE {}
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE {}
  virtual void OnRevivedPacket() OVERRIDE {}
  virtual bool OnProtocolVersionMismatch(QuicVersion version) OVERRIDE;
  virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE {}
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual bool OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE {}
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE;
  virtual void OnPacketComplete() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpFramerVisitor);
};

class FramerVisitorCapturingPublicReset : public NoOpFramerVisitor {
 public:
  FramerVisitorCapturingPublicReset();
  virtual ~FramerVisitorCapturingPublicReset();

  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE;

  const QuicPublicResetPacket public_reset_packet() {
    return public_reset_packet_;
  }

 private:
  QuicPublicResetPacket public_reset_packet_;
};

class FramerVisitorCapturingFrames : public NoOpFramerVisitor {
 public:
  FramerVisitorCapturingFrames();
  virtual ~FramerVisitorCapturingFrames();

  // Reset the visitor to it's initial state.
  void Reset();

  // NoOpFramerVisitor
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual bool OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE;

  size_t frame_count() const { return frame_count_; }
  QuicPacketHeader* header() { return &header_; }
  const std::vector<QuicStreamFrame>* stream_frames() const {
    return &stream_frames_;
  }
  const std::vector<string*>& stream_data() const {
    return stream_data_;
  }
  QuicAckFrame* ack() { return ack_.get(); }
  QuicCongestionFeedbackFrame* feedback() { return feedback_.get(); }
  QuicRstStreamFrame* rst() { return rst_.get(); }
  QuicConnectionCloseFrame* close() { return close_.get(); }
  QuicGoAwayFrame* goaway() { return goaway_.get(); }
  QuicVersionNegotiationPacket* version_negotiation_packet() {
    return version_negotiation_packet_.get();
  }

 private:
  size_t frame_count_;
  QuicPacketHeader header_;
  std::vector<QuicStreamFrame> stream_frames_;
  std::vector<std::string*> stream_data_;
  scoped_ptr<QuicAckFrame> ack_;
  scoped_ptr<QuicCongestionFeedbackFrame> feedback_;
  scoped_ptr<QuicRstStreamFrame> rst_;
  scoped_ptr<QuicConnectionCloseFrame> close_;
  scoped_ptr<QuicGoAwayFrame> goaway_;
  scoped_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;

  DISALLOW_COPY_AND_ASSIGN(FramerVisitorCapturingFrames);
};

class MockConnectionVisitor : public QuicConnectionVisitorInterface {
 public:
  MockConnectionVisitor();
  virtual ~MockConnectionVisitor();

  MOCK_METHOD1(OnStreamFrames, bool(const std::vector<QuicStreamFrame>& frame));
  MOCK_METHOD1(OnRstStream, void(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnGoAway, void(const QuicGoAwayFrame& frame));
  MOCK_METHOD2(OnConnectionClosed, void(QuicErrorCode error, bool from_peer));
  MOCK_METHOD0(OnCanWrite, bool());
  MOCK_CONST_METHOD0(HasPendingHandshake, bool());
  MOCK_METHOD1(OnSuccessfulVersionNegotiation,
               void(const QuicVersion& version));
  MOCK_METHOD0(OnConfigNegotiated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionVisitor);
};

class MockHelper : public QuicConnectionHelperInterface {
 public:
  MockHelper();
  virtual ~MockHelper();

  MOCK_METHOD1(SetConnection, void(QuicConnection* connection));
  const QuicClock* GetClock() const;
  QuicRandom* GetRandomGenerator();
  void AdvanceTime(QuicTime::Delta delta);
  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate);

 private:
  MockClock clock_;
  MockRandom random_generator_;
};

class MockConnection : public QuicConnection {
 public:
  // Uses a MockHelper, GUID of 42, and 127.0.0.1:123.
  explicit MockConnection(bool is_server);

  // Uses a MockHelper, GUID of 42.
  MockConnection(IPEndPoint address,
                 bool is_server);

  // Uses a MockHelper, and 127.0.0.1:123
  MockConnection(QuicGuid guid,
                 bool is_server);

  virtual ~MockConnection();

  // If the constructor that uses a MockHelper has been used then this method
  // will advance the time of the MockClock.
  void AdvanceTime(QuicTime::Delta delta);

  MOCK_METHOD3(ProcessUdpPacket, void(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet));
  MOCK_METHOD1(SendConnectionClose, void(QuicErrorCode error));
  MOCK_METHOD2(SendConnectionCloseWithDetails, void(QuicErrorCode error,
                                                    const string& details));
  MOCK_METHOD2(SendRstStream, void(QuicStreamId id,
                                   QuicRstStreamErrorCode error));
  MOCK_METHOD3(SendGoAway, void(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const string& reason));
  MOCK_METHOD0(OnCanWrite, bool());
  MOCK_CONST_METHOD0(HasPendingHandshake, bool());

  void ProcessUdpPacketInternal(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) {
    QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  virtual bool OnProtocolVersionMismatch(QuicVersion version) OVERRIDE {
    return false;
  }

 private:
  scoped_ptr<QuicPacketWriter> writer_;
  scoped_ptr<QuicConnectionHelperInterface> helper_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class PacketSavingConnection : public MockConnection {
 public:
  explicit PacketSavingConnection(bool is_server);
  virtual ~PacketSavingConnection();

  virtual bool SendOrQueuePacket(EncryptionLevel level,
                                 const SerializedPacket& packet,
                                 TransmissionType transmission_type) OVERRIDE;

  std::vector<QuicPacket*> packets_;
  std::vector<QuicEncryptedPacket*> encrypted_packets_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PacketSavingConnection);
};

class MockSession : public QuicSession {
 public:
  explicit MockSession(QuicConnection* connection);
  virtual ~MockSession();

  MOCK_METHOD4(OnPacket, bool(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address,
                              const QuicPacketHeader& header,
                              const std::vector<QuicStreamFrame>& frame));
  MOCK_METHOD2(OnConnectionClosed, void(QuicErrorCode error, bool from_peer));
  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(GetCryptoStream, QuicCryptoStream*());
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());
  MOCK_METHOD6(WritevData,
               QuicConsumedData(QuicStreamId id,
                                const struct iovec* iov,
                                int count,
                                QuicStreamOffset offset,
                                bool fin,
                                QuicAckNotifier::DelegateInterface*));
  MOCK_METHOD0(IsHandshakeComplete, bool());
  MOCK_METHOD0(IsCryptoHandshakeConfirmed, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSession);
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, const QuicConfig& config);
  virtual ~TestSession();

  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());

  void SetCryptoStream(QuicCryptoStream* stream);

  virtual QuicCryptoStream* GetCryptoStream();

 private:
  QuicCryptoStream* crypto_stream_;
  DISALLOW_COPY_AND_ASSIGN(TestSession);
};

class MockPacketWriter : public QuicPacketWriter {
 public:
  MockPacketWriter();
  virtual ~MockPacketWriter();

  MOCK_METHOD5(WritePacket,
               WriteResult(const char* buffer,
                           size_t buf_len,
                           const IPAddressNumber& self_address,
                           const IPEndPoint& peer_address,
                           QuicBlockedWriterInterface* blocked_writer));
  MOCK_CONST_METHOD0(IsWriteBlockedDataBuffered, bool());
};

class MockSendAlgorithm : public SendAlgorithmInterface {
 public:
  MockSendAlgorithm();
  virtual ~MockSendAlgorithm();

  MOCK_METHOD2(SetFromConfig, void(const QuicConfig& config, bool is_server));
  MOCK_METHOD1(SetMaxPacketSize, void(QuicByteCount max_packet_size));
  MOCK_METHOD3(OnIncomingQuicCongestionFeedbackFrame,
               void(const QuicCongestionFeedbackFrame&,
                    QuicTime feedback_receive_time,
                    const SentPacketsMap&));
  MOCK_METHOD3(OnPacketAcked,
               void(QuicPacketSequenceNumber, QuicByteCount, QuicTime::Delta));
  MOCK_METHOD2(OnPacketLost, void(QuicPacketSequenceNumber, QuicTime));
  MOCK_METHOD5(OnPacketSent,
               bool(QuicTime sent_time, QuicPacketSequenceNumber, QuicByteCount,
                    TransmissionType, HasRetransmittableData));
  MOCK_METHOD0(OnRetransmissionTimeout, void());
  MOCK_METHOD2(OnPacketAbandoned, void(QuicPacketSequenceNumber sequence_number,
                                      QuicByteCount abandoned_bytes));
  MOCK_METHOD4(TimeUntilSend, QuicTime::Delta(QuicTime now, TransmissionType,
                                              HasRetransmittableData,
                                              IsHandshake));
  MOCK_CONST_METHOD0(BandwidthEstimate, QuicBandwidth(void));
  MOCK_CONST_METHOD0(SmoothedRtt, QuicTime::Delta(void));
  MOCK_CONST_METHOD0(RetransmissionDelay, QuicTime::Delta(void));
  MOCK_CONST_METHOD0(GetCongestionWindow, QuicByteCount());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSendAlgorithm);
};

class TestEntropyCalculator :
      public QuicReceivedEntropyHashCalculatorInterface {
 public:
  TestEntropyCalculator();
  virtual ~TestEntropyCalculator();

  virtual QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const OVERRIDE;
};

class MockEntropyCalculator : public TestEntropyCalculator {
 public:
  MockEntropyCalculator();
  virtual ~MockEntropyCalculator();

  MOCK_CONST_METHOD1(
      EntropyHash,
      QuicPacketEntropyHash(QuicPacketSequenceNumber sequence_number));
};

class TestDecompressorVisitor : public QuicSpdyDecompressor::Visitor {
 public:
  virtual ~TestDecompressorVisitor() {}
  virtual bool OnDecompressedData(base::StringPiece data) OVERRIDE;
  virtual void OnDecompressionError() OVERRIDE;

  string data() { return data_; }
  bool error() { return error_; }

 private:
  string data_;
  bool error_;
};

class MockAckNotifierDelegate : public QuicAckNotifier::DelegateInterface {
 public:
  MockAckNotifierDelegate();
  virtual ~MockAckNotifierDelegate();

  MOCK_METHOD0(OnAckNotification, void());
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
