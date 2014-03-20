// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "grit/component_strings.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeCollection.h"
#include "third_party/WebKit/public/web/WebOptionElement.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebNode;
using blink::WebNodeCollection;
using blink::WebOptionElement;
using blink::WebString;

namespace {

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
const size_t kMaximumTextSizeForAutofill = 1000;

// The maximum number of data list elements to send to the browser process
// via IPC (to prevent long IPC messages).
const size_t kMaximumDataListSizeForAutofill = 30;


// Gets all the data list values (with corresponding label) for the given
// element.
void GetDataListSuggestions(const blink::WebInputElement& element,
                            bool ignore_current_value,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  WebNodeCollection options = element.dataListOptions();
  if (options.isNull())
    return;

  base::string16 prefix;
  if (!ignore_current_value) {
    prefix = element.editingValue();
    if (element.isMultiple() &&
        element.formControlType() == WebString::fromUTF8("email")) {
      std::vector<base::string16> parts;
      base::SplitStringDontTrim(prefix, ',', &parts);
      if (parts.size() > 0)
        TrimWhitespace(parts[parts.size() - 1], TRIM_LEADING, &prefix);
    }
  }
  for (WebOptionElement option = options.firstItem().to<WebOptionElement>();
       !option.isNull(); option = options.nextItem().to<WebOptionElement>()) {
    if (!StartsWith(option.value(), prefix, false) ||
        option.value() == prefix ||
        !element.isValidValue(option.value()))
      continue;

    values->push_back(option.value());
    if (option.value() != option.label())
      labels->push_back(option.label());
    else
      labels->push_back(base::string16());
  }
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaximumDataListSizeForAutofill)
    strings->resize(kMaximumDataListSizeForAutofill);

  // Limit the size of the strings in the vector.
  for (size_t i = 0; i < strings->size(); ++i) {
    if ((*strings)[i].length() > kMaximumTextSizeForAutofill)
      (*strings)[i].resize(kMaximumTextSizeForAutofill);
  }
}

gfx::RectF GetScaledBoundingBox(float scale, WebInputElement* element) {
  gfx::Rect bounding_box(element->boundsInViewportSpace());
  return gfx::RectF(bounding_box.x() * scale,
                    bounding_box.y() * scale,
                    bounding_box.width() * scale,
                    bounding_box.height() * scale);
}

}  // namespace

namespace autofill {

AutofillAgent::AutofillAgent(content::RenderView* render_view,
                             PasswordAutofillAgent* password_autofill_agent)
    : content::RenderViewObserver(render_view),
      password_autofill_agent_(password_autofill_agent),
      autofill_query_id_(0),
      autofill_action_(AUTOFILL_NONE),
      web_view_(render_view->GetWebView()),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      did_set_node_text_(false),
      has_new_forms_for_browser_(false),
      ignore_text_changes_(false),
      weak_ptr_factory_(this) {
  render_view->GetWebView()->setAutofillClient(this);

  // The PageClickTracker is a RenderViewObserver, and hence will be freed when
  // the RenderView is destroyed.
  new PageClickTracker(render_view, this);
}

AutofillAgent::~AutofillAgent() {}

bool AutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormDataFilled, OnFormDataFilled)
    IPC_MESSAGE_HANDLER(AutofillMsg_FieldTypePredictionsAvailable,
                        OnFieldTypePredictionsAvailable)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetAutofillActionFill,
                        OnSetAutofillActionFill)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearForm,
                        OnClearForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetAutofillActionPreview,
                        OnSetAutofillActionPreview)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearPreviewedForm,
                        OnClearPreviewedForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetNodeText,
                        OnSetNodeText)
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptDataListSuggestion,
                        OnAcceptDataListSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptPasswordAutofillSuggestion,
                        OnAcceptPasswordAutofillSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_RequestAutocompleteResult,
                        OnRequestAutocompleteResult)
    IPC_MESSAGE_HANDLER(AutofillMsg_PageShown,
                        OnPageShown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillAgent::DidFinishDocumentLoad(WebFrame* frame) {
  // Record timestamp on document load. This is used to record overhead of
  // Autofill feature.
  forms_seen_timestamp_ = base::TimeTicks::Now();

  // The document has now been fully loaded.  Scan for forms to be sent up to
  // the browser.
  std::vector<FormData> forms;
  bool has_more_forms = false;
  if (!frame->parent()) {
    form_elements_.clear();
    has_more_forms = form_cache_.ExtractFormsAndFormElements(
        *frame, kRequiredAutofillFields, &forms, &form_elements_);
  } else {
    form_cache_.ExtractForms(*frame, &forms);
  }

  autofill::FormsSeenState state = has_more_forms ?
      autofill::PARTIAL_FORMS_SEEN : autofill::NO_SPECIAL_FORMS_SEEN;

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->parent()) {
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       forms_seen_timestamp_,
                                       state));
  }
}

void AutofillAgent::DidCommitProvisionalLoad(WebFrame* frame,
                                             bool is_new_navigation) {
  in_flight_request_form_.reset();
}

void AutofillAgent::FrameDetached(WebFrame* frame) {
  form_cache_.ResetFrame(*frame);
}

void AutofillAgent::WillSubmitForm(WebFrame* frame,
                                   const WebFormElement& form) {
  FormData form_data;
  if (WebFormElementToFormData(form,
                               WebFormControlElement(),
                               REQUIRE_AUTOCOMPLETE,
                               static_cast<ExtractMask>(
                                   EXTRACT_VALUE | EXTRACT_OPTION_TEXT),
                               &form_data,
                               NULL)) {
    Send(new AutofillHostMsg_FormSubmitted(routing_id(), form_data,
                                           base::TimeTicks::Now()));
  }
}

void AutofillAgent::ZoomLevelChanged() {
  // Any time the zoom level changes, the page's content moves, so any Autofill
  // popups should be hidden. This is only needed for the new Autofill UI
  // because WebKit already knows to hide the old UI when this occurs.
  HideAutofillUI();
}

void AutofillAgent::FocusedNodeChanged(const blink::WebNode& node) {
  if (node.isNull() || !node.isElementNode())
    return;

  blink::WebElement web_element = node.toConst<blink::WebElement>();

  if (!web_element.document().frame())
      return;

  const WebInputElement* element = toWebInputElement(&web_element);

  if (!element || !element->isEnabled() || element->isReadOnly() ||
      !element->isTextField() || element->isPasswordField())
    return;

  element_ = *element;
}

void AutofillAgent::OrientationChangeEvent(int orientation) {
  HideAutofillUI();
}

void AutofillAgent::DidChangeScrollOffset(blink::WebFrame*) {
  HideAutofillUI();
}

void AutofillAgent::didRequestAutocomplete(blink::WebFrame* frame,
                                           const WebFormElement& form) {
  // Disallow the dialog over non-https or broken https, except when the
  // ignore SSL flag is passed. See http://crbug.com/272512.
  // TODO(palmer): this should be moved to the browser process after frames
  // get their own processes.
  GURL url(frame->document().url());
  content::SSLStatus ssl_status = render_view()->GetSSLStatusOfFrame(frame);
  bool is_safe = url.SchemeIs(content::kHttpsScheme) &&
      !net::IsCertStatusError(ssl_status.cert_status);
  bool allow_unsafe = CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kReduceSecurityForTesting);

  FormData form_data;
  if (!in_flight_request_form_.isNull() ||
      (!is_safe && !allow_unsafe) ||
      !WebFormElementToFormData(form,
                                WebFormControlElement(),
                                REQUIRE_AUTOCOMPLETE,
                                EXTRACT_OPTIONS,
                                &form_data,
                                NULL)) {
    WebFormElement(form).finishRequestAutocomplete(
        WebFormElement::AutocompleteResultErrorDisabled);
    return;
  }

  // Cancel any pending Autofill requests and hide any currently showing popups.
  ++autofill_query_id_;
  HideAutofillUI();

  in_flight_request_form_ = form;
  Send(new AutofillHostMsg_RequestAutocomplete(routing_id(), form_data, url));
}

void AutofillAgent::setIgnoreTextChanges(bool ignore) {
  ignore_text_changes_ = ignore;
}

void AutofillAgent::InputElementClicked(const WebInputElement& element,
                                        bool was_focused,
                                        bool is_focused) {
  if (was_focused)
    ShowSuggestions(element, true, false, true, false);
}

void AutofillAgent::InputElementLostFocus() {
  HideAutofillUI();
}

void AutofillAgent::didClearAutofillSelection(const WebNode& node) {
  if (password_autofill_agent_->DidClearAutofillSelection(node))
    return;

  if (!element_.isNull() && node == element_) {
    ClearPreviewedFormWithElement(element_, was_query_node_autofilled_);
  } else {
    // TODO(isherman): There seem to be rare cases where this code *is*
    // reachable: see [ http://crbug.com/96321#c6 ].  Ideally we would
    // understand those cases and fix the code to avoid them.  However, so far I
    // have been unable to reproduce such a case locally.  If you hit this
    // NOTREACHED(), please file a bug against me.
    NOTREACHED();
  }
}

void AutofillAgent::textFieldDidEndEditing(const WebInputElement& element) {
  password_autofill_agent_->TextFieldDidEndEditing(element);
  has_shown_autofill_popup_for_current_edit_ = false;
  Send(new AutofillHostMsg_DidEndTextFieldEditing(routing_id()));
}

void AutofillAgent::textFieldDidChange(const WebInputElement& element) {
  if (ignore_text_changes_)
    return;

  if (did_set_node_text_) {
    did_set_node_text_ = false;
    return;
  }

  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AutofillAgent::TextFieldDidChangeImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 element));
}

void AutofillAgent::TextFieldDidChangeImpl(const WebInputElement& element) {
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.focused())
    return;

  if (password_autofill_agent_->TextDidChangeInTextField(element)) {
    element_ = element;
    return;
  }

  ShowSuggestions(element, false, true, false, false);

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForInputElement(element, &form, &field, REQUIRE_NONE)) {
    Send(new AutofillHostMsg_TextFieldDidChange(routing_id(), form, field,
                                                base::TimeTicks::Now()));
  }
}

void AutofillAgent::textFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (password_autofill_agent_->TextFieldHandlingKeyDown(element, event)) {
    element_ = element;
    return;
  }

  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP)
    ShowSuggestions(element, true, true, true, false);
}

void AutofillAgent::openTextDataListChooser(const WebInputElement& element) {
    ShowSuggestions(element, true, false, false, true);
}

void AutofillAgent::AcceptDataListSuggestion(
    const base::string16& suggested_value) {
  base::string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (element_.isMultiple() &&
      element_.formControlType() == WebString::fromUTF8("email")) {
    std::vector<base::string16> parts;

    base::SplitStringDontTrim(element_.editingValue(), ',', &parts);
    if (parts.size() == 0)
      parts.push_back(base::string16());

    base::string16 last_part = parts.back();
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!IsWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts[parts.size() - 1] = last_part;

    new_value = JoinString(parts, ',');
  }
  SetNodeText(new_value, &element_);
}

void AutofillAgent::OnFormDataFilled(int query_id,
                                     const FormData& form) {
  if (!render_view()->GetWebView() || query_id != autofill_query_id_)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();

  switch (autofill_action_) {
    case AUTOFILL_FILL:
      FillForm(form, element_);
      Send(new AutofillHostMsg_DidFillAutofillFormData(routing_id(),
                                                       base::TimeTicks::Now()));
      break;
    case AUTOFILL_PREVIEW:
      PreviewForm(form, element_);
      Send(new AutofillHostMsg_DidPreviewAutofillFormData(routing_id()));
      break;
    default:
      NOTREACHED();
  }
  autofill_action_ = AUTOFILL_NONE;
}

void AutofillAgent::OnFieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  for (size_t i = 0; i < forms.size(); ++i) {
    form_cache_.ShowPredictions(forms[i]);
  }
}

void AutofillAgent::OnSetAutofillActionFill() {
  autofill_action_ = AUTOFILL_FILL;
}

void AutofillAgent::OnClearForm() {
  form_cache_.ClearFormWithElement(element_);
}

void AutofillAgent::OnSetAutofillActionPreview() {
  autofill_action_ = AUTOFILL_PREVIEW;
}

void AutofillAgent::OnClearPreviewedForm() {
  didClearAutofillSelection(element_);
}

void AutofillAgent::OnSetNodeText(const base::string16& value) {
  SetNodeText(value, &element_);
}

void AutofillAgent::OnAcceptDataListSuggestion(const base::string16& value) {
  AcceptDataListSuggestion(value);
}

void AutofillAgent::OnAcceptPasswordAutofillSuggestion(
    const base::string16& username) {
  // We need to make sure this is handled here because the browser process
  // skipped it handling because it believed it would be handled here. If it
  // isn't handled here then the browser logic needs to be updated.
  bool handled = password_autofill_agent_->DidAcceptAutofillSuggestion(
      element_,
      username);
  DCHECK(handled);
}

void AutofillAgent::OnRequestAutocompleteResult(
    WebFormElement::AutocompleteResult result, const FormData& form_data) {
  if (in_flight_request_form_.isNull())
    return;

  if (result == WebFormElement::AutocompleteResultSuccess) {
    FillFormIncludingNonFocusableElements(form_data, in_flight_request_form_);
    if (!in_flight_request_form_.checkValidityWithoutDispatchingEvents())
      result = WebFormElement::AutocompleteResultErrorInvalid;
  }

  in_flight_request_form_.finishRequestAutocomplete(result);
  in_flight_request_form_.reset();
}

void AutofillAgent::OnPageShown() {
}

void AutofillAgent::ShowSuggestions(const WebInputElement& element,
                                    bool autofill_on_empty_values,
                                    bool requires_caret_at_end,
                                    bool display_warning_if_disabled,
                                    bool datalist_only) {
  if (!element.isEnabled() || element.isReadOnly() || !element.isTextField() ||
      element.isPasswordField())
    return;
  if (!datalist_only && !element.suggestedValue().isEmpty())
    return;

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.editingValue();
  if (!datalist_only &&
      (value.length() > kMaximumTextSizeForAutofill ||
       (!autofill_on_empty_values && value.isEmpty()) ||
       (requires_caret_at_end &&
        (element.selectionStart() != element.selectionEnd() ||
         element.selectionEnd() != static_cast<int>(value.length()))))) {
    // Any popup currently showing is obsolete.
    HideAutofillUI();
    return;
  }

  element_ = element;
  if (password_autofill_agent_->ShowSuggestions(element))
    return;

  // If autocomplete is disabled at the field level, ensure that the native
  // UI won't try to show a warning, since that may conflict with a custom
  // popup. Note that we cannot use the WebKit method element.autoComplete()
  // as it does not allow us to distinguish the case where autocomplete is
  // disabled for *both* the element and for the form.
  const base::string16 autocomplete_attribute =
      element.getAttribute("autocomplete");
  if (LowerCaseEqualsASCII(autocomplete_attribute, "off"))
    display_warning_if_disabled = false;

  QueryAutofillSuggestions(element,
                           display_warning_if_disabled,
                           datalist_only);
}

void AutofillAgent::QueryAutofillSuggestions(const WebInputElement& element,
                                             bool display_warning_if_disabled,
                                             bool datalist_only) {
  if (!element.document().frame())
    return;

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  display_warning_if_disabled_ = display_warning_if_disabled;

  // If autocomplete is disabled at the form level, we want to see if there
  // would have been any suggestions were it enabled, so that we can show a
  // warning.  Otherwise, we want to ignore fields that disable autocomplete, so
  // that the suggestions list does not include suggestions for these form
  // fields -- see comment 1 on http://crbug.com/69914
  const RequirementsMask requirements =
      element.autoComplete() ? REQUIRE_AUTOCOMPLETE : REQUIRE_NONE;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForInputElement(element, &form, &field, requirements)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, EXTRACT_VALUE, &field);
  }
  if (datalist_only)
    field.should_autocomplete = false;

  gfx::RectF bounding_box_scaled =
      GetScaledBoundingBox(web_view_->pageScaleFactor(), &element_);

  // Find the datalist values and send them to the browser process.
  std::vector<base::string16> data_list_values;
  std::vector<base::string16> data_list_labels;
  GetDataListSuggestions(element_,
                         datalist_only,
                         &data_list_values,
                         &data_list_labels);
  TrimStringVectorForIPC(&data_list_values);
  TrimStringVectorForIPC(&data_list_labels);

  Send(new AutofillHostMsg_SetDataList(routing_id(),
                                       data_list_values,
                                       data_list_labels));

  Send(new AutofillHostMsg_QueryFormFieldAutofill(routing_id(),
                                                  autofill_query_id_,
                                                  form,
                                                  field,
                                                  bounding_box_scaled,
                                                  display_warning_if_disabled));
}

void AutofillAgent::FillAutofillFormData(const WebNode& node,
                                         int unique_id,
                                         AutofillAction action) {
  DCHECK_GT(unique_id, 0);

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForInputElement(node.toConst<WebInputElement>(), &form,
                                       &field, REQUIRE_AUTOCOMPLETE)) {
    return;
  }

  autofill_action_ = action;
  Send(new AutofillHostMsg_FillAutofillFormData(
      routing_id(), autofill_query_id_, form, field, unique_id));
}

void AutofillAgent::SetNodeText(const base::string16& value,
                                blink::WebInputElement* node) {
  did_set_node_text_ = true;
  base::string16 substring = value;
  substring = substring.substr(0, node->maxLength());

  node->setEditingValue(substring);
}

void AutofillAgent::HideAutofillUI() {
  Send(new AutofillHostMsg_HideAutofillUI(routing_id()));
}

// TODO(isherman): Decide if we want to support non-password autofill with AJAX.
void AutofillAgent::didAssociateFormControls(
    const blink::WebVector<blink::WebNode>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    blink::WebFrame* frame = nodes[i].document().frame();
    // Only monitors dynamic forms created in the top frame. Dynamic forms
    // inserted in iframes are not captured yet.
    if (!frame->parent()) {
      password_autofill_agent_->OnDynamicFormsSeen(frame);
      return;
    }
  }
}

}  // namespace autofill
