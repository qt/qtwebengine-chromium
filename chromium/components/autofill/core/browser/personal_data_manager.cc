// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill-inl.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

using content::BrowserContext;

namespace autofill {
namespace {

const base::string16::value_type kCreditCardPrefix[] = {'*', 0};

template<typename T>
class FormGroupMatchesByGUIDFunctor {
 public:
  explicit FormGroupMatchesByGUIDFunctor(const std::string& guid)
      : guid_(guid) {
  }

  bool operator()(const T& form_group) {
    return form_group.guid() == guid_;
  }

  bool operator()(const T* form_group) {
    return form_group->guid() == guid_;
  }

 private:
  std::string guid_;
};

template<typename T, typename C>
bool FindByGUID(const C& container, const std::string& guid) {
  return std::find_if(
      container.begin(),
      container.end(),
      FormGroupMatchesByGUIDFunctor<T>(guid)) != container.end();
}

template<typename T>
class DereferenceFunctor {
 public:
  template<typename T_Iterator>
  const T& operator()(const T_Iterator& iterator) {
    return *iterator;
  }
};

template<typename T>
T* address_of(T& v) {
  return &v;
}

// Returns true if minimum requirements for import of a given |profile| have
// been met.  An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is preformed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& app_locale) {
  // All countries require at least one address line.
  if (profile.GetRawInfo(ADDRESS_HOME_LINE1).empty())
    return false;
  std::string country_code =
      UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));

  if (country_code.empty())
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);

  AutofillCountry country(country_code, app_locale);

  if (country.requires_city() && profile.GetRawInfo(ADDRESS_HOME_CITY).empty())
    return false;

  if (country.requires_state() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty())
    return false;

  if (country.requires_zip() && profile.GetRawInfo(ADDRESS_HOME_ZIP).empty())
    return false;

  return true;
}

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(const std::set<ServerFieldType>& types_seen,
                              ServerFieldType field_type,
                              const base::string16& value) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for PHONE_HOME_NUMBER however as both prefix and
  // suffix are stored against this type, and for EMAIL_ADDRESS because it is
  // common to see second 'confirm email address' fields on forms.
  if (types_seen.count(field_type) &&
      field_type != PHONE_HOME_NUMBER &&
      field_type != EMAIL_ADDRESS)
    return false;

  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value))
    return false;

  return true;
}

}  // namespace

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : browser_context_(NULL),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_creditcards_query_(0),
      app_locale_(app_locale),
      metric_logger_(new AutofillMetrics),
      has_logged_profile_count_(false) {}

void PersonalDataManager::Init(BrowserContext* browser_context) {
  browser_context_ = browser_context;

  if (!browser_context_->IsOffTheRecord())
    metric_logger_->LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));

  // WebDataService may not be available in tests.
  if (!autofill_data.get())
    return;

  LoadProfiles();
  LoadCreditCards();

  autofill_data->AddObserver(this);
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);

  if (!browser_context_)
    return;

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (autofill_data.get())
    autofill_data->RemoveObserver(this);
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_profiles_query_ || pending_creditcards_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_)
      pending_creditcards_query_ = 0;
    else if (h == pending_profiles_query_)
      pending_profiles_query_ = 0;
    return;
  }

  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT ||
         result->GetType() == AUTOFILL_CREDITCARDS_RESULT);

  switch (result->GetType()) {
    case AUTOFILL_PROFILES_RESULT:
      ReceiveLoadedProfiles(h, result);
      break;
    case AUTOFILL_CREDITCARDS_RESULT:
      ReceiveLoadedCreditCards(h, result);
      break;
    default:
      NOTREACHED();
  }

  // If both requests have responded, then all personal data is loaded.
  if (pending_profiles_query_ == 0 && pending_creditcards_query_ == 0) {
    is_data_loaded_ = true;
    std::vector<AutofillProfile*> profile_pointers(web_profiles_.size());
    std::copy(web_profiles_.begin(), web_profiles_.end(),
              profile_pointers.begin());
    AutofillProfile::AdjustInferredLabels(&profile_pointers);
    FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                      OnPersonalDataChanged());
  }
}

void PersonalDataManager::AutofillMultipleChanged() {
  Refresh();
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool PersonalDataManager::ImportFormData(
    const FormStructure& form,
    const CreditCard** imported_credit_card) {
  scoped_ptr<AutofillProfile> imported_profile(new AutofillProfile);
  scoped_ptr<CreditCard> local_imported_credit_card(new CreditCard);

  const std::string origin = form.source_url().spec();
  imported_profile->set_origin(origin);
  local_imported_credit_card->set_origin(origin);

  // Parse the form and construct a profile based on the information that is
  // possible to import.
  int importable_credit_card_fields = 0;

  // Detect and discard forms with multiple fields of the same type.
  // TODO(isherman): Some types are overlapping but not equal, e.g. phone number
  // parts, address parts.
  std::set<ServerFieldType> types_seen;

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper home;

  for (size_t i = 0; i < form.field_count(); ++i) {
    const AutofillField* field = form.field(i);
    base::string16 value = CollapseWhitespace(field->value, false);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    AutofillType field_type = field->Type();
    ServerFieldType server_field_type = field_type.GetStorableType();
    FieldTypeGroup group(field_type.group());

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    if (server_field_type == EMAIL_ADDRESS) {
      if (types_seen.count(server_field_type) &&
          imported_profile->GetRawInfo(EMAIL_ADDRESS) != value) {
        imported_profile.reset();
        break;
      }
    }

    // If the |field_type| and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value)) {
      imported_profile.reset();
      local_imported_credit_card.reset();
      break;
    }

    types_seen.insert(server_field_type);

    if (group == CREDIT_CARD) {
      if (LowerCaseEqualsASCII(field->form_control_type, "month")) {
        DCHECK_EQ(CREDIT_CARD_EXP_MONTH, server_field_type);
        local_imported_credit_card->SetInfoForMonthInputType(value);
      } else {
        local_imported_credit_card->SetInfo(field_type, value, app_locale_);
      }
      ++importable_credit_card_fields;
    } else {
      // We need to store phone data in the variables, before building the whole
      // number at the end. The rest of the fields are set "as is".
      // If the fields are not the phone fields in question home.SetInfo() is
      // going to return false.
      if (!home.SetInfo(field_type, value))
        imported_profile->SetInfo(field_type, value, app_locale_);

      // Reject profiles with invalid country information.
      if (server_field_type == ADDRESS_HOME_COUNTRY &&
          !value.empty() &&
          imported_profile->GetRawInfo(ADDRESS_HOME_COUNTRY).empty()) {
        imported_profile.reset();
        break;
      }
    }
  }

  // Construct the phone number. Reject the profile if the number is invalid.
  if (imported_profile.get() && !home.IsEmpty()) {
    base::string16 constructed_number;
    if (!home.ParseNumber(*imported_profile, app_locale_,
                          &constructed_number) ||
        !imported_profile->SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                                   constructed_number,
                                   app_locale_)) {
      imported_profile.reset();
    }
  }

  // Reject the profile if minimum address and validation requirements are not
  // met.
  if (imported_profile.get() &&
      !IsValidLearnableProfile(*imported_profile, app_locale_))
    imported_profile.reset();

  // Reject the credit card if we did not detect enough filled credit card
  // fields or if the credit card number does not seem to be valid.
  if (local_imported_credit_card.get() &&
      !local_imported_credit_card->IsComplete()) {
    local_imported_credit_card.reset();
  }

  // Don't import if we already have this info.
  // Don't present an infobar if we have already saved this card number.
  bool merged_credit_card = false;
  if (local_imported_credit_card.get()) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards_.begin();
         iter != credit_cards_.end();
         ++iter) {
      // Make a local copy so that the data in |credit_cards_| isn't modified
      // directly by the UpdateFromImportedCard() call.
      CreditCard card = **iter;
      if (card.UpdateFromImportedCard(*local_imported_credit_card.get(),
                                      app_locale_)) {
        merged_credit_card = true;
        UpdateCreditCard(card);
        local_imported_credit_card.reset();
        break;
      }
    }
  }

  if (imported_profile.get()) {
    // We always save imported profiles.
    SaveImportedProfile(*imported_profile);
  }
  *imported_credit_card = local_imported_credit_card.release();

  if (imported_profile.get() || *imported_credit_card || merged_credit_card) {
    return true;
  } else {
    FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                      OnInsufficientFormData());
    return false;
  }
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  if (browser_context_->IsOffTheRecord())
    return;

  if (profile.IsEmpty(app_locale_))
    return;

  // Don't add an existing profile.
  if (FindByGUID<AutofillProfile>(web_profiles_, profile.guid()))
    return;

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(web_profiles_, profile))
    return;

  // Add the new profile to the web database.
  autofill_data->AddAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (browser_context_->IsOffTheRecord())
    return;

  AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
  if (!existing_profile)
    return;

  // Don't overwrite the origin for a profile that is already stored.
  if (existing_profile->Compare(profile) == 0)
    return;

  if (profile.IsEmpty(app_locale_)) {
    RemoveByGUID(profile.guid());
    return;
  }

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Make the update.
  autofill_data->UpdateAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) {
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->guid() == guid)
      return *iter;
  }
  return NULL;
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (browser_context_->IsOffTheRecord())
    return;

  if (credit_card.IsEmpty(app_locale_))
    return;

  if (FindByGUID<CreditCard>(credit_cards_, credit_card.guid()))
    return;

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  autofill_data->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  if (browser_context_->IsOffTheRecord())
    return;

  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card)
    return;

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0)
    return;

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Make the update.
  autofill_data->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (browser_context_->IsOffTheRecord())
    return;

  bool is_credit_card = FindByGUID<CreditCard>(credit_cards_, guid);
  bool is_profile = !is_credit_card &&
      FindByGUID<AutofillProfile>(web_profiles_, guid);
  if (!is_credit_card && !is_profile)
    return;

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  if (is_credit_card)
    autofill_data->RemoveCreditCard(guid);
  else
    autofill_data->RemoveAutofillProfile(guid);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    if ((*iter)->guid() == guid)
      return *iter;
  }
  return NULL;
}

void PersonalDataManager::GetNonEmptyTypes(
    ServerFieldTypeSet* non_empty_types) {
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    (*iter)->GetNonEmptyTypes(app_locale_, non_empty_types);
  }

  for (ScopedVector<CreditCard>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    (*iter)->GetNonEmptyTypes(app_locale_, non_empty_types);
  }
}

bool PersonalDataManager::IsDataLoaded() const {
  return is_data_loaded_;
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles() {
  if (!user_prefs::UserPrefs::Get(browser_context_)->GetBoolean(
          prefs::kAutofillAuxiliaryProfilesEnabled)) {
    return web_profiles();
  }

  profiles_.clear();

  // Populates |auxiliary_profiles_|.
  LoadAuxiliaryProfiles();

  profiles_.insert(profiles_.end(), web_profiles_.begin(), web_profiles_.end());
  profiles_.insert(profiles_.end(),
      auxiliary_profiles_.begin(), auxiliary_profiles_.end());
  return profiles_;
}

const std::vector<AutofillProfile*>& PersonalDataManager::web_profiles() const {
  return web_profiles_.get();
}

const std::vector<CreditCard*>& PersonalDataManager::GetCreditCards() const {
  return credit_cards_.get();
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
}

void PersonalDataManager::GetProfileSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool field_is_autofilled,
    std::vector<ServerFieldType> other_field_types,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<GUIDPair>* guid_pairs) {
  values->clear();
  labels->clear();
  icons->clear();
  guid_pairs->clear();

  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  std::vector<AutofillProfile*> matched_profiles;
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    AutofillProfile* profile = *iter;

    // The value of the stored data for this field type in the |profile|.
    std::vector<base::string16> multi_values;
    profile->GetMultiInfo(type, app_locale_, &multi_values);

    for (size_t i = 0; i < multi_values.size(); ++i) {
      if (!field_is_autofilled) {
        // Suggest data that starts with what the user has typed.
        if (!multi_values[i].empty() &&
            StartsWith(multi_values[i], field_contents, false)) {
          matched_profiles.push_back(profile);
          values->push_back(multi_values[i]);
          guid_pairs->push_back(GUIDPair(profile->guid(), i));
        }
      } else {
        if (multi_values[i].empty())
          continue;

        base::string16 profile_value_lower_case(
            StringToLowerASCII(multi_values[i]));
        base::string16 field_value_lower_case(
            StringToLowerASCII(field_contents));
        // Phone numbers could be split in US forms, so field value could be
        // either prefix or suffix of the phone.
        bool matched_phones = false;
        if (type.GetStorableType() == PHONE_HOME_NUMBER &&
            !field_value_lower_case.empty() &&
            profile_value_lower_case.find(field_value_lower_case) !=
                base::string16::npos) {
          matched_phones = true;
        }

        // Suggest variants of the profile that's already been filled in.
        if (matched_phones ||
            profile_value_lower_case == field_value_lower_case) {
          for (size_t j = 0; j < multi_values.size(); ++j) {
            if (!multi_values[j].empty()) {
              values->push_back(multi_values[j]);
              guid_pairs->push_back(GUIDPair(profile->guid(), j));
            }
          }

          // We've added all the values for this profile so move on to the
          // next.
          break;
        }
      }
    }
  }

  if (!field_is_autofilled) {
    AutofillProfile::CreateInferredLabels(
        &matched_profiles, &other_field_types,
        type.GetStorableType(), 1, labels);
  } else {
    // No sub-labels for previously filled fields.
    labels->resize(values->size());
  }

  // No icons for profile suggestions.
  icons->resize(values->size());
}

void PersonalDataManager::GetCreditCardSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<GUIDPair>* guid_pairs) {
  values->clear();
  labels->clear();
  icons->clear();
  guid_pairs->clear();

  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    base::string16 creditcard_field_value =
        credit_card->GetInfo(type, app_locale_);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field_contents, false)) {
      if (type.GetStorableType() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      base::string16 label;
      if (credit_card->number().empty()) {
        // If there is no CC number, return name to show something.
        label =
            credit_card->GetInfo(AutofillType(CREDIT_CARD_NAME), app_locale_);
      } else {
        label = kCreditCardPrefix;
        label.append(credit_card->LastFourDigits());
      }

      values->push_back(creditcard_field_value);
      labels->push_back(label);
      icons->push_back(UTF8ToUTF16(credit_card->type()));
      guid_pairs->push_back(GUIDPair(credit_card->guid(), 0));
    }
  }
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return user_prefs::UserPrefs::Get(browser_context_)->GetBoolean(
      prefs::kAutofillEnabled);
}

// static
bool PersonalDataManager::IsValidLearnableProfile(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  if (!IsMinimumAddress(profile, app_locale))
    return false;

  base::string16 email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email))
    return false;

  // Reject profiles with invalid US state information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE))
    return false;

  // Reject profiles with invalid US zip information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP))
    return false;

  return true;
}

// static
std::string PersonalDataManager::MergeProfile(
    const AutofillProfile& new_profile,
    const std::vector<AutofillProfile*>& existing_profiles,
    const std::string& app_locale,
    std::vector<AutofillProfile>* merged_profiles) {
  merged_profiles->clear();

  // Set to true if |existing_profiles| already contains an equivalent profile.
  bool matching_profile_found = false;
  std::string guid = new_profile.guid();

  // If we have already saved this address, merge in any missing values.
  // Only merge with the first match.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           existing_profiles.begin();
       iter != existing_profiles.end(); ++iter) {
    AutofillProfile* existing_profile = *iter;
    if (!matching_profile_found &&
        !new_profile.PrimaryValue().empty() &&
        StringToLowerASCII(existing_profile->PrimaryValue()) ==
            StringToLowerASCII(new_profile.PrimaryValue())) {
      // Unverified profiles should always be updated with the newer data,
      // whereas verified profiles should only ever be overwritten by verified
      // data.  If an automatically aggregated profile would overwrite a
      // verified profile, just drop it.
      matching_profile_found = true;
      guid = existing_profile->guid();
      if (!existing_profile->IsVerified() || new_profile.IsVerified())
        existing_profile->OverwriteWithOrAddTo(new_profile, app_locale);
    }
    merged_profiles->push_back(*existing_profile);
  }

  // If the new profile was not merged with an existing one, add it to the list.
  if (!matching_profile_found)
    merged_profiles->push_back(new_profile);

  return guid;
}

void PersonalDataManager::SetProfiles(std::vector<AutofillProfile>* profiles) {
  if (browser_context_->IsOffTheRecord())
    return;

  // Remove empty profiles from input.
  for (std::vector<AutofillProfile>::iterator it = profiles->begin();
       it != profiles->end();) {
    if (it->IsEmpty(app_locale_))
      profiles->erase(it);
    else
      it++;
  }

  // Ensure that profile labels are up to date.  Currently, sync relies on
  // labels to identify a profile.
  // TODO(dhollowa): We need to deprecate labels and update the way sync
  // identifies profiles.
  std::vector<AutofillProfile*> profile_pointers(profiles->size());
  std::transform(profiles->begin(), profiles->end(), profile_pointers.begin(),
      address_of<AutofillProfile>);
  AutofillProfile::AdjustInferredLabels(&profile_pointers);

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Any profiles that are not in the new profile list should be removed from
  // the web database.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           web_profiles_.begin();
       iter != web_profiles_.end(); ++iter) {
    if (!FindByGUID<AutofillProfile>(*profiles, (*iter)->guid()))
      autofill_data->RemoveAutofillProfile((*iter)->guid());
  }

  // Update the web database with the existing profiles.
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (FindByGUID<AutofillProfile>(web_profiles_, iter->guid()))
      autofill_data->UpdateAutofillProfile(*iter);
  }

  // Add the new profiles to the web database.  Don't add a duplicate.
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (!FindByGUID<AutofillProfile>(web_profiles_, iter->guid()) &&
        !FindByContents(web_profiles_, *iter))
      autofill_data->AddAutofillProfile(*iter);
  }

  // Copy in the new profiles.
  web_profiles_.clear();
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    web_profiles_.push_back(new AutofillProfile(*iter));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (browser_context_->IsOffTheRecord())
    return;

  // Remove empty credit cards from input.
  for (std::vector<CreditCard>::iterator it = credit_cards->begin();
       it != credit_cards->end();) {
    if (it->IsEmpty(app_locale_))
      credit_cards->erase(it);
    else
      it++;
  }

  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get())
    return;

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    if (!FindByGUID<CreditCard>(*credit_cards, (*iter)->guid()))
      autofill_data->RemoveCreditCard((*iter)->guid());
  }

  // Update the web database with the existing credit cards.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (FindByGUID<CreditCard>(credit_cards_, iter->guid()))
      autofill_data->UpdateCreditCard(*iter);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (!FindByGUID<CreditCard>(credit_cards_, iter->guid()) &&
        !FindByContents(credit_cards_, *iter))
      autofill_data->AddCreditCard(*iter);
  }

  // Copy in the new credit cards.
  credit_cards_.clear();
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    credit_cards_.push_back(new CreditCard(*iter));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);

  pending_profiles_query_ = autofill_data->GetAutofillProfiles(this);
}

// Win and Linux implementations do nothing. Mac and Android implementations
// fill in the contents of |auxiliary_profiles_|.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
void PersonalDataManager::LoadAuxiliaryProfiles() {
}
#endif

void PersonalDataManager::LoadCreditCards() {
  scoped_refptr<AutofillWebDataService> autofill_data(
      AutofillWebDataService::FromBrowserContext(browser_context_));
  if (!autofill_data.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);

  pending_creditcards_query_ = autofill_data->GetCreditCards(this);
}

void PersonalDataManager::ReceiveLoadedProfiles(WebDataServiceBase::Handle h,
                                                const WDTypedResult* result) {
  DCHECK_EQ(pending_profiles_query_, h);

  pending_profiles_query_ = 0;
  web_profiles_.clear();

  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);

  std::vector<AutofillProfile*> profiles = r->GetValue();
  for (std::vector<AutofillProfile*>::iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    web_profiles_.push_back(*iter);
  }

  LogProfileCount();
}

void PersonalDataManager::ReceiveLoadedCreditCards(
    WebDataServiceBase::Handle h, const WDTypedResult* result) {
  DCHECK_EQ(pending_creditcards_query_, h);

  pending_creditcards_query_ = 0;
  credit_cards_.clear();

  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  for (std::vector<CreditCard*>::iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    credit_cards_.push_back(*iter);
  }
}

void PersonalDataManager::CancelPendingQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    scoped_refptr<AutofillWebDataService> autofill_data(
        AutofillWebDataService::FromBrowserContext(browser_context_));
    if (!autofill_data.get()) {
      NOTREACHED();
      return;
    }
    autofill_data->CancelRequest(*handle);
  }
  *handle = 0;
}

std::string PersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  if (browser_context_->IsOffTheRecord())
    return std::string();

  // Don't save a web profile if the data in the profile is a subset of an
  // auxiliary profile.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           auxiliary_profiles_.begin();
       iter != auxiliary_profiles_.end(); ++iter) {
    if (imported_profile.IsSubsetOf(**iter, app_locale_))
      return (*iter)->guid();
  }

  std::vector<AutofillProfile> profiles;
  std::string guid =
      MergeProfile(imported_profile, web_profiles_.get(), app_locale_,
                   &profiles);
  SetProfiles(&profiles);
  return guid;
}


std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  if (browser_context_->IsOffTheRecord())
    return std::string();

  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (std::vector<CreditCard*>::const_iterator card = credit_cards_.begin();
       card != credit_cards_.end();
       ++card) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged &&
        (*card)->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = (*card)->guid();
      merged = true;
    }

    credit_cards.push_back(**card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);
  return guid;
}

void PersonalDataManager::LogProfileCount() const {
  if (!has_logged_profile_count_) {
    metric_logger_->LogStoredProfileCount(web_profiles_.size());
    has_logged_profile_count_ = true;
  }
}

const AutofillMetrics* PersonalDataManager::metric_logger() const {
  return metric_logger_.get();
}

void PersonalDataManager::set_metric_logger(
    const AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

void PersonalDataManager::set_browser_context(
    content::BrowserContext* context) {
  browser_context_ = context;
}

}  // namespace autofill
