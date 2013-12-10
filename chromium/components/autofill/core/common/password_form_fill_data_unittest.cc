// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests that the when there is a single preferred match, and no extra
// matches, the PasswordFormFillData is filled in correctly.
TEST(PasswordFormFillDataTest, TestSinglePreferredMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16("test@gmail.com");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16("test");
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.ssl_valid = true;
  form_on_page.preferred = false;
  form_on_page.scheme = PasswordForm::SCHEME_HTML;

  // Create an exact match in the database.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://foo.com/");
  preferred_match.action = GURL("https://foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.ssl_valid = true;
  preferred_match.preferred = true;
  preferred_match.scheme = PasswordForm::SCHEME_HTML;

  PasswordFormMap matches;

  PasswordFormFillData result;
  InitPasswordFormFillData(form_on_page,
                           matches,
                           &preferred_match,
                           true,
                           false,
                           &result);

  // |wait_for_username| should reflect the |wait_for_username_before_autofill|
  // argument of InitPasswordFormFillData which in this case is true.
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should be empty since it's the same as the realm of
  // the form.
  EXPECT_EQ(result.preferred_realm, "");

  PasswordFormFillData result2;
  InitPasswordFormFillData(form_on_page,
                           matches,
                           &preferred_match,
                           false,
                           false,
                           &result2);

  // |wait_for_username| should reflect the |wait_for_username_before_autofill|
  // argument of InitPasswordFormFillData which in this case is false.
  EXPECT_FALSE(result2.wait_for_username);
}

// Tests that the InitPasswordFormFillData behaves correctly when there is a
// preferred match that was found using public suffix matching, an additional
// result that also used public suffix matching, and a third result that was
// found without using public suffix matching.
TEST(PasswordFormFillDataTest, TestPublicSuffixDomainMatching) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16("test@gmail.com");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16("test");
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.ssl_valid = true;
  form_on_page.preferred = false;
  form_on_page.scheme = PasswordForm::SCHEME_HTML;

  // Create a match from the database that matches using public suffix.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://mobile.foo.com/");
  preferred_match.action = GURL("https://mobile.foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://mobile.foo.com/";
  preferred_match.original_signon_realm = "https://foo.com/";
  preferred_match.ssl_valid = true;
  preferred_match.preferred = true;
  preferred_match.scheme = PasswordForm::SCHEME_HTML;

  // Create a match that matches exactly, so |original_signon_realm| is not set.
  PasswordForm exact_match;
  exact_match.origin = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16("test");
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.ssl_valid = true;
  exact_match.preferred = false;
  exact_match.scheme = PasswordForm::SCHEME_HTML;

  // Create a match that was matched using public suffix, so
  // |original_signon_realm| is set to where the result came from.
  PasswordForm public_suffix_match;
  public_suffix_match.origin = GURL("https://foo.com/");
  public_suffix_match.action = GURL("https://foo.com/login");
  public_suffix_match.username_element = ASCIIToUTF16("username");
  public_suffix_match.username_value = ASCIIToUTF16("test2@gmail.com");
  public_suffix_match.password_element = ASCIIToUTF16("password");
  public_suffix_match.password_value = ASCIIToUTF16("test");
  public_suffix_match.submit_element = ASCIIToUTF16("");
  public_suffix_match.original_signon_realm = "https://subdomain.foo.com/";
  public_suffix_match.signon_realm = "https://foo.com/";
  public_suffix_match.ssl_valid = true;
  public_suffix_match.preferred = false;
  public_suffix_match.scheme = PasswordForm::SCHEME_HTML;

  // Add one exact match and one public suffix match.
  PasswordFormMap matches;
  matches[exact_match.username_value] = &exact_match;
  matches[public_suffix_match.username_value] = &public_suffix_match;

  PasswordFormFillData result;
  InitPasswordFormFillData(form_on_page,
                           matches,
                           &preferred_match,
                           true,
                           false,
                           &result);
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should match the original signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(result.preferred_realm,
            preferred_match.original_signon_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      result.additional_logins.find(exact_match.username_value);
  EXPECT_EQ(iter->second.realm, "");

  // The realm of the public suffix match should be set to the original signon
  // realm so the user can see where the result came from.
  iter = result.additional_logins.find(public_suffix_match.username_value);
  EXPECT_EQ(iter->second.realm, public_suffix_match.original_signon_realm);
}

}  // namespace autofill
