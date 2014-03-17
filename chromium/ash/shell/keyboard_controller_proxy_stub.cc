// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/keyboard_controller_proxy_stub.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/input_method_event_filter.h"

using namespace content;

namespace ash {

KeyboardControllerProxyStub::KeyboardControllerProxyStub() {
}

KeyboardControllerProxyStub::~KeyboardControllerProxyStub() {
}

aura::Window* KeyboardControllerProxyStub::GetKeyboardWindow() {
  if (!keyboard_) {
    keyboard_.reset(new aura::Window(&delegate_));
    keyboard_->Init(ui::LAYER_NOT_DRAWN);
  }
  return keyboard_.get();
}

BrowserContext* KeyboardControllerProxyStub::GetBrowserContext() {
  // TODO(oshima): investigate which profile to use.
  return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
}

ui::InputMethod* KeyboardControllerProxyStub::GetInputMethod() {
  return Shell::GetInstance()->input_method_filter()->input_method();
}

void KeyboardControllerProxyStub::RequestAudioInput(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
}

}  // namespace ash
