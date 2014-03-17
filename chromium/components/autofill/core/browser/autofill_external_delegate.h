// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/browser/password_autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Rect;
}

namespace autofill {

class AutofillDriver;
class AutofillManager;

// TODO(csharp): A lot of the logic in this class is copied from autofillagent.
// Once Autofill is moved out of WebKit this class should be the only home for
// this logic. See http://crbug.com/51644

// Delegate for in-browser Autocomplete and Autofill display and selection.
class AutofillExternalDelegate
    : public AutofillPopupDelegate {
 public:
  // Creates an AutofillExternalDelegate for the specified AutofillManager and
  // AutofillDriver.
  AutofillExternalDelegate(AutofillManager* autofill_manager,
                           AutofillDriver* autofill_driver);
  virtual ~AutofillExternalDelegate();

  // AutofillPopupDelegate implementation.
  virtual void OnPopupShown() OVERRIDE;
  virtual void OnPopupHidden() OVERRIDE;
  virtual bool ShouldRepostEvent(const ui::MouseEvent& event) OVERRIDE;
  virtual void DidSelectSuggestion(int identifier) OVERRIDE;
  virtual void DidAcceptSuggestion(const base::string16& value,
                                   int identifier) OVERRIDE;
  virtual void RemoveSuggestion(const base::string16& value,
                                int identifier) OVERRIDE;
  virtual void ClearPreviewedForm() OVERRIDE;

  // Records and associates a query_id with web form data.  Called
  // when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative. |display_warning_if_disabled| tells us if we should
  // display warnings (such as autofill is disabled, but had suggestions).
  // We might not want to display the warning if a website has disabled
  // Autocomplete because they have their own popup, and showing our popup
  // on to of theirs would be a poor user experience.
  virtual void OnQuery(int query_id,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& element_bounds,
                       bool display_warning_if_disabled);

  // Records query results and correctly formats them before sending them off
  // to be displayed.  Called when an Autofill query result is available.
  virtual void OnSuggestionsReturned(
      int query_id,
      const std::vector<base::string16>& autofill_values,
      const std::vector<base::string16>& autofill_labels,
      const std::vector<base::string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids);

  // Show password suggestions in the popup.
  void OnShowPasswordSuggestions(const std::vector<base::string16>& suggestions,
                                 const std::vector<base::string16>& realms,
                                 const FormFieldData& field,
                                 const gfx::RectF& bounds);

  // Set the data list value associated with the current field.
  void SetCurrentDataListValues(
      const std::vector<base::string16>& data_list_values,
      const std::vector<base::string16>& data_list_labels);

  // Inform the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  // Returns the delegate to its starting state by removing any page specific
  // values or settings.
  void Reset();

  // Inform the Password Manager of a filled form.
  void AddPasswordFormMapping(
      const FormFieldData& form,
      const PasswordFormFillData& fill_data);

 protected:
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr();

 private:
  // Fills the form with the Autofill data corresponding to |unique_id|.
  // If |is_preview| is true then this is just a preview to show the user what
  // would be selected and if |is_preview| is false then the user has selected
  // this data.
  void FillAutofillFormData(int unique_id, bool is_preview);

  // Handle applying any Autofill warnings to the Autofill popup.
  void ApplyAutofillWarnings(std::vector<base::string16>* autofill_values,
                             std::vector<base::string16>* autofill_labels,
                             std::vector<base::string16>* autofill_icons,
                             std::vector<int>* autofill_unique_ids);

  // Handle applying any Autofill option listings to the Autofill popup.
  // This function should only get called when there is at least one
  // multi-field suggestion in the list of suggestions.
  void ApplyAutofillOptions(std::vector<base::string16>* autofill_values,
                            std::vector<base::string16>* autofill_labels,
                            std::vector<base::string16>* autofill_icons,
                            std::vector<int>* autofill_unique_ids);

  // Insert the data list values at the start of the given list, including
  // any required separators.
  void InsertDataListValues(std::vector<base::string16>* autofill_values,
                            std::vector<base::string16>* autofill_labels,
                            std::vector<base::string16>* autofill_icons,
                            std::vector<int>* autofill_unique_ids);

  AutofillManager* autofill_manager_;  // weak.

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* autofill_driver_;  // weak

  // Password Autofill manager, handles all password-related Autofilling.
  PasswordAutofillManager password_autofill_manager_;

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The current form and field selected by Autofill.
  FormData autofill_query_form_;
  FormFieldData autofill_query_field_;

  // The bounds of the form field that user is interacting with.
  gfx::RectF element_bounds_;

  // Should we display a warning if Autofill is disabled?
  bool display_warning_if_disabled_;

  // Does the popup include any Autofill profile or credit card suggestions?
  bool has_autofill_suggestion_;

  // Have we already shown Autofill suggestions for the field the user is
  // currently editing?  Used to keep track of state for metrics logging.
  bool has_shown_autofill_popup_for_current_edit_;

  // The current data list values.
  std::vector<base::string16> data_list_values_;
  std::vector<base::string16> data_list_labels_;

  base::WeakPtrFactory<AutofillExternalDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
