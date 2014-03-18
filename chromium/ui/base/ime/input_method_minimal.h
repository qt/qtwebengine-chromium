// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_MINIMAL_H_
#define UI_BASE_IME_INPUT_METHOD_MINIMAL_H_

#include "ui/base/ime/input_method_base.h"

namespace ui {

// A minimal implementation of ui::InputMethod, which supports only the direct
// input without any compositions or conversions.
class UI_EXPORT InputMethodMinimal : public InputMethodBase {
 public:
  explicit InputMethodMinimal(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodMinimal();

  // Overriden from InputMethod.
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodMinimal);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_MINIMAL_H_
