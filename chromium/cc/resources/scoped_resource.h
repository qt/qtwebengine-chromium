// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SCOPED_RESOURCE_H_
#define CC_RESOURCES_SCOPED_RESOURCE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource.h"

#ifndef NDEBUG
#include "base/threading/platform_thread.h"
#endif

namespace cc {

class CC_EXPORT ScopedResource : public Resource {
 public:
  static scoped_ptr<ScopedResource> Create(
      ResourceProvider* resource_provider) {
    return make_scoped_ptr(new ScopedResource(resource_provider));
  }
  virtual ~ScopedResource();

  void Allocate(gfx::Size size,
                ResourceProvider::TextureUsageHint hint,
                ResourceFormat format);
  void AllocateManaged(gfx::Size size, GLenum target, ResourceFormat format);
  void Free();
  void Leak();

 protected:
  explicit ScopedResource(ResourceProvider* provider);

 private:
  ResourceProvider* resource_provider_;

#ifndef NDEBUG
  base::PlatformThreadId allocate_thread_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedResource);
};

}  // namespace cc

#endif  // CC_RESOURCES_SCOPED_RESOURCE_H_
