// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_background_controller.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"

namespace ash {
namespace internal {

SystemBackgroundController::SystemBackgroundController(
    aura::Window* root_window,
    SkColor color)
    : root_window_(root_window),
      layer_(new ui::Layer(ui::LAYER_SOLID_COLOR)) {
  root_window_->AddObserver(this);
  layer_->SetColor(color);

  ui::Layer* root_layer = root_window_->layer();
  layer_->SetBounds(gfx::Rect(root_layer->bounds().size()));
  root_layer->Add(layer_.get());
  root_layer->StackAtBottom(layer_.get());
}

SystemBackgroundController::~SystemBackgroundController() {
  root_window_->RemoveObserver(this);
}

void SystemBackgroundController::SetColor(SkColor color) {
  layer_->SetColor(color);
}

void SystemBackgroundController::OnWindowBoundsChanged(
    aura::Window* root,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  DCHECK_EQ(root_window_, root);
  layer_->SetBounds(gfx::Rect(root_window_->layer()->bounds().size()));
}

}  // namespace internal
}  // namespace ash
