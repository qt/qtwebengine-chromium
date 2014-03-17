// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
#define NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_

#include <map>

#include "base/containers/hash_tables.h"
#include "net/quic/quic_protocol.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<net::QuicAckNotifier*> {
  std::size_t operator()(const net::QuicAckNotifier* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}
#endif

namespace net {

class QuicAckNotifier;

// The AckNotifierManager is used by the QuicSentPacketManager to keep track of
// all the AckNotifiers currently active. It owns the AckNotifiers which it gets
// from the serialized packets passed into OnSerializedPacket. It maintains both
// a set of AckNotifiers and a map from sequence number to AckNotifier the sake
// of efficiency - we can quickly check the map to see if any AckNotifiers are
// interested in a given sequence number.

class NET_EXPORT_PRIVATE AckNotifierManager {
 public:
  AckNotifierManager();
  virtual ~AckNotifierManager();

  // Called when the connection receives a new AckFrame.  If |sequence_number|
  // exists in ack_notifier_map_ then the corresponding AckNotifiers will have
  // their OnAck method called.
  void OnPacketAcked(QuicPacketSequenceNumber sequence_number);

  // If a packet has been retransmitted with a new sequence number, then this
  // will be called. It updates the mapping in ack_notifier_map_, and also
  // updates the internal set of sequence numbers in each matching AckNotifier.
  void UpdateSequenceNumber(QuicPacketSequenceNumber old_sequence_number,
                            QuicPacketSequenceNumber new_sequence_number);

  // This is called after a packet has been serialized, is ready to be sent, and
  // contains retransmittable frames (which may have associated AckNotifiers).
  // If any of the retransmittable frames included in |serialized_packet| have
  // AckNotifiers registered, then add them to our internal map and additionally
  // inform the AckNotifier of the sequence number which it should track.
  void OnSerializedPacket(const SerializedPacket& serialized_packet);

 private:
  typedef base::hash_set<QuicAckNotifier*> AckNotifierSet;
  typedef std::map<QuicPacketSequenceNumber, AckNotifierSet> AckNotifierMap;

  // On every ACK frame received by the connection, all the ack_notifiers_ will
  // be told which sequeunce numbers were ACKed.
  // Once a given QuicAckNotifier has seen all the sequence numbers it is
  // interested in, it will be deleted, and removed from this set.
  // Owns the AckNotifiers in this set.
  AckNotifierSet ack_notifiers_;

  // Maps from sequence number to the AckNotifiers which are registered
  // for that sequence number. On receipt of an ACK for a given sequence
  // number, call OnAck for all mapped AckNotifiers.
  // Does not own the AckNotifiers.
  AckNotifierMap ack_notifier_map_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_ACK_NOTIFIER_MANAGER_H_
