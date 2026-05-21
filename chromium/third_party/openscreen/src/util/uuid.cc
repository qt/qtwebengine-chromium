// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/uuid.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>

#include "util/big_endian.h"
#include "util/crypto/random_bytes.h"
#include "util/hashing.h"
#include "util/osp_logging.h"
#include "util/string_util.h"
#include "util/stringprintf.h"

namespace openscreen {

namespace {

constexpr bool IsHyphenPosition(size_t i) {
  return i == 8 || i == 13 || i == 18 || i == 23;
}

// Returns a canonical Uuid string given that `input` is validly formatted
// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, such that x is a hexadecimal digit.
// If `strict`, x must be a lower-case hexadecimal digit.
std::string GetCanonicalUuidInternal(std::string_view input, bool strict) {
  constexpr size_t kUuidLength = 36;
  if (input.length() != kUuidLength) {
    return {};
  }

  std::string lowercase;
  lowercase.resize(kUuidLength);
  for (size_t i = 0; i < input.length(); ++i) {
    auto current = input[i];
    if (IsHyphenPosition(i)) {
      if (current != '-') {
        return {};
      }
      lowercase[i] = '-';
    } else {
      if (strict ? !string_util::ascii_islowerhex(current)
                 : !string_util::ascii_ishex(current)) {
        return {};
      }
      lowercase[i] = static_cast<char>(string_util::ascii_tolower(current));
    }
  }

  return lowercase;
}

}  // namespace

// static
Uuid Uuid::GenerateRandomV4() {
  return FormatRandomDataAsV4Impl(GenerateRandomBytes16());
}

// static
Uuid Uuid::FormatRandomDataAsV4Impl(ByteView input) {
  OSP_CHECK_EQ(input.size(), kGuidV4InputLength);

  auto first_u64 = ReadBigEndian<uint64_t>(input.first(8).data());
  auto second_u64 = ReadBigEndian<uint64_t>(input.last(8).data());

  // Set the Uuid to version 4 as described in RFC 4122, section 4.4.
  // The format of Uuid version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, a, b].

  // Clear the version bits and set the version to 4:
  first_u64 &= 0xffffffff'ffff0fffULL;
  first_u64 |= 0x00000000'00004000ULL;

  // Clear bit 65 and set bit 64, to set the 'var' field to 0b10 per RFC 9562
  // section 5.4.
  second_u64 &= 0x3fffffff'ffffffffULL;
  second_u64 |= 0x80000000'00000000ULL;

  Uuid uuid;
  uuid.lowercase_ =
      StringFormat("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
                   static_cast<uint32_t>(first_u64 >> 32),
                   static_cast<uint32_t>((first_u64 >> 16) & 0x0000'ffff),
                   static_cast<uint32_t>(first_u64 & 0x0000'ffff),
                   static_cast<uint32_t>(second_u64 >> 48),
                   second_u64 & 0x0000'ffff'ffff'ffffULL);
  return uuid;
}

// static
Uuid Uuid::ParseCaseInsensitive(std::string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/false);
  return uuid;
}

// static
Uuid Uuid::ParseLowercase(std::string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/true);
  return uuid;
}

Uuid::Uuid() = default;
Uuid::Uuid(const Uuid& other) = default;
Uuid::Uuid(Uuid&& other) noexcept = default;
Uuid& Uuid::operator=(const Uuid& other) = default;
Uuid& Uuid::operator=(Uuid&& other) = default;

const std::string& Uuid::AsLowercaseString() const {
  return lowercase_;
}

std::ostream& operator<<(std::ostream& out, const Uuid& uuid) {
  return out << uuid.AsLowercaseString();
}

size_t UuidHash::operator()(const Uuid& uuid) const {
  return ComputeAggregateHash(uuid.AsLowercaseString());
}

}  // namespace openscreen
