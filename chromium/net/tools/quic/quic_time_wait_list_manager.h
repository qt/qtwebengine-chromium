// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Handles packets for guids in time wait state by discarding the packet and
// sending the clients a public reset packet with exponential backoff.

#ifndef NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_
#define NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_

#include <deque>

#include "base/containers/hash_tables.h"
#include "base/strings/string_piece.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_clock.h"

namespace net {
namespace tools {

class GuidCleanUpAlarm;

namespace test {
class QuicTimeWaitListManagerPeer;
}  // namespace test

// Maintains a list of all guids that have been recently closed. A guid lives in
// this state for kTimeWaitPeriod. All packets received for guids in this state
// are handed over to the QuicTimeWaitListManager by the QuicDispatcher.
// Decides whether to send a public reset packet, a copy of the previously sent
// connection close packet, or nothing to the client which sent a packet
// with the guid in time wait state.  After the guid expires its time wait
// period, a new connection/session will be created if a packet is received
// for this guid.
class QuicTimeWaitListManager : public QuicBlockedWriterInterface,
                                public QuicFramerVisitorInterface {
 public:
  // writer - the entity that writes to the socket. (Owned by the dispatcher)
  // epoll_server - used to run clean up alarms. (Owned by the dispatcher)
  QuicTimeWaitListManager(QuicPacketWriter* writer,
                          EpollServer* epoll_server,
                          const QuicVersionVector& supported_versions);
  virtual ~QuicTimeWaitListManager();

  // Adds the given guid to time wait state for kTimeWaitPeriod. Henceforth,
  // any packet bearing this guid should not be processed while the guid remains
  // in this list. If a non-NULL |close_packet| is provided, it is sent again
  // when packets are received for added guids. If NULL, a public reset packet
  // is sent with the specified |version|. DCHECKs that guid is not already on
  // the list.
  void AddGuidToTimeWait(QuicGuid guid,
                         QuicVersion version,
                         QuicEncryptedPacket* close_packet);  // Owned.

  // Returns true if the guid is in time wait state, false otherwise. Packets
  // received for this guid should not lead to creation of new QuicSessions.
  bool IsGuidInTimeWait(QuicGuid guid) const;

  // Called when a packet is received for a guid that is in time wait state.
  // Sends a public reset packet to the client which sent this guid. Sending
  // of the public reset packet is throttled by using exponential back off.
  // DCHECKs for the guid to be in time wait state.
  // virtual to override in tests.
  virtual void ProcessPacket(const IPEndPoint& server_address,
                             const IPEndPoint& client_address,
                             QuicGuid guid,
                             const QuicEncryptedPacket& packet);

  // Called by the dispatcher when the underlying socket becomes writable again,
  // since we might need to send pending public reset packets which we didn't
  // send because the underlying socket was write blocked.
  virtual bool OnCanWrite() OVERRIDE;

  // Used to delete guid entries that have outlived their time wait period.
  void CleanUpOldGuids();

  // QuicFramerVisitorInterface
  virtual void OnError(QuicFramer* framer) OVERRIDE;
  virtual bool OnProtocolVersionMismatch(QuicVersion received_version) OVERRIDE;
  virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnPacket() OVERRIDE {}
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& /*packet*/) OVERRIDE {}
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& /*packet*/) OVERRIDE {}

  virtual void OnPacketComplete() OVERRIDE {}
  // The following methods should never get called because we always return
  // false from OnUnauthenticatedHeader(). We never process the encrypted bytes.
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnRevivedPacket() OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece /*payload*/) OVERRIDE;
  virtual bool OnStreamFrame(const QuicStreamFrame& /*frame*/) OVERRIDE;
  virtual bool OnAckFrame(const QuicAckFrame& /*frame*/) OVERRIDE;
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& /*frame*/) OVERRIDE;
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& /*frame*/) OVERRIDE;
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame & /*frame*/) OVERRIDE;
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& /*frame*/) OVERRIDE;
  virtual void OnFecData(const QuicFecData& /*fec*/) OVERRIDE;

 private:
  friend class test::QuicTimeWaitListManagerPeer;

  // Stores the guid and the time it was added to time wait state.
  struct GuidAddTime;
  // Internal structure to store pending public reset packets.
  class QueuedPacket;

  // Decides if a packet should be sent for this guid based on the number of
  // received packets.
  bool ShouldSendResponse(int received_packet_count);

  // Given a GUID that exists in the time wait list, returns the QuicVersion
  // associated with it. Used internally to set the framer version before
  // writing the public reset packet.
  QuicVersion GetQuicVersionFromGuid(QuicGuid guid);

  // Creates a public reset packet and sends it or queues it to be sent later.
  void SendPublicReset(const IPEndPoint& server_address,
                       const IPEndPoint& client_address,
                       QuicGuid guid,
                       QuicPacketSequenceNumber rejected_sequence_number);

  // Either sends the packet and deletes it or makes pending_packets_queue_ the
  // owner of the packet.
  void SendOrQueuePacket(QueuedPacket* packet);

  // Should only be called when write_blocked_ == false. We only care if the
  // writing was unsuccessful because the socket got blocked, which can be
  // tested using write_blocked_ == true. In case of all other errors we drop
  // the packet. Hence, we return void.
  void WriteToWire(QueuedPacket* packet);

  // Register the alarm with the epoll server to wake up at appropriate time.
  void SetGuidCleanUpAlarm();

  // A map from a recently closed guid to the number of packets received after
  // the termination of the connection bound to the guid.
  struct GuidData {
    GuidData(int num_packets_,
             QuicVersion version_,
             QuicEncryptedPacket* close_packet)
        : num_packets(num_packets_),
          version(version_),
          close_packet(close_packet) {}
    int num_packets;
    QuicVersion version;
    QuicEncryptedPacket* close_packet;
  };
  base::hash_map<QuicGuid, GuidData> guid_map_;
  typedef base::hash_map<QuicGuid, GuidData>::iterator GuidMapIterator;

  // Maintains a list of GuidAddTime elements which it owns, in the
  // order they should be deleted.
  std::deque<GuidAddTime*> time_ordered_guid_list_;

  // Pending public reset packets that need to be sent out to the client
  // when we are given a chance to write by the dispatcher.
  std::deque<QueuedPacket*> pending_packets_queue_;

  // Used to parse incoming packets.
  QuicFramer framer_;

  // Server and client address of the last packet processed.
  IPEndPoint server_address_;
  IPEndPoint client_address_;

  // Used to schedule alarms to delete old guids which have been in the list for
  // too long. Owned by the dispatcher.
  EpollServer* epoll_server_;

  // Time period for which guids should remain in time wait state.
  const QuicTime::Delta kTimeWaitPeriod_;

  // Alarm registered with the epoll server to clean up guids that have out
  // lived their duration in time wait state.
  scoped_ptr<GuidCleanUpAlarm> guid_clean_up_alarm_;

  // Clock to efficiently measure approximate time from the epoll server.
  QuicEpollClock clock_;

  // Interface that writes given buffer to the socket. Owned by the dispatcher.
  QuicPacketWriter* writer_;

  // True if the underlying udp socket is write blocked, i.e will return EAGAIN
  // on sendmsg.
  bool is_write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicTimeWaitListManager);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_
