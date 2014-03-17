// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_H_
#define PRINTING_PRINTING_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "printing/print_settings.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
}

namespace printing {

// An abstraction of a printer context, implemented by objects that describe the
// user selected printing context. This includes the OS-dependent UI to ask the
// user about the print settings. Concrete implementations directly talk to the
// printer and manage the document and page breaks.
class PRINTING_EXPORT PrintingContext {
 public:
  // Tri-state result for user behavior-dependent functions.
  enum Result {
    OK,
    CANCEL,
    FAILED,
  };

  virtual ~PrintingContext();

  // Callback of AskUserForSettings, used to notify the PrintJobWorker when
  // print settings are available.
  typedef base::Callback<void(Result)> PrintSettingsCallback;

  // Asks the user what printer and format should be used to print. Updates the
  // context with the select device settings. The result of the call is returned
  // in the callback. This is necessary for Linux, which only has an
  // asynchronous printing API.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  const PrintSettingsCallback& callback) = 0;

  // Selects the user's default printer and format. Updates the context with the
  // default device settings.
  virtual Result UseDefaultSettings() = 0;

  // Returns paper size to be used for PDF or Cloud Print in device units.
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() = 0;

  // Updates printer settings.
  // |external_preview| is true if pdf is going to be opened in external
  // preview. Used by MacOS only now to open Preview.app.
  virtual Result UpdatePrinterSettings(bool external_preview) = 0;

  // Updates Print Settings. |job_settings| contains all print job
  // settings information. |ranges| has the new page range settings.
  Result UpdatePrintSettings(const base::DictionaryValue& job_settings,
                             const PageRanges& ranges);

  // Initializes with predefined settings.
  virtual Result InitWithSettings(const PrintSettings& settings) = 0;

  // Does platform specific setup of the printer before the printing. Signal the
  // printer that a document is about to be spooled.
  // Warning: This function enters a message loop. That may cause side effects
  // like IPC message processing! Some printers have side-effects on this call
  // like virtual printers that ask the user for the path of the saved document;
  // for example a PDF printer.
  virtual Result NewDocument(const base::string16& document_name) = 0;

  // Starts a new page.
  virtual Result NewPage() = 0;

  // Closes the printed page.
  virtual Result PageDone() = 0;

  // Closes the printing job. After this call the object is ready to start a new
  // document.
  virtual Result DocumentDone() = 0;

  // Cancels printing. Can be used in a multi-threaded context. Takes effect
  // immediately.
  virtual void Cancel() = 0;

  // Releases the native printing context.
  virtual void ReleaseContext() = 0;

  // Returns the native context used to print.
  virtual gfx::NativeDrawingContext context() const = 0;

  // Creates an instance of this object. Implementers of this interface should
  // implement this method to create an object of their implementation. The
  // caller owns the returned object.
  static PrintingContext* Create(const std::string& app_locale);

  void set_margin_type(MarginType type);

  const PrintSettings& settings() const {
    return settings_;
  }

 protected:
  explicit PrintingContext(const std::string& app_locale);

  // Reinitializes the settings for object reuse.
  void ResetSettings();

  // Does bookkeeping when an error occurs.
  PrintingContext::Result OnError();

  // Complete print context settings.
  PrintSettings settings_;

  // The dialog box has been dismissed.
  volatile bool dialog_box_dismissed_;

  // Is a print job being done.
  volatile bool in_print_job_;

  // Did the user cancel the print job.
  volatile bool abort_printing_;

  // The application locale.
  std::string app_locale_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintingContext);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_H_
