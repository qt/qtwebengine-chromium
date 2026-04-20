// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha2.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <utility>

#include "base/logging.h"
#include "base/sys_byteorder.h"

namespace base {

namespace {

// Reference: http://dx.doi.org/10.6028/NIST.FIPS.180-4

// From section 2.2.2:
uint32_t RotateRight(uint32_t x, uint8_t n) {
  return (x >> n) | (x << (32 - n));
}

// From section 4.1.2:
uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (~x & z);
}

uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t Sum0(uint32_t x) {
  return RotateRight(x, 2) ^ RotateRight(x, 13) ^ RotateRight(x, 22);
}

uint32_t Sum1(uint32_t x) {
  return RotateRight(x, 6) ^ RotateRight(x, 11) ^ RotateRight(x, 25);
}

uint32_t Sigma0(uint32_t x) {
  return RotateRight(x, 7) ^ RotateRight(x, 18) ^ (x >> 3);
}

uint32_t Sigma1(uint32_t x) {
  return RotateRight(x, 17) ^ RotateRight(x, 19) ^ (x >> 10);
}

// From section 4.2.2:
constexpr std::array<uint32_t, 64> kConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

class Sha256Hasher {
 public:
  Sha256Hasher() = default;

  // Pre-requisite: `chunk.size() == 64`, since SHA256 operates on 512-bit
  // blocks.
  void Update(std::string_view chunk) {
    CHECK(chunk.size() == 64);

    // From section 6.2.2, step 1: "Prepare the message schedule"
    memcpy(w_.data(), chunk.data(), chunk.size());
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    for (int t = 0; t < 16; ++t) {
      w_[t] = ByteSwap(w_[t]);
    }
#endif
    for (int t = 16; t < 64; ++t) {
      w_[t] = Sigma1(w_[t - 2]) + w_[t - 7] + Sigma0(w_[t - 15]) + w_[t - 16];
    }

    // From section 6.2.2, step 2: "Initialize the eight working variables"
    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];
    uint32_t e = hash_[4];
    uint32_t f = hash_[5];
    uint32_t g = hash_[6];
    uint32_t h = hash_[7];

    // From section 6.2.2, step 3:
    for (int t = 0; t < 64; ++t) {
      uint32_t tmp1 = h + Sum1(e) + Ch(e, f, g) + kConstants[t] + w_[t];
      uint32_t tmp2 = Sum0(a) + Maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + tmp1;
      d = c;
      c = b;
      b = a;
      a = tmp1 + tmp2;
    }

    // From section 6.2.2, step 4:
    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
    hash_[5] += f;
    hash_[6] += g;
    hash_[7] += h;
  }

  std::array<uint8_t, kSha256Length> Finalize(std::string_view chunk,
                                              uint64_t original_size) && {
    std::array<char, 64> padding_chunk = {};
    auto next = std::copy(chunk.begin(), chunk.end(), padding_chunk.begin());
    // From section 5.1.1, the padding consists of a 0x80 byte, followed by a
    // 64-bit block with the length of the message in bits in big-endian order.
    *next++ = uint8_t{0x80};

    // If there's not enough space for the length, pad out one additional chunk.
    if (padding_chunk.end() - next < 8) {
      Update(std::string_view(padding_chunk.data(), padding_chunk.size()));
      std::fill(padding_chunk.begin(), padding_chunk.begin() + 56, 0);
    }

    const uint64_t original_size_in_bits = original_size * 8;
    padding_chunk[56] = (original_size_in_bits >> 56) & 0xffu;
    padding_chunk[57] = (original_size_in_bits >> 48) & 0xffu;
    padding_chunk[58] = (original_size_in_bits >> 40) & 0xffu;
    padding_chunk[59] = (original_size_in_bits >> 32) & 0xffu;
    padding_chunk[60] = (original_size_in_bits >> 24) & 0xffu;
    padding_chunk[61] = (original_size_in_bits >> 16) & 0xffu;
    padding_chunk[62] = (original_size_in_bits >> 8) & 0xffu;
    padding_chunk[63] = original_size_in_bits & 0xffu;
    Update(std::string_view(padding_chunk.data(), padding_chunk.size()));

    std::array<uint8_t, kSha256Length> result;
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    for (uint32_t& word : hash_) {
      word = ByteSwap(word);
    }
#endif
    memcpy(result.data(), hash_.data(), sizeof(result));
    return result;
  }

 private:
  // From section 5.3.3:
  std::array<uint32_t, 8> hash_ = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  };
  // The message schedule for intermediate states. Member to make it easier to
  // reuse code between `Update()` and `Finalize()`.
  std::array<uint32_t, 64> w_ = {};
};

}  // namespace

std::array<uint8_t, kSha256Length> Sha256(std::string_view str) {
  const size_t original_size = str.size();
  Sha256Hasher hasher;
  while (str.size() >= 64) {
    std::string_view next = str.substr(0, 64);
    hasher.Update(next);
    str.remove_prefix(64);
  }
  return std::move(hasher).Finalize(str, original_size);
}

}  // namespace base
