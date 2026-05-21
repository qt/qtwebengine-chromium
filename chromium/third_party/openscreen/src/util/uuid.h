// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_UUID_H_
#define UTIL_UUID_H_

#include <stdint.h>

#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>

#include "platform/base/span.h"

namespace openscreen {

// UUID implementation strongly based off of Chromium's base::Uuid
// implementation. Provides securely generated random Uuids as well as parsing
// logic for inputted UUIDs.
class Uuid {
 public:
  // Length in bytes of the input required to format the input as a Uuid in the
  // form of version 4.
  static constexpr size_t kGuidV4InputLength = 16;

  // Generate a 128-bit random Uuid in the form of version 4. see RFC 4122,
  // section 4.4. The format of Uuid version 4 must be
  // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx, where y is one of [8, 9, a, b]. The
  // hexadecimal values "a" through "f" are output as lower case characters.
  static Uuid GenerateRandomV4();

  // Returns a valid Uuid if the input string conforms to the Uuid format, and
  // an invalid Uuid otherwise. Accepts both lower case and upper case hex
  // characters.
  static Uuid ParseCaseInsensitive(std::string_view input);

  // Similar to ParseCaseInsensitive(), but all hexadecimal values "a" through
  // "f" must be lower case characters.
  static Uuid ParseLowercase(std::string_view input);

  // Constructs an invalid Uuid.
  Uuid();

  Uuid(const Uuid& other);
  Uuid(Uuid&& other) noexcept;
  Uuid& operator=(const Uuid& other);
  Uuid& operator=(Uuid&& other);

  bool is_valid() const { return !lowercase_.empty(); }

  // Returns the Uuid in a lowercase string format if it is valid, and an empty
  // string otherwise. The returned value is guaranteed to be parsed by
  // ParseLowercase().
  const std::string& AsLowercaseString() const;

  // Invalid Uuids are equal.
  friend bool operator==(const Uuid&, const Uuid&) = default;
  // Uuids are 128bit chunks of data so must be indistinguishable if equivalent.
  friend std::strong_ordering operator<=>(const Uuid&, const Uuid&) = default;

 private:
  static Uuid FormatRandomDataAsV4Impl(ByteView input);

  // The lowercase form of the Uuid. Empty for invalid Uuids.
  std::string lowercase_;
};

// For runtime usage only. Do not store the result of this hash, as it may
// change in the future.
struct UuidHash {
  size_t operator()(const Uuid& uuid) const;
};

// Stream operator so Uuid objects can be used in logging statements.
std::ostream& operator<<(std::ostream& out, const Uuid& uuid);

}  // namespace openscreen

#endif  // UTIL_UUID_H_
