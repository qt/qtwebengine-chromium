// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/gfx/rect.h"

namespace ui {

DummyTextInputClient::DummyTextInputClient() {
}

DummyTextInputClient::~DummyTextInputClient() {
}

void DummyTextInputClient::SetCompositionText(
    const ui::CompositionText& composition) {
}

void DummyTextInputClient::ConfirmCompositionText() {
}

void DummyTextInputClient::ClearCompositionText() {
}

void DummyTextInputClient::InsertText(const string16& text) {
}

void DummyTextInputClient::InsertChar(char16 ch, int flags) {
}

gfx::NativeWindow DummyTextInputClient::GetAttachedWindow() const {
  return NULL;
}

ui::TextInputType DummyTextInputClient::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

ui::TextInputMode DummyTextInputClient::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

bool DummyTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect DummyTextInputClient::GetCaretBounds() {
  return gfx::Rect();
}

bool DummyTextInputClient::GetCompositionCharacterBounds(uint32 index,
                                                         gfx::Rect* rect) {
  return false;
}

bool DummyTextInputClient::HasCompositionText() {
  return false;
}

bool DummyTextInputClient::GetTextRange(gfx::Range* range) {
  return false;
}

bool DummyTextInputClient::GetCompositionTextRange(gfx::Range* range) {
  return false;
}

bool DummyTextInputClient::GetSelectionRange(gfx::Range* range) {
  return false;
}

bool DummyTextInputClient::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool DummyTextInputClient::DeleteRange(const gfx::Range& range) {
  return false;
}

bool DummyTextInputClient::GetTextFromRange(const gfx::Range& range,
                                            string16* text) {
  return false;
}

void DummyTextInputClient::OnInputMethodChanged() {
}

bool DummyTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

void DummyTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                    size_t after) {
}

void DummyTextInputClient::EnsureCaretInRect(const gfx::Rect& rect)  {
}

}  // namespace ui
