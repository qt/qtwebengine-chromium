// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_driver_impl.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_delegate.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "ipc/ipc_message_macros.h"

namespace autofill {

namespace {

const char kAutofillDriverImplWebContentsUserDataKey[] =
    "web_contents_autofill_driver_impl";

}  // namespace

// static
void AutofillDriverImpl::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    autofill::AutofillManagerDelegate* delegate,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(kAutofillDriverImplWebContentsUserDataKey,
                        new AutofillDriverImpl(contents,
                                               delegate,
                                               app_locale,
                                               enable_download_manager));
}

// static
AutofillDriverImpl* AutofillDriverImpl::FromWebContents(
    content::WebContents* contents) {
  return static_cast<AutofillDriverImpl*>(
      contents->GetUserData(kAutofillDriverImplWebContentsUserDataKey));
}

AutofillDriverImpl::AutofillDriverImpl(
    content::WebContents* web_contents,
    autofill::AutofillManagerDelegate* delegate,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : content::WebContentsObserver(web_contents),
      autofill_manager_(new AutofillManager(
          this, delegate, app_locale, enable_download_manager)),
      autofill_external_delegate_(autofill_manager_.get(), this),
      request_autocomplete_manager_(this) {
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

AutofillDriverImpl::~AutofillDriverImpl() {}

bool AutofillDriverImpl::IsOffTheRecord() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

net::URLRequestContextGetter* AutofillDriverImpl::GetURLRequestContext() {
  return web_contents()->GetBrowserContext()->GetRequestContext();
}

content::WebContents* AutofillDriverImpl::GetWebContents() {
  return web_contents();
}

base::SequencedWorkerPool* AutofillDriverImpl::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

bool AutofillDriverImpl::RendererIsAvailable() {
  return (web_contents()->GetRenderViewHost() != NULL);
}

void AutofillDriverImpl::SetRendererActionOnFormDataReception(
    RendererFormDataAction action) {
  if (!RendererIsAvailable())
    return;

  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  switch(action) {
    case FORM_DATA_ACTION_PREVIEW:
      host->Send(new AutofillMsg_SetAutofillActionPreview(
          host->GetRoutingID()));
      return;
    case FORM_DATA_ACTION_FILL:
      host->Send(new AutofillMsg_SetAutofillActionFill(host->GetRoutingID()));
      return;
  }
}

void AutofillDriverImpl::SendFormDataToRenderer(int query_id,
                                                const FormData& data) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_FormDataFilled(host->GetRoutingID(), query_id, data));
}

void AutofillDriverImpl::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillTypePredictions))
    return;

  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (!host)
    return;

  std::vector<FormDataPredictions> type_predictions;
  FormStructure::GetFieldTypePredictions(forms, &type_predictions);
  host->Send(
      new AutofillMsg_FieldTypePredictionsAvailable(host->GetRoutingID(),
                                                    type_predictions));
}

void AutofillDriverImpl::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_AcceptDataListSuggestion(host->GetRoutingID(),
                                                      value));
}

void AutofillDriverImpl::RendererShouldAcceptPasswordAutofillSuggestion(
    const base::string16& username) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_AcceptPasswordAutofillSuggestion(host->GetRoutingID(),
                                                       username));
}

void AutofillDriverImpl::RendererShouldClearFilledForm() {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_ClearForm(host->GetRoutingID()));
}

void AutofillDriverImpl::RendererShouldClearPreviewedForm() {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_ClearPreviewedForm(host->GetRoutingID()));
}

void AutofillDriverImpl::RendererShouldSetNodeText(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_SetNodeText(host->GetRoutingID(), value));
}

bool AutofillDriverImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillDriverImpl, message)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FormsSeen, autofill_manager_.get(),
                        AutofillManager::OnFormsSeen)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FormSubmitted, autofill_manager_.get(),
                        AutofillManager::OnFormSubmitted)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_TextFieldDidChange,
                        autofill_manager_.get(),
                        AutofillManager::OnTextFieldDidChange)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_QueryFormFieldAutofill,
                        autofill_manager_.get(),
                        AutofillManager::OnQueryFormFieldAutofill)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowAutofillDialog,
                        autofill_manager_.get(),
                        AutofillManager::OnShowAutofillDialog)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FillAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnFillAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidPreviewAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnDidPreviewAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidFillAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnDidFillAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidEndTextFieldEditing,
                        autofill_manager_.get(),
                        AutofillManager::OnDidEndTextFieldEditing)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_HideAutofillUI, autofill_manager_.get(),
                        AutofillManager::OnHideAutofillUI)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_AddPasswordFormMapping,
                        autofill_manager_.get(),
                        AutofillManager::OnAddPasswordFormMapping)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowPasswordSuggestions,
                        autofill_manager_.get(),
                        AutofillManager::OnShowPasswordSuggestions)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_SetDataList, autofill_manager_.get(),
                        AutofillManager::OnSetDataList)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_RequestAutocomplete,
                        &request_autocomplete_manager_,
                        RequestAutocompleteManager::OnRequestAutocomplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillDriverImpl::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_navigation_to_different_page())
    autofill_manager_->Reset();
}

void AutofillDriverImpl::SetAutofillManager(
    scoped_ptr<AutofillManager> manager) {
  autofill_manager_ = manager.Pass();
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

void AutofillDriverImpl::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  autofill_manager_->delegate()->HideAutofillPopup();
}

void AutofillDriverImpl::WasHidden() {
  autofill_manager_->delegate()->HideAutofillPopup();
}

}  // namespace autofill
