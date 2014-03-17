// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_BITMAP_H_
#define CC_RESOURCES_UI_RESOURCE_BITMAP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace cc {

class ETC1PixelRef;

// A bitmap class that contains a ref-counted reference to a SkPixelRef that
// holds the content of the bitmap (cannot use SkBitmap because of ETC1).
// Thread-safety (by ways of SkPixelRef) ensures that both main and impl threads
// can hold references to the bitmap and that asynchronous uploads are allowed.
class CC_EXPORT UIResourceBitmap {
 public:
  enum UIResourceFormat {
    RGBA8,
    ETC1
  };
  enum UIResourceWrapMode {
    CLAMP_TO_EDGE,
    REPEAT
  };

  gfx::Size GetSize() const { return size_; }
  UIResourceFormat GetFormat() const { return format_; }
  UIResourceWrapMode GetWrapMode() const { return wrap_mode_; }
  void SetWrapMode(UIResourceWrapMode wrap_mode) { wrap_mode_ = wrap_mode; }
  bool GetOpaque() const { return opaque_; }
  void SetOpaque(bool opaque) { opaque_ = opaque; }

  // User must ensure that |skbitmap| is immutable.  The SkBitmap Format should
  // be 32-bit RGBA.
  explicit UIResourceBitmap(const SkBitmap& skbitmap);

  UIResourceBitmap(const skia::RefPtr<ETC1PixelRef>& etc1_pixel_ref,
                   gfx::Size size);

  ~UIResourceBitmap();

 private:
  friend class AutoLockUIResourceBitmap;
  void Create(const skia::RefPtr<SkPixelRef>& pixel_ref,
              UIResourceFormat format,
              gfx::Size size);

  skia::RefPtr<SkPixelRef> pixel_ref_;
  UIResourceFormat format_;
  UIResourceWrapMode wrap_mode_;
  gfx::Size size_;
  bool opaque_;
};

class CC_EXPORT AutoLockUIResourceBitmap {
 public:
  explicit AutoLockUIResourceBitmap(const UIResourceBitmap& bitmap);
  ~AutoLockUIResourceBitmap();
  const uint8_t* GetPixels() const;

 private:
  const UIResourceBitmap& bitmap_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_BITMAP_H_
