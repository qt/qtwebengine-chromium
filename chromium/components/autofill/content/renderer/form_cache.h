// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_

#include <map>
#include <set>
#include <vector>

#include "base/strings/string16.h"

namespace blink {
class WebDocument;
class WebFormElement;
class WebFrame;
class WebInputElement;
class WebSelectElement;
}

namespace autofill {

struct FormData;
struct FormDataPredictions;

// Manages the forms in a RenderView.
class FormCache {
 public:
  FormCache();
  ~FormCache();

  // Scans the DOM in |frame| extracting and storing forms.
  // Fills |forms| with extracted forms.
  void ExtractForms(const blink::WebFrame& frame,
                    std::vector<FormData>* forms);

  // Scans the DOM in |frame| extracting and storing forms.
  // Fills |forms| with extracted forms and |web_form_elements| with associated
  // web form elements. Returns true if there are unextracted forms due to
  // |minimum_required_fields| limit, else false.
  bool ExtractFormsAndFormElements(
      const blink::WebFrame& frame,
      size_t minimum_required_fields,
      std::vector<FormData>* forms,
      std::vector<blink::WebFormElement>* web_form_elements);

  // Resets the forms for the specified |frame|.
  void ResetFrame(const blink::WebFrame& frame);

  // Clears the values of all input elements in the form that contains
  // |element|.  Returns false if the form is not found.
  bool ClearFormWithElement(const blink::WebInputElement& element);

  // For each field in the |form|, sets the field's placeholder text to the
  // field's overall predicted type.  Also sets the title to include the field's
  // heuristic type, server type, and signature; as well as the form's signature
  // and the experiment id for the server predictions.
  bool ShowPredictions(const FormDataPredictions& form);

 private:
  // The cached web frames.
  std::set<blink::WebDocument> web_documents_;

  // The cached initial values for <select> elements.
  std::map<const blink::WebSelectElement, base::string16>
      initial_select_values_;

  // The cached initial values for checkable <input> elements.
  std::map<const blink::WebInputElement, bool> initial_checked_state_;

  DISALLOW_COPY_AND_ASSIGN(FormCache);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
