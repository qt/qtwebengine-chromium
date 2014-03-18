// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "components/autofill/core/common/form_data.h"

namespace base {
class SequencedWorkerPool;
}

namespace net {
class URLRequestContextGetter;
}

namespace autofill {

class FormStructure;

// Interface that allows Autofill core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class AutofillDriver {
 public:
   // The possible actions that the renderer can take on receiving form data.
  enum RendererFormDataAction {
    // The renderer should fill the form data.
    FORM_DATA_ACTION_FILL,
    // The renderer should preview the form data.
    FORM_DATA_ACTION_PREVIEW
  };

  virtual ~AutofillDriver() {}

  // Returns whether the user is currently operating in an off-the-record
  // (i.e., incognito) context.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the URL request context information associated with this driver.
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;

  // Returns the SequencedWorkerPool on which core Autofill code should run
  // tasks that may block. This pool must live at least as long as the driver.
  virtual base::SequencedWorkerPool* GetBlockingPool() = 0;

  // Returns true iff the renderer is available for communication.
  virtual bool RendererIsAvailable() = 0;

  // Informs the renderer what action to take with the next form data that it
  // receives. Must be called before each call to |SendFormDataToRenderer|.
  virtual void SetRendererActionOnFormDataReception(
      RendererFormDataAction action) = 0;

  // Forwards |data| to the renderer. |query_id| is the id of the renderer's
  // original request for the data. This method is a no-op if the renderer is
  // not currently available.
  virtual void SendFormDataToRenderer(int query_id, const FormData& data) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) = 0;

  // Tells the renderer to accept the password autofill suggestion for
  // |username|.
  virtual void RendererShouldAcceptPasswordAutofillSuggestion(
      const base::string16& username) = 0;

  // Tells the renderer to clear the currently filled Autofill results.
  virtual void RendererShouldClearFilledForm() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  // Tells the renderer to set the node text.
  virtual void RendererShouldSetNodeText(const base::string16& value) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
