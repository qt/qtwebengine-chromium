// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha2.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "util/test/test.h"

namespace base {

namespace {

std::string Sha256AsHexString(std::string_view in) {
  std::array<uint8_t, kSha256Length> result = Sha256(in);
  return HexEncode(result.data(), result.size());
}

TEST(Sha2Test, Basic) {
  EXPECT_EQ("E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
            Sha256AsHexString(""));

  // Reference values from
  // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha256.pdf
  EXPECT_EQ("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
            Sha256AsHexString("abc"));
  EXPECT_EQ("248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1",
            Sha256AsHexString(
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
  // Additional tests from
  // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA2_Additional.pdf
  EXPECT_EQ("68325720AABD7C82F30F554B313D0570C95ACCBB7DC4B5AAE11204C08FFE732B",
            Sha256AsHexString("\xbd"));
  EXPECT_EQ("7ABC22C0AE5AF26CE93DBB94433A0E0B2E119D014F8E7F65BD56C61CCCCD9504",
            Sha256AsHexString("\xc9\x8c\x8e\x55"));
  EXPECT_EQ("02779466CDEC163811D078815C633F21901413081449002F24AA3E80F0B88EF7",
            Sha256AsHexString(std::string(55, '\0')));
  EXPECT_EQ("D4817AA5497628E7C77E6B606107042BBBA3130888C5F47A375E6179BE789FBB",
            Sha256AsHexString(std::string(56, '\0')));
  EXPECT_EQ("65A16CB7861335D5ACE3C60718B5052E44660726DA4CD13BB745381B235A1785",
            Sha256AsHexString(std::string(57, '\0')));
  EXPECT_EQ("F5A5FD42D16A20302798EF6ED309979B43003D2320D9F0E8EA9831A92759FB4B",
            Sha256AsHexString(std::string(64, '\0')));
  EXPECT_EQ("541B3E9DAA09B20BF85FA273E5CBD3E80185AA4EC298E765DB87742B70138A53",
            Sha256AsHexString(std::string(1000, '\0')));
  EXPECT_EQ("C2E686823489CED2017F6059B8B239318B6364F6DCD835D0A519105A1EADD6E4",
            Sha256AsHexString(std::string(1000, 'A')));
  EXPECT_EQ("F4D62DDEC0F3DD90EA1380FA16A5FF8DC4C54B21740650F24AFC4120903552B0",
            Sha256AsHexString(std::string(1005, 'U')));
  EXPECT_EQ("D29751F2649B32FF572B5E0A9F541EA660A50F94FF0BEEDFB0B692B924CC8025",
            Sha256AsHexString(std::string(1000000, '\0')));
}

}  // namespace

}  // namespace base
