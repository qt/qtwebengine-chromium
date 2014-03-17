// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/phone_number.h"

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/phone_number_i18n.h"

namespace autofill {
namespace {

const char16 kPhoneNumberSeparators[] = { ' ', '.', '(', ')', '-', 0 };

void StripPunctuation(base::string16* number) {
  base::RemoveChars(*number, kPhoneNumberSeparators, number);
}

// Returns the region code for this phone number, which is an ISO 3166 2-letter
// country code.  The returned value is based on the |profile|; if the |profile|
// does not have a country code associated with it, falls back to the country
// code corresponding to the |app_locale|.
std::string GetRegion(const AutofillProfile& profile,
                      const std::string& app_locale) {
  base::string16 country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    return UTF16ToASCII(country_code);

  return AutofillCountry::CountryCodeForLocale(app_locale);
}

}  // namespace

PhoneNumber::PhoneNumber(AutofillProfile* profile)
    : profile_(profile) {
}

PhoneNumber::PhoneNumber(const PhoneNumber& number)
    : profile_(NULL) {
  *this = number;
}

PhoneNumber::~PhoneNumber() {}

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;

  number_ = number.number_;
  profile_ = number.profile_;
  cached_parsed_phone_ = number.cached_parsed_phone_;
  return *this;
}

void PhoneNumber::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(PHONE_HOME_WHOLE_NUMBER);
  supported_types->insert(PHONE_HOME_NUMBER);
  supported_types->insert(PHONE_HOME_CITY_CODE);
  supported_types->insert(PHONE_HOME_CITY_AND_NUMBER);
  supported_types->insert(PHONE_HOME_COUNTRY_CODE);
}

base::string16 PhoneNumber::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(PHONE_HOME, AutofillType(type).group());
  if (type == PHONE_HOME_WHOLE_NUMBER)
    return number_;

  // Only the whole number is available as raw data.  All of the other types are
  // parsed from this raw info, and parsing requires knowledge of the phone
  // number's region, which is only available via GetInfo().
  return base::string16();
}

void PhoneNumber::SetRawInfo(ServerFieldType type,
                             const base::string16& value) {
  DCHECK_EQ(PHONE_HOME, AutofillType(type).group());
  if (type != PHONE_HOME_CITY_AND_NUMBER && type != PHONE_HOME_WHOLE_NUMBER) {
    // Only full phone numbers should be set directly.  The remaining field
    // field types are read-only.
    return;
  }

  number_ = value;

  // Invalidate the cached number.
  cached_parsed_phone_ = i18n::PhoneObject();
}

// Normalize phones if |type| is a whole number:
//   (650)2345678 -> 6502345678
//   1-800-FLOWERS -> 18003569377
// If the phone cannot be normalized, returns the stored value verbatim.
base::string16 PhoneNumber::GetInfo(const AutofillType& type,
                                    const std::string& app_locale) const {
  ServerFieldType storable_type = type.GetStorableType();
  UpdateCacheIfNeeded(app_locale);

  // Queries for whole numbers will return the non-normalized number if
  // normalization for the number fails.  All other field types require
  // normalization.
  if (storable_type != PHONE_HOME_WHOLE_NUMBER &&
      !cached_parsed_phone_.IsValidNumber())
    return base::string16();

  switch (storable_type) {
    case PHONE_HOME_WHOLE_NUMBER:
      return cached_parsed_phone_.GetWholeNumber();

    case PHONE_HOME_NUMBER:
      return cached_parsed_phone_.number();

    case PHONE_HOME_CITY_CODE:
      return cached_parsed_phone_.city_code();

    case PHONE_HOME_COUNTRY_CODE:
      return cached_parsed_phone_.country_code();

    case PHONE_HOME_CITY_AND_NUMBER:
      return
          cached_parsed_phone_.city_code() + cached_parsed_phone_.number();

    default:
      NOTREACHED();
      return base::string16();
  }
}

bool PhoneNumber::SetInfo(const AutofillType& type,
                          const base::string16& value,
                          const std::string& app_locale) {
  SetRawInfo(type.GetStorableType(), value);

  if (number_.empty())
    return true;

  // Store a formatted (i.e., pretty printed) version of the number.
  UpdateCacheIfNeeded(app_locale);
  number_ = cached_parsed_phone_.GetFormattedNumber();
  return !number_.empty();
}

void PhoneNumber::GetMatchingTypes(const base::string16& text,
                                   const std::string& app_locale,
                                   ServerFieldTypeSet* matching_types) const {
  base::string16 stripped_text = text;
  StripPunctuation(&stripped_text);
  FormGroup::GetMatchingTypes(stripped_text, app_locale, matching_types);

  // For US numbers, also compare to the three-digit prefix and the four-digit
  // suffix, since web sites often split numbers into these two fields.
  base::string16 number = GetInfo(AutofillType(PHONE_HOME_NUMBER), app_locale);
  if (GetRegion(*profile_, app_locale) == "US" &&
      number.size() == (kPrefixLength + kSuffixLength)) {
    base::string16 prefix = number.substr(kPrefixOffset, kPrefixLength);
    base::string16 suffix = number.substr(kSuffixOffset, kSuffixLength);
    if (text == prefix || text == suffix)
      matching_types->insert(PHONE_HOME_NUMBER);
  }

  base::string16 whole_number =
      GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), app_locale);
  if (!whole_number.empty()) {
    base::string16 normalized_number =
        i18n::NormalizePhoneNumber(text, GetRegion(*profile_, app_locale));
    if (normalized_number == whole_number)
      matching_types->insert(PHONE_HOME_WHOLE_NUMBER);
  }
}

void PhoneNumber::UpdateCacheIfNeeded(const std::string& app_locale) const {
  std::string region = GetRegion(*profile_, app_locale);
  if (!number_.empty() && cached_parsed_phone_.region() != region)
    cached_parsed_phone_ = i18n::PhoneObject(number_, region);
}

PhoneNumber::PhoneCombineHelper::PhoneCombineHelper() {
}

PhoneNumber::PhoneCombineHelper::~PhoneCombineHelper() {
}

bool PhoneNumber::PhoneCombineHelper::SetInfo(const AutofillType& type,
                                              const base::string16& value) {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == PHONE_HOME_COUNTRY_CODE) {
    country_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_CITY_CODE) {
    city_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_CITY_AND_NUMBER) {
    phone_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_WHOLE_NUMBER) {
    whole_number_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_NUMBER) {
    phone_.append(value);
    return true;
  }

  return false;
}

bool PhoneNumber::PhoneCombineHelper::ParseNumber(
    const AutofillProfile& profile,
    const std::string& app_locale,
    base::string16* value) {
  if (IsEmpty())
    return false;

  if (!whole_number_.empty()) {
    *value = whole_number_;
    return true;
  }

  return i18n::ConstructPhoneNumber(
      country_, city_, phone_, GetRegion(profile, app_locale), value);
}

bool PhoneNumber::PhoneCombineHelper::IsEmpty() const {
  return phone_.empty() && whole_number_.empty();
}

}  // namespace autofill
