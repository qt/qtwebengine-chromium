// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "ui/gfx/rect.h"

using testing::_;
using blink::WebAutofillClient;

namespace autofill {

namespace {

// A constant value to use as the Autofill query ID.
const int kQueryId = 5;

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() {}
  // Mock methods to enable testability.
  MOCK_METHOD1(SetRendererActionOnFormDataReception,
               void(RendererFormDataAction action));
  MOCK_METHOD1(RendererShouldAcceptDataListSuggestion,
               void(const base::string16&));
  MOCK_METHOD0(RendererShouldClearFilledForm, void());
  MOCK_METHOD0(RendererShouldClearPreviewedForm, void());
  MOCK_METHOD1(RendererShouldSetNodeText, void(const base::string16&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class MockAutofillManagerDelegate
    : public autofill::TestAutofillManagerDelegate {
 public:
  MockAutofillManagerDelegate() {}

  MOCK_METHOD7(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<base::string16>& values,
                    const std::vector<base::string16>& labels,
                    const std::vector<base::string16>& icons,
                    const std::vector<int>& identifiers,
                    base::WeakPtr<AutofillPopupDelegate> delegate));

  MOCK_METHOD2(UpdateAutofillPopupDataListValues,
               void(const std::vector<base::string16>& values,
                    const std::vector<base::string16>& lables));

  MOCK_METHOD0(HideAutofillPopup, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManagerDelegate);
};

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver,
                      MockAutofillManagerDelegate* delegate)
      // Force to use the constructor designated for unit test, but we don't
      // really need personal_data in this test so we pass a NULL pointer.
      : AutofillManager(driver, delegate, NULL) {
  }
  virtual ~MockAutofillManager() {}

  MOCK_METHOD4(OnFillAutofillFormData,
               void(int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    int unique_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    autofill_driver_.reset(new MockAutofillDriver());
    autofill_manager_.reset(
        new MockAutofillManager(autofill_driver_.get(),
                                &manager_delegate_));
    external_delegate_.reset(
        new AutofillExternalDelegate(
            autofill_manager_.get(), autofill_driver_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    external_delegate_.reset();
    autofill_driver_.reset();
  }

  // Issue an OnQuery call with the given |query_id|.
  void IssueOnQuery(int query_id) {
    const FormData form;
    FormFieldData field;
    field.is_focusable = true;
    field.should_autocomplete = true;
    const gfx::RectF element_bounds;

    external_delegate_->OnQuery(query_id, form, field, element_bounds, true);
  }

  MockAutofillManagerDelegate manager_delegate_;
  scoped_ptr<MockAutofillDriver> autofill_driver_;
  scoped_ptr<MockAutofillManager> autofill_manager_;
  scoped_ptr<AutofillExternalDelegate> external_delegate_;

  base::MessageLoop message_loop_;
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery(kQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      kAutofillProfileId,
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDAutofillOptions)),
                  _));

  // This should call ShowAutofillPopup.
  std::vector<base::string16> autofill_item;
  autofill_item.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(kAutofillProfileId);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);

  // Called by DidAutofillSuggestions, add expectation to remove warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  EXPECT_CALL(*autofill_driver_, SetRendererActionOnFormDataReception(
      AutofillDriver::FORM_DATA_ACTION_FILL));

  EXPECT_CALL(manager_delegate_, HideAutofillPopup());

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0], autofill_ids[0]);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery(kQueryId);

  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDDataListEntry),
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      kAutofillProfileId,
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDAutofillOptions)),
                  _));

  // This should call ShowAutofillPopup.
  std::vector<base::string16> autofill_item;
  autofill_item.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(kAutofillProfileId);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  // The enum must be cast to an int to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDDataListEntry)),
                  _));

  autofill_item = std::vector<base::string16>();
  autofill_ids = std::vector<int>();
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery(kQueryId);

  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(_, _, _, _, _, _, _)).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDDataListEntry),
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      kAutofillProfileId,
                      static_cast<int>(WebAutofillClient::MenuItemIDSeparator),
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDAutofillOptions)),
                  _));

  // Ensure the popup is displayed.
  std::vector<base::string16> autofill_item;
  autofill_item.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(kAutofillProfileId);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate_->OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.push_back(base::string16());

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              UpdateAutofillPopupDataListValues(data_list_items,
                                                data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery(kQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      static_cast<int>(
                          WebAutofillClient::MenuItemIDWarningMessage)),
                  _));

  // This should call ShowAutofillPopup.
  std::vector<base::string16> autofill_item;
  autofill_item.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(WebAutofillClient::MenuItemIDWarningMessage);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);
}

// Test that the Autofill popup doesn't display a warning explaining why
// Autofill is disabled for a website when there are no Autofill suggestions.
// Regression test for http://crbug.com/105636
TEST_F(AutofillExternalDelegateUnitTest, NoAutofillWarningsWithoutSuggestions) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;
  const gfx::RectF element_bounds;

  external_delegate_->OnQuery(kQueryId, form, field, element_bounds, true);

  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(manager_delegate_, HideAutofillPopup()).Times(1);

  // This should not call ShowAutofillPopup.
  std::vector<base::string16> autofill_item;
  autofill_item.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(WebAutofillClient::MenuItemIDAutocompleteEntry);
  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_item,
                                            autofill_item,
                                            autofill_item,
                                            autofill_ids);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _)).Times(0);
  EXPECT_CALL(*autofill_driver_,
              SetRendererActionOnFormDataReception(_)).Times(0);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(-1);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(manager_delegate_, HideAutofillPopup());
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _)).Times(0);
  EXPECT_CALL(*autofill_driver_,
              SetRendererActionOnFormDataReception(_)).Times(0);
  external_delegate_->DidAcceptSuggestion(base::string16(), -1);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Called by DidSelectSuggestion, add expectation to remove warning.
  EXPECT_CALL(*autofill_manager_, OnFillAutofillFormData(_, _, _, _));

  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(
      WebAutofillClient::MenuItemIDPasswordEntry);

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_, SetRendererActionOnFormDataReception(
      AutofillDriver::FORM_DATA_ACTION_PREVIEW));
  external_delegate_->DidSelectSuggestion(1);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(manager_delegate_, ShowAutofillPopup(_, _, _, _, _, _, _));
  autofill::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(manager_delegate_, HideAutofillPopup());
  external_delegate_->DidEndTextFieldEditing();
}

// Test that the popup is marked as visible after recieving password
// suggestions.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegatePasswordSuggestions) {
  static const base::string16 kUsername = ASCIIToUTF16("username");
  static const base::string16 kSignonRealm = ASCIIToUTF16("http://foo.com/");
  std::vector<base::string16> suggestions;
  suggestions.push_back(kUsername);
  std::vector<base::string16> realms;
  realms.push_back(kSignonRealm);

  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  const gfx::RectF element_bounds;

  FormFieldData username_field_data;
  username_field_data.value = kUsername;
  PasswordFormFillData password_form_fill_data;
  password_form_fill_data.basic_data.fields.push_back(username_field_data);
  external_delegate_->AddPasswordFormMapping(field, password_form_fill_data);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(manager_delegate_,
              ShowAutofillPopup(
                  _, _, _, _, _,
                  testing::ElementsAre(
                      static_cast<int>(
                           WebAutofillClient::MenuItemIDPasswordEntry)),
                  _));

  external_delegate_->OnShowPasswordSuggestions(suggestions,
                                                realms,
                                                field,
                                                element_bounds);

  EXPECT_CALL(manager_delegate_, HideAutofillPopup());

  // This should trigger a call to hide the popup since
  // we've selected an option.
  external_delegate_->DidAcceptSuggestion(
      suggestions[0],
      WebAutofillClient::MenuItemIDPasswordEntry);
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateAcceptSuggestion) {
  EXPECT_CALL(manager_delegate_, HideAutofillPopup());
  base::string16 dummy_string(ASCIIToUTF16("baz qux"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldAcceptDataListSuggestion(dummy_string));
  external_delegate_->DidAcceptSuggestion(
      dummy_string,
      WebAutofillClient::MenuItemIDDataListEntry);
}

// Test that the driver is directed to clear the form after being notified that
// the user accepted the suggestion to clear the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearForm) {
  EXPECT_CALL(manager_delegate_, HideAutofillPopup());
  EXPECT_CALL(*autofill_driver_, RendererShouldClearFilledForm());

  external_delegate_->DidAcceptSuggestion(
      base::string16(),
      WebAutofillClient::MenuItemIDClearForm);
}

TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateHideWarning) {
  // Set up a field that shouldn't get autocompleted or display warnings.
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;
  const gfx::RectF element_bounds;

  external_delegate_->OnQuery(kQueryId, form, field, element_bounds, false);

  std::vector<base::string16> autofill_items;
  autofill_items.push_back(base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(WebAutofillClient::MenuItemIDAutocompleteEntry);

  // Ensure the popup tries to hide itself, since it is not allowed to show
  // anything.
  EXPECT_CALL(manager_delegate_, HideAutofillPopup());

  external_delegate_->OnSuggestionsReturned(kQueryId,
                                            autofill_items,
                                            autofill_items,
                                            autofill_items,
                                            autofill_ids);
}

TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateSetNodeText) {
  EXPECT_CALL(manager_delegate_, HideAutofillPopup());
  base::string16 dummy_string(ASCIIToUTF16("baz foo"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldSetNodeText(dummy_string));
  external_delegate_->DidAcceptSuggestion(
      dummy_string,
      WebAutofillClient::MenuItemIDAutocompleteEntry);
}

}  // namespace autofill
