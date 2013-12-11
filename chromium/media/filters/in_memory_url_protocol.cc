// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/in_memory_url_protocol.h"

namespace media {

InMemoryUrlProtocol::InMemoryUrlProtocol(const uint8* data, int64 size,
                                         bool streaming)
    : data_(data),
      size_(size >= 0 ? size : 0),
      position_(0),
      streaming_(streaming) {
}

InMemoryUrlProtocol::~InMemoryUrlProtocol() {}

int InMemoryUrlProtocol::Read(int size, uint8* data) {
  int available_bytes = size_ - position_;
  if (size > available_bytes)
    size = available_bytes;

  memcpy(data, data_ + position_, size);
  position_ += size;
  return size;
}

bool InMemoryUrlProtocol::GetPosition(int64* position_out) {
  if (!position_out)
    return false;

  *position_out = position_;
  return true;
}

bool InMemoryUrlProtocol::SetPosition(int64 position) {
  if (position < 0 || position > size_)
    return false;
  position_ = position;
  return true;
}

bool InMemoryUrlProtocol::GetSize(int64* size_out) {
  if (!size_out)
    return false;

  *size_out = size_;
  return true;
}

bool InMemoryUrlProtocol::IsStreaming() {
  return streaming_;
}

}  // namespace media
