// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
#define UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/views_export.h"

namespace views {

class PrefixDelegate;

// PrefixSelector is used to change the selection in a view as the user
// types characters.
class VIEWS_EXPORT PrefixSelector : public ui::TextInputClient {
 public:
  explicit PrefixSelector(PrefixDelegate* delegate);
  virtual ~PrefixSelector();

  // Invoked from the view when it loses focus.
  void OnViewBlur();

  // ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual ui::TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(gfx::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(gfx::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(gfx::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const gfx::Range& range) OVERRIDE;
  virtual bool DeleteRange(const gfx::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                string16* text) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) OVERRIDE;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE;

 private:
  // Invoked when text is typed. Tries to change the selection appropriately.
  void OnTextInput(const string16& text);

  // Returns true if the text of the node at |row| starts with |lower_text|.
  bool TextAtRowMatchesText(int row, const string16& lower_text);

  // Clears |current_text_| and resets |time_of_last_key_|.
  void ClearText();

  PrefixDelegate* prefix_delegate_;

  // Time OnTextInput() was last invoked.
  base::TimeTicks time_of_last_key_;

  string16 current_text_;

  DISALLOW_COPY_AND_ASSIGN(PrefixSelector);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
