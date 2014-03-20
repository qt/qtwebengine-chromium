// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address.h"

#include <stddef.h>
#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

Address::Address() {}

Address::Address(const Address& address) : FormGroup() {
  *this = address;
}

Address::~Address() {}

Address& Address::operator=(const Address& address) {
  if (this == &address)
    return *this;

  street_address_ = address.street_address_;
  dependent_locality_ = address.dependent_locality_;
  city_ = address.city_;
  state_ = address.state_;
  country_code_ = address.country_code_;
  zip_code_ = address.zip_code_;
  sorting_code_ = address.sorting_code_;
  return *this;
}

base::string16 Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      return street_address_.size() > 0 ? street_address_[0] : base::string16();

    case ADDRESS_HOME_LINE2:
      return street_address_.size() > 1 ? street_address_[1] : base::string16();

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return dependent_locality_;

    case ADDRESS_HOME_CITY:
      return city_;

    case ADDRESS_HOME_STATE:
      return state_;

    case ADDRESS_HOME_ZIP:
      return zip_code_;

    case ADDRESS_HOME_SORTING_CODE:
      return sorting_code_;

    case ADDRESS_HOME_COUNTRY:
      return ASCIIToUTF16(country_code_);

    case ADDRESS_HOME_STREET_ADDRESS:
      return JoinString(street_address_, '\n');

    default:
      NOTREACHED();
      return base::string16();
  }
}

void Address::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      if (street_address_.empty())
        street_address_.resize(1);
      street_address_[0] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_LINE2:
      if (street_address_.size() < 2)
        street_address_.resize(2);
      street_address_[1] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      dependent_locality_ = value;
      break;

    case ADDRESS_HOME_CITY:
      city_ = value;
      break;

    case ADDRESS_HOME_STATE:
      state_ = value;
      break;

    case ADDRESS_HOME_COUNTRY:
      DCHECK(value.empty() ||
             (value.length() == 2u && IsStringASCII(value)));
      country_code_ = UTF16ToASCII(value);
      break;

    case ADDRESS_HOME_ZIP:
      zip_code_ = value;
      break;

    case ADDRESS_HOME_SORTING_CODE:
      sorting_code_ = value;
      break;

    case ADDRESS_HOME_STREET_ADDRESS:
      base::SplitString(value, char16('\n'), &street_address_);
      break;

    default:
      NOTREACHED();
  }
}

base::string16 Address::GetInfo(const AutofillType& type,
                                const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE)
    return ASCIIToUTF16(country_code_);

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !country_code_.empty())
    return AutofillCountry(country_code_, app_locale).name();

  return GetRawInfo(storable_type);
}

bool Address::SetInfo(const AutofillType& type,
                      const base::string16& value,
                      const std::string& app_locale) {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    if (!value.empty() && (value.size() != 2u || !IsStringASCII(value))) {
      country_code_ = std::string();
      return false;
    }

    country_code_ = StringToUpperASCII(UTF16ToASCII(value));
    return true;
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    country_code_ = AutofillCountry::GetCountryCode(value, app_locale);
    return !country_code_.empty();
  }

  // If the address doesn't have any newlines, don't attempt to parse it into
  // lines, since this is potentially a user-entered address in the user's own
  // format, so the code would have to rely on iffy heuristics at best.
  // Instead, just give up when importing addresses like this.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS && !value.empty() &&
      value.find(char16('\n')) == base::string16::npos) {
    street_address_.clear();
    return false;
  }

  SetRawInfo(storable_type, value);

  // Likewise, give up when importing addresses with any entirely blank lines.
  // There's a good chance that this formatting is not intentional, but it's
  // also not obviously safe to just strip the newlines.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS &&
      std::find(street_address_.begin(), street_address_.end(),
                base::string16()) != street_address_.end()) {
    street_address_.clear();
    return false;
  }

  return true;
}

void Address::GetMatchingTypes(const base::string16& text,
                               const std::string& app_locale,
                               ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string country_code = AutofillCountry::GetCountryCode(text, app_locale);
  if (!country_code.empty() && country_code_ == country_code)
    matching_types->insert(ADDRESS_HOME_COUNTRY);
}

void Address::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_STREET_ADDRESS);
  supported_types->insert(ADDRESS_HOME_DEPENDENT_LOCALITY);
  supported_types->insert(ADDRESS_HOME_CITY);
  supported_types->insert(ADDRESS_HOME_STATE);
  supported_types->insert(ADDRESS_HOME_ZIP);
  supported_types->insert(ADDRESS_HOME_SORTING_CODE);
  supported_types->insert(ADDRESS_HOME_COUNTRY);
}

void Address::TrimStreetAddress() {
  while (!street_address_.empty() && street_address_.back().empty()) {
    street_address_.pop_back();
  }
}

}  // namespace autofill
