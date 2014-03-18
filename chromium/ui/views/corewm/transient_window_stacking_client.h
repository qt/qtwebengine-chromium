// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TRANSIENT_WINDOW_STACKING_CLIENT_H_
#define UI_VIEWS_COREWM_TRANSIENT_WINDOW_STACKING_CLIENT_H_

#include "ui/aura/client/window_stacking_client.h"
#include "ui/views/views_export.h"

namespace views {
namespace corewm {

class VIEWS_EXPORT TransientWindowStackingClient
    : public aura::client::WindowStackingClient {
 public:
  TransientWindowStackingClient();
  virtual ~TransientWindowStackingClient();

  // WindowStackingClient:
  virtual void AdjustStacking(aura::Window** child,
                              aura::Window** target,
                              aura::Window::StackDirection* direction) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientWindowStackingClient);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TRANSIENT_WINDOW_STACKING_CLIENT_H_
