// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_RANDOM_BYTES_H_
#define UTIL_CRYPTO_RANDOM_BYTES_H_

#include <array>
#include <cstdint>

#include "platform/base/span.h"

namespace openscreen {

std::array<uint8_t, 16> GenerateRandomBytes16();
void GenerateRandomBytes(ByteBuffer out);

}  // namespace openscreen

#endif  // UTIL_CRYPTO_RANDOM_BYTES_H_
