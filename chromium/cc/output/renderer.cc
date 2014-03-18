// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer.h"

namespace cc {

bool Renderer::HasAllocatedResourcesForTesting(RenderPass::Id id) const {
  return false;
}

bool Renderer::IsContextLost() {
  return false;
}

}  // namespace cc
