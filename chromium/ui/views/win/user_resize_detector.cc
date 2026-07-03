// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/user_resize_detector.h"

#include <windows.h>

#include "ui/views/win/hwnd_message_handler_delegate.h"

namespace views {

static bool g_in_move_resize_loop = false;

UserResizeDetector::UserResizeDetector(
    HWNDMessageHandlerDelegate* hwnd_delegate)
    : hwnd_delegate_(hwnd_delegate) {}

void UserResizeDetector::OnEnterSizeMove() {
  if (state_ == State::kNotResizing) {
    state_ = State::kInSizeMove;
  }
}

void UserResizeDetector::OnSizing() {
  if (state_ == State::kInSizeMove) {
    g_in_move_resize_loop = true;
    state_ = State::kInSizing;
    hwnd_delegate_->HandleBeginUserResize();
  }
}

void UserResizeDetector::OnExitSizeMove() {
  if (state_ == State::kInSizing) {
    hwnd_delegate_->HandleEndUserResize();
  }
  g_in_move_resize_loop = false;
  state_ = State::kNotResizing;
}

// static
bool UserResizeDetector::InMoveResizeLoop() {
  return g_in_move_resize_loop;
}

}  // namespace views
