// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/tooltip_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::TooltipClient*)
DECLARE_WINDOW_PROPERTY_TYPE(base::string16*)

namespace aura {
namespace client {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(
    TooltipClient*, kRootWindowTooltipClientKey, NULL);
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(base::string16*, kTooltipTextKey, NULL);

void SetTooltipClient(Window* root_window, TooltipClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowTooltipClientKey, client);
}

TooltipClient* GetTooltipClient(Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowTooltipClientKey) : NULL;
}

void SetTooltipText(Window* window, base::string16* tooltip_text) {
  window->SetProperty(kTooltipTextKey, tooltip_text);
}

const base::string16 GetTooltipText(Window* window) {
  base::string16* string_ptr = window->GetProperty(kTooltipTextKey);
  return string_ptr ? *string_ptr : base::string16();
}

}  // namespace client
}  // namespace aura
