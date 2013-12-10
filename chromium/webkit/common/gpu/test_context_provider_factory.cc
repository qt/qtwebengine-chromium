// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/test_context_provider_factory.h"

#include "base/logging.h"
#include "cc/output/context_provider.h"
#include "webkit/common/gpu/context_provider_in_process.h"

namespace webkit {
namespace gpu {

static TestContextProviderFactory* context_provider_instance = NULL;

// static
TestContextProviderFactory* TestContextProviderFactory::GetInstance() {
  if (!context_provider_instance)
    context_provider_instance = new TestContextProviderFactory();
  return context_provider_instance;
}

TestContextProviderFactory::TestContextProviderFactory() {}

TestContextProviderFactory::~TestContextProviderFactory() {}

scoped_refptr<cc::ContextProvider> TestContextProviderFactory::
    OffscreenContextProviderForMainThread() {
  if (!main_thread_.get() || main_thread_->DestroyedOnMainThread()) {
    main_thread_ = ContextProviderInProcess::CreateOffscreen();
    if (main_thread_.get() && !main_thread_->BindToCurrentThread())
      main_thread_ = NULL;
  }
  return main_thread_;
}

}  // namespace gpu
}  // namespace webkit
