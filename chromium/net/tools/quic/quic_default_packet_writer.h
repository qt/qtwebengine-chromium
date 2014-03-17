// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_packet_writer.h"

namespace net {

class QuicBlockedWriterInterface;
struct WriteResult;

namespace tools {

// Default packet writer which wraps QuicSocketUtils WritePacket.
class QuicDefaultPacketWriter : public QuicPacketWriter {
 public:
  explicit QuicDefaultPacketWriter(int fd);
  virtual ~QuicDefaultPacketWriter();

  // QuicPacketWriter
  virtual WriteResult WritePacket(
      const char* buffer, size_t buf_len,
      const net::IPAddressNumber& self_address,
      const net::IPEndPoint& peer_address,
      QuicBlockedWriterInterface* blocked_writer) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;

 private:
  int fd_;
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
