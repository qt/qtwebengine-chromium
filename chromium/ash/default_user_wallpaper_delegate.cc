// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/default_user_wallpaper_delegate.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"

namespace ash {

int DefaultUserWallpaperDelegate::GetAnimationType() {
  return views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

bool DefaultUserWallpaperDelegate::ShouldShowInitialAnimation() {
  return false;
}

void DefaultUserWallpaperDelegate::UpdateWallpaper() {
}

void DefaultUserWallpaperDelegate::InitializeWallpaper() {
  ash::Shell::GetInstance()->desktop_background_controller()->
      CreateEmptyWallpaper();
}

void DefaultUserWallpaperDelegate::OpenSetWallpaperPage() {
}

bool DefaultUserWallpaperDelegate::CanOpenSetWallpaperPage() {
  return false;
}

void DefaultUserWallpaperDelegate::OnWallpaperAnimationFinished() {
}

void DefaultUserWallpaperDelegate::OnWallpaperBootAnimationFinished() {
}

}  // namespace ash
