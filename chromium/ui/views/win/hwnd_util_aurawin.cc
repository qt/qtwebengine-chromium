// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace views {

HWND HWNDForView(const View* view) {
  return view->GetWidget() ? HWNDForWidget(view->GetWidget()) : NULL;
}

HWND HWNDForWidget(const Widget* widget) {
  return HWNDForNativeWindow(widget->GetNativeWindow());
}

HWND HWNDForNativeView(const gfx::NativeView view) {
  return view && view->GetRootWindow() ?
      view->GetDispatcher()->host()->GetAcceleratedWidget() : NULL;
}

HWND HWNDForNativeWindow(const gfx::NativeWindow window) {
  return window && window->GetRootWindow() ?
      window->GetDispatcher()->host()->GetAcceleratedWidget() : NULL;
}

gfx::Rect GetWindowBoundsForClientBounds(View* view,
                                         const gfx::Rect& client_bounds) {
  DCHECK(view);
  aura::WindowEventDispatcher* dispatcher =
      view->GetWidget()->GetNativeWindow()->GetDispatcher();
  if (dispatcher) {
    HWND hwnd = dispatcher->host()->GetAcceleratedWidget();
    RECT rect = client_bounds.ToRECT();
    DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
    DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return gfx::Rect(rect);
  }
  return client_bounds;
}

}  // namespace views
