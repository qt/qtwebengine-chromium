// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebPasswordFormData.h"

using WebKit::WebFormElement;
using WebKit::WebPasswordFormData;

namespace autofill {
namespace {

scoped_ptr<PasswordForm> InitPasswordFormFromWebPasswordForm(
    const WebFormElement& web_form,
    const WebKit::WebPasswordFormData& web_password_form) {
  PasswordForm* password_form = new PasswordForm();
  password_form->signon_realm = web_password_form.signonRealm.utf8();
  password_form->origin = web_password_form.origin;
  password_form->action = web_password_form.action;
  password_form->submit_element = web_password_form.submitElement;
  password_form->username_element = web_password_form.userNameElement;
  password_form->username_value = web_password_form.userNameValue;
  password_form->other_possible_usernames.insert(
      password_form->other_possible_usernames.begin(),
      web_password_form.possibleUserNames.data(),
      web_password_form.possibleUserNames.data() +
      web_password_form.possibleUserNames.size());
  password_form->password_element = web_password_form.passwordElement;
  password_form->password_value = web_password_form.passwordValue;
  password_form->password_autocomplete_set =
      web_password_form.passwordShouldAutocomplete;
  password_form->old_password_element = web_password_form.oldPasswordElement;
  password_form->old_password_value = web_password_form.oldPasswordValue;
  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->ssl_valid = false;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;
  WebFormElementToFormData(web_form,
                           WebKit::WebFormControlElement(),
                           REQUIRE_NONE,
                           EXTRACT_NONE,
                           &password_form->form_data,
                           NULL /* FormFieldData */);
  return scoped_ptr<PasswordForm>(password_form);
}

}  // namespace

scoped_ptr<PasswordForm> CreatePasswordForm(const WebFormElement& webform) {
  WebPasswordFormData web_password_form(webform);
  if (web_password_form.isValid())
    return InitPasswordFormFromWebPasswordForm(webform, web_password_form);
  return scoped_ptr<PasswordForm>();
}

}  // namespace autofill
