// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_sspi_win.h"
#include "net/http/mock_sspi_library_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void MatchDomainUserAfterSplit(const std::wstring& combined,
                               const std::wstring& expected_domain,
                               const std::wstring& expected_user) {
  std::wstring actual_domain;
  std::wstring actual_user;
  SplitDomainAndUser(combined, &actual_domain, &actual_user);
  EXPECT_EQ(expected_domain, actual_domain);
  EXPECT_EQ(expected_user, actual_user);
}

const ULONG kMaxTokenLength = 100;

}  // namespace

TEST(HttpAuthSSPITest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(L"foobar", L"", L"foobar");
  MatchDomainUserAfterSplit(L"FOO\\bar", L"FOO", L"bar");
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_Normal) {
  SecPkgInfoW package_info;
  memset(&package_info, 0x0, sizeof(package_info));
  package_info.cbMaxToken = 1337;

  MockSSPILibrary mock_library;
  mock_library.ExpectQuerySecurityPackageInfo(L"NTLM", SEC_E_OK, &package_info);
  ULONG max_token_length = kMaxTokenLength;
  int rv = DetermineMaxTokenLength(&mock_library, L"NTLM", &max_token_length);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(1337, max_token_length);
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_InvalidPackage) {
  MockSSPILibrary mock_library;
  mock_library.ExpectQuerySecurityPackageInfo(L"Foo", SEC_E_SECPKG_NOT_FOUND,
                                              NULL);
  ULONG max_token_length = kMaxTokenLength;
  int rv = DetermineMaxTokenLength(&mock_library, L"Foo", &max_token_length);
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME, rv);
  // |DetermineMaxTokenLength()| interface states that |max_token_length| should
  // not change on failure.
  EXPECT_EQ(100, max_token_length);
}

TEST(HttpAuthSSPITest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  MockSSPILibrary mock_library;
  HttpAuthSSPI auth_sspi(&mock_library, "Negotiate",
                         NEGOSSP_NAME, kMaxTokenLength);
  std::string challenge_text = "Negotiate";
  HttpAuth::ChallengeTokenizer challenge(challenge_text.begin(),
                                         challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_TwoRounds) {
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  MockSSPILibrary mock_library;
  HttpAuthSSPI auth_sspi(&mock_library, "Negotiate",
                         NEGOSSP_NAME, kMaxTokenLength);
  std::string first_challenge_text = "Negotiate";
  HttpAuth::ChallengeTokenizer first_challenge(first_challenge_text.begin(),
                                               first_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  // Generate an auth token and create another thing.
  std::string auth_token;
  EXPECT_EQ(OK, auth_sspi.GenerateAuthToken(NULL, "HTTP/intranet.google.com",
                                            &auth_token));

  std::string second_challenge_text = "Negotiate Zm9vYmFy";
  HttpAuth::ChallengeTokenizer second_challenge(second_challenge_text.begin(),
                                                second_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_UnexpectedTokenFirstRound) {
  // If the first round challenge has an additional authentication token, it
  // should be treated as an invalid challenge from the server.
  MockSSPILibrary mock_library;
  HttpAuthSSPI auth_sspi(&mock_library, "Negotiate",
                         NEGOSSP_NAME, kMaxTokenLength);
  std::string challenge_text = "Negotiate Zm9vYmFy";
  HttpAuth::ChallengeTokenizer challenge(challenge_text.begin(),
                                         challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  MockSSPILibrary mock_library;
  HttpAuthSSPI auth_sspi(&mock_library, "Negotiate",
                         NEGOSSP_NAME, kMaxTokenLength);
  std::string first_challenge_text = "Negotiate";
  HttpAuth::ChallengeTokenizer first_challenge(first_challenge_text.begin(),
                                               first_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK, auth_sspi.GenerateAuthToken(NULL, "HTTP/intranet.google.com",
                                            &auth_token));
  std::string second_challenge_text = "Negotiate";
  HttpAuth::ChallengeTokenizer second_challenge(second_challenge_text.begin(),
                                                second_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_NonBase64EncodedToken) {
  // If a later-round challenge has an invalid base64 encoded token, it should
  // be treated as an invalid challenge.
  MockSSPILibrary mock_library;
  HttpAuthSSPI auth_sspi(&mock_library, "Negotiate",
                         NEGOSSP_NAME, kMaxTokenLength);
  std::string first_challenge_text = "Negotiate";
  HttpAuth::ChallengeTokenizer first_challenge(first_challenge_text.begin(),
                                               first_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK, auth_sspi.GenerateAuthToken(NULL, "HTTP/intranet.google.com",
                                            &auth_token));
  std::string second_challenge_text = "Negotiate =happyjoy=";
  HttpAuth::ChallengeTokenizer second_challenge(second_challenge_text.begin(),
                                                second_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&second_challenge));
}

}  // namespace net
