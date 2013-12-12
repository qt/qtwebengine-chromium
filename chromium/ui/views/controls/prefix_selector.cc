// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/prefix_selector.h"

#include "base/i18n/case_conversion.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

const int64 kTimeBeforeClearingMS = 1000;

void ConvertRectToScreen(const views::View* src, gfx::Rect* r) {
  DCHECK(src);

  gfx::Point new_origin = r->origin();
  views::View::ConvertPointToScreen(src, &new_origin);
  r->set_origin(new_origin);
}

}  // namespace

PrefixSelector::PrefixSelector(PrefixDelegate* delegate)
    : prefix_delegate_(delegate) {
}

PrefixSelector::~PrefixSelector() {
}

void PrefixSelector::OnViewBlur() {
  ClearText();
}

void PrefixSelector::SetCompositionText(
    const ui::CompositionText& composition) {
}

void PrefixSelector::ConfirmCompositionText() {
}

void PrefixSelector::ClearCompositionText() {
}

void PrefixSelector::InsertText(const string16& text) {
  OnTextInput(text);
}

void PrefixSelector::InsertChar(char16 ch, int flags) {
  OnTextInput(string16(1, ch));
}

gfx::NativeWindow PrefixSelector::GetAttachedWindow() const {
  return prefix_delegate_->GetWidget()->GetNativeWindow();
}

ui::TextInputType PrefixSelector::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_TEXT;
}

ui::TextInputMode PrefixSelector::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

bool PrefixSelector::CanComposeInline() const {
  return false;
}

gfx::Rect PrefixSelector::GetCaretBounds() {
  gfx::Rect rect(prefix_delegate_->GetVisibleBounds().origin(), gfx::Size());
  // TextInputClient::GetCaretBounds is expected to return a value in screen
  // coordinates.
  ConvertRectToScreen(prefix_delegate_, &rect);
  return rect;
}

bool PrefixSelector::GetCompositionCharacterBounds(uint32 index,
                                                   gfx::Rect* rect) {
  // TextInputClient::GetCompositionCharacterBounds is expected to fill |rect|
  // in screen coordinates and GetCaretBounds returns screen coordinates.
  *rect = GetCaretBounds();
  return false;
}

bool PrefixSelector::HasCompositionText() {
  return false;
}

bool PrefixSelector::GetTextRange(gfx::Range* range) {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::GetCompositionTextRange(gfx::Range* range) {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::GetSelectionRange(gfx::Range* range) {
  *range = gfx::Range();
  return false;
}

bool PrefixSelector::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool PrefixSelector::DeleteRange(const gfx::Range& range) {
  return false;
}

bool PrefixSelector::GetTextFromRange(const gfx::Range& range,
                                        string16* text) {
  return false;
}

void PrefixSelector::OnInputMethodChanged() {
  ClearText();
}

bool PrefixSelector::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return true;
}

void PrefixSelector::ExtendSelectionAndDelete(size_t before, size_t after) {
}

void PrefixSelector::EnsureCaretInRect(const gfx::Rect& rect) {
}

void PrefixSelector::OnTextInput(const string16& text) {
  // Small hack to filter out 'tab' and 'enter' input, as the expectation is
  // that they are control characters and will not affect the currently-active
  // prefix.
  if (text.length() == 1 &&
      (text[0] == L'\t' || text[0] == L'\r' || text[0] == L'\n'))
    return;

  const int row_count = prefix_delegate_->GetRowCount();
  if (row_count == 0)
    return;

  // Search for |text| if it has been a while since the user typed, otherwise
  // append |text| to |current_text_| and search for that. If it has been a
  // while search after the current row, otherwise search starting from the
  // current row.
  int row = std::max(0, prefix_delegate_->GetSelectedRow());
  const base::TimeTicks now(base::TimeTicks::Now());
  if ((now - time_of_last_key_).InMilliseconds() < kTimeBeforeClearingMS) {
    current_text_ += text;
  } else {
    current_text_ = text;
    if (prefix_delegate_->GetSelectedRow() >= 0)
      row = (row + 1) % row_count;
  }
  time_of_last_key_ = now;

  const int start_row = row;
  const string16 lower_text(base::i18n::ToLower(current_text_));
  do {
    if (TextAtRowMatchesText(row, current_text_)) {
      prefix_delegate_->SetSelectedRow(row);
      return;
    }
    row = (row + 1) % row_count;
  } while (row != start_row);
}

bool PrefixSelector::TextAtRowMatchesText(int row,
                                          const string16& lower_text) {
  const string16 model_text(
      base::i18n::ToLower(prefix_delegate_->GetTextForRow(row)));
  return (model_text.size() >= lower_text.size()) &&
      (model_text.compare(0, lower_text.size(), lower_text) == 0);
}

void PrefixSelector::ClearText() {
  current_text_.clear();
  time_of_last_key_ = base::TimeTicks();
}

}  // namespace views
