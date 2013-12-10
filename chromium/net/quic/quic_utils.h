// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic

#ifndef NET_QUIC_QUIC_UTILS_H_
#define NET_QUIC_QUIC_UTILS_H_

#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicUtils {
 public:
  enum Priority {
    LOCAL_PRIORITY,
    PEER_PRIORITY,
  };

  // returns the 64 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint64 FNV1a_64_Hash(const char* data, int len);

  // returns the 128 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint128 FNV1a_128_Hash(const char* data, int len);

  // FindMutualTag sets |out_result| to the first tag in the priority list that
  // is also in the other list and returns true. If there is no intersection it
  // returns false.
  //
  // Which list has priority is determined by |priority|.
  //
  // If |out_index| is non-NULL and a match is found then the index of that
  // match in |their_tags| is written to |out_index|.
  static bool FindMutualTag(const QuicTagVector& our_tags,
                            const QuicTag* their_tags,
                            size_t num_their_tags,
                            Priority priority,
                            QuicTag* out_result,
                            size_t* out_index);

  // SerializeUint128 writes |v| in little-endian form to |out|.
  static void SerializeUint128(uint128 v, uint8* out);

  // Returns the name of the QuicRstStreamErrorCode as a char*
  static const char* StreamErrorToString(QuicRstStreamErrorCode error);

  // Returns the name of the QuicErrorCode as a char*
  static const char* ErrorToString(QuicErrorCode error);

  // Returns the level of encryption as a char*
  static const char* EncryptionLevelToString(EncryptionLevel level);

  // TagToString is a utility function for pretty-printing handshake messages
  // that converts a tag to a string. It will try to maintain the human friendly
  // name if possible (i.e. kABCD -> "ABCD"), or will just treat it as a number
  // if not.
  static std::string TagToString(QuicTag tag);

  // Given a binary buffer, return a hex+ASCII dump in the style of
  // tcpdump's -X and -XX options:
  // "0x0000:  0090 69bd 5400 000d 610f 0189 0800 4500  ..i.T...a.....E.\n"
  // "0x0010:  001c fb98 4000 4001 7e18 d8ef 2301 455d  ....@.@.~...#.E]\n"
  // "0x0020:  7fe2 0800 6bcb 0bc6 806e                 ....k....n\n"
  static std::string StringToHexASCIIDump(base::StringPiece in_buffer);

  static char* AsChars(unsigned char* data) {
    return reinterpret_cast<char*>(data);
  }
};

}  // namespace net

#endif  // NET_QUIC_QUIC_UTILS_H_
