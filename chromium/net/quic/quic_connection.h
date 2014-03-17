// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The entity that handles framing writes for a Quic client or server.
// Each QuicSession will have a connection associated with it.
//
// On the server side, the Dispatcher handles the raw reads, and hands off
// packets via ProcessUdpPacket for framing and processing.
//
// On the client side, the Connection handles the raw reads, as well as the
// processing.
//
// Note: this class is not thread-safe.

#ifndef NET_QUIC_QUIC_CONNECTION_H_
#define NET_QUIC_QUIC_CONNECTION_H_

#include <stddef.h>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/logging.h"
#include "net/base/iovec.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/iovector.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_alarm.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_packet_generator.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_received_packet_manager.h"
#include "net/quic/quic_sent_entropy_manager.h"
#include "net/quic/quic_sent_packet_manager.h"

NET_EXPORT_PRIVATE extern int FLAGS_fake_packet_loss_percentage;
NET_EXPORT_PRIVATE extern bool FLAGS_bundle_ack_with_outgoing_packet;

namespace net {

class QuicClock;
class QuicConfig;
class QuicConnection;
class QuicDecrypter;
class QuicEncrypter;
class QuicFecGroup;
class QuicRandom;

namespace test {
class QuicConnectionPeer;
}  // namespace test

// Class that receives callbacks from the connection when frames are received
// and when other interesting events happen.
class NET_EXPORT_PRIVATE QuicConnectionVisitorInterface {
 public:
  virtual ~QuicConnectionVisitorInterface() {}

  // A simple visitor interface for dealing with data frames.  The session
  // should determine if all frames will be accepted, and return true if so.
  // If any frames can't be processed or buffered, none of the data should
  // be used, and the callee should return false.
  virtual bool OnStreamFrames(const std::vector<QuicStreamFrame>& frames) = 0;

  // Called when the stream is reset by the peer.
  virtual void OnRstStream(const QuicRstStreamFrame& frame) = 0;

  // Called when the connection is going away according to the peer.
  virtual void OnGoAway(const QuicGoAwayFrame& frame) = 0;

  // Called when the connection is closed either locally by the framer, or
  // remotely by the peer.
  virtual void OnConnectionClosed(QuicErrorCode error,
                                  bool from_peer) = 0;

  // Called once a specific QUIC version is agreed by both endpoints.
  virtual void OnSuccessfulVersionNegotiation(const QuicVersion& version) = 0;

  // Indicates a new QuicConfig has been negotiated.
  virtual void OnConfigNegotiated() = 0;

  // Called when a blocked socket becomes writable.  If all pending bytes for
  // this visitor are consumed by the connection successfully this should
  // return true, otherwise it should return false.
  virtual bool OnCanWrite() = 0;

  // Called to ask if any handshake messages are pending in this visitor.
  virtual bool HasPendingHandshake() const = 0;
};

// Interface which gets callbacks from the QuicConnection at interesting
// points.  Implementations must not mutate the state of the connection
// as a result of these callbacks.
class NET_EXPORT_PRIVATE QuicConnectionDebugVisitorInterface
    : public QuicPacketGenerator::DebugDelegateInterface {
 public:
  virtual ~QuicConnectionDebugVisitorInterface() {}

  // Called when a packet has been sent.
  virtual void OnPacketSent(QuicPacketSequenceNumber sequence_number,
                            EncryptionLevel level,
                            const QuicEncryptedPacket& packet,
                            WriteResult result) = 0;

  // Called when the contents of a packet have been retransmitted as
  // a new packet.
  virtual void OnPacketRetransmitted(
      QuicPacketSequenceNumber old_sequence_number,
      QuicPacketSequenceNumber new_sequence_number) = 0;

  // Called when a packet has been received, but before it is
  // validated or parsed.
  virtual void OnPacketReceived(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) = 0;

  // Called when the protocol version on the received packet doensn't match
  // current protocol version of the connection.
  virtual void OnProtocolVersionMismatch(QuicVersion version) = 0;

  // Called when the complete header of a packet has been parsed.
  virtual void OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a StreamFrame has been parsed.
  virtual void OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a AckFrame has been parsed.
  virtual void OnAckFrame(const QuicAckFrame& frame) = 0;

  // Called when a CongestionFeedbackFrame has been parsed.
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when a public reset packet has been received.
  virtual void OnPublicResetPacket(const QuicPublicResetPacket& packet) = 0;

  // Called when a version negotiation packet has been received.
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) = 0;

  // Called after a packet has been successfully parsed which results
  // in the revival of a packet via FEC.
  virtual void OnRevivedPacket(const QuicPacketHeader& revived_header,
                               base::StringPiece payload) = 0;
};

class NET_EXPORT_PRIVATE QuicConnectionHelperInterface {
 public:
  virtual ~QuicConnectionHelperInterface() {}

  // Returns a QuicClock to be used for all time related functions.
  virtual const QuicClock* GetClock() const = 0;

  // Returns a QuicRandom to be used for all random number related functions.
  virtual QuicRandom* GetRandomGenerator() = 0;

  // Creates a new platform-specific alarm which will be configured to
  // notify |delegate| when the alarm fires.  Caller takes ownership
  // of the new alarm, which will not yet be "set" to fire.
  virtual QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) = 0;
};

class NET_EXPORT_PRIVATE QuicConnection
    : public QuicFramerVisitorInterface,
      public QuicBlockedWriterInterface,
      public QuicPacketGenerator::DelegateInterface,
      public QuicSentPacketManager::HelperInterface {
 public:
  enum Force {
    NO_FORCE,
    FORCE
  };

  // Constructs a new QuicConnection for the specified |guid| and |address|.
  // |helper| and |writer| must outlive this connection.
  QuicConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelperInterface* helper,
                 QuicPacketWriter* writer,
                 bool is_server,
                 const QuicVersionVector& supported_versions);
  virtual ~QuicConnection();

  // Sets connection parameters from the supplied |config|.
  void SetFromConfig(const QuicConfig& config);

  // Send the data in |data| to the peer in as few packets as possible.
  // Returns a pair with the number of bytes consumed from data, and a boolean
  // indicating if the fin bit was consumed.  This does not indicate the data
  // has been sent on the wire: it may have been turned into a packet and queued
  // if the socket was unexpectedly blocked.
  // If |delegate| is provided, then it will be informed once ACKs have been
  // received for all the packets written in this call.
  // The |delegate| is not owned by the QuicConnection and must outlive it.
  QuicConsumedData SendStreamData(QuicStreamId id,
                                  const IOVector& data,
                                  QuicStreamOffset offset,
                                  bool fin,
                                  QuicAckNotifier::DelegateInterface* delegate);

  // Send a stream reset frame to the peer.
  virtual void SendRstStream(QuicStreamId id,
                             QuicRstStreamErrorCode error);

  // Sends the connection close packet without affecting the state of the
  // connection.  This should only be called if the session is actively being
  // destroyed: otherwise call SendConnectionCloseWithDetails instead.
  virtual void SendConnectionClosePacket(QuicErrorCode error,
                                         const std::string& details);

  // Sends a connection close frame to the peer, and closes the connection by
  // calling CloseConnection(notifying the visitor as it does so).
  virtual void SendConnectionClose(QuicErrorCode error);
  virtual void SendConnectionCloseWithDetails(QuicErrorCode error,
                                              const std::string& details);
  // Notifies the visitor of the close and marks the connection as disconnected.
  virtual void CloseConnection(QuicErrorCode error, bool from_peer) OVERRIDE;
  virtual void SendGoAway(QuicErrorCode error,
                          QuicStreamId last_good_stream_id,
                          const std::string& reason);

  // Returns statistics tracked for this connection.
  const QuicConnectionStats& GetStats();

  // Processes an incoming UDP packet (consisting of a QuicEncryptedPacket) from
  // the peer.  If processing this packet permits a packet to be revived from
  // its FEC group that packet will be revived and processed.
  virtual void ProcessUdpPacket(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet);

  // QuicBlockedWriterInterface
  // Called when the underlying connection becomes writable to allow queued
  // writes to happen.  Returns false if the socket has become blocked.
  virtual bool OnCanWrite() OVERRIDE;

  // Called when a packet has been finally sent to the network.
  bool OnPacketSent(WriteResult result);

  // If the socket is not blocked, this allows queued writes to happen. Returns
  // false if the socket has become blocked.
  bool WriteIfNotBlocked();

  // Do any work which logically would be done in OnPacket but can not be
  // safely done until the packet is validated.  Returns true if the packet
  // can be handled, false otherwise.
  bool ProcessValidatedPacket();

  // The version of the protocol this connection is using.
  QuicVersion version() const { return framer_.version(); }

  // The versions of the protocol that this connection supports.
  const QuicVersionVector& supported_versions() const {
    return framer_.supported_versions();
  }

  // From QuicFramerVisitorInterface
  virtual void OnError(QuicFramer* framer) OVERRIDE;
  virtual bool OnProtocolVersionMismatch(QuicVersion received_version) OVERRIDE;
  virtual void OnPacket() OVERRIDE;
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE;
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE;
  virtual void OnRevivedPacket() OVERRIDE;
  virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE;
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual bool OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE;
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE;
  virtual void OnPacketComplete() OVERRIDE;

  // QuicPacketGenerator::DelegateInterface
  virtual bool ShouldGeneratePacket(TransmissionType transmission_type,
                                    HasRetransmittableData retransmittable,
                                    IsHandshake handshake) OVERRIDE;
  virtual QuicAckFrame* CreateAckFrame() OVERRIDE;
  virtual QuicCongestionFeedbackFrame* CreateFeedbackFrame() OVERRIDE;
  virtual bool OnSerializedPacket(const SerializedPacket& packet) OVERRIDE;

  // QuicSentPacketManager::HelperInterface
  virtual QuicPacketSequenceNumber GetNextPacketSequenceNumber() OVERRIDE;

  // Accessors
  void set_visitor(QuicConnectionVisitorInterface* visitor) {
    visitor_ = visitor;
  }
  void set_debug_visitor(QuicConnectionDebugVisitorInterface* debug_visitor) {
    debug_visitor_ = debug_visitor;
    packet_generator_.set_debug_delegate(debug_visitor);
  }
  const IPEndPoint& self_address() const { return self_address_; }
  const IPEndPoint& peer_address() const { return peer_address_; }
  QuicGuid guid() const { return guid_; }
  const QuicClock* clock() const { return clock_; }
  QuicRandom* random_generator() const { return random_generator_; }

  QuicPacketCreator::Options* options() { return packet_creator_.options(); }

  bool connected() const { return connected_; }

  // Must only be called on client connections.
  const QuicVersionVector& server_supported_versions() const {
    DCHECK(!is_server_);
    return server_supported_versions_;
  }

  size_t NumFecGroups() const { return group_map_.size(); }

  // Testing only.
  size_t NumQueuedPackets() const { return queued_packets_.size(); }

  QuicEncryptedPacket* ReleaseConnectionClosePacket() {
    return connection_close_packet_.release();
  }

  // Flush any queued frames immediately.  Preserves the batch write mode and
  // does nothing if there are no pending frames.
  void Flush();

  // Returns true if the connection has queued packets or frames.
  bool HasQueuedData() const;

  // Sets (or resets) the idle state connection timeout. Also, checks and times
  // out the connection if network timer has expired for |timeout|.
  void SetIdleNetworkTimeout(QuicTime::Delta timeout);
  // Sets (or resets) the total time delta the connection can be alive for.
  // Also, checks and times out the connection if timer has expired for
  // |timeout|. Used to limit the time a connection can be alive before crypto
  // handshake finishes.
  void SetOverallConnectionTimeout(QuicTime::Delta timeout);

  // If the connection has timed out, this will close the connection and return
  // true.  Otherwise, it will return false and will reset the timeout alarm.
  bool CheckForTimeout();

  // Sets up a packet with an QuicAckFrame and sends it out.
  void SendAck();

  // Called when an RTO fires.  Resets the retransmission alarm if there are
  // remaining unacked packets.
  void OnRetransmissionTimeout();

  // Retransmits all unacked packets with retransmittable frames if
  // |retransmission_type| is ALL_PACKETS, otherwise retransmits only initially
  // encrypted packets. Used when the negotiated protocol version is different
  // from what was initially assumed and when the visitor wants to re-transmit
  // initially encrypted packets when the initial encrypter changes.
  void RetransmitUnackedPackets(RetransmissionType retransmission_type);

  // Changes the encrypter used for level |level| to |encrypter|. The function
  // takes ownership of |encrypter|.
  void SetEncrypter(EncryptionLevel level, QuicEncrypter* encrypter);
  const QuicEncrypter* encrypter(EncryptionLevel level) const;

  // SetDefaultEncryptionLevel sets the encryption level that will be applied
  // to new packets.
  void SetDefaultEncryptionLevel(EncryptionLevel level);

  // SetDecrypter sets the primary decrypter, replacing any that already exists,
  // and takes ownership. If an alternative decrypter is in place then the
  // function DCHECKs. This is intended for cases where one knows that future
  // packets will be using the new decrypter and the previous decrypter is now
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

  bool is_server() const { return is_server_; }

  // Returns the underlying sent packet manager.
  const QuicSentPacketManager& sent_packet_manager() const {
    return sent_packet_manager_;
  }

  bool CanWrite(TransmissionType transmission_type,
                HasRetransmittableData retransmittable,
                IsHandshake handshake);

 protected:
  // Send a packet to the peer using encryption |level|. If |sequence_number|
  // is present in the |retransmission_map_|, then contents of this packet will
  // be retransmitted with a new sequence number if it's not acked by the peer.
  // Deletes |packet| via WritePacket call or transfers ownership to
  // QueuedPacket, ultimately deleted via WritePacket. Updates the
  // entropy map corresponding to |sequence_number| using |entropy_hash|.
  // |transmission_type| and |retransmittable| are supplied to the congestion
  // manager, and when |forced| is true, it bypasses the congestion manager.
  // TODO(wtc): none of the callers check the return value.
  virtual bool SendOrQueuePacket(EncryptionLevel level,
                                 const SerializedPacket& packet,
                                 TransmissionType transmission_type);

  // Writes the given packet to socket, encrypted with |level|, with the help
  // of helper. Returns true on successful write, false otherwise. However,
  // behavior is undefined if connection is not established or broken. In any
  // circumstances, a return value of true implies that |packet| has been
  // deleted and should not be accessed. If |sequence_number| is present in
  // |retransmission_map_| it also sets up retransmission of the given packet
  // in case of successful write. If |force| is FORCE, then the packet will be
  // sent immediately and the send scheduler will not be consulted.
  bool WritePacket(EncryptionLevel level,
                   QuicPacketSequenceNumber sequence_number,
                   QuicPacket* packet,
                   TransmissionType transmission_type,
                   HasRetransmittableData retransmittable,
                   IsHandshake handshake,
                   Force force);

  // Make sure an ack we got from our peer is sane.
  bool ValidateAckFrame(const QuicAckFrame& incoming_ack);

  QuicConnectionHelperInterface* helper() { return helper_; }

  // Selects and updates the version of the protocol being used by selecting a
  // version from |available_versions| which is also supported. Returns true if
  // such a version exists, false otherwise.
  bool SelectMutualVersion(const QuicVersionVector& available_versions);

  QuicFramer framer_;

 private:
  // Stores current batch state for connection, puts the connection
  // into batch mode, and destruction restores the stored batch state.
  // While the bundler is in scope, any generated frames are bundled
  // as densely as possible into packets.  In addition, this bundler
  // can be configured to ensure that an ACK frame is included in the
  // first packet created, if there's new ack information to be sent.
  class ScopedPacketBundler {
   public:
    // In addition to all outgoing frames being bundled when the
    // bundler is in scope, setting |include_ack| to true ensures that
    // an ACK frame is opportunistically bundled with the first
    // outgoing packet.
    ScopedPacketBundler(QuicConnection* connection, bool include_ack);
    ~ScopedPacketBundler();

   private:
    QuicConnection* connection_;
    bool already_in_batch_mode_;
  };

  friend class ScopedPacketBundler;
  friend class test::QuicConnectionPeer;

  // Packets which have not been written to the wire.
  // Owns the QuicPacket* packet.
  struct QueuedPacket {
    QueuedPacket(QuicPacketSequenceNumber sequence_number,
                 QuicPacket* packet,
                 EncryptionLevel level,
                 TransmissionType transmission_type,
                 HasRetransmittableData retransmittable,
                 IsHandshake handshake,
                 Force forced)
        : sequence_number(sequence_number),
          packet(packet),
          encryption_level(level),
          transmission_type(transmission_type),
          retransmittable(retransmittable),
          handshake(handshake),
          forced(forced) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicPacket* packet;
    const EncryptionLevel encryption_level;
    TransmissionType transmission_type;
    HasRetransmittableData retransmittable;
    IsHandshake handshake;
    Force forced;
  };

  struct RetransmissionInfo {
    RetransmissionInfo(QuicPacketSequenceNumber sequence_number,
                       QuicSequenceNumberLength sequence_number_length,
                       QuicTime sent_time)
        : sequence_number(sequence_number),
          sequence_number_length(sequence_number_length),
          sent_time(sent_time),
          number_nacks(0),
          number_retransmissions(0) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicSequenceNumberLength sequence_number_length;
    QuicTime sent_time;
    size_t number_nacks;
    size_t number_retransmissions;
  };

  struct RetransmissionTime {
    RetransmissionTime(QuicPacketSequenceNumber sequence_number,
                       const QuicTime& scheduled_time,
                       bool for_fec)
        : sequence_number(sequence_number),
          scheduled_time(scheduled_time),
          for_fec(for_fec) { }

    QuicPacketSequenceNumber sequence_number;
    QuicTime scheduled_time;
    bool for_fec;
  };

  struct PendingWrite {
    PendingWrite(QuicPacketSequenceNumber sequence_number,
                 TransmissionType transmission_type,
                 HasRetransmittableData retransmittable,
                 EncryptionLevel level,
                 bool is_fec_packet,
                 size_t length)
        : sequence_number(sequence_number),
          transmission_type(transmission_type),
          retransmittable(retransmittable),
          level(level),
          is_fec_packet(is_fec_packet),
          length(length) { }

    QuicPacketSequenceNumber sequence_number;
    TransmissionType transmission_type;
    HasRetransmittableData retransmittable;
    EncryptionLevel level;
    bool is_fec_packet;
    size_t length;
  };

  class RetransmissionTimeComparator {
   public:
    bool operator()(const RetransmissionTime& lhs,
                    const RetransmissionTime& rhs) const {
      DCHECK(lhs.scheduled_time.IsInitialized() &&
             rhs.scheduled_time.IsInitialized());
      return lhs.scheduled_time > rhs.scheduled_time;
    }
  };

  typedef std::list<QueuedPacket> QueuedPacketList;
  typedef std::map<QuicFecGroupNumber, QuicFecGroup*> FecGroupMap;
  typedef std::priority_queue<RetransmissionTime,
                              std::vector<RetransmissionTime>,
                              RetransmissionTimeComparator>
      RetransmissionTimeouts;

  // Sends a version negotiation packet to the peer.
  void SendVersionNegotiationPacket();

  void SetupRetransmissionAlarm(QuicPacketSequenceNumber sequence_number);
  bool IsRetransmission(QuicPacketSequenceNumber sequence_number);

  void SetupAbandonFecTimer(QuicPacketSequenceNumber sequence_number);

  // Clears any accumulated frames from the last received packet.
  void ClearLastFrames();

  // Called from OnCanWrite and WriteIfNotBlocked to write queued packets.
  // Returns false if the socket has become blocked.
  bool DoWrite();

  // Calculates the smallest sequence number length that can also represent four
  // times the maximum of the congestion window and the difference between the
  // least_packet_awaited_by_peer_ and |sequence_number|.
  QuicSequenceNumberLength CalculateSequenceNumberLength(
      QuicPacketSequenceNumber sequence_number);

  // Drop packet corresponding to |sequence_number| by deleting entries from
  // |unacked_packets_| and |retransmission_map_|, if present. We need to drop
  // all packets with encryption level NONE after the default level has been set
  // to FORWARD_SECURE.
  void DropPacket(QuicPacketSequenceNumber sequence_number);

  // Writes as many queued packets as possible.  The connection must not be
  // blocked when this is called.
  bool WriteQueuedPackets();

  // Writes as many pending retransmissions as possible.
  void WritePendingRetransmissions();

  // Returns true if the packet should be discarded and not sent.
  bool ShouldDiscardPacket(EncryptionLevel level,
                           QuicPacketSequenceNumber sequence_number,
                           HasRetransmittableData retransmittable);

  // Queues |packet| in the hopes that it can be decrypted in the
  // future, when a new key is installed.
  void QueueUndecryptablePacket(const QuicEncryptedPacket& packet);

  // Attempts to process any queued undecryptable packets.
  void MaybeProcessUndecryptablePackets();

  // If a packet can be revived from the current FEC group, then
  // revive and process the packet.
  void MaybeProcessRevivedPacket();

  void ProcessAckFrame(const QuicAckFrame& incoming_ack);

  // Update the |sent_info| for an outgoing ack.
  void UpdateSentPacketInfo(SentPacketInfo* sent_info);

  // Checks if the last packet should instigate an ack.
  bool ShouldLastPacketInstigateAck();

  // Sends any packets which are a response to the last packet, including both
  // acks and pending writes if an ack opened the congestion window.
  void MaybeSendInResponseToPacket(bool send_ack_immediately,
                                   bool last_packet_should_instigate_ack);

  // Get the FEC group associate with the last processed packet or NULL, if the
  // group has already been deleted.
  QuicFecGroup* GetFecGroup();

  // Closes any FEC groups protecting packets before |sequence_number|.
  void CloseFecGroupsBefore(QuicPacketSequenceNumber sequence_number);

  QuicConnectionHelperInterface* helper_;  // Not owned.
  QuicPacketWriter* writer_;  // Not owned.
  EncryptionLevel encryption_level_;
  const QuicClock* clock_;
  QuicRandom* random_generator_;

  const QuicGuid guid_;
  // Address on the last successfully processed packet received from the
  // client.
  IPEndPoint self_address_;
  IPEndPoint peer_address_;

  bool last_packet_revived_;  // True if the last packet was revived from FEC.
  size_t last_size_;  // Size of the last received packet.
  QuicPacketHeader last_header_;
  std::vector<QuicStreamFrame> last_stream_frames_;
  std::vector<QuicAckFrame> last_ack_frames_;
  std::vector<QuicCongestionFeedbackFrame> last_congestion_frames_;
  std::vector<QuicRstStreamFrame> last_rst_frames_;
  std::vector<QuicGoAwayFrame> last_goaway_frames_;
  std::vector<QuicConnectionCloseFrame> last_close_frames_;

  QuicCongestionFeedbackFrame outgoing_congestion_feedback_;

  // Track some peer state so we can do less bookkeeping
  // Largest sequence sent by the peer which had an ack frame (latest ack info).
  QuicPacketSequenceNumber largest_seen_packet_with_ack_;

  // Collection of packets which were received before encryption was
  // established, but which could not be decrypted.  We buffer these on
  // the assumption that they could not be processed because they were
  // sent with the INITIAL encryption and the CHLO message was lost.
  std::deque<QuicEncryptedPacket*> undecryptable_packets_;

  // When the version negotiation packet could not be sent because the socket
  // was not writable, this is set to true.
  bool pending_version_negotiation_packet_;

  // When packets could not be sent because the socket was not writable,
  // they are added to this list.  All corresponding frames are in
  // unacked_packets_ if they are to be retransmitted.
  QueuedPacketList queued_packets_;

  // Contains information about the current write in progress, if any.
  scoped_ptr<PendingWrite> pending_write_;

  // Contains the connection close packet if the connection has been closed.
  scoped_ptr<QuicEncryptedPacket> connection_close_packet_;

  // True when the socket becomes unwritable.
  bool write_blocked_;

  FecGroupMap group_map_;

  QuicReceivedPacketManager received_packet_manager_;
  QuicSentEntropyManager sent_entropy_manager_;

  // An alarm that fires when an ACK should be sent to the peer.
  scoped_ptr<QuicAlarm> ack_alarm_;
  // An alarm that fires when a packet needs to be retransmitted.
  scoped_ptr<QuicAlarm> retransmission_alarm_;
  // An alarm that is scheduled when the sent scheduler requires a
  // a delay before sending packets and fires when the packet may be sent.
  scoped_ptr<QuicAlarm> send_alarm_;
  // An alarm that is scheduled when the connection can still write and there
  // may be more data to send.
  scoped_ptr<QuicAlarm> resume_writes_alarm_;
  // An alarm that fires when the connection may have timed out.
  scoped_ptr<QuicAlarm> timeout_alarm_;

  QuicConnectionVisitorInterface* visitor_;
  QuicConnectionDebugVisitorInterface* debug_visitor_;
  QuicPacketCreator packet_creator_;
  QuicPacketGenerator packet_generator_;

  // Network idle time before we kill of this connection.
  QuicTime::Delta idle_network_timeout_;
  // Overall connection timeout.
  QuicTime::Delta overall_connection_timeout_;
  // Connection creation time.
  QuicTime creation_time_;

  // Statistics for this session.
  QuicConnectionStats stats_;

  // The time that we got a packet for this connection.
  // This is used for timeouts, and does not indicate the packet was processed.
  QuicTime time_of_last_received_packet_;

  // The time that we last sent a packet for this connection.
  QuicTime time_of_last_sent_packet_;

  // Sequence number of the last packet guaranteed to be sent in packet sequence
  // number order.  Not set when packets are queued, since that may cause
  // re-ordering.
  QuicPacketSequenceNumber sequence_number_of_last_inorder_packet_;

  // Sent packet manager which tracks the status of packets sent by this
  // connection and contains the send and receive algorithms to determine when
  // to send packets.
  QuicSentPacketManager sent_packet_manager_;

  // The state of connection in version negotiation finite state machine.
  QuicVersionNegotiationState version_negotiation_state_;

  // Tracks if the connection was created by the server.
  bool is_server_;

  // True by default.  False if we've received or sent an explicit connection
  // close.
  bool connected_;

  // Set to true if the udp packet headers have a new self or peer address.
  // This is checked later on validating a data or version negotiation packet.
  bool address_migrating_;

  // An AckNotifier can register to be informed when ACKs have been received for
  // all packets that a given block of data was sent in. The AckNotifierManager
  // maintains the currently active notifiers.
  AckNotifierManager ack_notifier_manager_;

  // If non-empty this contains the set of versions received in a
  // version negotiation packet.
  QuicVersionVector server_supported_versions_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnection);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_H_
