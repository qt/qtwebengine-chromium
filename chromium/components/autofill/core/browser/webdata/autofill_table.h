// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Time;
}

namespace autofill {

class AutofillChange;
class AutofillEntry;
class AutofillProfile;
class AutofillTableTest;
class CreditCard;

struct FormFieldData;

// This class manages the various Autofill tables within the SQLite database
// passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
//
// autofill
//   name               The name of the input as specified in the html.
//   value              The literal contents of the text field.
//   value_lower        The contents of the text field made lower_case.
//   pair_id            An ID number unique to the row in the table.
//   count              How many times the user has entered the string |value|
//                      in a field of name |name|.
//
// autofill_dates       This table associates a row to each separate time the
//                      user submits a form containing a certain name/value
//                      pair.  The |pair_id| should match the |pair_id| field
//                      in the appropriate row of the autofill table.
//   pair_id
//   date_created
//
// autofill_profiles    This table contains Autofill profile data added by the
//                      user with the Autofill dialog.  Most of the columns are
//                      standard entries in a contact information form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   company_name
//   street_address     The combined lines of the street address.
//                      Added in version 54.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.  Added in version 54.
//   city
//   state
//   zipcode
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.  The canonical example is CEDEX in France.
//                      Added in version 54.
//   country_code
//   date_modified      The date on which this profile was last modified.
//                      Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//
// autofill_profile_names
//                      This table contains the multi-valued name fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//   first_name
//   middle_name
//   last_name
//
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      phone number belongs.
//   number
//
// autofill_profiles_trash
//                      This table contains guids of "trashed" autofill
//                      profiles.  When a profile is removed its guid is added
//                      to this table so that Sync can perform deferred removal.
//
//   guid               The guid string that identifies the trashed profile.
//
// credit_cards         This table contains credit card data added by the user
//                      with the Autofill dialog.  Most of the columns are
//                      standard entries in a credit card form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   name_on_card
//   expiration_month
//   expiration_year
//   card_number_encrypted
//                      Stores encrypted credit card number.
//   date_modified      The date on which this entry was last modified.
//                      Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//
class AutofillTable : public WebDatabaseTable {
 public:
  explicit AutofillTable(const std::string& app_locale);
  virtual ~AutofillTable();

  // Retrieves the AutofillTable* owned by |database|.
  static AutofillTable* FromWebDatabase(WebDatabase* db);

  virtual WebDatabaseTable::TypeKey GetTypeKey() const OVERRIDE;
  virtual bool Init(sql::Connection* db, sql::MetaTable* meta_table) OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;
  virtual bool MigrateToVersion(int version,
                                bool* update_compatible_version) OVERRIDE;

  // Records the form elements in |elements| in the database in the
  // autofill table.  A list of all added and updated autofill entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<FormFieldData>& elements,
                          std::vector<AutofillChange>* changes);

  // Records a single form element in the database in the autofill table. A list
  // of all added and updated autofill entries is returned in the changes out
  // parameter.
  bool AddFormFieldValue(const FormFieldData& element,
                         std::vector<AutofillChange>* changes);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|.  The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const base::string16& name,
                                   const base::string16& prefix,
                                   std::vector<base::string16>* values,
                                   int limit);

  // Returns whether any form elements are stored in the database.
  bool HasFormElements();

  // Removes rows from autofill_dates if they were created on or after
  // |delete_begin| and strictly before |delete_end|.  Decrements the
  // count of the corresponding rows in the autofill table, and
  // removes those rows if the count goes to 0.  A list of all changed
  // keys and whether each was updater or removed is returned in the
  // changes out parameter.
  bool RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      std::vector<AutofillChange>* changes);

  // Removes rows from autofill_dates if they were accessed strictly before
  // |AutofillEntry::ExpirationTime()|. Removes the corresponding row from the
  // autofill table. Also culls timestamps to only two. TODO(georgey): remove
  // culling in future versions.
  bool RemoveExpiredFormElements(std::vector<AutofillChange>* changes);

  // Removes from autofill_dates rows with given pair_id where date_created lies
  // between |delete_begin| and |delete_end|.
  bool RemoveFormElementForTimeRange(int64 pair_id,
                                     const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int* how_many);

  // Increments the count in the row corresponding to |pair_id| by |delta|.
  bool AddToCountOfFormElement(int64 pair_id, int delta);

  // Counts how many timestamp data rows are in the |autofill_dates| table for
  // a given |pair_id|. GetCountOfFormElement() on the other hand gives the
  // |count| property for a given id.
  int CountTimestampsData(int64 pair_id);

  // Gets the pair_id and count entries from name and value specified in
  // |element|.  Sets *pair_id and *count to 0 if there is no such row in
  // the table.
  bool GetIDAndCountOfFormElement(const FormFieldData& element,
                                  int64* pair_id,
                                  int* count);

  // Gets the count only given the pair_id.
  bool GetCountOfFormElement(int64 pair_id, int* count);

  // Updates the count entry in the row corresponding to |pair_id| to |count|.
  bool SetCountOfFormElement(int64 pair_id, int count);

  // Adds a new row to the autofill table with name and value given in
  // |element|.  Sets *pair_id to the pair_id of the new row.
  bool InsertFormElement(const FormFieldData& element,
                         int64* pair_id);

  // Adds a new row to the autofill_dates table.
  bool InsertPairIDAndDate(int64 pair_id, const base::Time& date_created);

  // Deletes last access to the Autofill data from the autofill_dates table.
  bool DeleteLastAccess(int64 pair_id);

  // Removes row from the autofill tables given |pair_id|.
  bool RemoveFormElementForID(int64 pair_id);

  // Removes row from the autofill tables for the given |name| |value| pair.
  virtual bool RemoveFormElement(const base::string16& name,
                                 const base::string16& value);

  // Retrieves all of the entries in the autofill table.
  virtual bool GetAllAutofillEntries(std::vector<AutofillEntry>* entries);

  // Retrieves a single entry from the autofill table.
  virtual bool GetAutofillTimestamps(const base::string16& name,
                                     const base::string16& value,
                                     std::vector<base::Time>* timestamps);

  // Replaces existing autofill entries with the entries supplied in
  // the argument.  If the entry does not already exist, it will be
  // added.
  virtual bool UpdateAutofillEntries(const std::vector<AutofillEntry>& entries);

  // Records a single Autofill profile in the autofill_profiles table.
  virtual bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Mulit-value aware.
  virtual bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes a row from the autofill_profiles table.  |guid| is the identifier
  // of the profile to remove.
  virtual bool RemoveAutofillProfile(const std::string& guid);

  // Retrieves a profile with guid |guid|.  The caller owns |profile|.
  bool GetAutofillProfile(const std::string& guid, AutofillProfile** profile);

  // Retrieves all profiles in the database.  Caller owns the returned profiles.
  virtual bool GetAutofillProfiles(std::vector<AutofillProfile*>* profiles);

  // Records a single credit card in the credit_cards table.
  bool AddCreditCard(const CreditCard& credit_card);

  // Updates the database values for the specified credit card.
  bool UpdateCreditCard(const CreditCard& credit_card);

  // Removes a row from the credit_cards table.  |guid| is the identifer  of the
  // credit card to remove.
  bool RemoveCreditCard(const std::string& guid);

  // Retrieves a credit card with guid |guid|.  The caller owns
  // |credit_card_id|.
  bool GetCreditCard(const std::string& guid, CreditCard** credit_card);

  // Retrieves all credit cards in the database.  Caller owns the returned
  // credit cards.
  virtual bool GetCreditCards(std::vector<CreditCard*>* credit_cards);

  // Removes rows from autofill_profiles and credit_cards if they were created
  // on or after |delete_begin| and strictly before |delete_end|.  Returns the
  // list of deleted profile guids in |profile_guids|.  Return value is true if
  // all rows were successfully removed.  Returns false on database error.  In
  // that case, the output vector state is undefined, and may be partially
  // filled.
  bool RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::string>* profile_guids,
      std::vector<std::string>* credit_card_guids);

  // Removes origin URLs from the autofill_profiles and credit_cards tables if
  // they were written on or after |delete_begin| and strictly before
  // |delete_end|.  Returns the list of modified profiles in |profiles|.  Return
  // value is true if all rows were successfully updated.  Returns false on
  // database error.  In that case, the output vector state is undefined, and
  // may be partially filled.
  bool RemoveOriginURLsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      ScopedVector<AutofillProfile>* profiles);

  // Retrieves all profiles in the database that have been deleted since last
  // "empty" of the trash.
  bool GetAutofillProfilesInTrash(std::vector<std::string>* guids);

  // Empties the Autofill profiles "trash can".
  bool EmptyAutofillProfilesTrash();

  // Removes empty values for autofill that were incorrectly stored in the DB
  // See bug http://crbug.com/6111
  bool ClearAutofillEmptyValueElements();

  // Retrieves all profiles in the database that have been deleted since last
  // "empty" of the trash.
  bool AddAutofillGUIDToTrash(const std::string& guid);

  // Clear all profiles.
  bool ClearAutofillProfiles();

  // Table migration functions.
  bool MigrateToVersion23AddCardNumberEncryptedColumn();
  bool MigrateToVersion24CleanupOversizedStringFields();
  bool MigrateToVersion27UpdateLegacyCreditCards();
  bool MigrateToVersion30AddDateModifed();
  bool MigrateToVersion31AddGUIDToCreditCardsAndProfiles();
  bool MigrateToVersion32UpdateProfilesAndCreditCards();
  bool MigrateToVersion33ProfilesBasedOnFirstName();
  bool MigrateToVersion34ProfilesBasedOnCountryCode();
  bool MigrateToVersion35GreatBritainCountryCodes();
  bool MigrateToVersion37MergeAndCullOlderProfiles();
  bool MigrateToVersion51AddOriginColumn();
  bool MigrateToVersion54AddI18nFieldsAndRemoveDeprecatedFields();

  // Max data length saved in the table;
  static const size_t kMaxDataLength;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_RemoveBetweenChanges);

  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateAutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrash);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrashInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveAutofillDataModifiedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateCreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_OneResult);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoDistinct);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoSame);

  // Methods for adding autofill entries at a specified time.  For
  // testing only.
  bool AddFormFieldValuesTime(
      const std::vector<FormFieldData>& elements,
      std::vector<AutofillChange>* changes,
      base::Time time);
  bool AddFormFieldValueTime(const FormFieldData& element,
                             std::vector<AutofillChange>* changes,
                             base::Time time);

  // Insert a single AutofillEntry into the autofill/autofill_dates tables.
  bool InsertAutofillEntry(const AutofillEntry& entry);

  // Checks if the trash is empty.
  bool IsAutofillProfilesTrashEmpty();

  // Checks if the guid is in the trash.
  bool IsAutofillGUIDInTrash(const std::string& guid);

  bool InitMainTable();
  bool InitCreditCardsTable();
  bool InitDatesTable();
  bool InitProfilesTable();
  bool InitProfileNamesTable();
  bool InitProfileEmailsTable();
  bool InitProfilePhonesTable();
  bool InitProfileTrashTable();

  // The application locale.  The locale is needed for the migration to version
  // 35. Since it must be read on the UI thread, it is set when the table is
  // created (on the UI thread), and cached here so that it can be used for
  // migrations (on the DB thread).
  std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(AutofillTable);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
