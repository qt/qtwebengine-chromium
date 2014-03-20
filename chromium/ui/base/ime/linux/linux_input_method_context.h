// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_

#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

namespace gfx {
class Rect;
}

namespace ui {

struct CompositionText;
class KeyEvent;

// An interface of input method context for input method frameworks on
// GNU/Linux and likes.
class UI_EXPORT LinuxInputMethodContext {
 public:
  virtual ~LinuxInputMethodContext() {}

  // Dispatches the key event to an underlying IME.  Returns true if the key
  // event is handled, otherwise false.  A client must set the text input type
  // before dispatching a key event.
  virtual bool DispatchKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Resets the context.  A client needs to call OnTextInputTypeChanged() again
  // before calling DispatchKeyEvent().
  virtual void Reset() = 0;

  // Returns the text direction of the current keyboard layout or input method.
  virtual base::i18n::TextDirection GetInputTextDirection() const = 0;

  // Notifies the context that the text input type has changed.
  virtual void OnTextInputTypeChanged(TextInputType text_input_type) = 0;

  // Notifies the context that the caret bounds have changed.  |caret_bounds| is
  // relative to screen coordinates.
  virtual void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) = 0;
};

// An interface of callback functions called from LinuxInputMethodContext.
class UI_EXPORT LinuxInputMethodContextDelegate {
 public:
  virtual ~LinuxInputMethodContextDelegate() {}

  // Commits the |text| to the text input client.
  virtual void OnCommit(const base::string16& text) = 0;

  // Sets the composition text to the text input client.
  virtual void OnPreeditChanged(const CompositionText& composition_text) = 0;

  // Cleans up a composition session and makes sure that the composition text is
  // cleared.
  virtual void OnPreeditEnd() = 0;

  // Prepares things for a new composition session.
  virtual void OnPreeditStart() = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
