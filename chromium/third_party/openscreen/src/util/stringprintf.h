// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STRINGPRINTF_H_
#define UTIL_STRINGPRINTF_H_

#include <stdint.h>

#include <format>
#include <ostream>
#include <string>
#include <utility>

#include "platform/base/span.h"

namespace openscreen {

// TODO(crbug.com/364687926): remove and replace with direct calls to
// std::format now that we are on C++20.
template <typename... Args>
[[nodiscard]] std::string StringFormat(std::format_string<Args...> fmt,
                                       Args&&... args) {
  return std::format(fmt, std::forward<Args>(args)...);
}

// Returns a hex string representation of the given `bytes`.
std::string HexEncode(const uint8_t* bytes, size_t len);
std::string HexEncode(ByteView bytes);

}  // namespace openscreen

#endif  // UTIL_STRINGPRINTF_H_
