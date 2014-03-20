// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/capture_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::CaptureClient*)

namespace aura {
namespace client {

DEFINE_WINDOW_PROPERTY_KEY(
    CaptureClient*, kRootWindowCaptureClientKey, NULL);

void SetCaptureClient(Window* root_window, CaptureClient* client) {
  root_window->SetProperty(kRootWindowCaptureClientKey, client);
}

CaptureClient* GetCaptureClient(Window* root_window) {
  return root_window ?
      root_window->GetProperty(kRootWindowCaptureClientKey) : NULL;
}

Window* GetCaptureWindow(Window* window) {
  Window* root_window = window->GetRootWindow();
  if (!root_window)
    return NULL;
  CaptureClient* capture_client = GetCaptureClient(root_window);
  return capture_client ? capture_client->GetCaptureWindow() : NULL;
}

}  // namespace client
}  // namespace aura
