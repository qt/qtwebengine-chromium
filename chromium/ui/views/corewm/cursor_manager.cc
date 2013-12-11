// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/cursor_manager.h"

#include "base/logging.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/views/corewm/native_cursor_manager.h"
#include "ui/views/corewm/native_cursor_manager_delegate.h"

namespace views {
namespace corewm {

namespace internal {

// Represents the cursor state which is composed of cursor type, visibility, and
// mouse events enable state. When mouse events are disabled, the cursor is
// always invisible.
class CursorState {
 public:
  CursorState()
      : cursor_(ui::kCursorNone),
        visible_(true),
        scale_(1.f),
        cursor_set_(ui::CURSOR_SET_NORMAL),
        mouse_events_enabled_(true),
        visible_on_mouse_events_enabled_(true) {
  }

  gfx::NativeCursor cursor() const { return cursor_; }
  void set_cursor(gfx::NativeCursor cursor) { cursor_ = cursor; }

  bool visible() const { return visible_; }
  void SetVisible(bool visible) {
    if (mouse_events_enabled_)
      visible_ = visible;
    // Ignores the call when mouse events disabled.
  }

  float scale() const { return scale_; }
  void set_scale(float scale) {
    scale_ = scale;
  }

  ui::CursorSetType cursor_set() const { return cursor_set_; }
  void set_cursor_set(ui::CursorSetType cursor_set) {
    cursor_set_ = cursor_set;
  }

  bool mouse_events_enabled() const { return mouse_events_enabled_; }
  void SetMouseEventsEnabled(bool enabled) {
    if (mouse_events_enabled_ == enabled)
      return;
    mouse_events_enabled_ = enabled;

    // Restores the visibility when mouse events are enabled.
    if (enabled) {
      visible_ = visible_on_mouse_events_enabled_;
    } else {
      visible_on_mouse_events_enabled_ = visible_;
      visible_ = false;
    }
  }

 private:
  gfx::NativeCursor cursor_;
  bool visible_;
  float scale_;
  ui::CursorSetType cursor_set_;
  bool mouse_events_enabled_;

  // The visibility to set when mouse events are enabled.
  bool visible_on_mouse_events_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CursorState);
};

}  // namespace internal

CursorManager::CursorManager(scoped_ptr<NativeCursorManager> delegate)
    : delegate_(delegate.Pass()),
      cursor_lock_count_(0),
      current_state_(new internal::CursorState),
      state_on_unlock_(new internal::CursorState) {
}

CursorManager::~CursorManager() {
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  state_on_unlock_->set_cursor(cursor);
  if (cursor_lock_count_ == 0 &&
      GetCurrentCursor() != state_on_unlock_->cursor()) {
    delegate_->SetCursor(state_on_unlock_->cursor(), this);
  }
}

void CursorManager::ShowCursor() {
  state_on_unlock_->SetVisible(true);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    FOR_EACH_OBSERVER(aura::client::CursorClientObserver, observers_,
                      OnCursorVisibilityChanged(true));
  }
}

void CursorManager::HideCursor() {
  state_on_unlock_->SetVisible(false);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    FOR_EACH_OBSERVER(aura::client::CursorClientObserver, observers_,
                      OnCursorVisibilityChanged(false));
  }
}

bool CursorManager::IsCursorVisible() const {
  return current_state_->visible();
}

void CursorManager::SetScale(float scale) {
  state_on_unlock_->set_scale(scale);
  if (GetCurrentScale() != state_on_unlock_->scale())
    delegate_->SetScale(state_on_unlock_->scale(), this);
}

void CursorManager::SetCursorSet(ui::CursorSetType cursor_set) {
  state_on_unlock_->set_cursor_set(cursor_set);
  if (GetCurrentCursorSet() != state_on_unlock_->cursor_set())
    delegate_->SetCursorSet(state_on_unlock_->cursor_set(), this);
}

float CursorManager::GetCurrentScale() const {
  return current_state_->scale();
}

ui::CursorSetType CursorManager::GetCurrentCursorSet() const {
  return current_state_->cursor_set();
}

void CursorManager::EnableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(true);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
}

void CursorManager::DisableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(false);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
}

bool CursorManager::IsMouseEventsEnabled() const {
  return current_state_->mouse_events_enabled();
}

void CursorManager::SetDisplay(const gfx::Display& display) {
  delegate_->SetDisplay(display, this);
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ > 0)
    return;

  if (GetCurrentCursor() != state_on_unlock_->cursor()) {
    delegate_->SetCursor(state_on_unlock_->cursor(), this);
  }
  if (IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
  if (IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(),
                             this);
  }
}

bool CursorManager::IsCursorLocked() const {
  return cursor_lock_count_ > 0;
}

void CursorManager::AddObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.AddObserver(observer);
}

void CursorManager::RemoveObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::NativeCursor CursorManager::GetCurrentCursor() const {
  return current_state_->cursor();
}

bool CursorManager::GetCurrentVisibility() const {
  return current_state_->visible();
}

bool CursorManager::GetMouseEventsEnabled() const {
  return current_state_->mouse_events_enabled();
}

void CursorManager::CommitCursor(gfx::NativeCursor cursor) {
  current_state_->set_cursor(cursor);
}

void CursorManager::CommitVisibility(bool visible) {
  FOR_EACH_OBSERVER(aura::client::CursorClientObserver, observers_,
                    OnCursorVisibilityChanged(visible));
  current_state_->SetVisible(visible);
}

void CursorManager::CommitScale(float scale) {
  current_state_->set_scale(scale);
}

void CursorManager::CommitCursorSet(ui::CursorSetType cursor_set) {
  current_state_->set_cursor_set(cursor_set);
}

void CursorManager::CommitMouseEventsEnabled(bool enabled) {
  current_state_->SetMouseEventsEnabled(enabled);
}

}  // namespace corewm
}  // namespace views
