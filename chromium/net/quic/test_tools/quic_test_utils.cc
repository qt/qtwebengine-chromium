// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_test_utils.h"

#include "base/stl_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/spdy/spdy_frame_builder.h"

using base::StringPiece;
using std::max;
using std::min;
using std::string;
using testing::_;

namespace net {
namespace test {
namespace {

// No-op alarm implementation used by MockHelper.
class TestAlarm : public QuicAlarm {
 public:
  explicit TestAlarm(QuicAlarm::Delegate* delegate)
      : QuicAlarm(delegate) {
  }

  virtual void SetImpl() OVERRIDE {}
  virtual void CancelImpl() OVERRIDE {}
};

}  // namespace

MockFramerVisitor::MockFramerVisitor() {
  // By default, we want to accept packets.
  ON_CALL(*this, OnProtocolVersionMismatch(_))
      .WillByDefault(testing::Return(false));

  // By default, we want to accept packets.
  ON_CALL(*this, OnPacketHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStreamFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnAckFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnCongestionFeedbackFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnRstStreamFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnConnectionCloseFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnGoAwayFrame(_))
      .WillByDefault(testing::Return(true));
}

MockFramerVisitor::~MockFramerVisitor() {
}

bool NoOpFramerVisitor::OnProtocolVersionMismatch(QuicVersion version) {
  return false;
}

bool NoOpFramerVisitor::OnPacketHeader(const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnStreamFrame(const QuicStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnAckFrame(const QuicAckFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnRstStreamFrame(
    const QuicRstStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  return true;
}

FramerVisitorCapturingFrames::FramerVisitorCapturingFrames() : frame_count_(0) {
}

FramerVisitorCapturingFrames::~FramerVisitorCapturingFrames() {
}

bool FramerVisitorCapturingFrames::OnPacketHeader(
    const QuicPacketHeader& header) {
  header_ = header;
  frame_count_ = 0;
  return true;
}

bool FramerVisitorCapturingFrames::OnStreamFrame(const QuicStreamFrame& frame) {
  // TODO(ianswett): Own the underlying string, so it will not exist outside
  // this callback.
  stream_frames_.push_back(frame);
  ++frame_count_;
  return true;
}

bool FramerVisitorCapturingFrames::OnAckFrame(const QuicAckFrame& frame) {
  ack_.reset(new QuicAckFrame(frame));
  ++frame_count_;
  return true;
}

bool FramerVisitorCapturingFrames::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  feedback_.reset(new QuicCongestionFeedbackFrame(frame));
  ++frame_count_;
  return true;
}

bool FramerVisitorCapturingFrames::OnRstStreamFrame(
    const QuicRstStreamFrame& frame) {
  rst_.reset(new QuicRstStreamFrame(frame));
  ++frame_count_;
  return true;
}

bool FramerVisitorCapturingFrames::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  close_.reset(new QuicConnectionCloseFrame(frame));
  ++frame_count_;
  return true;
}

bool FramerVisitorCapturingFrames::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  goaway_.reset(new QuicGoAwayFrame(frame));
  ++frame_count_;
  return true;
}

void FramerVisitorCapturingFrames::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& packet) {
  version_negotiation_packet_.reset(new QuicVersionNegotiationPacket(packet));
  frame_count_ = 0;
}

FramerVisitorCapturingPublicReset::FramerVisitorCapturingPublicReset() {
}

FramerVisitorCapturingPublicReset::~FramerVisitorCapturingPublicReset() {
}

void FramerVisitorCapturingPublicReset::OnPublicResetPacket(
    const QuicPublicResetPacket& public_reset) {
  public_reset_packet_ = public_reset;
}

MockConnectionVisitor::MockConnectionVisitor() {
}

MockConnectionVisitor::~MockConnectionVisitor() {
}

MockHelper::MockHelper() {
}

MockHelper::~MockHelper() {
}

const QuicClock* MockHelper::GetClock() const {
  return &clock_;
}

QuicRandom* MockHelper::GetRandomGenerator() {
  return &random_generator_;
}

QuicAlarm* MockHelper::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new TestAlarm(delegate);
}

void MockHelper::AdvanceTime(QuicTime::Delta delta) {
  clock_.AdvanceTime(delta);
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               bool is_server)
    : QuicConnection(guid, address, new testing::NiceMock<MockHelper>(),
                     is_server, QuicVersionMax()),
      has_mock_helper_(true) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               bool is_server)
    : QuicConnection(guid, address, helper, is_server, QuicVersionMax()),
      has_mock_helper_(false) {
}

MockConnection::~MockConnection() {
}

void MockConnection::AdvanceTime(QuicTime::Delta delta) {
  CHECK(has_mock_helper_) << "Cannot advance time unless a MockClock is being"
                             " used";
  static_cast<MockHelper*>(helper())->AdvanceTime(delta);
}

PacketSavingConnection::PacketSavingConnection(QuicGuid guid,
                                               IPEndPoint address,
                                               bool is_server)
    : MockConnection(guid, address, is_server) {
}

PacketSavingConnection::~PacketSavingConnection() {
  STLDeleteElements(&packets_);
  STLDeleteElements(&encrypted_packets_);
}

bool PacketSavingConnection::SendOrQueuePacket(
    EncryptionLevel level,
    QuicPacketSequenceNumber sequence_number,
    QuicPacket* packet,
    QuicPacketEntropyHash /* entropy_hash */,
    HasRetransmittableData /* retransmittable */,
    Force /* forced */) {
  packets_.push_back(packet);
  QuicEncryptedPacket* encrypted =
      framer_.EncryptPacket(level, sequence_number, *packet);
  encrypted_packets_.push_back(encrypted);
  return true;
}

MockSession::MockSession(QuicConnection* connection, bool is_server)
    : QuicSession(connection, DefaultQuicConfig(), is_server) {
  ON_CALL(*this, WritevData(_, _, _, _, _))
      .WillByDefault(testing::Return(QuicConsumedData(0, false)));
}

MockSession::~MockSession() {
}

TestSession::TestSession(QuicConnection* connection,
                         const QuicConfig& config,
                         bool is_server)
    : QuicSession(connection, config, is_server),
      crypto_stream_(NULL) {
}

TestSession::~TestSession() {}

void TestSession::SetCryptoStream(QuicCryptoStream* stream) {
  crypto_stream_ = stream;
}

QuicCryptoStream* TestSession::GetCryptoStream() {
  return crypto_stream_;
}

MockSendAlgorithm::MockSendAlgorithm() {
}

MockSendAlgorithm::~MockSendAlgorithm() {
}

MockAckNotifierDelegate::MockAckNotifierDelegate() {
}

MockAckNotifierDelegate::~MockAckNotifierDelegate() {
}

namespace {

string HexDumpWithMarks(const char* data, int length,
                        const bool* marks, int mark_length) {
  static const char kHexChars[] = "0123456789abcdef";
  static const int kColumns = 4;

  const int kSizeLimit = 1024;
  if (length > kSizeLimit || mark_length > kSizeLimit) {
    LOG(ERROR) << "Only dumping first " << kSizeLimit << " bytes.";
    length = min(length, kSizeLimit);
    mark_length = min(mark_length, kSizeLimit);
  }

  string hex;
  for (const char* row = data; length > 0;
       row += kColumns, length -= kColumns) {
    for (const char *p = row; p < row + 4; ++p) {
      if (p < row + length) {
        const bool mark =
            (marks && (p - data) < mark_length && marks[p - data]);
        hex += mark ? '*' : ' ';
        hex += kHexChars[(*p & 0xf0) >> 4];
        hex += kHexChars[*p & 0x0f];
        hex += mark ? '*' : ' ';
      } else {
        hex += "    ";
      }
    }
    hex = hex + "  ";

    for (const char *p = row; p < row + 4 && p < row + length; ++p)
      hex += (*p >= 0x20 && *p <= 0x7f) ? (*p) : '.';

    hex = hex + '\n';
  }
  return hex;
}

}  // namespace

void CompareCharArraysWithHexError(
    const string& description,
    const char* actual,
    const int actual_len,
    const char* expected,
    const int expected_len) {
  const int min_len = min(actual_len, expected_len);
  const int max_len = max(actual_len, expected_len);
  scoped_ptr<bool[]> marks(new bool[max_len]);
  bool identical = (actual_len == expected_len);
  for (int i = 0; i < min_len; ++i) {
    if (actual[i] != expected[i]) {
      marks[i] = true;
      identical = false;
    } else {
      marks[i] = false;
    }
  }
  for (int i = min_len; i < max_len; ++i) {
    marks[i] = true;
  }
  if (identical) return;
  ADD_FAILURE()
      << "Description:\n"
      << description
      << "\n\nExpected:\n"
      << HexDumpWithMarks(expected, expected_len, marks.get(), max_len)
      << "\nActual:\n"
      << HexDumpWithMarks(actual, actual_len, marks.get(), max_len);
}

static QuicPacket* ConstructPacketFromHandshakeMessage(
    QuicGuid guid,
    const CryptoHandshakeMessage& message,
    bool should_include_version) {
  CryptoFramer crypto_framer;
  scoped_ptr<QuicData> data(crypto_framer.ConstructHandshakeMessage(message));
  QuicFramer quic_framer(QuicVersionMax(), QuicTime::Zero(), false);

  QuicPacketHeader header;
  header.public_header.guid = guid;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = should_include_version;
  header.packet_sequence_number = 1;
  header.entropy_flag = false;
  header.entropy_hash = 0;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicStreamFrame stream_frame(kCryptoStreamId, false, 0,
                               data->AsStringPiece());

  QuicFrame frame(&stream_frame);
  QuicFrames frames;
  frames.push_back(frame);
  return quic_framer.BuildUnsizedDataPacket(header, frames).packet;
}

QuicPacket* ConstructHandshakePacket(QuicGuid guid, QuicTag tag) {
  CryptoHandshakeMessage message;
  message.set_tag(tag);
  return ConstructPacketFromHandshakeMessage(guid, message, false);
}

size_t GetPacketLengthForOneStream(
    QuicVersion version,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group,
    size_t* payload_length) {
  *payload_length = 1;
  const size_t stream_length =
      NullEncrypter().GetCiphertextSize(*payload_length) +
      QuicPacketCreator::StreamFramePacketOverhead(
          version, PACKET_8BYTE_GUID, include_version,
          sequence_number_length, is_in_fec_group);
  const size_t ack_length = NullEncrypter().GetCiphertextSize(
      QuicFramer::GetMinAckFrameSize()) +
      GetPacketHeaderSize(PACKET_8BYTE_GUID, include_version,
                          sequence_number_length, is_in_fec_group);
  if (stream_length < ack_length) {
    *payload_length = 1 + ack_length - stream_length;
  }

  return NullEncrypter().GetCiphertextSize(*payload_length) +
      QuicPacketCreator::StreamFramePacketOverhead(
          version, PACKET_8BYTE_GUID, include_version,
          sequence_number_length, is_in_fec_group);
}

// Size in bytes of the stream frame fields for an arbitrary StreamID and
// offset and the last frame in a packet.
size_t GetMinStreamFrameSize(QuicVersion version) {
  return kQuicFrameTypeSize + kQuicMaxStreamIdSize + kQuicMaxStreamOffsetSize;
}

QuicPacketEntropyHash TestEntropyCalculator::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  return 1u;
}

QuicConfig DefaultQuicConfig() {
  QuicConfig config;
  config.SetDefaults();
  return config;
}

bool TestDecompressorVisitor::OnDecompressedData(StringPiece data) {
  data.AppendToString(&data_);
  return true;
}

void TestDecompressorVisitor::OnDecompressionError() {
  error_ = true;
}

}  // namespace test
}  // namespace net
