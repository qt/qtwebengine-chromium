// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_error_tracker.h"

#include "ui/base/x/x11_util.h"

namespace {

unsigned char g_x11_error_code = 0;

int X11ErrorHandler(XDisplay* display, XErrorEvent* error) {
  g_x11_error_code = error->error_code;
  return 0;
}

}

namespace ui {

X11ErrorTracker::X11ErrorTracker() {
  old_handler_ = XSetErrorHandler(X11ErrorHandler);
}

X11ErrorTracker::~X11ErrorTracker() {
  XSetErrorHandler(old_handler_);
}

bool X11ErrorTracker::FoundNewError() {
  XSync(gfx::GetXDisplay(), False);
  unsigned char error = g_x11_error_code;
  g_x11_error_code = 0;
  return error != 0;
}

}  // namespace ui
