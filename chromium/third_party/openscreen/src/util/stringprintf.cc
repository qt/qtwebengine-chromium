// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/stringprintf.h"

#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <sstream>

#include "util/osp_logging.h"

namespace openscreen {

std::string HexEncode(const uint8_t* bytes, size_t len) {
  return HexEncode(ByteView(bytes, len));
}

std::string HexEncode(ByteView bytes) {
  std::ostringstream hex_dump;
  hex_dump << std::setfill('0') << std::hex;
  for (uint8_t byte : bytes) {
    hex_dump << std::setw(2) << static_cast<int>(byte);
  }
  return hex_dump.str();
}

}  // namespace openscreen
