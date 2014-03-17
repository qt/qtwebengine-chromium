// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card.h"

#include <stddef.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/component_strings.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(blundell): Eliminate the need for this conditional include.
// crbug.com/328150
#if !defined(OS_IOS)
#include "grit/webkit_resources.h"
#endif

namespace autofill {

namespace {

const char16 kCreditCardObfuscationSymbol = '*';

// This is the maximum obfuscated symbols displayed.
// It is introduced to avoid rare cases where the credit card number is
// too large and fills the screen.
const size_t kMaxObfuscationSize = 20;

bool ConvertYear(const base::string16& year, int* num) {
  // If the |year| is empty, clear the stored value.
  if (year.empty()) {
    *num = 0;
    return true;
  }

  // Try parsing the |year| as a number.
  if (base::StringToInt(year, num))
    return true;

  *num = 0;
  return false;
}

bool ConvertMonth(const base::string16& month,
                  const std::string& app_locale,
                  int* num) {
  // If the |month| is empty, clear the stored value.
  if (month.empty()) {
    *num = 0;
    return true;
  }

  // Try parsing the |month| as a number.
  if (base::StringToInt(month, num))
    return true;

  // If the locale is unknown, give up.
  if (app_locale.empty())
    return false;

  // Otherwise, try parsing the |month| as a named month, e.g. "January" or
  // "Jan".
  base::string16 lowercased_month = StringToLowerASCII(month);

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale(app_locale.c_str());
  icu::DateFormatSymbols date_format_symbols(locale, status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);

  int32_t num_months;
  const icu::UnicodeString* months = date_format_symbols.getMonths(num_months);
  for (int32_t i = 0; i < num_months; ++i) {
    const base::string16 icu_month = base::string16(months[i].getBuffer(),
                                        months[i].length());
    if (lowercased_month == StringToLowerASCII(icu_month)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }

  months = date_format_symbols.getShortMonths(num_months);
  for (int32_t i = 0; i < num_months; ++i) {
    const base::string16 icu_month = base::string16(months[i].getBuffer(),
                                        months[i].length());
    if (lowercased_month == StringToLowerASCII(icu_month)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }

  *num = 0;
  return false;
}

}  // namespace

CreditCard::CreditCard(const std::string& guid, const std::string& origin)
    : AutofillDataModel(guid, origin),
      type_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0) {
}

CreditCard::CreditCard()
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      type_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0) {
}

CreditCard::CreditCard(const CreditCard& credit_card)
    : AutofillDataModel(std::string(), std::string()) {
  operator=(credit_card);
}

CreditCard::~CreditCard() {}

// static
const base::string16 CreditCard::StripSeparators(const base::string16& number) {
  const base::char16 kSeparators[] = {'-', ' ', '\0'};
  base::string16 stripped;
  base::RemoveChars(number, kSeparators, &stripped);
  return stripped;
}

// static
base::string16 CreditCard::TypeForDisplay(const std::string& type) {
  if (type == kAmericanExpressCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  if (type == kDinersCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  if (type == kDiscoverCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  if (type == kJCBCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  if (type == kMasterCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  if (type == kUnionPay)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  if (type == kVisaCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, type);
  return base::string16();
}

// static
int CreditCard::IconResourceId(const std::string& type) {
  // TODO(blundell): Either move these resources out of webkit_resources or
  // this function into //components/autofill/content/browser to eliminate the
  // need for this ifdef-ing. crbug.com/328150
#if defined(OS_IOS)
  return 0;
#else
  if (type == kAmericanExpressCard)
    return IDR_AUTOFILL_CC_AMEX;
  if (type == kDinersCard)
    return IDR_AUTOFILL_CC_DINERS;
  if (type == kDiscoverCard)
    return IDR_AUTOFILL_CC_DISCOVER;
  if (type == kJCBCard)
    return IDR_AUTOFILL_CC_JCB;
  if (type == kMasterCard)
    return IDR_AUTOFILL_CC_MASTERCARD;
  if (type == kUnionPay)
    return IDR_AUTOFILL_CC_GENERIC;  // Needs resource: http://crbug.com/259211
  if (type == kVisaCard)
    return IDR_AUTOFILL_CC_VISA;

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, type);
  return IDR_AUTOFILL_CC_GENERIC;
#endif  // defined(OS_IOS)
}

// static
std::string CreditCard::GetCreditCardType(const base::string16& number) {
  // Credit card number specifications taken from:
  // http://en.wikipedia.org/wiki/Credit_card_numbers,
  // http://en.wikipedia.org/wiki/List_of_Issuer_Identification_Numbers,
  // http://www.discovernetwork.com/merchants/images/Merchant_Marketing_PDF.pdf,
  // http://www.regular-expressions.info/creditcard.html,
  // http://developer.ean.com/general_info/Valid_Credit_Card_Types,
  // http://www.bincodes.com/,
  // http://www.fraudpractice.com/FL-binCC.html, and
  // http://www.beachnet.com/~hstiles/cardtype.html
  //
  // The last site is currently unavailable, but a cached version remains at
  // http://web.archive.org/web/20120923111349/http://www.beachnet.com/~hstiles/cardtype.html
  //
  // Card Type              Prefix(es)                      Length
  // ---------------------------------------------------------------
  // Visa                   4                               13,16
  // American Express       34,37                           15
  // Diners Club            300-305,3095,36,38-39           14
  // Discover Card          6011,644-649,65                 16
  // JCB                    3528-3589                       16
  // MasterCard             51-55                           16
  // UnionPay               62                              16-19

  // Check for prefixes of length 1.
  if (number.empty())
    return kGenericCard;

  if (number[0] == '4')
    return kVisaCard;

  // Check for prefixes of length 2.
  if (number.size() < 2)
    return kGenericCard;

  int first_two_digits = 0;
  if (!base::StringToInt(number.substr(0, 2), &first_two_digits))
    return kGenericCard;

  if (first_two_digits == 34 || first_two_digits == 37)
    return kAmericanExpressCard;

  if (first_two_digits == 36 ||
      first_two_digits == 38 ||
      first_two_digits == 39)
    return kDinersCard;

  if (first_two_digits >= 51 && first_two_digits <= 55)
    return kMasterCard;

  if (first_two_digits == 62)
    return kUnionPay;

  if (first_two_digits == 65)
    return kDiscoverCard;

  // Check for prefixes of length 3.
  if (number.size() < 3)
    return kGenericCard;

  int first_three_digits = 0;
  if (!base::StringToInt(number.substr(0, 3), &first_three_digits))
    return kGenericCard;

  if (first_three_digits >= 300 && first_three_digits <= 305)
    return kDinersCard;

  if (first_three_digits >= 644 && first_three_digits <= 649)
    return kDiscoverCard;

  // Check for prefixes of length 4.
  if (number.size() < 4)
    return kGenericCard;

  int first_four_digits = 0;
  if (!base::StringToInt(number.substr(0, 4), &first_four_digits))
    return kGenericCard;

  if (first_four_digits == 3095)
    return kDinersCard;

  if (first_four_digits >= 3528 && first_four_digits <= 3589)
    return kJCBCard;

  if (first_four_digits == 6011)
    return kDiscoverCard;

  return kGenericCard;
}

base::string16 CreditCard::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(CREDIT_CARD, AutofillType(type).group());
  switch (type) {
    case CREDIT_CARD_NAME:
      return name_on_card_;

    case CREDIT_CARD_EXP_MONTH:
      return ExpirationMonthAsString();

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return Expiration2DigitYearAsString();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return Expiration4DigitYearAsString();

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      base::string16 month = ExpirationMonthAsString();
      base::string16 year = Expiration2DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return base::string16();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      base::string16 month = ExpirationMonthAsString();
      base::string16 year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return base::string16();
    }

    case CREDIT_CARD_TYPE:
      return TypeForDisplay();

    case CREDIT_CARD_NUMBER:
      return number_;

    case CREDIT_CARD_VERIFICATION_CODE:
      // Chrome doesn't store credit card verification codes.
      return base::string16();

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return base::string16();
  }
}

void CreditCard::SetRawInfo(ServerFieldType type,
                            const base::string16& value) {
  DCHECK_EQ(CREDIT_CARD, AutofillType(type).group());
  switch (type) {
    case CREDIT_CARD_NAME:
      name_on_card_ = value;
      break;

    case CREDIT_CARD_EXP_MONTH:
      SetExpirationMonthFromString(value, std::string());
      break;

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_TYPE:
      // This is a read-only attribute, determined by the credit card number.
      break;

    case CREDIT_CARD_NUMBER: {
      // Don't change the real value if the input is an obfuscated string.
      if (value.size() > 0 && value[0] != kCreditCardObfuscationSymbol)
        SetNumber(value);
      break;
    }

    case CREDIT_CARD_VERIFICATION_CODE:
      // Chrome doesn't store the credit card verification code.
      break;

    default:
      NOTREACHED() << "Attempting to set unknown info-type " << type;
      break;
  }
}

base::string16 CreditCard::GetInfo(const AutofillType& type,
                                   const std::string& app_locale) const {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_NUMBER)
    return StripSeparators(number_);

  return GetRawInfo(storable_type);
}

bool CreditCard::SetInfo(const AutofillType& type,
                         const base::string16& value,
                         const std::string& app_locale) {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_NUMBER)
    SetRawInfo(storable_type, StripSeparators(value));
  else if (storable_type == CREDIT_CARD_EXP_MONTH)
    SetExpirationMonthFromString(value, app_locale);
  else
    SetRawInfo(storable_type, value);

  return true;
}

void CreditCard::GetMatchingTypes(const base::string16& text,
                                  const std::string& app_locale,
                                  ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  base::string16 card_number =
      GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale);
  if (!card_number.empty() && StripSeparators(text) == card_number)
    matching_types->insert(CREDIT_CARD_NUMBER);

  int month;
  if (ConvertMonth(text, app_locale, &month) && month != 0 &&
      month == expiration_month_) {
    matching_types->insert(CREDIT_CARD_EXP_MONTH);
  }
}

const base::string16 CreditCard::Label() const {
  base::string16 label;
  if (number().empty())
    return name_on_card_;  // No CC number, return name only.

  base::string16 obfuscated_cc_number = ObfuscatedNumber();
  if (!expiration_month_ || !expiration_year_)
    return obfuscated_cc_number;  // No expiration date set.

  // TODO(georgey): Internationalize date.
  base::string16 formatted_date(ExpirationMonthAsString());
  formatted_date.append(ASCIIToUTF16("/"));
  formatted_date.append(Expiration4DigitYearAsString());

  label = l10n_util::GetStringFUTF16(IDS_CREDIT_CARD_NUMBER_PREVIEW_FORMAT,
                                     obfuscated_cc_number,
                                     formatted_date);
  return label;
}

void CreditCard::SetInfoForMonthInputType(const base::string16& value) {
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  if (!autofill::MatchesPattern(value, UTF8ToUTF16("^[0-9]{4}-[0-9]{1,2}$")))
    return;

  std::vector<base::string16> year_month;
  base::SplitString(value, L'-', &year_month);
  DCHECK_EQ((int)year_month.size(), 2);
  int num = 0;
  bool converted = false;
  converted = base::StringToInt(year_month[0], &num);
  DCHECK(converted);
  SetExpirationYear(num);
  converted = base::StringToInt(year_month[1], &num);
  DCHECK(converted);
  SetExpirationMonth(num);
}

base::string16 CreditCard::ObfuscatedNumber() const {
  // If the number is shorter than four digits, there's no need to obfuscate it.
  if (number_.size() < 4)
    return number_;

  base::string16 number = StripSeparators(number_);

  // Avoid making very long obfuscated numbers.
  size_t obfuscated_digits = std::min(kMaxObfuscationSize, number.size() - 4);
  base::string16 result(obfuscated_digits, kCreditCardObfuscationSymbol);
  return result.append(LastFourDigits());
}

base::string16 CreditCard::LastFourDigits() const {
  static const size_t kNumLastDigits = 4;

  base::string16 number = StripSeparators(number_);
  if (number.size() < kNumLastDigits)
    return base::string16();

  return number.substr(number.size() - kNumLastDigits, kNumLastDigits);
}

base::string16 CreditCard::TypeForDisplay() const {
  return CreditCard::TypeForDisplay(type_);
}

base::string16 CreditCard::TypeAndLastFourDigits() const {
  base::string16 type = TypeForDisplay();
  // TODO(estade): type may be empty, we probably want to return
  // "Card - 1234" or something in that case.

  base::string16 digits = LastFourDigits();
  if (digits.empty())
    return type;

  // TODO(estade): i18n.
  return type + ASCIIToUTF16(" - ") + digits;
}

void CreditCard::operator=(const CreditCard& credit_card) {
  if (this == &credit_card)
    return;

  number_ = credit_card.number_;
  name_on_card_ = credit_card.name_on_card_;
  type_ = credit_card.type_;
  expiration_month_ = credit_card.expiration_month_;
  expiration_year_ = credit_card.expiration_year_;

  set_guid(credit_card.guid());
  set_origin(credit_card.origin());
}

bool CreditCard::UpdateFromImportedCard(const CreditCard& imported_card,
                                        const std::string& app_locale) {
  if (this->GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale) !=
          imported_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale)) {
    return false;
  }

  // Heuristically aggregated data should never overwrite verified data.
  // Instead, discard any heuristically aggregated credit cards that disagree
  // with explicitly entered data, so that the UI is not cluttered with
  // duplicate cards.
  if (this->IsVerified() && !imported_card.IsVerified())
    return true;

  set_origin(imported_card.origin());

  // Note that the card number is intentionally not updated, so as to preserve
  // any formatting (i.e. separator characters).  Since the card number is not
  // updated, there is no reason to update the card type, either.
  if (!imported_card.name_on_card_.empty())
    name_on_card_ = imported_card.name_on_card_;

  // The expiration date for |imported_card| should always be set.
  DCHECK(imported_card.expiration_month_ && imported_card.expiration_year_);
  expiration_month_ = imported_card.expiration_month_;
  expiration_year_ = imported_card.expiration_year_;

  return true;
}

int CreditCard::Compare(const CreditCard& credit_card) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // credit card.
  const ServerFieldType types[] = { CREDIT_CARD_NAME,
                                    CREDIT_CARD_NUMBER,
                                    CREDIT_CARD_EXP_MONTH,
                                    CREDIT_CARD_EXP_4_DIGIT_YEAR };
  for (size_t i = 0; i < arraysize(types); ++i) {
    int comparison =
        GetRawInfo(types[i]).compare(credit_card.GetRawInfo(types[i]));
    if (comparison != 0)
      return comparison;
  }

  return 0;
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  return guid() == credit_card.guid() &&
         origin() == credit_card.origin() &&
         Compare(credit_card) == 0;
}

bool CreditCard::operator!=(const CreditCard& credit_card) const {
  return !operator==(credit_card);
}

bool CreditCard::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool CreditCard::IsComplete() const {
  return
      autofill::IsValidCreditCardNumber(number_) &&
      expiration_month_ != 0 &&
      expiration_year_ != 0;
}

bool CreditCard::IsValid() const {
  return autofill::IsValidCreditCardNumber(number_) &&
         autofill::IsValidCreditCardExpirationDate(
             expiration_year_, expiration_month_, base::Time::Now());
}

void CreditCard::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(CREDIT_CARD_NAME);
  supported_types->insert(CREDIT_CARD_NUMBER);
  supported_types->insert(CREDIT_CARD_TYPE);
  supported_types->insert(CREDIT_CARD_EXP_MONTH);
  supported_types->insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
}

base::string16 CreditCard::ExpirationMonthAsString() const {
  if (expiration_month_ == 0)
    return base::string16();

  base::string16 month = base::IntToString16(expiration_month_);
  if (expiration_month_ >= 10)
    return month;

  base::string16 zero = ASCIIToUTF16("0");
  zero.append(month);
  return zero;
}

base::string16 CreditCard::Expiration4DigitYearAsString() const {
  if (expiration_year_ == 0)
    return base::string16();

  return base::IntToString16(Expiration4DigitYear());
}

base::string16 CreditCard::Expiration2DigitYearAsString() const {
  if (expiration_year_ == 0)
    return base::string16();

  return base::IntToString16(Expiration2DigitYear());
}

void CreditCard::SetExpirationMonthFromString(const base::string16& text,
                                              const std::string& app_locale) {
  int month;
  if (!ConvertMonth(text, app_locale, &month))
    return;

  SetExpirationMonth(month);
}

void CreditCard::SetExpirationYearFromString(const base::string16& text) {
  int year;
  if (!ConvertYear(text, &year))
    return;

  SetExpirationYear(year);
}

void CreditCard::SetNumber(const base::string16& number) {
  number_ = number;
  type_ = GetCreditCardType(StripSeparators(number_));
}

void CreditCard::SetExpirationMonth(int expiration_month) {
  if (expiration_month < 0 || expiration_month > 12)
    return;

  expiration_month_ = expiration_month;
}

void CreditCard::SetExpirationYear(int expiration_year) {
  if (expiration_year != 0 &&
      (expiration_year < 2006 || expiration_year > 10000)) {
    return;
  }

  expiration_year_ = expiration_year;
}

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card) {
  return os
      << UTF16ToUTF8(credit_card.Label())
      << " "
      << credit_card.guid()
      << " "
      << credit_card.origin()
      << " "
      << UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NAME))
      << " "
      << UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_TYPE))
      << " "
      << UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NUMBER))
      << " "
      << UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_EXP_MONTH))
      << " "
      << UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

// These values must match the values in WebKitPlatformSupportImpl in
// webkit/glue. We send these strings to WebKit, which then asks
// WebKitPlatformSupportImpl to load the image data.
const char* const kAmericanExpressCard = "americanExpressCC";
const char* const kDinersCard = "dinersCC";
const char* const kDiscoverCard = "discoverCC";
const char* const kGenericCard = "genericCC";
const char* const kJCBCard = "jcbCC";
const char* const kMasterCard = "masterCardCC";
const char* const kUnionPay = "unionPayCC";
const char* const kVisaCard = "visaCC";

}  // namespace autofill
