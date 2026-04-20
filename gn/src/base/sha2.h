// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHA2_H_
#define BASE_SHA2_H_

#include <stdint.h>

#include <array>
#include <string_view>

namespace base {

static const size_t kSha256Length = 32;  // Length in bytes of a SHA-256 hash.

// Computes the SHA-256 hash of `bytes` and returns the digest as an array of
// bytes.
std::array<uint8_t, kSha256Length> Sha256(std::string_view bytes);

}  // namespace base

#endif  // BASE_SHA2_H_
