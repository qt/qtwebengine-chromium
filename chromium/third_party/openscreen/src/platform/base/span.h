// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_SPAN_H_
#define PLATFORM_BASE_SPAN_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <cassert>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "platform/base/type_util.h"

namespace openscreen {

// In Open Screen code, use these aliases for the most common types of Spans.
// TODO(crbug.com/364687926): rename to byte_view.h and remove Span alias.
using ByteView = std::span<const uint8_t>;
using ByteBuffer = std::span<uint8_t>;
template <typename T>
using Span = std::span<T>;

inline ByteView ByteViewFromString(std::string_view str) {
  return ByteView(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

inline std::string ByteViewToString(ByteView bytes) {
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace openscreen

#endif  // PLATFORM_BASE_SPAN_H_
