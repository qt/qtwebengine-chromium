// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace gaia {

namespace {
const char kGmailDomain[] = "gmail.com";
}

std::string CanonicalizeEmail(const std::string& email_address) {
  std::vector<std::string> parts;
  char at = '@';
  base::SplitString(email_address, at, &parts);
  if (parts.size() != 2U)
    NOTREACHED() << "expecting exactly one @, but got " << parts.size();
  else if (parts[1] == kGmailDomain)  // only strip '.' for gmail accounts.
    base::RemoveChars(parts[0], ".", &parts[0]);
  std::string new_email = StringToLowerASCII(JoinString(parts, at));
  VLOG(1) << "Canonicalized " << email_address << " to " << new_email;
  return new_email;
}

std::string CanonicalizeDomain(const std::string& domain) {
  // Canonicalization of domain names means lower-casing them. Make sure to
  // update this function in sync with Canonicalize if this ever changes.
  return StringToLowerASCII(domain);
}

std::string SanitizeEmail(const std::string& email_address) {
  std::string sanitized(email_address);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos) {
    sanitized += '@';
    sanitized += kGmailDomain;
  }

  return sanitized;
}

bool AreEmailsSame(const std::string& email1, const std::string& email2) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email1)) ==
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(email2));
}

std::string ExtractDomainName(const std::string& email_address) {
  // First canonicalize which will also verify we have proper domain part.
  std::string email = CanonicalizeEmail(email_address);
  size_t separator_pos = email.find('@');
  if (separator_pos != email.npos && separator_pos < email.length() - 1)
    return email.substr(separator_pos + 1);
  else
    NOTREACHED() << "Not a proper email address: " << email;
  return std::string();
}

bool IsGaiaSignonRealm(const GURL& url) {
  if (!url.SchemeIsSecure())
    return false;

  return url == GaiaUrls::GetInstance()->gaia_url();
}


std::vector<std::string> ParseListAccountsData(const std::string& data) {
  std::vector<std::string> account_ids;

  // Parse returned data and make sure we have data.
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value)
    return account_ids;

  base::ListValue* list;
  if (!value->GetAsList(&list) || list->GetSize() < 2)
    return account_ids;

  // Get list of account info.
  base::ListValue* accounts;
  if (!list->GetList(1, &accounts) || accounts == NULL)
    return account_ids;

  // Build a vector of accounts from the cookie.  Order is important: the first
  // account in the list is the primary account.
  for (size_t i = 0; i < accounts->GetSize(); ++i) {
    base::ListValue* account;
    if (accounts->GetList(i, &account) && account != NULL) {
      std::string email;
      if (account->GetString(3, &email) && !email.empty())
        account_ids.push_back(email);
    }
  }

  return account_ids;
}

}  // namespace gaia
