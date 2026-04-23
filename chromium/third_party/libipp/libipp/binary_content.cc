// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_content.h"

#include <cstdint>
#include <string>

namespace {

uint32_t TwosComplementEncoding(int value) {
  if (value >= 0) {
    return value;
  }
  uint32_t binary = -static_cast<int64_t>(value);
  binary = ~binary;
  ++binary;
  return binary;
}

}  // namespace

void BinaryContent::s(std::string s) {
  for (auto c : s) {
    data.push_back(static_cast<uint8_t>(c));
  }
}

void BinaryContent::u1(int v) {
  const uint32_t b = TwosComplementEncoding(v);
  data.push_back(b & 0xffu);
}

void BinaryContent::u2(int v) {
  const uint32_t b = TwosComplementEncoding(v);
  data.push_back((b >> 8) & 0xffu);
  data.push_back(b & 0xffu);
}

void BinaryContent::u4(int v) {
  const uint32_t b = TwosComplementEncoding(v);
  data.push_back((b >> 24) & 0xffu);
  data.push_back((b >> 16) & 0xffu);
  data.push_back((b >> 8) & 0xffu);
  data.push_back(b & 0xffu);
}
