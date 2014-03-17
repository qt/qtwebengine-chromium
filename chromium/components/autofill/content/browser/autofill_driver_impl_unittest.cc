// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const std::string kAppLocale = "en-US";
const AutofillManager::AutofillDownloadManagerState kDownloadState =
    AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

}  // namespace

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver,
                      AutofillManagerDelegate* delegate)
      : AutofillManager(driver, delegate, kAppLocale, kDownloadState) {
  }
  virtual ~MockAutofillManager() {}

  MOCK_METHOD0(Reset, void());
};

class TestAutofillDriverImpl : public AutofillDriverImpl {
 public:
  TestAutofillDriverImpl(content::WebContents* contents,
                         AutofillManagerDelegate* delegate)
      : AutofillDriverImpl(contents, delegate, kAppLocale, kDownloadState) {
    scoped_ptr<AutofillManager> autofill_manager(
        new MockAutofillManager(this, delegate));
    SetAutofillManager(autofill_manager.Pass());
  }
  virtual ~TestAutofillDriverImpl() {}

  virtual MockAutofillManager* mock_autofill_manager() {
    return static_cast<MockAutofillManager*>(autofill_manager());
  }

  using AutofillDriverImpl::DidNavigateMainFrame;
};

class AutofillDriverImplTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    test_manager_delegate_.reset(new TestAutofillManagerDelegate());
    driver_.reset(new TestAutofillDriverImpl(web_contents(),
                                             test_manager_delegate_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Reset the driver now to cause all pref observers to be removed and avoid
    // crashes that otherwise occur in the destructor.
    driver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Searches for an |AutofillMsg_FormDataFilled| message in the queue of sent
  // IPC messages. If none is present, returns false. Otherwise, extracts the
  // first |AutofillMsg_FormDataFilled| message, fills the output parameters
  // with the values of the message's parameters, and clears the queue of sent
  // messages.
  bool GetAutofillFormDataFilledMessage(int* page_id, FormData* results) {
    const uint32 kMsgID = AutofillMsg_FormDataFilled::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<int, FormData> autofill_param;
    AutofillMsg_FormDataFilled::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (results)
      *results = autofill_param.b;
    process()->sink().ClearMessages();
    return true;
  }

  // Searches for an |AutofillMsg_FieldTypePredictionsAvailable| message in the
  // queue of sent IPC messages. If none is present, returns false. Otherwise,
  // extracts the first |AutofillMsg_FieldTypePredictionsAvailable| message,
  // fills the output parameter with the values of the message's parameter, and
  // clears the queue of sent messages.
  bool GetFieldTypePredictionsAvailable(
      std::vector<FormDataPredictions>* predictions) {
    const uint32 kMsgID = AutofillMsg_FieldTypePredictionsAvailable::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple1<std::vector<FormDataPredictions> > autofill_param;
    AutofillMsg_FieldTypePredictionsAvailable::Read(message, &autofill_param);
    if (predictions)
      *predictions = autofill_param.a;

    process()->sink().ClearMessages();
    return true;
  }

  // Searches for a message matching |messageID| in the queue of sent IPC
  // messages. If none is present, returns false. Otherwise, extracts the first
  // matching message, fills the output parameter with the string16 from the
  // message's parameter, and clears the queue of sent messages.
  bool GetString16FromMessageWithID(uint32 messageID, base::string16* value) {
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(messageID);
    if (!message)
      return false;
    Tuple1<base::string16> autofill_param;
    switch (messageID) {
      case AutofillMsg_SetNodeText::ID:
        AutofillMsg_SetNodeText::Read(message, &autofill_param);
        break;
      case AutofillMsg_AcceptDataListSuggestion::ID:
        AutofillMsg_AcceptDataListSuggestion::Read(message, &autofill_param);
        break;
    case AutofillMsg_AcceptPasswordAutofillSuggestion::ID:
        AutofillMsg_AcceptPasswordAutofillSuggestion::Read(
            message,
            &autofill_param);
        break;
      default:
        NOTREACHED();
    }
    if (value)
      *value = autofill_param.a;
    process()->sink().ClearMessages();
    return true;
  }

  // Searches for a message matching |messageID| in the queue of sent IPC
  // messages. If none is present, returns false. Otherwise, clears the queue
  // of sent messages and returns true.
  bool HasMessageMatchingID(uint32 messageID) {
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(messageID);
    if (!message)
      return false;
    process()->sink().ClearMessages();
    return true;
  }

  scoped_ptr<TestAutofillManagerDelegate> test_manager_delegate_;
  scoped_ptr<TestAutofillDriverImpl> driver_;
};

TEST_F(AutofillDriverImplTest, GetURLRequestContext) {
  net::URLRequestContextGetter* request_context =
      driver_->GetURLRequestContext();
  net::URLRequestContextGetter* expected_request_context =
      web_contents()->GetBrowserContext()->GetRequestContext();
  EXPECT_EQ(request_context, expected_request_context);
}

TEST_F(AutofillDriverImplTest, NavigatedToDifferentPage) {
  EXPECT_CALL(*driver_->mock_autofill_manager(), Reset());
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  ASSERT_TRUE(details.is_navigation_to_different_page());
  content::FrameNavigateParams params = content::FrameNavigateParams();
  driver_->DidNavigateMainFrame(details, params);
}

TEST_F(AutofillDriverImplTest, NavigatedWithinSamePage) {
  EXPECT_CALL(*driver_->mock_autofill_manager(), Reset()).Times(0);
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = false;
  ASSERT_TRUE(!details.is_navigation_to_different_page());
  content::FrameNavigateParams params = content::FrameNavigateParams();
  driver_->DidNavigateMainFrame(details, params);
}

TEST_F(AutofillDriverImplTest, FormDataSentToRenderer) {
  int input_page_id = 42;
  FormData input_form_data;
  test::CreateTestAddressFormData(&input_form_data);
  driver_->SendFormDataToRenderer(input_page_id, input_form_data);

  int output_page_id = 0;
  FormData output_form_data;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&output_page_id,
                                               &output_form_data));
  EXPECT_EQ(input_page_id, output_page_id);
  EXPECT_EQ(input_form_data, output_form_data);
}

TEST_F(AutofillDriverImplTest, TypePredictionsNotSentToRendererWhenDisabled) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormStructure form_structure(form);
  std::vector<FormStructure*> forms(1, &form_structure);
  driver_->SendAutofillTypePredictionsToRenderer(forms);
  EXPECT_FALSE(GetFieldTypePredictionsAvailable(NULL));
}

TEST_F(AutofillDriverImplTest, TypePredictionsSentToRendererWhenEnabled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kShowAutofillTypePredictions);

  FormData form;
  test::CreateTestAddressFormData(&form);
  FormStructure form_structure(form);
  std::vector<FormStructure*> forms(1, &form_structure);
  std::vector<FormDataPredictions> expected_type_predictions;
  FormStructure::GetFieldTypePredictions(forms, &expected_type_predictions);
  driver_->SendAutofillTypePredictionsToRenderer(forms);

  std::vector<FormDataPredictions> output_type_predictions;
  EXPECT_TRUE(GetFieldTypePredictionsAvailable(&output_type_predictions));
  EXPECT_EQ(expected_type_predictions, output_type_predictions);
}

TEST_F(AutofillDriverImplTest, PreviewActionSentToRenderer) {
  driver_->SetRendererActionOnFormDataReception(
      AutofillDriver::FORM_DATA_ACTION_PREVIEW);
  EXPECT_TRUE(HasMessageMatchingID(AutofillMsg_SetAutofillActionPreview::ID));
}

TEST_F(AutofillDriverImplTest, FillActionSentToRenderer) {
  driver_->SetRendererActionOnFormDataReception(
      AutofillDriver::FORM_DATA_ACTION_FILL);
  EXPECT_TRUE(HasMessageMatchingID(AutofillMsg_SetAutofillActionFill::ID));
}

TEST_F(AutofillDriverImplTest, AcceptDataListSuggestion) {
  base::string16 input_value(ASCIIToUTF16("barfoo"));
  base::string16 output_value;
  driver_->RendererShouldAcceptDataListSuggestion(input_value);
  EXPECT_TRUE(GetString16FromMessageWithID(
      AutofillMsg_AcceptDataListSuggestion::ID,
      &output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_F(AutofillDriverImplTest, AcceptPasswordAutofillSuggestion) {
  base::string16 input_value(ASCIIToUTF16("barbaz"));
  base::string16 output_value;
  driver_->RendererShouldAcceptPasswordAutofillSuggestion(input_value);
  EXPECT_TRUE(GetString16FromMessageWithID(
      AutofillMsg_AcceptPasswordAutofillSuggestion::ID,
      &output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_F(AutofillDriverImplTest, ClearFilledFormSentToRenderer) {
  driver_->RendererShouldClearFilledForm();
  EXPECT_TRUE(HasMessageMatchingID(AutofillMsg_ClearForm::ID));
}

TEST_F(AutofillDriverImplTest, ClearPreviewedFormSentToRenderer) {
  driver_->RendererShouldClearPreviewedForm();
  EXPECT_TRUE(HasMessageMatchingID(AutofillMsg_ClearPreviewedForm::ID));
}

TEST_F(AutofillDriverImplTest, SetNodeText) {
  base::string16 input_value(ASCIIToUTF16("barqux"));
  base::string16 output_value;
  driver_->RendererShouldSetNodeText(input_value);
  EXPECT_TRUE(GetString16FromMessageWithID(AutofillMsg_SetNodeText::ID,
                                           &output_value));
  EXPECT_EQ(input_value, output_value);
}

}  // namespace autofill
