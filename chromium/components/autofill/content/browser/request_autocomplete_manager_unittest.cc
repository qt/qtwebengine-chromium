// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/content/browser/request_autocomplete_manager.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const char kAppLocale[] = "en-US";
const AutofillManager::AutofillDownloadManagerState kDownloadState =
    AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      AutofillManagerDelegate* delegate)
      : AutofillManager(driver, delegate, kAppLocale, kDownloadState),
        autofill_enabled_(true) {
  }
  virtual ~TestAutofillManager() {}

  virtual bool IsAutofillEnabled() const OVERRIDE { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

 private:
  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

class CustomTestAutofillManagerDelegate : public TestAutofillManagerDelegate {
  public:
    CustomTestAutofillManagerDelegate() : should_simulate_success_(true) {}

    virtual ~CustomTestAutofillManagerDelegate() {}

    virtual void ShowRequestAutocompleteDialog(
        const FormData& form,
        const GURL& source_url,
        const base::Callback<void(const FormStructure*)>& callback) OVERRIDE {
      if (should_simulate_success_) {
        FormStructure form_structure(form);
        callback.Run(&form_structure);
      } else {
        callback.Run(NULL);
      }
    }

    void set_should_simulate_success(bool should_simulate_success) {
      should_simulate_success_ = should_simulate_success;
    }

  private:
    // Enable testing the path where a callback is called without a
    // valid FormStructure.
    bool should_simulate_success_;

    DISALLOW_COPY_AND_ASSIGN(CustomTestAutofillManagerDelegate);
};

class TestAutofillDriverImpl : public AutofillDriverImpl {
 public:
  TestAutofillDriverImpl(content::WebContents* contents,
                         AutofillManagerDelegate* delegate)
      : AutofillDriverImpl(contents, delegate, kAppLocale, kDownloadState) {
    SetAutofillManager(make_scoped_ptr<AutofillManager>(
        new TestAutofillManager(this, delegate)));
  }
  virtual ~TestAutofillDriverImpl() {}

  TestAutofillManager* mock_autofill_manager() {
    return static_cast<TestAutofillManager*>(autofill_manager());
  }

  using AutofillDriverImpl::DidNavigateMainFrame;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriverImpl);
};

}  // namespace

class RequestAutocompleteManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  RequestAutocompleteManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    driver_.reset(new TestAutofillDriverImpl(web_contents(),
                                             &manager_delegate_));
    request_autocomplete_manager_.reset(
        new RequestAutocompleteManager(driver_.get()));
  }

  virtual void TearDown() OVERRIDE {
    // Reset the driver now to cause all pref observers to be removed and avoid
    // crashes that otherwise occur in the destructor.
    driver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Searches for an |AutofillMsg_RequestAutocompleteResult| message in the
  // queue of sent IPC messages. If none is present, returns false. Otherwise,
  // extracts the first |AutofillMsg_RequestAutocompleteResult| message, fills
  // the output parameter with the value of the message's parameter, and
  // clears the queue of sent messages.
  bool GetAutocompleteResultMessage(
      blink::WebFormElement::AutocompleteResult* result) {
    const uint32 kMsgID = AutofillMsg_RequestAutocompleteResult::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<blink::WebFormElement::AutocompleteResult, FormData> autofill_param;
    AutofillMsg_RequestAutocompleteResult::Read(message, &autofill_param);
    *result = autofill_param.a;
    process()->sink().ClearMessages();
    return true;
  }

 protected:
  CustomTestAutofillManagerDelegate manager_delegate_;
  scoped_ptr<TestAutofillDriverImpl> driver_;
  scoped_ptr<RequestAutocompleteManager> request_autocomplete_manager_;

  DISALLOW_COPY_AND_ASSIGN(RequestAutocompleteManagerTest);
};

TEST_F(RequestAutocompleteManagerTest, OnRequestAutocompleteSuccess) {
  blink::WebFormElement::AutocompleteResult result;
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(result, blink::WebFormElement::AutocompleteResultSuccess);
}

TEST_F(RequestAutocompleteManagerTest, OnRequestAutocompleteCancel) {
  blink::WebFormElement::AutocompleteResult result;
  manager_delegate_.set_should_simulate_success(false);
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(result, blink::WebFormElement::AutocompleteResultErrorCancel);
}

TEST_F(RequestAutocompleteManagerTest,
       OnRequestAutocompleteWithAutocompleteDisabled) {
  blink::WebFormElement::AutocompleteResult result;
  driver_->mock_autofill_manager()->set_autofill_enabled(false);
  request_autocomplete_manager_->OnRequestAutocomplete(FormData(), GURL());
  EXPECT_TRUE(GetAutocompleteResultMessage(&result));
  EXPECT_EQ(result, blink::WebFormElement::AutocompleteResultErrorDisabled);
}

}  // namespace autofill
