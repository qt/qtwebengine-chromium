// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_delegate.h"

namespace content {

bool RenderWidgetHostDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool RenderWidgetHostDelegate::PreHandleWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  return false;
}

#if defined(OS_WIN) && defined(USE_AURA)
gfx::NativeViewAccessible
RenderWidgetHostDelegate::GetParentNativeViewAccessible() {
  return NULL;
}
#endif

}  // namespace content
