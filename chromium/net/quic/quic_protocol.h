// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROTOCOL_H_
#define NET_QUIC_QUIC_PROTOCOL_H_

#include <stddef.h>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/iovector.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_time.h"

namespace net {

using ::operator<<;

class QuicAckNotifier;
class QuicPacket;
struct QuicPacketHeader;

typedef uint64 QuicGuid;
typedef uint32 QuicStreamId;
typedef uint64 QuicStreamOffset;
typedef uint64 QuicPacketSequenceNumber;
typedef QuicPacketSequenceNumber QuicFecGroupNumber;
typedef uint64 QuicPublicResetNonceProof;
typedef uint8 QuicPacketEntropyHash;
typedef uint32 QuicHeaderId;
// QuicTag is the type of a tag in the wire protocol.
typedef uint32 QuicTag;
typedef std::vector<QuicTag> QuicTagVector;
typedef uint32 QuicPriority;

// TODO(rch): Consider Quic specific names for these constants.
// Default and initial maximum size in bytes of a QUIC packet.
const QuicByteCount kDefaultMaxPacketSize = 1200;
// The maximum packet size of any QUIC packet, based on ethernet's max size,
// minus the IP and UDP headers. IPv6 has a 40 byte header, UPD adds an
// additional 8 bytes.  This is a total overhead of 48 bytes.  Ethernet's
// max packet size is 1500 bytes,  1500 - 48 = 1452.
const QuicByteCount kMaxPacketSize = 1452;

// Maximum size of the initial congestion window in packets.
const size_t kDefaultInitialWindow = 10;
// TODO(ianswett): Temporarily changed to 10 due to a large number of clients
// mistakenly negotiating 100 initially and suffering the consequences.
const size_t kMaxInitialWindow = 10;

// Maximum size of the congestion window, in packets, for TCP congestion control
// algorithms.
const size_t kMaxTcpCongestionWindow = 200;

// Don't allow a client to suggest an RTT longer than 15 seconds.
const size_t kMaxInitialRoundTripTimeUs = 15 * kNumMicrosPerSecond;

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Number of bytes reserved for public flags in the packet header.
const size_t kPublicFlagsSize = 1;
// Number of bytes reserved for version number in the packet header.
const size_t kQuicVersionSize = 4;
// Number of bytes reserved for private flags in the packet header.
const size_t kPrivateFlagsSize = 1;
// Number of bytes reserved for FEC group in the packet header.
const size_t kFecGroupSize = 1;
// Number of bytes reserved for the nonce proof in public reset packet.
const size_t kPublicResetNonceSize = 8;

// Signifies that the QuicPacket will contain version of the protocol.
const bool kIncludeVersion = true;

// Index of the first byte in a QUIC packet which is used in hash calculation.
const size_t kStartOfHashData = 0;

// Limit on the delta between stream IDs.
const QuicStreamId kMaxStreamIdDelta = 100;
// Limit on the delta between header IDs.
const QuicHeaderId kMaxHeaderIdDelta = 100;

// Reserved ID for the crypto stream.
// TODO(rch): ensure that this is not usable by any other streams.
const QuicStreamId kCryptoStreamId = 1;

// This is the default network timeout a for connection till the crypto
// handshake succeeds and the negotiated timeout from the handshake is received.
const int64 kDefaultInitialTimeoutSecs = 120;  // 2 mins.
const int64 kDefaultTimeoutSecs = 60 * 10;  // 10 minutes.
const int64 kDefaultMaxTimeForCryptoHandshakeSecs = 5;  // 5 secs.

// We define an unsigned 16-bit floating point value, inspired by IEEE floats
// (http://en.wikipedia.org/wiki/Half_precision_floating-point_format),
// with 5-bit exponent (bias 1), 11-bit mantissa (effective 12 with hidden
// bit) and denormals, but without signs, transfinites or fractions. Wire format
// 16 bits (little-endian byte order) are split into exponent (high 5) and
// mantissa (low 11) and decoded as:
//   uint64 value;
//   if (exponent == 0) value = mantissa;
//   else value = (mantissa | 1 << 11) << (exponent - 1)
const int kUFloat16ExponentBits = 5;
const int kUFloat16MaxExponent = (1 << kUFloat16ExponentBits) - 2;  // 30
const int kUFloat16MantissaBits = 16 - kUFloat16ExponentBits;  // 11
const int kUFloat16MantissaEffectiveBits = kUFloat16MantissaBits + 1;  // 12
const uint64 kUFloat16MaxValue =  // 0x3FFC0000000
    ((GG_UINT64_C(1) << kUFloat16MantissaEffectiveBits) - 1) <<
    kUFloat16MaxExponent;

enum TransmissionType {
  NOT_RETRANSMISSION,
  NACK_RETRANSMISSION,
  RTO_RETRANSMISSION,
};

enum RetransmissionType {
  INITIAL_ENCRYPTION_ONLY,
  ALL_PACKETS
};

enum HasRetransmittableData {
  NO_RETRANSMITTABLE_DATA,
  HAS_RETRANSMITTABLE_DATA,
};

enum IsHandshake {
  NOT_HANDSHAKE,
  IS_HANDSHAKE
};

enum QuicFrameType {
  PADDING_FRAME = 0,
  RST_STREAM_FRAME,
  CONNECTION_CLOSE_FRAME,
  GOAWAY_FRAME,
  STREAM_FRAME,
  ACK_FRAME,
  CONGESTION_FEEDBACK_FRAME,
  NUM_FRAME_TYPES
};

enum QuicGuidLength {
  PACKET_0BYTE_GUID = 0,
  PACKET_1BYTE_GUID = 1,
  PACKET_4BYTE_GUID = 4,
  PACKET_8BYTE_GUID = 8
};

enum InFecGroup {
  NOT_IN_FEC_GROUP,
  IN_FEC_GROUP,
};

enum QuicSequenceNumberLength {
  PACKET_1BYTE_SEQUENCE_NUMBER = 1,
  PACKET_2BYTE_SEQUENCE_NUMBER = 2,
  PACKET_4BYTE_SEQUENCE_NUMBER = 4,
  PACKET_6BYTE_SEQUENCE_NUMBER = 6
};

// Used to indicate a QuicSequenceNumberLength using two flag bits.
enum QuicSequenceNumberLengthFlags {
  PACKET_FLAGS_1BYTE_SEQUENCE = 0,  // 00
  PACKET_FLAGS_2BYTE_SEQUENCE = 1,  // 01
  PACKET_FLAGS_4BYTE_SEQUENCE = 1 << 1,  // 10
  PACKET_FLAGS_6BYTE_SEQUENCE = 1 << 1 | 1,  // 11
};

// The public flags are specified in one byte.
enum QuicPacketPublicFlags {
  PACKET_PUBLIC_FLAGS_NONE = 0,

  // Bit 0: Does the packet header contains version info?
  PACKET_PUBLIC_FLAGS_VERSION = 1 << 0,

  // Bit 1: Is this packet a public reset packet?
  PACKET_PUBLIC_FLAGS_RST = 1 << 1,

  // Bits 2 and 3 specify the length of the GUID as follows:
  // ----00--: 0 bytes
  // ----01--: 1 byte
  // ----10--: 4 bytes
  // ----11--: 8 bytes
  PACKET_PUBLIC_FLAGS_0BYTE_GUID = 0,
  PACKET_PUBLIC_FLAGS_1BYTE_GUID = 1 << 2,
  PACKET_PUBLIC_FLAGS_4BYTE_GUID = 1 << 3,
  PACKET_PUBLIC_FLAGS_8BYTE_GUID = 1 << 3 | 1 << 2,

  // Bits 4 and 5 describe the packet sequence number length as follows:
  // --00----: 1 byte
  // --01----: 2 bytes
  // --10----: 4 bytes
  // --11----: 6 bytes
  PACKET_PUBLIC_FLAGS_1BYTE_SEQUENCE = PACKET_FLAGS_1BYTE_SEQUENCE << 4,
  PACKET_PUBLIC_FLAGS_2BYTE_SEQUENCE = PACKET_FLAGS_2BYTE_SEQUENCE << 4,
  PACKET_PUBLIC_FLAGS_4BYTE_SEQUENCE = PACKET_FLAGS_4BYTE_SEQUENCE << 4,
  PACKET_PUBLIC_FLAGS_6BYTE_SEQUENCE = PACKET_FLAGS_6BYTE_SEQUENCE << 4,

  // All bits set (bits 6 and 7 are not currently used): 00111111
  PACKET_PUBLIC_FLAGS_MAX = (1 << 6) - 1
};

// The private flags are specified in one byte.
enum QuicPacketPrivateFlags {
  PACKET_PRIVATE_FLAGS_NONE = 0,

  // Bit 0: Does this packet contain an entropy bit?
  PACKET_PRIVATE_FLAGS_ENTROPY = 1 << 0,

  // Bit 1: Payload is part of an FEC group?
  PACKET_PRIVATE_FLAGS_FEC_GROUP = 1 << 1,

  // Bit 2: Payload is FEC as opposed to frames?
  PACKET_PRIVATE_FLAGS_FEC = 1 << 2,

  // All bits set (bits 3-7 are not currently used): 00000111
  PACKET_PRIVATE_FLAGS_MAX = (1 << 3) - 1
};

// The available versions of QUIC. Guaranteed that the integer value of the enum
// will match the version number.
// When adding a new version to this enum you should add it to
// kSupportedQuicVersions (if appropriate), and also add a new case to the
// helper methods QuicVersionToQuicTag, QuicTagToQuicVersion, and
// QuicVersionToString.
enum QuicVersion {
  // Special case to indicate unknown/unsupported QUIC version.
  QUIC_VERSION_UNSUPPORTED = 0,

  QUIC_VERSION_12 = 12,  // Current version.
};

// This vector contains QUIC versions which we currently support.
// This should be ordered such that the highest supported version is the first
// element, with subsequent elements in descending order (versions can be
// skipped as necessary).
//
// IMPORTANT: if you are addding to this list, follow the instructions at
// http://sites/quic/adding-and-removing-versions
static const QuicVersion kSupportedQuicVersions[] = {QUIC_VERSION_12};

typedef std::vector<QuicVersion> QuicVersionVector;

// Returns a vector of QUIC versions in kSupportedQuicVersions.
NET_EXPORT_PRIVATE QuicVersionVector QuicSupportedVersions();

// QuicTag is written to and read from the wire, but we prefer to use
// the more readable QuicVersion at other levels.
// Helper function which translates from a QuicVersion to a QuicTag. Returns 0
// if QuicVersion is unsupported.
NET_EXPORT_PRIVATE QuicTag QuicVersionToQuicTag(const QuicVersion version);

// Returns appropriate QuicVersion from a QuicTag.
// Returns QUIC_VERSION_UNSUPPORTED if version_tag cannot be understood.
NET_EXPORT_PRIVATE QuicVersion QuicTagToQuicVersion(const QuicTag version_tag);

// Helper function which translates from a QuicVersion to a string.
// Returns strings corresponding to enum names (e.g. QUIC_VERSION_6).
NET_EXPORT_PRIVATE std::string QuicVersionToString(const QuicVersion version);

// Returns comma separated list of string representations of QuicVersion enum
// values in the supplied |versions| vector.
NET_EXPORT_PRIVATE std::string QuicVersionVectorToString(
    const QuicVersionVector& versions);

// Version and Crypto tags are written to the wire with a big-endian
// representation of the name of the tag.  For example
// the client hello tag (CHLO) will be written as the
// following 4 bytes: 'C' 'H' 'L' 'O'.  Since it is
// stored in memory as a little endian uint32, we need
// to reverse the order of the bytes.

// MakeQuicTag returns a value given the four bytes. For example:
//   MakeQuicTag('C', 'H', 'L', 'O');
NET_EXPORT_PRIVATE QuicTag MakeQuicTag(char a, char b, char c, char d);

// Size in bytes of the data or fec packet header.
NET_EXPORT_PRIVATE size_t GetPacketHeaderSize(QuicPacketHeader header);

NET_EXPORT_PRIVATE size_t GetPacketHeaderSize(
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group);

// Size in bytes of the public reset packet.
NET_EXPORT_PRIVATE size_t GetPublicResetPacketSize();

// Index of the first byte in a QUIC packet of FEC protected data.
NET_EXPORT_PRIVATE size_t GetStartOfFecProtectedData(
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length);
// Index of the first byte in a QUIC packet of encrypted data.
NET_EXPORT_PRIVATE size_t GetStartOfEncryptedData(
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length);

enum QuicRstStreamErrorCode {
  QUIC_STREAM_NO_ERROR = 0,

  // There was some error which halted stream processing.
  QUIC_ERROR_PROCESSING_STREAM,
  // We got two fin or reset offsets which did not match.
  QUIC_MULTIPLE_TERMINATION_OFFSETS,
  // We got bad payload and can not respond to it at the protocol level.
  QUIC_BAD_APPLICATION_PAYLOAD,
  // Stream closed due to connection error. No reset frame is sent when this
  // happens.
  QUIC_STREAM_CONNECTION_ERROR,
  // GoAway frame sent. No more stream can be created.
  QUIC_STREAM_PEER_GOING_AWAY,
  // The stream has been cancelled.
  QUIC_STREAM_CANCELLED,

  // No error. Used as bound while iterating.
  QUIC_STREAM_LAST_ERROR,
};

// These values must remain stable as they are uploaded to UMA histograms.
// To add a new error code, use the current value of QUIC_LAST_ERROR and
// increment QUIC_LAST_ERROR.
enum QuicErrorCode {
  QUIC_NO_ERROR = 0,

  // Connection has reached an invalid state.
  QUIC_INTERNAL_ERROR = 1,
  // There were data frames after the a fin or reset.
  QUIC_STREAM_DATA_AFTER_TERMINATION = 2,
  // Control frame is malformed.
  QUIC_INVALID_PACKET_HEADER = 3,
  // Frame data is malformed.
  QUIC_INVALID_FRAME_DATA = 4,
  // The packet contained no payload.
  QUIC_MISSING_PAYLOAD = 48,
  // FEC data is malformed.
  QUIC_INVALID_FEC_DATA = 5,
  // STREAM frame data is malformed.
  QUIC_INVALID_STREAM_DATA = 46,
  // RST_STREAM frame data is malformed.
  QUIC_INVALID_RST_STREAM_DATA = 6,
  // CONNECTION_CLOSE frame data is malformed.
  QUIC_INVALID_CONNECTION_CLOSE_DATA = 7,
  // GOAWAY frame data is malformed.
  QUIC_INVALID_GOAWAY_DATA = 8,
  // ACK frame data is malformed.
  QUIC_INVALID_ACK_DATA = 9,
  // CONGESTION_FEEDBACK frame data is malformed.
  QUIC_INVALID_CONGESTION_FEEDBACK_DATA = 47,
  // Version negotiation packet is malformed.
  QUIC_INVALID_VERSION_NEGOTIATION_PACKET = 10,
  // Public RST packet is malformed.
  QUIC_INVALID_PUBLIC_RST_PACKET = 11,
  // There was an error decrypting.
  QUIC_DECRYPTION_FAILURE = 12,
  // There was an error encrypting.
  QUIC_ENCRYPTION_FAILURE = 13,
  // The packet exceeded kMaxPacketSize.
  QUIC_PACKET_TOO_LARGE = 14,
  // Data was sent for a stream which did not exist.
  QUIC_PACKET_FOR_NONEXISTENT_STREAM = 15,
  // The peer is going away.  May be a client or server.
  QUIC_PEER_GOING_AWAY = 16,
  // A stream ID was invalid.
  QUIC_INVALID_STREAM_ID = 17,
  // A priority was invalid.
  QUIC_INVALID_PRIORITY = 49,
  // Too many streams already open.
  QUIC_TOO_MANY_OPEN_STREAMS = 18,
  // Received public reset for this connection.
  QUIC_PUBLIC_RESET = 19,
  // Invalid protocol version.
  QUIC_INVALID_VERSION = 20,
  // Stream reset before headers decompressed.
  QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED = 21,
  // The Header ID for a stream was too far from the previous.
  QUIC_INVALID_HEADER_ID = 22,
  // Negotiable parameter received during handshake had invalid value.
  QUIC_INVALID_NEGOTIATED_VALUE = 23,
  // There was an error decompressing data.
  QUIC_DECOMPRESSION_FAILURE = 24,
  // We hit our prenegotiated (or default) timeout
  QUIC_CONNECTION_TIMED_OUT = 25,
  // There was an error encountered migrating addresses
  QUIC_ERROR_MIGRATING_ADDRESS = 26,
  // There was an error while writing to the socket.
  QUIC_PACKET_WRITE_ERROR = 27,
  // There was an error while reading from the socket.
  QUIC_PACKET_READ_ERROR = 51,
  // We received a STREAM_FRAME with no data and no fin flag set.
  QUIC_INVALID_STREAM_FRAME = 50,


  // Crypto errors.

  // Hanshake failed.
  QUIC_HANDSHAKE_FAILED = 28,
  // Handshake message contained out of order tags.
  QUIC_CRYPTO_TAGS_OUT_OF_ORDER = 29,
  // Handshake message contained too many entries.
  QUIC_CRYPTO_TOO_MANY_ENTRIES = 30,
  // Handshake message contained an invalid value length.
  QUIC_CRYPTO_INVALID_VALUE_LENGTH = 31,
  // A crypto message was received after the handshake was complete.
  QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE = 32,
  // A crypto message was received with an illegal message tag.
  QUIC_INVALID_CRYPTO_MESSAGE_TYPE = 33,
  // A crypto message was received with an illegal parameter.
  QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER = 34,
  // An invalid channel id signature was supplied.
  QUIC_INVALID_CHANNEL_ID_SIGNATURE = 52,
  // A crypto message was received with a mandatory parameter missing.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND = 35,
  // A crypto message was received with a parameter that has no overlap
  // with the local parameter.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP = 36,
  // A crypto message was received that contained a parameter with too few
  // values.
  QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND = 37,
  // An internal error occured in crypto processing.
  QUIC_CRYPTO_INTERNAL_ERROR = 38,
  // A crypto handshake message specified an unsupported version.
  QUIC_CRYPTO_VERSION_NOT_SUPPORTED = 39,
  // There was no intersection between the crypto primitives supported by the
  // peer and ourselves.
  QUIC_CRYPTO_NO_SUPPORT = 40,
  // The server rejected our client hello messages too many times.
  QUIC_CRYPTO_TOO_MANY_REJECTS = 41,
  // The client rejected the server's certificate chain or signature.
  QUIC_PROOF_INVALID = 42,
  // A crypto message was received with a duplicate tag.
  QUIC_CRYPTO_DUPLICATE_TAG = 43,
  // A crypto message was received with the wrong encryption level (i.e. it
  // should have been encrypted but was not.)
  QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT = 44,
  // The server config for a server has expired.
  QUIC_CRYPTO_SERVER_CONFIG_EXPIRED = 45,
  // We failed to setup the symmetric keys for a connection.
  QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED = 53,
  // A handshake message arrived, but we are still validating the
  // previous handshake message.
  QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO = 54,
  // This connection involved a version negotiation which appears to have been
  // tampered with.
  QUIC_VERSION_NEGOTIATION_MISMATCH = 55,

  // No error. Used as bound while iterating.
  QUIC_LAST_ERROR = 56,
};

struct NET_EXPORT_PRIVATE QuicPacketPublicHeader {
  QuicPacketPublicHeader();
  explicit QuicPacketPublicHeader(const QuicPacketPublicHeader& other);
  ~QuicPacketPublicHeader();

  // Universal header. All QuicPacket headers will have a guid and public flags.
  QuicGuid guid;
  QuicGuidLength guid_length;
  bool reset_flag;
  bool version_flag;
  QuicSequenceNumberLength sequence_number_length;
  QuicVersionVector versions;
};

// Header for Data or FEC packets.
struct NET_EXPORT_PRIVATE QuicPacketHeader {
  QuicPacketHeader();
  explicit QuicPacketHeader(const QuicPacketPublicHeader& header);

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicPacketHeader& s);

  QuicPacketPublicHeader public_header;
  bool fec_flag;
  bool entropy_flag;
  QuicPacketEntropyHash entropy_hash;
  QuicPacketSequenceNumber packet_sequence_number;
  InFecGroup is_in_fec_group;
  QuicFecGroupNumber fec_group;
};

struct NET_EXPORT_PRIVATE QuicPublicResetPacket {
  QuicPublicResetPacket() {}
  explicit QuicPublicResetPacket(const QuicPacketPublicHeader& header)
      : public_header(header) {}
  QuicPacketPublicHeader public_header;
  QuicPacketSequenceNumber rejected_sequence_number;
  QuicPublicResetNonceProof nonce_proof;
};

enum QuicVersionNegotiationState {
  START_NEGOTIATION = 0,
  // Server-side this implies we've sent a version negotiation packet and are
  // waiting on the client to select a compatible version.  Client-side this
  // implies we've gotten a version negotiation packet, are retransmitting the
  // initial packets with a supported version and are waiting for our first
  // packet from the server.
  NEGOTIATION_IN_PROGRESS,
  // This indicates this endpoint has received a packet from the peer with a
  // version this endpoint supports.  Version negotiation is complete, and the
  // version number will no longer be sent with future packets.
  NEGOTIATED_VERSION
};

typedef QuicPacketPublicHeader QuicVersionNegotiationPacket;

// A padding frame contains no payload.
struct NET_EXPORT_PRIVATE QuicPaddingFrame {
};

struct NET_EXPORT_PRIVATE QuicStreamFrame {
  QuicStreamFrame();
  QuicStreamFrame(const QuicStreamFrame& frame);
  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  QuicStreamOffset offset,
                  IOVector data);

  // Returns a copy of the IOVector |data| as a heap-allocated string.
  // Caller must take ownership of the returned string.
  std::string* GetDataAsString() const;

  QuicStreamId stream_id;
  bool fin;
  QuicStreamOffset offset;  // Location of this data in the stream.
  IOVector data;

  // If this is set, then when this packet is ACKed the AckNotifier will be
  // informed.
  QuicAckNotifier* notifier;
};

// TODO(ianswett): Re-evaluate the trade-offs of hash_set vs set when framing
// is finalized.
typedef std::set<QuicPacketSequenceNumber> SequenceNumberSet;
// TODO(pwestin): Add a way to enforce the max size of this map.
typedef std::map<QuicPacketSequenceNumber, QuicTime> TimeMap;

struct NET_EXPORT_PRIVATE ReceivedPacketInfo {
  ReceivedPacketInfo();
  ~ReceivedPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const ReceivedPacketInfo& s);

  // Entropy hash of all packets up to largest observed not including missing
  // packets.
  QuicPacketEntropyHash entropy_hash;

  // The highest packet sequence number we've observed from the peer.
  //
  // In general, this should be the largest packet number we've received.  In
  // the case of truncated acks, we may have to advertise a lower "upper bound"
  // than largest received, to avoid implicitly acking missing packets that
  // don't fit in the missing packet list due to size limitations.  In this
  // case, largest_observed may be a packet which is also in the missing packets
  // list.
  QuicPacketSequenceNumber largest_observed;

  // Time elapsed since largest_observed was received until this Ack frame was
  // sent.
  QuicTime::Delta delta_time_largest_observed;

  // TODO(satyamshekhar): Can be optimized using an interval set like data
  // structure.
  // The set of packets which we're expecting and have not received.
  SequenceNumberSet missing_packets;

  // Whether the ack had to be truncated when sent.
  bool is_truncated;
};

// True if the sequence number is greater than largest_observed or is listed
// as missing.
// Always returns false for sequence numbers less than least_unacked.
bool NET_EXPORT_PRIVATE IsAwaitingPacket(
    const ReceivedPacketInfo& received_info,
    QuicPacketSequenceNumber sequence_number);

// Inserts missing packets between [lower, higher).
void NET_EXPORT_PRIVATE InsertMissingPacketsBetween(
    ReceivedPacketInfo* received_info,
    QuicPacketSequenceNumber lower,
    QuicPacketSequenceNumber higher);

struct NET_EXPORT_PRIVATE SentPacketInfo {
  SentPacketInfo();
  ~SentPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const SentPacketInfo& s);

  // Entropy hash of all packets up to, but not including, the least unacked
  // packet.
  QuicPacketEntropyHash entropy_hash;
  // The lowest packet we've sent which is unacked, and we expect an ack for.
  QuicPacketSequenceNumber least_unacked;
};

struct NET_EXPORT_PRIVATE QuicAckFrame {
  QuicAckFrame() {}
  // Testing convenience method to construct a QuicAckFrame with all packets
  // from least_unacked to largest_observed acked.
  QuicAckFrame(QuicPacketSequenceNumber largest_observed,
               QuicTime largest_observed_receive_time,
               QuicPacketSequenceNumber least_unacked);

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicAckFrame& s);

  SentPacketInfo sent_info;
  ReceivedPacketInfo received_info;
};

// Defines for all types of congestion feedback that will be negotiated in QUIC,
// kTCP MUST be supported by all QUIC implementations to guarantee 100%
// compatibility.
enum CongestionFeedbackType {
  kTCP,  // Used to mimic TCP.
  kInterArrival,  // Use additional inter arrival information.
  kFixRate,  // Provided for testing.
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageTCP {
  uint16 accumulated_number_of_lost_packets;
  QuicByteCount receive_window;
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageInterArrival {
  CongestionFeedbackMessageInterArrival();
  ~CongestionFeedbackMessageInterArrival();
  uint16 accumulated_number_of_lost_packets;
  // The set of received packets since the last feedback was sent, along with
  // their arrival times.
  TimeMap received_packet_times;
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageFixRate {
  CongestionFeedbackMessageFixRate();
  QuicBandwidth bitrate;
};

struct NET_EXPORT_PRIVATE QuicCongestionFeedbackFrame {
  QuicCongestionFeedbackFrame();
  ~QuicCongestionFeedbackFrame();

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicCongestionFeedbackFrame& c);

  CongestionFeedbackType type;
  // This should really be a union, but since the inter arrival struct
  // is non-trivial, C++ prohibits it.
  CongestionFeedbackMessageTCP tcp;
  CongestionFeedbackMessageInterArrival inter_arrival;
  CongestionFeedbackMessageFixRate fix_rate;
};

struct NET_EXPORT_PRIVATE QuicRstStreamFrame {
  QuicRstStreamFrame() {}
  QuicRstStreamFrame(QuicStreamId stream_id, QuicRstStreamErrorCode error_code)
      : stream_id(stream_id), error_code(error_code) {
    DCHECK_LE(error_code, std::numeric_limits<uint8>::max());
  }

  QuicStreamId stream_id;
  QuicRstStreamErrorCode error_code;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicConnectionCloseFrame {
  QuicErrorCode error_code;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicGoAwayFrame {
  QuicGoAwayFrame() {}
  QuicGoAwayFrame(QuicErrorCode error_code,
                  QuicStreamId last_good_stream_id,
                  const std::string& reason);

  QuicErrorCode error_code;
  QuicStreamId last_good_stream_id;
  std::string reason_phrase;
};

// EncryptionLevel enumerates the stages of encryption that a QUIC connection
// progresses through. When retransmitting a packet, the encryption level needs
// to be specified so that it is retransmitted at a level which the peer can
// understand.
enum EncryptionLevel {
  ENCRYPTION_NONE = 0,
  ENCRYPTION_INITIAL = 1,
  ENCRYPTION_FORWARD_SECURE = 2,

  NUM_ENCRYPTION_LEVELS,
};

struct NET_EXPORT_PRIVATE QuicFrame {
  QuicFrame() {}
  explicit QuicFrame(QuicPaddingFrame* padding_frame)
      : type(PADDING_FRAME),
        padding_frame(padding_frame) {
  }
  explicit QuicFrame(QuicStreamFrame* stream_frame)
      : type(STREAM_FRAME),
        stream_frame(stream_frame) {
  }
  explicit QuicFrame(QuicAckFrame* frame)
      : type(ACK_FRAME),
        ack_frame(frame) {
  }
  explicit QuicFrame(QuicCongestionFeedbackFrame* frame)
      : type(CONGESTION_FEEDBACK_FRAME),
        congestion_feedback_frame(frame) {
  }
  explicit QuicFrame(QuicRstStreamFrame* frame)
      : type(RST_STREAM_FRAME),
        rst_stream_frame(frame) {
  }
  explicit QuicFrame(QuicConnectionCloseFrame* frame)
      : type(CONNECTION_CLOSE_FRAME),
        connection_close_frame(frame) {
  }
  explicit QuicFrame(QuicGoAwayFrame* frame)
      : type(GOAWAY_FRAME),
        goaway_frame(frame) {
  }

  QuicFrameType type;
  union {
    QuicPaddingFrame* padding_frame;
    QuicStreamFrame* stream_frame;
    QuicAckFrame* ack_frame;
    QuicCongestionFeedbackFrame* congestion_feedback_frame;
    QuicRstStreamFrame* rst_stream_frame;
    QuicConnectionCloseFrame* connection_close_frame;
    QuicGoAwayFrame* goaway_frame;
  };
};

typedef std::vector<QuicFrame> QuicFrames;

struct NET_EXPORT_PRIVATE QuicFecData {
  QuicFecData();

  // The FEC group number is also the sequence number of the first
  // FEC protected packet.  The last protected packet's sequence number will
  // be one less than the sequence number of the FEC packet.
  QuicFecGroupNumber fec_group;
  base::StringPiece redundancy;
};

class NET_EXPORT_PRIVATE QuicData {
 public:
  QuicData(const char* buffer, size_t length)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(false) {}

  QuicData(char* buffer, size_t length, bool owns_buffer)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(owns_buffer) {}

  virtual ~QuicData();

  base::StringPiece AsStringPiece() const {
    return base::StringPiece(data(), length());
  }

  const char* data() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  const char* buffer_;
  size_t length_;
  bool owns_buffer_;

  DISALLOW_COPY_AND_ASSIGN(QuicData);
};

class NET_EXPORT_PRIVATE QuicPacket : public QuicData {
 public:
  static QuicPacket* NewDataPacket(
      char* buffer,
      size_t length,
      bool owns_buffer,
      QuicGuidLength guid_length,
      bool includes_version,
      QuicSequenceNumberLength sequence_number_length) {
    return new QuicPacket(buffer, length, owns_buffer, guid_length,
                          includes_version, sequence_number_length, false);
  }

  static QuicPacket* NewFecPacket(
      char* buffer,
      size_t length,
      bool owns_buffer,
      QuicGuidLength guid_length,
      bool includes_version,
      QuicSequenceNumberLength sequence_number_length) {
    return new QuicPacket(buffer, length, owns_buffer, guid_length,
                          includes_version, sequence_number_length, true);
  }

  base::StringPiece FecProtectedData() const;
  base::StringPiece AssociatedData() const;
  base::StringPiece BeforePlaintext() const;
  base::StringPiece Plaintext() const;

  bool is_fec_packet() const { return is_fec_packet_; }

  char* mutable_data() { return buffer_; }

 private:
  QuicPacket(char* buffer,
             size_t length,
             bool owns_buffer,
             QuicGuidLength guid_length,
             bool includes_version,
             QuicSequenceNumberLength sequence_number_length,
             bool is_fec_packet)
      : QuicData(buffer, length, owns_buffer),
        buffer_(buffer),
        is_fec_packet_(is_fec_packet),
        guid_length_(guid_length),
        includes_version_(includes_version),
        sequence_number_length_(sequence_number_length) {}

  char* buffer_;
  const bool is_fec_packet_;
  const QuicGuidLength guid_length_;
  const bool includes_version_;
  const QuicSequenceNumberLength sequence_number_length_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacket);
};

class NET_EXPORT_PRIVATE QuicEncryptedPacket : public QuicData {
 public:
  QuicEncryptedPacket(const char* buffer, size_t length)
      : QuicData(buffer, length) {}

  QuicEncryptedPacket(char* buffer, size_t length, bool owns_buffer)
      : QuicData(buffer, length, owns_buffer) {}

  // Clones the packet into a new packet which owns the buffer.
  QuicEncryptedPacket* Clone() const;

  // By default, gtest prints the raw bytes of an object. The bool data
  // member (in the base class QuicData) causes this object to have padding
  // bytes, which causes the default gtest object printer to read
  // uninitialize memory. So we need to teach gtest how to print this object.
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicEncryptedPacket& s);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEncryptedPacket);
};

class NET_EXPORT_PRIVATE RetransmittableFrames {
 public:
  RetransmittableFrames();
  ~RetransmittableFrames();

  // Allocates a local copy of the referenced StringPiece has QuicStreamFrame
  // use it.
  // Takes ownership of |stream_frame|.
  const QuicFrame& AddStreamFrame(QuicStreamFrame* stream_frame);
  // Takes ownership of the frame inside |frame|.
  const QuicFrame& AddNonStreamFrame(const QuicFrame& frame);
  const QuicFrames& frames() const { return frames_; }

  void set_encryption_level(EncryptionLevel level);
  EncryptionLevel encryption_level() const {
    return encryption_level_;
  }

 private:
  QuicFrames frames_;
  EncryptionLevel encryption_level_;
  // Data referenced by the StringPiece of a QuicStreamFrame.
  std::vector<std::string*> stream_data_;

  DISALLOW_COPY_AND_ASSIGN(RetransmittableFrames);
};

struct NET_EXPORT_PRIVATE SerializedPacket {
  SerializedPacket(QuicPacketSequenceNumber sequence_number,
                   QuicSequenceNumberLength sequence_number_length,
                   QuicPacket* packet,
                   QuicPacketEntropyHash entropy_hash,
                   RetransmittableFrames* retransmittable_frames);
  ~SerializedPacket();

  QuicPacketSequenceNumber sequence_number;
  QuicSequenceNumberLength sequence_number_length;
  QuicPacket* packet;
  QuicPacketEntropyHash entropy_hash;
  RetransmittableFrames* retransmittable_frames;

  // If set, these will be called when this packet is ACKed by the peer.
  std::set<QuicAckNotifier*> notifiers;
};

// A struct for functions which consume data payloads and fins.
struct QuicConsumedData {
  QuicConsumedData(size_t bytes_consumed, bool fin_consumed)
      : bytes_consumed(bytes_consumed),
        fin_consumed(fin_consumed) {}
  // By default, gtest prints the raw bytes of an object. The bool data
  // member causes this object to have padding bytes, which causes the
  // default gtest object printer to read uninitialize memory. So we need
  // to teach gtest how to print this object.
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicConsumedData& s);

  // How many bytes were consumed.
  size_t bytes_consumed;

  // True if an incoming fin was consumed.
  bool fin_consumed;
};

enum WriteStatus {
  WRITE_STATUS_OK,
  WRITE_STATUS_BLOCKED,
  WRITE_STATUS_ERROR,
};

// A struct used to return the result of write calls including either the number
// of bytes written or the error code, depending upon the status.
struct NET_EXPORT_PRIVATE WriteResult {
  WriteResult(WriteStatus status, int bytes_written_or_error_code) :
    status(status), bytes_written(bytes_written_or_error_code) {
  }

  WriteStatus status;
  union {
    int bytes_written;  // only valid when status is OK
    int error_code;  // only valid when status is ERROR
  };
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROTOCOL_H_
