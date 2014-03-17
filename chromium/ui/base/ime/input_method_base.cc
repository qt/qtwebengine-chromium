// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodBase::InputMethodBase()
  : delegate_(NULL),
    text_input_client_(NULL),
    system_toplevel_window_focused_(false) {
}

InputMethodBase::~InputMethodBase() {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnInputMethodDestroyed(this));
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::Init(bool focused) {
  if (focused)
    OnFocus();
}

void InputMethodBase::OnFocus() {
  DCHECK(!system_toplevel_window_focused_);
  system_toplevel_window_focused_ = true;
}

void InputMethodBase::OnBlur() {
  DCHECK(system_toplevel_window_focused_);
  system_toplevel_window_focused_ = false;
}

void InputMethodBase::SetFocusedTextInputClient(TextInputClient* client) {
  SetFocusedTextInputClientInternal(client);
}

void InputMethodBase::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ != client)
    return;
  SetFocusedTextInputClientInternal(NULL);
}

TextInputClient* InputMethodBase::GetTextInputClient() const {
  return system_toplevel_window_focused_ ? text_input_client_ : NULL;
}

void InputMethodBase::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputStateChanged(client);
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : TEXT_INPUT_TYPE_NONE;
}

TextInputMode InputMethodBase::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : TEXT_INPUT_MODE_DEFAULT;
}

bool InputMethodBase::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

void InputMethodBase::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InputMethodBase::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool InputMethodBase::IsTextInputClientFocused(const TextInputClient* client) {
  return client && (client == GetTextInputClient());
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  TextInputClient* client = GetTextInputClient();
  if (!IsTextInputTypeNone())
    client->OnInputMethodChanged();
}

bool InputMethodBase::DispatchKeyEventPostIME(
    const ui::KeyEvent& event) const {
  if (!delegate_)
    return false;

  if (!event.HasNativeEvent())
    return delegate_->DispatchFabricatedKeyEventPostIME(
        event.type(), event.key_code(), event.flags());

  return delegate_->DispatchKeyEventPostIME(event.native_event());
}

void InputMethodBase::NotifyTextInputStateChanged(
    const TextInputClient* client) {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnTextInputStateChanged(client));
}

void InputMethodBase::SetFocusedTextInputClientInternal(
    TextInputClient* client) {
  TextInputClient* old = text_input_client_;
  if (old == client)
    return;
  OnWillChangeFocusedClient(old, client);
  text_input_client_ = client;  // NULL allowed.
  OnDidChangeFocusedClient(old, client);
  NotifyTextInputStateChanged(text_input_client_);
}

void InputMethodBase::OnCandidateWindowShown() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&InputMethodBase::CandidateWindowShownCallback, AsWeakPtr()));
}

void InputMethodBase::OnCandidateWindowUpdated() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&InputMethodBase::CandidateWindowUpdatedCallback,
                 AsWeakPtr()));
}

void InputMethodBase::OnCandidateWindowHidden() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&InputMethodBase::CandidateWindowHiddenCallback, AsWeakPtr()));
}

void InputMethodBase::CandidateWindowShownCallback() {
  if (text_input_client_)
    text_input_client_->OnCandidateWindowShown();
}

void InputMethodBase::CandidateWindowUpdatedCallback() {
  if (text_input_client_)
    text_input_client_->OnCandidateWindowUpdated();
}

void InputMethodBase::CandidateWindowHiddenCallback() {
  if (text_input_client_)
    text_input_client_->OnCandidateWindowHidden();
}

}  // namespace ui
