// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_gtk.h"

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include "base/logging.h"
#include "base/values.h"
#include "printing/metafile.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace {

// Function pointer for creating print dialogs. |callback| is only used when
// |show_dialog| is true.
printing::PrintDialogGtkInterface* (*create_dialog_func_)(
    printing::PrintingContextGtk* context) = NULL;

}  // namespace

namespace printing {

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextGtk(app_locale));
}

PrintingContextGtk::PrintingContextGtk(const std::string& app_locale)
    : PrintingContext(app_locale),
      print_dialog_(NULL) {
}

PrintingContextGtk::~PrintingContextGtk() {
  ReleaseContext();

  if (print_dialog_)
    print_dialog_->ReleaseDialog();
}

// static
void PrintingContextGtk::SetCreatePrintDialogFunction(
    PrintDialogGtkInterface* (*create_dialog_func)(
        PrintingContextGtk* context)) {
  DCHECK(create_dialog_func);
  DCHECK(!create_dialog_func_);
  create_dialog_func_ = create_dialog_func;
}

void PrintingContextGtk::PrintDocument(const Metafile* metafile) {
  DCHECK(print_dialog_);
  DCHECK(metafile);
  print_dialog_->PrintDocument(metafile, document_name_);
}

void PrintingContextGtk::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    const PrintSettingsCallback& callback) {
  print_dialog_->ShowDialog(parent_view, has_selection, callback);
}

PrintingContext::Result PrintingContextGtk::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();
  if (!print_dialog_) {
    print_dialog_ = create_dialog_func_(this);
    print_dialog_->AddRefToDialog();
  }
  print_dialog_->UseDefaultSettings();

  return OK;
}

gfx::Size PrintingContextGtk::GetPdfPaperSizeDeviceUnits() {
  GtkPageSetup* page_setup = gtk_page_setup_new();

  gfx::SizeF paper_size(
      gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH),
      gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH));

  g_object_unref(page_setup);

  return gfx::Size(
      paper_size.width() * settings_.device_units_per_inch(),
      paper_size.height() * settings_.device_units_per_inch());
}

PrintingContext::Result PrintingContextGtk::UpdatePrinterSettings(
    bool external_preview) {
  DCHECK(!in_print_job_);
  DCHECK(!external_preview) << "Not implemented";

  if (!print_dialog_) {
    print_dialog_ = create_dialog_func_(this);
    print_dialog_->AddRefToDialog();
  }

  if (!print_dialog_->UpdateSettings(&settings_))
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextGtk::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  return OK;
}

PrintingContext::Result PrintingContextGtk::NewDocument(
    const base::string16& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  document_name_ = document_name;

  return OK;
}

PrintingContext::Result PrintingContextGtk::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextGtk::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextGtk::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  ResetSettings();
  return OK;
}

void PrintingContextGtk::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextGtk::ReleaseContext() {
  // Intentional No-op.
}

gfx::NativeDrawingContext PrintingContextGtk::context() const {
  // Intentional No-op.
  return NULL;
}

}  // namespace printing

