// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile.h"

#include <algorithm>
#include <functional>
#include <map>
#include <ostream>
#include <set>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/contact_info.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/component_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Like |AutofillType::GetStorableType()|, but also returns |NAME_FULL| for
// first, middle, and last name field types.
ServerFieldType GetStorableTypeCollapsingNames(ServerFieldType type) {
  ServerFieldType storable_type = AutofillType(type).GetStorableType();
  if (AutofillType(storable_type).group() == NAME)
    return NAME_FULL;

  return storable_type;
}

// Fills |distinguishing_fields| with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// |suggested_fields| if it is non-NULL; otherwise returns a default list.
// If |suggested_fields| is non-NULL, does not include |excluded_field| in the
// list. Otherwise, |excluded_field| is ignored, and should be set to
// |UNKNOWN_TYPE| by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    std::vector<ServerFieldType>* distinguishing_fields) {
  static const ServerFieldType kDefaultDistinguishingFields[] = {
    NAME_FULL,
    ADDRESS_HOME_LINE1,
    ADDRESS_HOME_LINE2,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_COUNTRY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER,
    COMPANY_NAME,
  };

  if (!suggested_fields) {
    DCHECK_EQ(excluded_field, UNKNOWN_TYPE);
    distinguishing_fields->assign(
        kDefaultDistinguishingFields,
        kDefaultDistinguishingFields + arraysize(kDefaultDistinguishingFields));
    return;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and the excluded field.
  std::set<ServerFieldType> seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  seen_fields.insert(GetStorableTypeCollapsingNames(excluded_field));

  distinguishing_fields->clear();
  for (std::vector<ServerFieldType>::const_iterator it =
           suggested_fields->begin();
       it != suggested_fields->end(); ++it) {
    ServerFieldType suggested_type = GetStorableTypeCollapsingNames(*it);
    if (seen_fields.insert(suggested_type).second)
      distinguishing_fields->push_back(suggested_type);
  }

  // Special case: If the excluded field is a partial name (e.g. first name) and
  // the suggested fields include other name fields, include |NAME_FULL| in the
  // list of distinguishing fields as a last-ditch fallback. This allows us to
  // distinguish between profiles that are identical except for the name.
  if (excluded_field != NAME_FULL &&
      GetStorableTypeCollapsingNames(excluded_field) == NAME_FULL) {
    for (std::vector<ServerFieldType>::const_iterator it =
             suggested_fields->begin();
         it != suggested_fields->end(); ++it) {
      if (*it != excluded_field &&
          GetStorableTypeCollapsingNames(*it) == NAME_FULL) {
        distinguishing_fields->push_back(NAME_FULL);
        break;
      }
    }
  }
}

// A helper function for string streaming.  Concatenates multi-valued entries
// stored for a given |type| into a single string.  This string is returned.
const base::string16 MultiString(const AutofillProfile& p,
                                 ServerFieldType type) {
  std::vector<base::string16> values;
  p.GetRawMultiInfo(type, &values);
  base::string16 accumulate;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0)
      accumulate += ASCIIToUTF16(" ");
    accumulate += values[i];
  }
  return accumulate;
}

base::string16 GetFormGroupInfo(const FormGroup& form_group,
                                const AutofillType& type,
                                const std::string& app_locale) {
  return app_locale.empty() ?
      form_group.GetRawInfo(type.GetStorableType()) :
      form_group.GetInfo(type, app_locale);
}

template <class T>
void CopyValuesToItems(ServerFieldType type,
                       const std::vector<base::string16>& values,
                       std::vector<T>* form_group_items,
                       const T& prototype) {
  form_group_items->resize(values.size(), prototype);
  for (size_t i = 0; i < form_group_items->size(); ++i) {
    (*form_group_items)[i].SetRawInfo(type, values[i]);
  }
  // Must have at least one (possibly empty) element.
  if (form_group_items->empty())
    form_group_items->resize(1, prototype);
}

template <class T>
void CopyItemsToValues(const AutofillType& type,
                       const std::vector<T>& form_group_items,
                       const std::string& app_locale,
                       std::vector<base::string16>* values) {
  values->resize(form_group_items.size());
  for (size_t i = 0; i < values->size(); ++i) {
    (*values)[i] = GetFormGroupInfo(form_group_items[i], type, app_locale);
  }
}

// Collapse compound field types to their "full" type.  I.e. First name
// collapses to full name, area code collapses to full phone, etc.
void CollapseCompoundFieldTypes(ServerFieldTypeSet* type_set) {
  ServerFieldTypeSet collapsed_set;
  for (ServerFieldTypeSet::iterator it = type_set->begin();
       it != type_set->end(); ++it) {
    switch (*it) {
      case NAME_FIRST:
      case NAME_MIDDLE:
      case NAME_LAST:
      case NAME_MIDDLE_INITIAL:
      case NAME_FULL:
      case NAME_SUFFIX:
        collapsed_set.insert(NAME_FULL);
        break;

      case PHONE_HOME_NUMBER:
      case PHONE_HOME_CITY_CODE:
      case PHONE_HOME_COUNTRY_CODE:
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_WHOLE_NUMBER:
        collapsed_set.insert(PHONE_HOME_WHOLE_NUMBER);
        break;

      default:
        collapsed_set.insert(*it);
    }
  }
  std::swap(*type_set, collapsed_set);
}

class FindByPhone {
 public:
  FindByPhone(const base::string16& phone,
              const std::string& country_code,
              const std::string& app_locale)
      : phone_(phone),
        country_code_(country_code),
        app_locale_(app_locale) {
  }

  bool operator()(const base::string16& phone) {
    return i18n::PhoneNumbersMatch(phone, phone_, country_code_, app_locale_);
  }

  bool operator()(const base::string16* phone) {
    return i18n::PhoneNumbersMatch(*phone, phone_, country_code_, app_locale_);
  }

 private:
  base::string16 phone_;
  std::string country_code_;
  std::string app_locale_;
};

// Functor used to check for case-insensitive equality of two strings.
struct CaseInsensitiveStringEquals
    : public std::binary_function<base::string16, base::string16, bool>
{
  bool operator()(const base::string16& x, const base::string16& y) const {
    return
        x.size() == y.size() && StringToLowerASCII(x) == StringToLowerASCII(y);
  }
};

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid,
                                 const std::string& origin)
    : AutofillDataModel(guid, origin),
      name_(1),
      email_(1),
      phone_number_(1, PhoneNumber(this)) {
}

AutofillProfile::AutofillProfile()
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      name_(1),
      email_(1),
      phone_number_(1, PhoneNumber(this)) {
}

AutofillProfile::AutofillProfile(const AutofillProfile& profile)
    : AutofillDataModel(std::string(), std::string()) {
  operator=(profile);
}

AutofillProfile::~AutofillProfile() {
}

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  if (this == &profile)
    return *this;

  set_guid(profile.guid());
  set_origin(profile.origin());

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  phone_number_ = profile.phone_number_;

  for (size_t i = 0; i < phone_number_.size(); ++i)
    phone_number_[i].set_profile(this);

  address_ = profile.address_;

  return *this;
}

void AutofillProfile::GetMatchingTypes(
    const base::string16& text,
    const std::string& app_locale,
    ServerFieldTypeSet* matching_types) const {
  FormGroupList info = FormGroups();
  for (FormGroupList::const_iterator it = info.begin(); it != info.end(); ++it)
    (*it)->GetMatchingTypes(text, app_locale, matching_types);
}

base::string16 AutofillProfile::GetRawInfo(ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return base::string16();

  return form_group->GetRawInfo(type);
}

void AutofillProfile::SetRawInfo(ServerFieldType type,
                                 const base::string16& value) {
  FormGroup* form_group = MutableFormGroupForType(AutofillType(type));
  if (form_group)
    form_group->SetRawInfo(type, value);
}

base::string16 AutofillProfile::GetInfo(const AutofillType& type,
                                        const std::string& app_locale) const {
  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return base::string16();

  return form_group->GetInfo(type, app_locale);
}

bool AutofillProfile::SetInfo(const AutofillType& type,
                              const base::string16& value,
                              const std::string& app_locale) {
  FormGroup* form_group = MutableFormGroupForType(type);
  if (!form_group)
    return false;

  base::string16 trimmed_value;
  TrimWhitespace(value, TRIM_ALL, &trimmed_value);
  return form_group->SetInfo(type, trimmed_value, app_locale);
}

base::string16 AutofillProfile::GetInfoForVariant(
    const AutofillType& type,
    size_t variant,
    const std::string& app_locale) const {
  std::vector<base::string16> values;
  GetMultiInfo(type, app_locale, &values);

  if (variant >= values.size()) {
    // If the variant is unavailable, bail. This case is reachable, for
    // example if Sync updates a profile during the filling process.
    return base::string16();
  }

  return values[variant];
}

void AutofillProfile::SetRawMultiInfo(
    ServerFieldType type,
    const std::vector<base::string16>& values) {
  switch (AutofillType(type).group()) {
    case NAME:
    case NAME_BILLING:
      CopyValuesToItems(type, values, &name_, NameInfo());
      break;
    case EMAIL:
      CopyValuesToItems(type, values, &email_, EmailInfo());
      break;
    case PHONE_HOME:
    case PHONE_BILLING:
      CopyValuesToItems(type,
                        values,
                        &phone_number_,
                        PhoneNumber(this));
      break;
    default:
      if (values.size() == 1) {
        SetRawInfo(type, values[0]);
      } else if (values.size() == 0) {
        SetRawInfo(type, base::string16());
      } else {
        // Shouldn't attempt to set multiple values on single-valued field.
        NOTREACHED();
      }
      break;
  }
}

void AutofillProfile::GetRawMultiInfo(
    ServerFieldType type,
    std::vector<base::string16>* values) const {
  GetMultiInfoImpl(AutofillType(type), std::string(), values);
}

void AutofillProfile::GetMultiInfo(const AutofillType& type,
                                   const std::string& app_locale,
                                   std::vector<base::string16>* values) const {
  GetMultiInfoImpl(type, app_locale, values);
}

bool AutofillProfile::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool AutofillProfile::IsPresentButInvalid(ServerFieldType type) const {
  std::string country = UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  base::string16 data = GetRawInfo(type);
  if (data.empty())
    return false;

  switch (type) {
    case ADDRESS_HOME_STATE:
      return country == "US" && !autofill::IsValidState(data);

    case ADDRESS_HOME_ZIP:
      return country == "US" && !autofill::IsValidZip(data);

    case PHONE_HOME_WHOLE_NUMBER:
      return !i18n::PhoneObject(data, country).IsValidNumber();

    case EMAIL_ADDRESS:
      return !autofill::IsValidEmailAddress(data);

    default:
      NOTREACHED();
      return false;
  }
}


int AutofillProfile::Compare(const AutofillProfile& profile) const {
  const ServerFieldType single_value_types[] = {
    COMPANY_NAME,
    ADDRESS_HOME_LINE1,
    ADDRESS_HOME_LINE2,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_SORTING_CODE,
    ADDRESS_HOME_COUNTRY,
  };

  for (size_t i = 0; i < arraysize(single_value_types); ++i) {
    int comparison = GetRawInfo(single_value_types[i]).compare(
        profile.GetRawInfo(single_value_types[i]));
    if (comparison != 0)
      return comparison;
  }

  const ServerFieldType multi_value_types[] = { NAME_FIRST,
                                                NAME_MIDDLE,
                                                NAME_LAST,
                                                EMAIL_ADDRESS,
                                                PHONE_HOME_WHOLE_NUMBER };

  for (size_t i = 0; i < arraysize(multi_value_types); ++i) {
    std::vector<base::string16> values_a;
    std::vector<base::string16> values_b;
    GetRawMultiInfo(multi_value_types[i], &values_a);
    profile.GetRawMultiInfo(multi_value_types[i], &values_b);
    if (values_a.size() < values_b.size())
      return -1;
    if (values_a.size() > values_b.size())
      return 1;
    for (size_t j = 0; j < values_a.size(); ++j) {
      int comparison = values_a[j].compare(values_b[j]);
      if (comparison != 0)
        return comparison;
    }
  }

  return 0;
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid() == profile.guid() &&
         origin() == profile.origin() &&
         Compare(profile) == 0;
}

bool AutofillProfile::operator!=(const AutofillProfile& profile) const {
  return !operator==(profile);
}

const base::string16 AutofillProfile::PrimaryValue() const {
  return GetRawInfo(ADDRESS_HOME_LINE1) + GetRawInfo(ADDRESS_HOME_CITY);
}

bool AutofillProfile::IsSubsetOf(const AutofillProfile& profile,
                                 const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);

  for (ServerFieldTypeSet::const_iterator it = types.begin(); it != types.end();
       ++it) {
    if (*it == NAME_FULL || *it == ADDRESS_HOME_STREET_ADDRESS) {
      // Ignore the compound "full name" field type.  We are only interested in
      // comparing the constituent parts.  For example, if |this| has a middle
      // name saved, but |profile| lacks one, |profile| could still be a subset
      // of |this|.  Likewise, ignore the compound "street address" type, as we
      // are only interested in matching line-by-line.
      continue;
    } else if (AutofillType(*it).group() == PHONE_HOME) {
      // Phone numbers should be canonicalized prior to being compared.
      if (*it != PHONE_HOME_WHOLE_NUMBER) {
        continue;
      } else if (!i18n::PhoneNumbersMatch(
            GetRawInfo(*it),
            profile.GetRawInfo(*it),
            UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
            app_locale)) {
        return false;
      }
    } else if (StringToLowerASCII(GetRawInfo(*it)) !=
                   StringToLowerASCII(profile.GetRawInfo(*it))) {
      return false;
    }
  }

  return true;
}

void AutofillProfile::OverwriteWithOrAddTo(const AutofillProfile& profile,
                                           const std::string& app_locale) {
  // Verified profiles should never be overwritten with unverified data.
  DCHECK(!IsVerified() || profile.IsVerified());
  set_origin(profile.origin());

  ServerFieldTypeSet field_types;
  profile.GetNonEmptyTypes(app_locale, &field_types);

  // Only transfer "full" types (e.g. full name) and not fragments (e.g.
  // first name, last name).
  CollapseCompoundFieldTypes(&field_types);

  // TODO(isherman): Revisit this decision in the context of i18n and storing
  // full addresses rather than storing 1-to-2 lines of an address.
  // For addresses, do the opposite: transfer individual address lines, rather
  // than full addresses.
  field_types.erase(ADDRESS_HOME_STREET_ADDRESS);

  for (ServerFieldTypeSet::const_iterator iter = field_types.begin();
       iter != field_types.end(); ++iter) {
    if (AutofillProfile::SupportsMultiValue(*iter)) {
      std::vector<base::string16> new_values;
      profile.GetRawMultiInfo(*iter, &new_values);
      std::vector<base::string16> existing_values;
      GetRawMultiInfo(*iter, &existing_values);

      // GetMultiInfo always returns at least one element, even if the profile
      // has no data stored for this field type.
      if (existing_values.size() == 1 && existing_values.front().empty())
        existing_values.clear();

      FieldTypeGroup group = AutofillType(*iter).group();
      for (std::vector<base::string16>::iterator value_iter =
               new_values.begin();
           value_iter != new_values.end(); ++value_iter) {
        // Don't add duplicates.
        if (group == PHONE_HOME) {
          AddPhoneIfUnique(*value_iter, app_locale, &existing_values);
        } else {
          std::vector<base::string16>::const_iterator existing_iter =
              std::find_if(
                  existing_values.begin(), existing_values.end(),
                  std::bind1st(CaseInsensitiveStringEquals(), *value_iter));
          if (existing_iter == existing_values.end())
            existing_values.insert(existing_values.end(), *value_iter);
        }
      }
      SetRawMultiInfo(*iter, existing_values);
    } else {
      base::string16 new_value = profile.GetRawInfo(*iter);
      if (StringToLowerASCII(GetRawInfo(*iter)) !=
              StringToLowerASCII(new_value)) {
        SetRawInfo(*iter, new_value);
      }
    }
  }
}

// static
bool AutofillProfile::SupportsMultiValue(ServerFieldType type) {
  FieldTypeGroup group = AutofillType(type).group();
  return group == NAME ||
         group == NAME_BILLING ||
         group == EMAIL ||
         group == PHONE_HOME ||
         group == PHONE_BILLING;
}

// static
void AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<AutofillProfile*>& profiles,
    std::vector<base::string16>* labels) {
  const size_t kMinimalFieldsShown = 2;
  CreateInferredLabels(profiles, NULL, UNKNOWN_TYPE, kMinimalFieldsShown,
                       labels);
  DCHECK_EQ(profiles.size(), labels->size());
}

// static
void AutofillProfile::CreateInferredLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    size_t minimal_fields_shown,
    std::vector<base::string16>* labels) {
  std::vector<ServerFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // Construct the default label for each profile. Also construct a map that
  // associates each label with the profiles that have this label. This map is
  // then used to detect which labels need further differentiating fields.
  std::map<base::string16, std::list<size_t> > labels_to_profiles;
  for (size_t i = 0; i < profiles.size(); ++i) {
    base::string16 label =
        profiles[i]->ConstructInferredLabel(fields_to_use,
                                            minimal_fields_shown);
    labels_to_profiles[label].push_back(i);
  }

  labels->resize(profiles.size());
  for (std::map<base::string16, std::list<size_t> >::const_iterator it =
           labels_to_profiles.begin();
       it != labels_to_profiles.end(); ++it) {
    if (it->second.size() == 1) {
      // This label is unique, so use it without any further ado.
      base::string16 label = it->first;
      size_t profile_index = it->second.front();
      (*labels)[profile_index] = label;
    } else {
      // We have more than one profile with the same label, so add
      // differentiating fields.
      CreateInferredLabelsHelper(profiles, it->second, fields_to_use,
                                 minimal_fields_shown, labels);
    }
  }
}

void AutofillProfile::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {
  FormGroupList info = FormGroups();
  for (FormGroupList::const_iterator it = info.begin(); it != info.end(); ++it)
    (*it)->GetSupportedTypes(supported_types);
}

void AutofillProfile::GetMultiInfoImpl(
    const AutofillType& type,
    const std::string& app_locale,
    std::vector<base::string16>* values) const {
  switch (type.group()) {
    case NAME:
    case NAME_BILLING:
      CopyItemsToValues(type, name_, app_locale, values);
      break;
    case EMAIL:
      CopyItemsToValues(type, email_, app_locale, values);
      break;
    case PHONE_HOME:
    case PHONE_BILLING:
      CopyItemsToValues(type, phone_number_, app_locale, values);
      break;
    default:
      values->resize(1);
      (*values)[0] = GetFormGroupInfo(*this, type, app_locale);
  }
}

void AutofillProfile::AddPhoneIfUnique(
    const base::string16& phone,
    const std::string& app_locale,
    std::vector<base::string16>* existing_phones) {
  DCHECK(existing_phones);
  // Phones allow "fuzzy" matching, so "1-800-FLOWERS", "18003569377",
  // "(800)356-9377" and "356-9377" are considered the same.
  std::string country_code = UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (std::find_if(existing_phones->begin(), existing_phones->end(),
                   FindByPhone(phone, country_code, app_locale)) ==
      existing_phones->end()) {
    existing_phones->push_back(phone);
  }
}

base::string16 AutofillProfile::ConstructInferredLabel(
    const std::vector<ServerFieldType>& included_fields,
    size_t num_fields_to_use) const {
  const base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  base::string16 label;
  size_t num_fields_used = 0;
  for (std::vector<ServerFieldType>::const_iterator it =
           included_fields.begin();
       it != included_fields.end() && num_fields_used < num_fields_to_use;
       ++it) {
    base::string16 field = GetRawInfo(*it);
    if (field.empty())
      continue;

    if (!label.empty())
      label.append(separator);

    label.append(field);
    ++num_fields_used;
  }

  // Flatten the label if need be.
  const char16 kNewline[] = { '\n', 0 };
  const base::string16 newline_separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_LINE_SEPARATOR);
  base::ReplaceChars(label, kNewline, newline_separator, &label);

  return label;
}

// static
void AutofillProfile::CreateInferredLabelsHelper(
    const std::vector<AutofillProfile*>& profiles,
    const std::list<size_t>& indices,
    const std::vector<ServerFieldType>& fields,
    size_t num_fields_to_include,
    std::vector<base::string16>* labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<ServerFieldType,
           std::map<base::string16, size_t> > field_text_frequencies_by_field;
  for (std::vector<ServerFieldType>::const_iterator field = fields.begin();
       field != fields.end(); ++field) {
    std::map<base::string16, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[*field];

    for (std::list<size_t>::const_iterator it = indices.begin();
         it != indices.end(); ++it) {
      const AutofillProfile* profile = profiles[*it];
      base::string16 field_text = profile->GetRawInfo(*field);

      // If this label is not already in the map, add it with frequency 0.
      if (!field_text_frequencies.count(field_text))
        field_text_frequencies[field_text] = 0;

      // Now, increment the frequency for this label.
      ++field_text_frequencies[field_text];
    }
  }

  // Now comes the meat of the algorithm. For each profile, we scan the list of
  // fields to use, looking for two things:
  //  1. A (non-empty) field that differentiates the profile from all others
  //  2. At least |num_fields_to_include| non-empty fields
  // Before we've satisfied condition (2), we include all fields, even ones that
  // are identical across all the profiles. Once we've satisfied condition (2),
  // we only include fields that that have at last two distinct values.
  for (std::list<size_t>::const_iterator it = indices.begin();
       it != indices.end(); ++it) {
    const AutofillProfile* profile = profiles[*it];

    std::vector<ServerFieldType> label_fields;
    bool found_differentiating_field = false;
    for (std::vector<ServerFieldType>::const_iterator field = fields.begin();
         field != fields.end(); ++field) {
      // Skip over empty fields.
      base::string16 field_text = profile->GetRawInfo(*field);
      if (field_text.empty())
        continue;

      std::map<base::string16, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[*field];
      found_differentiating_field |=
          !field_text_frequencies.count(base::string16()) &&
          (field_text_frequencies[field_text] == 1);

      // Once we've found enough non-empty fields, skip over any remaining
      // fields that are identical across all the profiles.
      if (label_fields.size() >= num_fields_to_include &&
          (field_text_frequencies.size() == 1))
        continue;

      label_fields.push_back(*field);

      // If we've (1) found a differentiating field and (2) found at least
      // |num_fields_to_include| non-empty fields, we're done!
      if (found_differentiating_field &&
          label_fields.size() >= num_fields_to_include)
        break;
    }

    (*labels)[*it] =
        profile->ConstructInferredLabel(label_fields, label_fields.size());
  }
}

AutofillProfile::FormGroupList AutofillProfile::FormGroups() const {
  FormGroupList v(5);
  v[0] = &name_[0];
  v[1] = &email_[0];
  v[2] = &company_;
  v[3] = &phone_number_[0];
  v[4] = &address_;
  return v;
}

const FormGroup* AutofillProfile::FormGroupForType(
    const AutofillType& type) const {
  return const_cast<AutofillProfile*>(this)->MutableFormGroupForType(type);
}

FormGroup* AutofillProfile::MutableFormGroupForType(const AutofillType& type) {
  switch (type.group()) {
    case NAME:
    case NAME_BILLING:
      return &name_[0];

    case EMAIL:
      return &email_[0];

    case COMPANY:
      return &company_;

    case PHONE_HOME:
    case PHONE_BILLING:
      return &phone_number_[0];

    case ADDRESS_HOME:
    case ADDRESS_BILLING:
      return &address_;

    case NO_GROUP:
    case CREDIT_CARD:
    case PASSWORD_FIELD:
        return NULL;
  }

  NOTREACHED();
  return NULL;
}

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  return os
      << profile.guid()
      << " "
      << profile.origin()
      << " "
      << UTF16ToUTF8(MultiString(profile, NAME_FIRST))
      << " "
      << UTF16ToUTF8(MultiString(profile, NAME_MIDDLE))
      << " "
      << UTF16ToUTF8(MultiString(profile, NAME_LAST))
      << " "
      << UTF16ToUTF8(MultiString(profile, EMAIL_ADDRESS))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(COMPANY_NAME))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE1))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE2))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_ZIP))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE))
      << " "
      << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))
      << " "
      << UTF16ToUTF8(MultiString(profile, PHONE_HOME_WHOLE_NUMBER));
}

}  // namespace autofill
