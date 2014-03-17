// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/fake_input_method_context.h"

namespace ui {

FakeInputMethodContext::FakeInputMethodContext() {}

// Overriden from ui::LinuxInputMethodContext

bool FakeInputMethodContext::DispatchKeyEvent(
    const ui::KeyEvent& /* key_event */) {
  return false;
}

void FakeInputMethodContext::Reset() {
}

base::i18n::TextDirection FakeInputMethodContext::GetInputTextDirection()
    const {
  return base::i18n::UNKNOWN_DIRECTION;
}

void FakeInputMethodContext::OnTextInputTypeChanged(
    ui::TextInputType /* text_input_type */) {
}

void FakeInputMethodContext::OnCaretBoundsChanged(
    const gfx::Rect& /* caret_bounds */) {
}

}  // namespace ui
