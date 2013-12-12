// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/webdata/encryptor/encryptor.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace autofill {
namespace {

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::MessageLoop::current()->Quit();
}

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  virtual ~PersonalDataLoadedObserverMock() {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

// Unlike the base AutofillMetrics, exposes copy and assignment constructors,
// which are handy for briefer test code.  The AutofillMetrics class is
// stateless, so this is safe.
class TestAutofillMetrics : public AutofillMetrics {
 public:
  TestAutofillMetrics() {}
  virtual ~TestAutofillMetrics() {}
};

}  // anonymous namespace

class PersonalDataManagerTest : public testing::Test {
 protected:
  PersonalDataManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    db_thread_.Start();

    profile_.reset(new TestingProfile);
    profile_->CreateWebDataService();

    test::DisableSystemServices(profile_.get());
    ResetPersonalDataManager();
  }

  virtual void TearDown() {
    // Destruction order is imposed explicitly here.
    personal_data_.reset(NULL);
    profile_.reset(NULL);

    // Schedule another task on the DB thread to notify us that it's safe to
    // stop the thread.
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  void ResetPersonalDataManager() {
    personal_data_.reset(new PersonalDataManager("en-US"));
    personal_data_->Init(profile_.get());
    personal_data_->AddObserver(&personal_data_observer_);

    // Verify that the web database has been updated and the notification sent.
    EXPECT_CALL(personal_data_observer_,
                OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
    base::MessageLoop::current()->Run();
  }

  void MakeProfileIncognito() {
    // Switch to an incognito profile.
    profile_->ForceIncognito(true);
    DCHECK(profile_->IsOffTheRecord());
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<PersonalDataManager> personal_data_;
  PersonalDataLoadedObserverMock personal_data_observer_;
};

TEST_F(PersonalDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(autofill::test::GetFullProfile());
  profile0.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("j@s.com"));
  personal_data_->AddProfile(profile0);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::GenerateGUID());
  personal_data_->AddProfile(profile0a);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the non-addition.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // New profile with different email.
  AutofillProfile profile1 = profile0;
  profile1.set_guid(base::GenerateGUID());
  profile1.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("john@smith.com"));

  // Add the different profile.  This should save as a separate profile.
  // Note that if this same profile was "merged" it would collapse to one
  // profile with a multi-valued entry for email.
  personal_data_->AddProfile(profile1);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(0, profile0.Compare(*results3[0]));
  EXPECT_EQ(0, profile1.Compare(*results3[1]));
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  // Add two test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));
  EXPECT_EQ(0, profile1.Compare(*results1[1]));

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  personal_data_->UpdateProfile(profile0);
  personal_data_->RemoveByGUID(profile1.guid());
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));
  EXPECT_EQ(0, profile2.Compare(*results2[1]));

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(0, profile0.Compare(*results3[0]));
  EXPECT_EQ(0, profile2.Compare(*results3[1]));
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card1,
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

  CreditCard credit_card2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card2,
      "Clyde Barrow", "347666888555" /* American Express */, "04", "2015");

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results1 = personal_data_->GetCreditCards();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, credit_card0.Compare(*results1[0]));
  EXPECT_EQ(0, credit_card1.Compare(*results1[1]));

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Joe"));
  personal_data_->UpdateCreditCard(credit_card0);
  personal_data_->RemoveByGUID(credit_card1.guid());
  personal_data_->AddCreditCard(credit_card2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(credit_card0, *results2[0]);
  EXPECT_EQ(credit_card2, *results2[1]);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results3 = personal_data_->GetCreditCards();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(credit_card0, *results3[0]);
  EXPECT_EQ(credit_card2, *results3[1]);
}

TEST_F(PersonalDataManagerTest, UpdateUnverifiedProfilesAndCreditCards) {
  // Start with unverified data.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  personal_data_->AddProfile(profile);
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles1.size());
  ASSERT_EQ(1U, cards1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards1[0]));

  // Try to update with just the origin changed.
  AutofillProfile original_profile(profile);
  CreditCard original_credit_card(credit_card);
  profile.set_origin("Chrome settings");
  credit_card.set_origin("Chrome settings");

  EXPECT_TRUE(profile.IsVerified());
  EXPECT_TRUE(credit_card.IsVerified());

  personal_data_->UpdateProfile(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Note: No refresh, as no update is expected.

  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles2.size());
  ASSERT_EQ(1U, cards2.size());
  EXPECT_NE(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(credit_card.origin(), cards2[0]->origin());
  EXPECT_EQ(original_profile.origin(), profiles2[0]->origin());
  EXPECT_EQ(original_credit_card.origin(), cards2[0]->origin());

  // Try to update with data changed as well.
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  credit_card.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Joe"));

  personal_data_->UpdateProfile(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& profiles3 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles3.size());
  ASSERT_EQ(1U, cards3.size());
  EXPECT_EQ(0, profile.Compare(*profiles3[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards3[0]));
  EXPECT_EQ(profile.origin(), profiles3[0]->origin());
  EXPECT_EQ(credit_card.origin(), cards3[0]->origin());
}

TEST_F(PersonalDataManagerTest, AddProfilesAndCreditCards) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card1,
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

  // Add two test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));
  EXPECT_EQ(0, profile1.Compare(*results1[1]));

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(credit_card0, *results2[0]);
  EXPECT_EQ(credit_card1, *results2[1]);

  // Determine uniqueness by inserting all of the GUIDs into a set and verifying
  // the size of the set matches the number of GUIDs.
  std::set<std::string> guids;
  guids.insert(profile0.guid());
  guids.insert(profile1.guid());
  guids.insert(credit_card0.guid());
  guids.insert(credit_card1.guid());
  EXPECT_EQ(4U, guids.size());
}

// Test for http://crbug.com/50047. Makes sure that guids are populated
// correctly on load.
TEST_F(PersonalDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "y", "", "", "", "", "", "", "", "", "", "", "");

  // Add the profile0 to the db.
  personal_data_->AddProfile(profile0);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "z", "", "", "", "", "", "", "", "", "", "", "");
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Make sure the two profiles have different GUIDs, both valid.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_NE(results3[0]->guid(), results3[1]->guid());
  EXPECT_TRUE(base::IsValidGUID(results3[0]->guid()));
  EXPECT_TRUE(base::IsValidGUID(results3[1]->guid()));
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "", "", "", "", "", "", "", "", "", "", "", "");

  // Add the empty profile to the database.
  personal_data_->AddProfile(profile0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "", "", "", "");

  // Add the empty credit card to the database.
  personal_data_->AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  // Add the test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Labels depend on other profiles in the list - update labels manually.
  std::vector<AutofillProfile *> profile_pointers;
  profile_pointers.push_back(&profile0);
  profile_pointers.push_back(&profile1);
  AutofillProfile::AdjustInferredLabels(&profile_pointers);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1[0]);
  EXPECT_EQ(profile1, *results1[1]);

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  // Adjust all labels.
  profile_pointers.push_back(&profile2);
  AutofillProfile::AdjustInferredLabels(&profile_pointers);

  scoped_refptr<AutofillWebDataService> wds =
      AutofillWebDataService::FromBrowserContext(profile_.get());
  ASSERT_TRUE(wds.get());
  wds->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(3U, results2.size());
  EXPECT_EQ(profile0, *results2[0]);
  EXPECT_EQ(profile1, *results2[1]);
  EXPECT_EQ(profile2, *results2[2]);

  wds->RemoveAutofillProfile(profile1.guid());
  wds->RemoveAutofillProfile(profile2.guid());

  // Before telling the PDM to refresh, simulate an edit to one of the deleted
  // profiles via a SetProfile update (this would happen if the Autofill window
  // was open with a previous snapshot of the profiles, and something
  // [e.g. sync] removed a profile from the browser.  In this edge case, we will
  // end up in a consistent state by dropping the write).
  profile0.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Mar"));
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jo"));
  personal_data_->UpdateProfile(profile0);
  personal_data_->AddProfile(profile1);
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(profile0, *results2[0]);
}

TEST_F(PersonalDataManagerTest, ImportFormData) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportFormDataBadEmail) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  ASSERT_EQ(static_cast<CreditCard*>(NULL), imported_credit_card);

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(PersonalDataManagerTest, ImportFormDataTwoEmails) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "George Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Confirm email:", "confirm_email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
}

// Tests two email fields containing different values blocks provile import.
TEST_F(PersonalDataManagerTest, ImportFormDataTwoDifferentEmails) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "George Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email2", "example2@example.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

TEST_F(PersonalDataManagerTest, ImportFormDataNotEnoughFilledFields) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressUSA) {
  // United States addresses must specifiy one address line, a city, state and
  // zip code.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressGB) {
  // British addresses do not require a state/province as the county is usually
  // not requested on forms.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "David Cameron", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "10 Downing Street", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "London", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Postcode:", "postcode", "SW1A 2AA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Country:", "country", "United Kingdom", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressGI) {
  // Gibraltar has the most minimal set of requirements for a valid address.
  // There are no cities or provinces and no postal/zip code system.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "Sir Adrian Johns", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "The Convent, Main Street", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Gibraltar", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportPhoneNumberSplitAcrossMultipleFields) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_area_code", "650", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_prefix", "555", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_suffix", "0000", "text", &field);
  field.max_length = 4;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", NULL, NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, "(650) 555-0000");
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  credit_card0.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("John"));
  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  credit_card1.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Paul"));
  CreditCard credit_card2(base::GenerateGUID(), "https://www.example.com");
  credit_card2.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Ringo"));
  CreditCard credit_card3(base::GenerateGUID(), "https://www.example.com");
  credit_card3.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Other"));
  CreditCard credit_card4(base::GenerateGUID(), "https://www.example.com");
  credit_card4.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Ozzy"));
  CreditCard credit_card5(base::GenerateGUID(), "https://www.example.com");
  credit_card5.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Dio"));

  // Add the test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);
  personal_data_->AddCreditCard(credit_card5);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(6U, results.size());
  EXPECT_EQ(credit_card0.guid(), results[0]->guid());
  EXPECT_EQ(credit_card1.guid(), results[1]->guid());
  EXPECT_EQ(credit_card2.guid(), results[2]->guid());
  EXPECT_EQ(credit_card3.guid(), results[3]->guid());
  EXPECT_EQ(credit_card4.guid(), results[4]->guid());
  EXPECT_EQ(credit_card5.guid(), results[5]->guid());
}

TEST_F(PersonalDataManagerTest, AggregateTwoDifferentProfiles) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create a completely different profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "John", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "22 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  AutofillProfile expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected2, "John", NULL,
      "Adams", "second@gmail.com", NULL, "22 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
  EXPECT_EQ(0, expected2.Compare(*results2[1]));
}

TEST_F(PersonalDataManagerTest, AggregateTwoProfilesWithMultiValue) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create a completely different profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "John", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Modify expected to include multi-valued fields.
  std::vector<base::string16> values;
  expected.GetRawMultiInfo(NAME_FULL, &values);
  values.push_back(ASCIIToUTF16("John Adams"));
  expected.SetRawMultiInfo(NAME_FULL, values);
  expected.GetRawMultiInfo(EMAIL_ADDRESS, &values);
  values.push_back(ASCIIToUTF16("second@gmail.com"));
  expected.SetRawMultiInfo(EMAIL_ADDRESS, values);

  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateSameProfileWithConflict) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Phone:", "phone", "6505556666", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(
      &expected, "George", NULL, "Washington", "theprez@gmail.com", NULL,
      "1600 Pennsylvania Avenue", "Suite A", "San Francisco", "California",
      "94102", NULL, "(650) 555-6666");
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create an updated profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  // Country gets added.
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form2.fields.push_back(field);
  // Phone gets updated.
  test::CreateTestFormField("Phone:", "phone", "6502231234", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Add multi-valued phone number to expectation.  Also, country gets added.
  std::vector<base::string16> values;
  expected.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  values.push_back(ASCIIToUTF16("(650) 223-1234"));
  expected.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInOld) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", NULL, NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  AutofillProfile expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected2, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInNew) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", "Government", "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  // Note missing Company field.
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Expect no change.
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithInsufficientAddress) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure1,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

TEST_F(PersonalDataManagerTest, AggregateExistingAuxiliaryProfile) {
  // Simulate having access to an auxiliary profile.
  // |auxiliary_profile| will be owned by |personal_data_|.
  AutofillProfile* auxiliary_profile =
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(auxiliary_profile,
      "Tester", "Frederick", "McAddressBookTesterson",
      "tester@example.com", "Acme Inc.", "1 Main", "Apt A", "San Francisco",
      "CA", "94102", "US", "1.415.888.9999");
  ScopedVector<AutofillProfile>& auxiliary_profiles =
      personal_data_->auxiliary_profiles_;
  auxiliary_profiles.push_back(auxiliary_profile);

  // Simulate a form submission with a subset of the info.
  // Note that the phone number format is different from the saved format.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "Tester", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "McAddressBookTesterson", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "tester@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "1 Main", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone:", "phone", "4158889999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Note: No refresh.

  // Expect no change.
  const std::vector<AutofillProfile*>& web_profiles =
      personal_data_->web_profiles();
  EXPECT_EQ(0U, web_profiles.size());
  ASSERT_EQ(1U, auxiliary_profiles.size());
  EXPECT_EQ(0, auxiliary_profile->Compare(*auxiliary_profiles[0]));
}

TEST_F(PersonalDataManagerTest, AggregateTwoDifferentCreditCards) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "5500 0000 0000 0004", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,"", "5500000000000004", "02", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
  EXPECT_EQ(0, expected2.Compare(*results2[1]));
}

TEST_F(PersonalDataManagerTest, AggregateInvalidCreditCard) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different invalid credit card.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Jim Johansen", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "1000000000000000", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure2,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateSameCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2012".
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateEmptyCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second credit card with no number.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure2,
                                              &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInNew) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2;
  // Note missing name.
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111111111111111", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));

  // Add a third credit card where the expiration date is missing.
  FormData form3;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Johnny McEnroe", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "5555555555554444", "text", &field);
  form3.fields.push_back(field);
  // Note missing expiration month and year..

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure3,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  // No change is expected.
  CreditCard expected3(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected3,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(0, expected3.Compare(*results3[0]));
}

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&saved_credit_card,
      "", "4111111111111111" /* Visa */, "01", "2011");
  personal_data_->AddCreditCard(saved_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);


  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_F(PersonalDataManagerTest, AggregateSameCreditCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&saved_credit_card,
      "Biggie Smalls", "4111 1111 1111 1111" /* Visa */, "01", "2011");
  personal_data_->AddCreditCard(saved_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results1[0]));

  // Import the same card info, but with different separators in the number.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results2[0]));
}

// Ensure that if a verified profile already exists, aggregated profiles cannot
// modify it in any way.
TEST_F(PersonalDataManagerTest, AggregateExistingVerifiedProfileWithConflict) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "Marion", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Morrison", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "other.email@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "123 Zoo St.", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Wait for the refresh, which in this case is a no-op.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(PersonalDataManagerTest,
       AggregateExistingVerifiedCreditCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), "Chrome settings");
  test::SetCreditCardInfo(&credit_card,
      "Biggie Smalls", "4111 1111 1111 1111" /* Visa */, "01", "2011");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Simulate a form submission with conflicting expiration year.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager();

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing unverified profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithVerifiedData) {
  // Start with an unverified profile.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(base::GenerateGUID());
  new_verified_profile.set_origin("Chrome settings");
  new_verified_profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, Inc."));
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the existing profile is not modified, and instead the new
  // profile is added.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, new_verified_profile.Compare(*results[0]));
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing verified profiles as well.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithExistingVerifiedData) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(base::GenerateGUID());
  new_verified_profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, Inc."));
  new_verified_profile.SetRawInfo(NAME_MIDDLE, base::string16());
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // The new profile should be merged into the existing one.
  AutofillProfile expected_profile = new_verified_profile;
  expected_profile.set_guid(profile.guid());
  std::vector<base::string16> names;
  expected_profile.GetRawMultiInfo(NAME_FULL, &names);
  names.insert(names.begin(), ASCIIToUTF16("Marion Mitchell Morrison"));
  expected_profile.SetRawMultiInfo(NAME_FULL, names);

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(expected_profile, *results[0]);
}

// Ensure that verified credit cards can be saved via SaveImportedCreditCard.
TEST_F(PersonalDataManagerTest, SaveImportedCreditCardWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), "Chrome settings");
  test::SetCreditCardInfo(&credit_card,
      "Biggie Smalls", "4111 1111 1111 1111" /* Visa */, "01", "2011");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(base::GenerateGUID());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("B. Small"));
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->SaveImportedCreditCard(new_verified_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(ASCIIToUTF16("B. Small"), results[0]->GetRawInfo(CREDIT_CARD_NAME));
}

TEST_F(PersonalDataManagerTest, GetNonEmptyTypes) {
  // Check that there are no available types with no profiles stored.
  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(0U, non_empty_types.size());

  // Test with one profile stored.
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", NULL, "Morrison",
      "johnwayne@me.xyz", NULL, "123 Zoo St.", NULL, "Hollywood", "CA",
      "91601", "US", "14155678910");

  personal_data_->AddProfile(profile0);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(14U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with multiple profiles stored.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "16502937549");

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "16502937549");

  personal_data_->AddProfile(profile1);
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(18U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with credit card information also stored.
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card,
                          "John Dillinger", "423456789012" /* Visa */,
                          "01", "2010");
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(26U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NAME));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_TYPE));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));
}

TEST_F(PersonalDataManagerTest, CaseInsensitiveMultiValueAggregation) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Phone number:", "phone_number", "817-555-6789", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, "(817) 555-6789");
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Upper-case the first name and change the phone number.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "GEORGE", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Phone number:", "phone_number", "214-555-1234", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataChanged()).WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Modify expected to include multi-valued fields.
  std::vector<base::string16> values;
  expected.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  values.push_back(ASCIIToUTF16("(214) 555-1234"));
  expected.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);

  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, IncognitoReadOnly) {
  ASSERT_TRUE(personal_data_->GetProfiles().empty());
  ASSERT_TRUE(personal_data_->GetCreditCards().empty());

  AutofillProfile steve_jobs(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&steve_jobs, "Steven", "Paul", "Jobs", "sjobs@apple.com",
      "Apple Computer, Inc.", "1 Infinite Loop", "", "Cupertino", "CA", "95014",
      "US", "(800) 275-2273");
  personal_data_->AddProfile(steve_jobs);

  CreditCard bill_gates(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(
      &bill_gates, "William H. Gates", "5555555555554444", "1", "2020");
  personal_data_->AddCreditCard(bill_gates);

  ResetPersonalDataManager();
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // After this point no adds, saves, or updates should take effect.
  MakeProfileIncognito();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  // Add profiles or credit card shouldn't work.
  personal_data_->AddProfile(test::GetFullProfile());

  CreditCard larry_page(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(
      &larry_page, "Lawrence Page", "4111111111111111", "10", "2025");
  personal_data_->AddCreditCard(larry_page);

  ResetPersonalDataManager();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Saving or creating profiles from imported profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Steve"));
  personal_data_->SaveImportedProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill Gates"));
  personal_data_->SaveImportedCreditCard(bill_gates);

  ResetPersonalDataManager();
  EXPECT_EQ(ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("William H. Gates"),
            personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME));

  // Updating existing profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Steve"));
  personal_data_->UpdateProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill Gates"));
  personal_data_->UpdateCreditCard(bill_gates);

  ResetPersonalDataManager();
  EXPECT_EQ(ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("William H. Gates"),
            personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME));

  // Removing shouldn't work.
  personal_data_->RemoveByGUID(steve_jobs.guid());
  personal_data_->RemoveByGUID(bill_gates.guid());

  ResetPersonalDataManager();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
}

}  // namespace autofill
