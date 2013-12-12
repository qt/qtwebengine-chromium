// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill {

// Helper struct for PasswordFormFillData
struct UsernamesCollectionKey {
  UsernamesCollectionKey();
  ~UsernamesCollectionKey();

  // Defined so that this struct can be used as a key in a std::map.
  bool operator<(const UsernamesCollectionKey& other) const;

  base::string16 username;
  base::string16 password;
  std::string realm;
};

struct PasswordAndRealm {
  base::string16 password;
  std::string realm;
};

// Structure used for autofilling password forms. Note that the realms in this
// struct are only set when the password's realm differs from the realm of the
// form that we are filling.
struct PasswordFormFillData {
  typedef std::map<base::string16, PasswordAndRealm> LoginCollection;
  typedef std::map<UsernamesCollectionKey,
                   std::vector<base::string16> > UsernamesCollection;

  // Identifies the HTML form on the page and preferred username/password for
  // login.
  FormData basic_data;

  // The signon realm of the preferred user/pass pair.
  std::string preferred_realm;

  // A list of other matching username->PasswordAndRealm pairs for the form.
  LoginCollection additional_logins;

  // A list of possible usernames in the case where we aren't completely sure
  // that the original saved username is correct. This data is keyed by the
  // saved username/password to ensure uniqueness, though the username is not
  // used.
  UsernamesCollection other_possible_usernames;

  // Tells us whether we need to wait for the user to enter a valid username
  // before we autofill the password. By default, this is off unless the
  // PasswordManager determined there is an additional risk associated with this
  // form. This can happen, for example, if action URI's of the observed form
  // and our saved representation don't match up.
  bool wait_for_username;

  PasswordFormFillData();
  ~PasswordFormFillData();
};

// Create a FillData structure in preparation for autofilling a form,
// from basic_data identifying which form to fill, and a collection of
// matching stored logins to use as username/password values.
// |preferred_match| should equal (address) one of matches.
// |wait_for_username_before_autofill| is true if we should not autofill
// anything until the user typed in a valid username and blurred the field.
// If |enable_possible_usernames| is true, we will populate possible_usernames
// in |result|.
void InitPasswordFormFillData(
    const PasswordForm& form_on_page,
    const PasswordFormMap& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    bool enable_other_possible_usernames,
    PasswordFormFillData* result);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H__
