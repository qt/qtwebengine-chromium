// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FRAMER_H_
#define NET_QUIC_QUIC_FRAMER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicFramerPeer;
}  // namespace test

class QuicDataReader;
class QuicDataWriter;
class QuicDecrypter;
class QuicEncrypter;
class QuicFramer;

// Number of bytes reserved for the frame type preceding each frame.
const size_t kQuicFrameTypeSize = 1;
// Number of bytes reserved for error code.
const size_t kQuicErrorCodeSize = 4;
// Number of bytes reserved to denote the length of error details field.
const size_t kQuicErrorDetailsLengthSize = 2;

// Maximum number of bytes reserved for stream id.
const size_t kQuicMaxStreamIdSize = 4;
// Maximum number of bytes reserved for byte offset in stream frame.
const size_t kQuicMaxStreamOffsetSize = 8;
// Number of bytes reserved to store payload length in stream frame.
const size_t kQuicStreamPayloadLengthSize = 2;

// Size in bytes of the entropy hash sent in ack frames.
const size_t kQuicEntropyHashSize = 1;
// Size in bytes reserved for the delta time of the largest observed
// sequence number in ack frames.
const size_t kQuicDeltaTimeLargestObservedSize = 2;
// Size in bytes reserved for the number of missing packets in ack frames.
const size_t kNumberOfMissingPacketsSize = 1;

// This class receives callbacks from the framer when packets
// are processed.
class NET_EXPORT_PRIVATE QuicFramerVisitorInterface {
 public:
  virtual ~QuicFramerVisitorInterface() {}

  // Called if an error is detected in the QUIC protocol.
  virtual void OnError(QuicFramer* framer) = 0;

  // Called only when |is_server_| is true and the the framer gets a packet with
  // version flag true and the version on the packet doesn't match
  // |quic_version_|. The visitor should return true after it updates the
  // version of the |framer_| to |received_version| or false to stop processing
  // this packet.
  virtual bool OnProtocolVersionMismatch(QuicVersion received_version) = 0;

  // Called when a new packet has been received, before it
  // has been validated or processed.
  virtual void OnPacket() = 0;

  // Called when a public reset packet has been parsed but has not yet
  // been validated.
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) = 0;

  // Called only when |is_server_| is false and a version negotiation packet has
  // been parsed.
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) = 0;

  // Called when a lost packet has been recovered via FEC,
  // before it has been processed.
  virtual void OnRevivedPacket() = 0;

  // Called when the unauthenticated portion of the header has been parsed.
  // If OnUnauthenticatedHeader returns false, framing for this packet will
  // cease.
  virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) = 0;

  // Called when the complete header of a packet had been parsed.
  // If OnPacketHeader returns false, framing for this packet will cease.
  virtual bool OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a data packet is parsed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnFecProtectedPayload(base::StringPiece payload) = 0;

  // Called when a StreamFrame has been parsed.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a AckFrame has been parsed.  If OnAckFrame returns false,
  // the framer will stop parsing the current packet.
  virtual bool OnAckFrame(const QuicAckFrame& frame) = 0;

  // Called when a CongestionFeedbackFrame has been parsed.
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when a GoAwayFrame has been parsed.
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) = 0;

  // Called when FEC data has been parsed.
  virtual void OnFecData(const QuicFecData& fec) = 0;

  // Called when a packet has been completely processed.
  virtual void OnPacketComplete() = 0;
};

class NET_EXPORT_PRIVATE QuicFecBuilderInterface {
 public:
  virtual ~QuicFecBuilderInterface() {}

  // Called when a data packet is constructed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) = 0;
};

// This class calculates the received entropy of the ack packet being
// framed, should it get truncated.
class NET_EXPORT_PRIVATE QuicReceivedEntropyHashCalculatorInterface {
 public:
  virtual ~QuicReceivedEntropyHashCalculatorInterface() {}

  // When an ack frame gets truncated while being framed the received
  // entropy of the ack frame needs to be calculated since the some of the
  // missing packets are not added and the largest observed might be lowered.
  // This should return the received entropy hash of the packets received up to
  // and including |sequence_number|.
  virtual QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const = 0;
};

// Class for parsing and constructing QUIC packets.  It has a
// QuicFramerVisitorInterface that is called when packets are parsed.
// It also has a QuicFecBuilder that is called when packets are constructed
// in order to generate FEC data for subsequently building FEC packets.
class NET_EXPORT_PRIVATE QuicFramer {
 public:
  // Constructs a new framer that installs a kNULL QuicEncrypter and
  // QuicDecrypter for level ENCRYPTION_NONE. |supported_versions| specifies the
  // list of supported QUIC versions. |quic_version_| is set to the maximum
  // version in |supported_versions|.
  QuicFramer(const QuicVersionVector& supported_versions,
             QuicTime creation_time,
             bool is_server);

  virtual ~QuicFramer();

  // Returns true if |version| is a supported protocol version.
  bool IsSupportedVersion(const QuicVersion version) const;

  // Returns true if the version flag is set in the public flags.
  static bool HasVersionFlag(const QuicEncryptedPacket& packet);

  // Calculates the largest observed packet to advertise in the case an Ack
  // Frame was truncated.  last_written in this case is the iterator for the
  // last missing packet which fit in the outgoing ack.
  static QuicPacketSequenceNumber CalculateLargestObserved(
      const SequenceNumberSet& missing_packets,
      SequenceNumberSet::const_iterator last_written);

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(QuicFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  // Set a builder to be called from the framer when building FEC protected
  // packets.  If this is called multiple times, only the last builder
  // will be used.  The builder need not be set.
  void set_fec_builder(QuicFecBuilderInterface* builder) {
    fec_builder_ = builder;
  }

  const QuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

  QuicVersion version() const {
    return quic_version_;
  }

  void set_version(const QuicVersion version);

  // Does not DCHECK for supported version. Used by tests to set unsupported
  // version to trigger version negotiation.
  void set_version_for_tests(const QuicVersion version) {
    quic_version_ = version;
  }

  // Set entropy calculator to be called from the framer when it needs the
  // entropy of a truncated ack frame. An entropy calculator must be set or else
  // the framer will likely crash. If this is called multiple times, only the
  // last calculator will be used.
  void set_received_entropy_calculator(
      QuicReceivedEntropyHashCalculatorInterface* entropy_calculator) {
    entropy_calculator_ = entropy_calculator;
  }

  QuicErrorCode error() const {
    return error_;
  }

  // Pass a UDP packet into the framer for parsing.
  // Return true if the packet was processed succesfully. |packet| must be a
  // single, complete UDP packet (not a frame of a packet).  This packet
  // might be null padded past the end of the payload, which will be correctly
  // ignored.
  bool ProcessPacket(const QuicEncryptedPacket& packet);

  // Pass a data packet that was revived from FEC data into the framer
  // for parsing.
  // Return true if the packet was processed succesfully. |payload| must be
  // the complete DECRYPTED payload of the revived packet.
  bool ProcessRevivedPacket(QuicPacketHeader* header,
                            base::StringPiece payload);

  // Largest size in bytes of all stream frame fields without the payload.
  static size_t GetMinStreamFrameSize(QuicVersion version,
                                      QuicStreamId stream_id,
                                      QuicStreamOffset offset,
                                      bool last_frame_in_packet);
  // Size in bytes of all ack frame fields without the missing packets.
  static size_t GetMinAckFrameSize(
      QuicVersion version,
      QuicSequenceNumberLength sequence_number_length,
      QuicSequenceNumberLength largest_observed_length);
  // Size in bytes of all reset stream frame without the error details.
  static size_t GetMinRstStreamFrameSize();
  // Size in bytes of all connection close frame fields without the error
  // details and the missing packets from the enclosed ack frame.
  static size_t GetMinConnectionCloseFrameSize();
  // Size in bytes of all GoAway frame fields without the reason phrase.
  static size_t GetMinGoAwayFrameSize();
  // The maximum number of nacks which can be transmitted in a single ack packet
  // without exceeding kDefaultMaxPacketSize.
  static size_t GetMaxUnackedPackets(QuicPacketHeader header);
  // Size in bytes required to serialize the stream id.
  static size_t GetStreamIdSize(QuicStreamId stream_id);
  // Size in bytes required to serialize the stream offset.
  static size_t GetStreamOffsetSize(QuicStreamOffset offset);
  // Size in bytes required for a serialized version negotiation packet
  static size_t GetVersionNegotiationPacketSize(size_t number_versions);


  static bool CanTruncate(
      QuicVersion version, const QuicFrame& frame, size_t free_bytes);

  // Returns the number of bytes added to the packet for the specified frame,
  // and 0 if the frame doesn't fit.  Includes the header size for the first
  // frame.
  size_t GetSerializedFrameLength(
      const QuicFrame& frame,
      size_t free_bytes,
      bool first_frame,
      bool last_frame,
      QuicSequenceNumberLength sequence_number_length);

  // Returns the associated data from the encrypted packet |encrypted| as a
  // stringpiece.
  static base::StringPiece GetAssociatedDataFromEncryptedPacket(
      const QuicEncryptedPacket& encrypted,
      QuicGuidLength guid_length,
      bool includes_version,
      QuicSequenceNumberLength sequence_number_length);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // and is populated with the fields in |header| and |frames|, or is NULL if
  // the packet could not be created.
  // TODO(ianswett): Used for testing only.
  SerializedPacket BuildUnsizedDataPacket(const QuicPacketHeader& header,
                                          const QuicFrames& frames);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // is created from the first |num_frames| frames, or is NULL if the packet
  // could not be created.  The packet must be of size |packet_size|.
  SerializedPacket BuildDataPacket(const QuicPacketHeader& header,
                                   const QuicFrames& frames,
                                   size_t packet_size);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // and is populated with the fields in |header| and |fec|, or is NULL if the
  // packet could not be created.
  SerializedPacket BuildFecPacket(const QuicPacketHeader& header,
                                  const QuicFecData& fec);

  // Returns a new public reset packet, owned by the caller.
  static QuicEncryptedPacket* BuildPublicResetPacket(
      const QuicPublicResetPacket& packet);

  QuicEncryptedPacket* BuildVersionNegotiationPacket(
      const QuicPacketPublicHeader& header,
      const QuicVersionVector& supported_versions);

  // SetDecrypter sets the primary decrypter, replacing any that already exists,
  // and takes ownership. If an alternative decrypter is in place then the
  // function DCHECKs. This is intended for cases where one knows that future
  // packets will be using the new decrypter and the previous decrypter is not
  // obsolete.
  void SetDecrypter(QuicDecrypter* decrypter);

  // SetAlternativeDecrypter sets a decrypter that may be used to decrypt
  // future packets and takes ownership of it. If |latch_once_used| is true,
  // then the first time that the decrypter is successful it will replace the
  // primary decrypter. Otherwise both decrypters will remain active and the
  // primary decrypter will be the one last used.
  void SetAlternativeDecrypter(QuicDecrypter* decrypter,
                               bool latch_once_used);

  const QuicDecrypter* decrypter() const;
  const QuicDecrypter* alternative_decrypter() const;

  // Changes the encrypter used for level |level| to |encrypter|. The function
  // takes ownership of |encrypter|.
  void SetEncrypter(EncryptionLevel level, QuicEncrypter* encrypter);
  const QuicEncrypter* encrypter(EncryptionLevel level) const;

  // SwapCryptersForTest exchanges the state of the crypters with |other|. To
  // be used in tests only.
  void SwapCryptersForTest(QuicFramer* other);

  // Returns a new encrypted packet, owned by the caller.
  QuicEncryptedPacket* EncryptPacket(EncryptionLevel level,
                                     QuicPacketSequenceNumber sequence_number,
                                     const QuicPacket& packet);

  // Returns the maximum length of plaintext that can be encrypted
  // to ciphertext no larger than |ciphertext_size|.
  size_t GetMaxPlaintextSize(size_t ciphertext_size);

  const std::string& detailed_error() { return detailed_error_; }

  // Read the full 8 byte guid from a packet header.
  // Return true on success, else false.
  static bool ReadGuidFromPacket(const QuicEncryptedPacket& packet,
                                 QuicGuid* guid);

  static QuicSequenceNumberLength ReadSequenceNumberLength(uint8 flags);

  // The minimum sequence number length required to represent |sequence_number|.
  static QuicSequenceNumberLength GetMinSequenceNumberLength(
      QuicPacketSequenceNumber sequence_number);

 private:
  friend class test::QuicFramerPeer;

  typedef std::map<QuicPacketSequenceNumber, uint8> NackRangeMap;

  struct AckFrameInfo {
    AckFrameInfo();
    ~AckFrameInfo();

    // The maximum delta between ranges.
    QuicPacketSequenceNumber max_delta;
    // Nack ranges starting with start sequence numbers and lengths.
    NackRangeMap nack_ranges;
  };

  QuicPacketEntropyHash GetPacketEntropyHash(
      const QuicPacketHeader& header) const;

  bool ProcessDataPacket(const QuicPacketPublicHeader& public_header,
                         const QuicEncryptedPacket& packet);

  bool ProcessPublicResetPacket(const QuicPacketPublicHeader& public_header);

  bool ProcessVersionNegotiationPacket(QuicPacketPublicHeader* public_header);

  bool ProcessPublicHeader(QuicPacketPublicHeader* header);

  bool ProcessPacketHeader(QuicPacketHeader* header,
                           const QuicEncryptedPacket& packet);

  bool ProcessPacketSequenceNumber(
      QuicSequenceNumberLength sequence_number_length,
      QuicPacketSequenceNumber* sequence_number);
  bool ProcessFrameData(const QuicPacketHeader& header);
  bool ProcessStreamFrame(uint8 frame_type, QuicStreamFrame* frame);
  bool ProcessAckFrame(const QuicPacketHeader& header,
                       uint8 frame_type,
                       QuicAckFrame* frame);
  bool ProcessReceivedInfo(uint8 frame_type, ReceivedPacketInfo* received_info);
  bool ProcessSentInfo(const QuicPacketHeader& public_header,
                       SentPacketInfo* sent_info);
  bool ProcessQuicCongestionFeedbackFrame(
      QuicCongestionFeedbackFrame* congestion_feedback);
  bool ProcessRstStreamFrame(QuicRstStreamFrame* frame);
  bool ProcessConnectionCloseFrame(QuicConnectionCloseFrame* frame);
  bool ProcessGoAwayFrame(QuicGoAwayFrame* frame);

  bool DecryptPayload(const QuicPacketHeader& header,
                      const QuicEncryptedPacket& packet);

  // Returns the full packet sequence number from the truncated
  // wire format version and the last seen packet sequence number.
  QuicPacketSequenceNumber CalculatePacketSequenceNumberFromWire(
      QuicSequenceNumberLength sequence_number_length,
      QuicPacketSequenceNumber packet_sequence_number) const;

  // Computes the wire size in bytes of the |ack| frame, assuming no truncation.
  size_t GetAckFrameSize(const QuicAckFrame& ack,
                         QuicSequenceNumberLength sequence_number_length);

  // Computes the wire size in bytes of the payload of |frame|.
  size_t ComputeFrameLength(const QuicFrame& frame,
                            bool last_frame_in_packet,
                            QuicSequenceNumberLength sequence_number_length);

  static bool AppendPacketSequenceNumber(
      QuicSequenceNumberLength sequence_number_length,
      QuicPacketSequenceNumber packet_sequence_number,
      QuicDataWriter* writer);

  static uint8 GetSequenceNumberFlags(
      QuicSequenceNumberLength sequence_number_length);

  static AckFrameInfo GetAckFrameInfo(const QuicAckFrame& frame);

  bool AppendPacketHeader(const QuicPacketHeader& header,
                          QuicDataWriter* writer);
  bool AppendTypeByte(const QuicFrame& frame,
                      bool last_frame_in_packet,
                      QuicDataWriter* writer);
  bool AppendStreamFramePayload(const QuicStreamFrame& frame,
                                bool last_frame_in_packet,
                                QuicDataWriter* builder);
  bool AppendAckFramePayloadAndTypeByte(const QuicPacketHeader& header,
                                        const QuicAckFrame& frame,
                                        QuicDataWriter* builder);
  bool AppendQuicCongestionFeedbackFramePayload(
      const QuicCongestionFeedbackFrame& frame,
      QuicDataWriter* builder);
  bool AppendRstStreamFramePayload(const QuicRstStreamFrame& frame,
                                   QuicDataWriter* builder);
  bool AppendConnectionCloseFramePayload(
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* builder);
  bool AppendGoAwayFramePayload(const QuicGoAwayFrame& frame,
                                QuicDataWriter* writer);
  bool RaiseError(QuicErrorCode error);

  void set_error(QuicErrorCode error) {
    error_ = error;
  }

  void set_detailed_error(const char* error) {
    detailed_error_ = error;
  }

  std::string detailed_error_;
  scoped_ptr<QuicDataReader> reader_;
  QuicFramerVisitorInterface* visitor_;
  QuicFecBuilderInterface* fec_builder_;
  QuicReceivedEntropyHashCalculatorInterface* entropy_calculator_;
  QuicErrorCode error_;
  // Updated by ProcessPacketHeader when it succeeds.
  QuicPacketSequenceNumber last_sequence_number_;
  // Updated by WritePacketHeader.
  QuicGuid last_serialized_guid_;
  // Buffer containing decrypted payload data during parsing.
  scoped_ptr<QuicData> decrypted_;
  // Version of the protocol being used.
  QuicVersion quic_version_;
  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary).
  QuicVersionVector supported_versions_;
  // Primary decrypter used to decrypt packets during parsing.
  scoped_ptr<QuicDecrypter> decrypter_;
  // Alternative decrypter that can also be used to decrypt packets.
  scoped_ptr<QuicDecrypter> alternative_decrypter_;
  // alternative_decrypter_latch_is true if, when |alternative_decrypter_|
  // successfully decrypts a packet, we should install it as the only
  // decrypter.
  bool alternative_decrypter_latch_;
  // Encrypters used to encrypt packets via EncryptPacket().
  scoped_ptr<QuicEncrypter> encrypter_[NUM_ENCRYPTION_LEVELS];
  // Tracks if the framer is being used by the entity that received the
  // connection or the entity that initiated it.
  bool is_server_;
  // The time this frames was created.  Time written to the wire will be
  // written as a delta from this value.
  QuicTime creation_time_;

  DISALLOW_COPY_AND_ASSIGN(QuicFramer);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FRAMER_H_
