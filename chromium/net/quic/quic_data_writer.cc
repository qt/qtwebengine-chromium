// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_writer.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"

using base::StringPiece;
using std::numeric_limits;

namespace net {

QuicDataWriter::QuicDataWriter(size_t size)
    : buffer_(new char[size]),
      capacity_(size),
      length_(0) {
}

QuicDataWriter::~QuicDataWriter() {
  delete[] buffer_;
}

char* QuicDataWriter::take() {
  char* rv = buffer_;
  buffer_ = NULL;
  capacity_ = 0;
  length_ = 0;
  return rv;
}

bool QuicDataWriter::WriteUInt8(uint8 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt16(uint16 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt32(uint32 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt48(uint64 value) {
  uint32 hi = value >> 32;
  uint32 lo = value & GG_UINT64_C(0x00000000FFFFFFFF);
  return WriteUInt32(lo) && WriteUInt16(hi);
}

bool QuicDataWriter::WriteUInt64(uint64 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteStringPiece16(StringPiece val) {
  if (val.length() > numeric_limits<uint16>::max()) {
    return false;
  }
  if (!WriteUInt16(val.size())) {
    return false;
  }
  return WriteBytes(val.data(), val.size());
}

char* QuicDataWriter::BeginWrite(size_t length) {
  if (length_ > capacity_) {
    return NULL;
  }

  if (capacity_ - length_ < length) {
    return NULL;
  }

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, numeric_limits<uint32>::max());
#endif

  return buffer_ + length_;
}

bool QuicDataWriter::WriteBytes(const void* data, size_t data_len) {
  char* dest = BeginWrite(data_len);
  if (!dest) {
    return false;
  }

  memcpy(dest, data, data_len);

  length_ += data_len;
  return true;
}

bool QuicDataWriter::WriteRepeatedByte(uint8 byte, size_t count) {
  char* dest = BeginWrite(count);
  if (!dest) {
    return false;
  }

  memset(dest, byte, count);

  length_ += count;
  return true;
}

void QuicDataWriter::WritePadding() {
  DCHECK_LE(length_, capacity_);
  if (length_ > capacity_) {
    return;
  }
  memset(buffer_ + length_, 0x00, capacity_ - length_);
  length_ = capacity_;
}

bool QuicDataWriter::WriteUInt8ToOffset(uint8 value, size_t offset) {
  DCHECK_LT(offset, capacity_);
  size_t latched_length = length_;
  length_ = offset;
  bool success = WriteUInt8(value);
  DCHECK_LE(length_, latched_length);
  length_ = latched_length;
  return success;
}

bool QuicDataWriter::WriteUInt32ToOffset(uint32 value, size_t offset) {
  DCHECK_LT(offset, capacity_);
  size_t latched_length = length_;
  length_ = offset;
  bool success = WriteUInt32(value);
  DCHECK_LE(length_, latched_length);
  length_ = latched_length;
  return success;
}

bool QuicDataWriter::WriteUInt48ToOffset(uint64 value, size_t offset) {
  DCHECK_LT(offset, capacity_);
  size_t latched_length = length_;
  length_ = offset;
  bool success = WriteUInt48(value);
  DCHECK_LE(length_, latched_length);
  length_ = latched_length;
  return success;
}

}  // namespace net
