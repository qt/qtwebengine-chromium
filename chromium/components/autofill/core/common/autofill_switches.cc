// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {
namespace switches {

// Flag used to tell Chrome the base url of the Autofill service.
const char kAutofillServiceUrl[]            = "autofill-service-url";

// Disables an interactive autocomplete UI. See kEnableInteractiveAutocomplete
// for a description.
const char kDisableInteractiveAutocomplete[] =
    "disable-interactive-autocomplete";

// Disables password generation when we detect that the user is going through
// account creation.
const char kDisablePasswordGeneration[]     = "disable-password-generation";

// Forces the password manager to ignore autocomplete='off' for password forms.
const char kEnableIgnoreAutocompleteOff[]  = "enable-ignore-autocomplete-off";

// Enables an interactive autocomplete UI and a way to invoke this UI from
// WebKit by enabling HTMLFormElement#requestAutocomplete (and associated
// autocomplete* events and logic).
const char kEnableInteractiveAutocomplete[] = "enable-interactive-autocomplete";

// Enables password generation when we detect that the user is going through
// account creation.
const char kEnablePasswordGeneration[]      = "enable-password-generation";

// Removes the requirement that we recieved a ping from the autofill servers.
// Used in testing.
const char kNoAutofillNecessaryForPasswordGeneration[] =
    "no-autofill-for-password-generation";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[]   = "show-autofill-type-predictions";

// Secure service URL for Online Wallet service. Used as the base url to escrow
// credit card numbers.
const char kWalletSecureServiceUrl[]        = "wallet-secure-service-url";

// Service URL for Online Wallet service. Used as the base url for Online Wallet
// API calls.
const char kWalletServiceUrl[]              = "wallet-service-url";

// Use the sandbox Online Wallet service URL (for developer testing).
const char kWalletServiceUseSandbox[]       = "wallet-service-use-sandbox";

}  // namespace switches
}  // namespace autofill
