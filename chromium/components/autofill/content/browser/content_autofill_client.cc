// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_client.h"

#include <memory>

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#if !BUILDFLAG(IS_QTWEBENGINE)
#include "components/autofill/core/browser/integrators/password_manager/password_manager_autofill_helper.h"
#endif

namespace autofill {

ContentAutofillClient::ContentAutofillClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ContentAutofillClient>(*web_contents),
#if !BUILDFLAG(IS_QTWEBENGINE)
      autofill_driver_factory_(web_contents, this),
      password_manager_autofill_helper_(
          std::make_unique<PasswordManagerAutofillHelper>(this)) {}
#else
      autofill_driver_factory_(web_contents, this) {}
#endif

ContentAutofillClient::~ContentAutofillClient() = default;

content::WebContents* ContentAutofillClient::web_contents() const {
  return autofill_driver_factory_.web_contents();
}

ContentAutofillDriverFactory&
ContentAutofillClient::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

bool ContentAutofillClient::DocumentUsedWebOTP() {
  return GetWebContents().GetPrimaryMainFrame()->DocumentUsedWebOTP();
}

PasswordManagerAutofillHelperDelegate*
ContentAutofillClient::GetPasswordManagerAutofillHelper() {
#if !BUILDFLAG(IS_QTWEBENGINE)
  return password_manager_autofill_helper_.get();
#else
  return nullptr;
#endif
}

AutofillManager*
ContentAutofillClient::GetAutofillManagerForPrimaryMainFrame() {
  if (auto* driver = ContentAutofillDriver::GetForRenderFrameHost(
          GetWebContents().GetPrimaryMainFrame())) {
    return &driver->GetAutofillManager();
  }
  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentAutofillClient);

}  // namespace autofill
