// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/webgraphicscontext3d_provider_impl.h"

#include "cc/output/context_provider.h"

namespace webkit {
namespace gpu {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<cc::ContextProvider> provider)
    : provider_(provider) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {}

blink::WebGraphicsContext3D* WebGraphicsContext3DProviderImpl::context3d() {
  return provider_->Context3d();
}

GrContext* WebGraphicsContext3DProviderImpl::grContext() {
  return provider_->GrContext();
}

}  // namespace gpu
}  // namespace webkit
