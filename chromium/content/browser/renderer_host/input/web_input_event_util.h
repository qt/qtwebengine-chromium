// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_

#include "content/common/content_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace WebKit {
class WebKeyboardEvent;
}

namespace content {

// Update |event|'s windowsKeyCode and keyIdentifer properties using the
// provided |windows_key_code|.
CONTENT_EXPORT void UpdateWindowsKeyCodeAndKeyIdentifier(
    WebKit::WebKeyboardEvent* event,
    ui::KeyboardCode windows_key_code);

}

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
