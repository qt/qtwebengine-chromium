// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_controller_proxy_stub.h"

#include "ash/shell.h"
#include "ui/views/corewm/input_method_event_filter.h"

using namespace content;

namespace ash {

KeyboardControllerProxyStub::KeyboardControllerProxyStub() {
}

KeyboardControllerProxyStub::~KeyboardControllerProxyStub() {
}

BrowserContext* KeyboardControllerProxyStub::GetBrowserContext() {
  return Shell::GetInstance()->browser_context();
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
