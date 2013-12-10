// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FAKE_INPUT_METHOD_H_
#define UI_BASE_IME_FAKE_INPUT_METHOD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ui_export.h"

namespace ui {

class InputMethodObserver;
class KeyEvent;
class TextInputClient;

// A fake ui::InputMethod implementation for minimum input support.
class UI_EXPORT FakeInputMethod : NON_EXPORTED_BASE(public InputMethod) {
 public:
  explicit FakeInputMethod(internal::InputMethodDelegate* delegate);
  virtual ~FakeInputMethod();

  // Overriden from InputMethod.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(bool focused) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE;
  virtual void SetStickyFocusedTextInputClient(
      TextInputClient* client) OVERRIDE;
  virtual void DetachTextInputClient(TextInputClient* client) OVERRIDE;
  virtual TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual bool DispatchKeyEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual bool DispatchFabricatedKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual TextInputType GetTextInputType() const OVERRIDE;
  virtual TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;
  virtual void AddObserver(InputMethodObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputMethodObserver* observer) OVERRIDE;

 private:
  internal::InputMethodDelegate* delegate_;
  TextInputClient* text_input_client_;
  bool is_sticky_text_input_client_;
  ObserverList<InputMethodObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_FAKE_INPUT_METHOD_H_
