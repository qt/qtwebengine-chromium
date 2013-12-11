// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_

#include <vector>

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();
  virtual ~TestPersonalDataManager();

  // Adds |profile| to |profiles_|. This does not take ownership of |profile|.
  void AddTestingProfile(AutofillProfile* profile);

  // Adds |credit_card| to |credit_cards_|. This does not take ownership of
  // |credit_card|.
  void AddTestingCreditCard(CreditCard* credit_card);

  virtual const std::vector<AutofillProfile*>& GetProfiles() OVERRIDE;
  virtual const std::vector<CreditCard*>& GetCreditCards() const OVERRIDE;

  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile) OVERRIDE;
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) OVERRIDE;

  const AutofillProfile& imported_profile() { return imported_profile_; }
  const CreditCard& imported_credit_card() { return imported_credit_card_; }

 private:
  std::vector<AutofillProfile*> profiles_;
  std::vector<CreditCard*> credit_cards_;
  AutofillProfile imported_profile_;
  CreditCard imported_credit_card_;
};

}  // namespace autofill

#endif  // COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_
