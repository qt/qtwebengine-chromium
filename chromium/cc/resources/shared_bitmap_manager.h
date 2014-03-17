// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SHARED_BITMAP_MANAGER_H_
#define CC_RESOURCES_SHARED_BITMAP_MANAGER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/resources/shared_bitmap.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT SharedBitmapManager {
 public:
  SharedBitmapManager() {}

  virtual scoped_ptr<SharedBitmap> AllocateSharedBitmap(gfx::Size) = 0;
  virtual scoped_ptr<SharedBitmap> GetSharedBitmapFromId(
      gfx::Size,
      const SharedBitmapId&) = 0;
  virtual scoped_ptr<SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory*) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedBitmapManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_SHARED_BITMAP_MANAGER_H_
