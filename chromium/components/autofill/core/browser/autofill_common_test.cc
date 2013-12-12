// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_common_test.h"

#include "base/guid.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webdata/encryptor/encryptor.h"
#include "content/public/browser/browser_context.h"

namespace autofill {
namespace test {

namespace {

const char kSettingsOrigin[] = "Chrome settings";

}  // namespace

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
}

void CreateTestAddressFormData(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

inline void check_and_set(
    FormGroup* profile, ServerFieldType type, const char* value) {
  if (value)
    profile->SetRawInfo(type, UTF8ToUTF16(value));
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  SetProfileInfo(&profile,
                 "John",
                 "H.",
                 "Doe",
                 "johndoe@hades.com",
                 "Underworld",
                 "666 Erebus St.",
                 "Apt 8",
                 "Elysium", "CA",
                 "91111",
                 "US",
                 "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  SetProfileInfo(&profile,
                 "Jane",
                 "A.",
                 "Smith",
                 "jsmith@example.com",
                 "ACME",
                 "123 Main Street",
                 "Unit 1",
                 "Greensdale", "MI",
                 "48838",
                 "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetVerifiedProfile2() {
  AutofillProfile profile(GetFullProfile2());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), "http://www.example.com");
  SetCreditCardInfo(
      &credit_card, "Test User", "4111111111111111" /* Visa */, "11", "2017");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  SetCreditCardInfo(
      &credit_card, "Someone Else", "378282246310005" /* AmEx */, "07", "2019");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

void SetProfileInfo(AutofillProfile* profile,
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email,
                 company, address1, address2, city, state, zipcode, country,
                 phone);
}

void SetCreditCardInfo(CreditCard* credit_card,
    const char* name_on_card, const char* card_number,
    const char* expiration_month, const char* expiration_year) {
  check_and_set(credit_card, CREDIT_CARD_NAME, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
}

void DisableSystemServices(content::BrowserContext* browser_context) {
  // Use a mock Keychain rather than the OS one to store credit card data.
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif

  // Disable auxiliary profiles for unit testing.  These reach out to system
  // services on the Mac.
  if (browser_context) {
    user_prefs::UserPrefs::Get(browser_context)->SetBoolean(
        prefs::kAutofillAuxiliaryProfilesEnabled, false);
  }
}

}  // namespace test
}  // namespace autofill
