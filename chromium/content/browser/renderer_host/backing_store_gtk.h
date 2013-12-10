// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_GTK_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_GTK_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/common/content_export.h"
#include "ui/gfx/x/x11_types.h"

namespace gfx {
class Point;
class Rect;
}

typedef struct _GdkDrawable GdkDrawable;

namespace content {

class CONTENT_EXPORT BackingStoreGtk : public BackingStore {
 public:
  // Create a backing store on the X server. The visual is an Xlib Visual
  // describing the format of the target window and the depth is the color
  // depth of the X window which will be drawn into.
  BackingStoreGtk(RenderWidgetHost* widget,
                  const gfx::Size& size,
                  void* visual,
                  int depth);

  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  BackingStoreGtk(RenderWidgetHost* widget, const gfx::Size& size);

  virtual ~BackingStoreGtk();

  XDisplay* display() const { return display_; }
  XID root_window() const { return root_window_; }

  // Copy from the server-side backing store to the target window
  //   origin: the destination rectangle origin
  //   damage: the area to copy
  //   target: the X id of the target window
  void XShowRect(const gfx::Point &origin, const gfx::Rect& damage,
                 XID target);

#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  void PaintToRect(const gfx::Rect& dest_rect, GdkDrawable* target);
#endif

  // BackingStore implementation.
  virtual size_t MemorySize() OVERRIDE;
  virtual void PaintToBackingStore(
      RenderProcessHost* process,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      float scale_factor,
      const base::Closure& completion_callback,
      bool* scheduled_completion_callback) OVERRIDE;
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformBitmap* output) OVERRIDE;
  virtual void ScrollBackingStore(const gfx::Vector2d& delta,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size) OVERRIDE;

 private:
  // Paints the bitmap from the renderer onto the backing store without
  // using Xrender to composite the pixmaps.
  void PaintRectWithoutXrender(TransportDIB* bitmap,
                               const gfx::Rect& bitmap_rect,
                               const std::vector<gfx::Rect>& copy_rects);

  // This is the connection to the X server where this backing store will be
  // displayed.
  XDisplay* const display_;
  // What flavor, if any, MIT-SHM (X shared memory) support we have.
  const ui::SharedMemorySupport shared_memory_support_;
  // If this is true, then we can use Xrender to composite our pixmaps.
  const bool use_render_;
  // If |use_render_| is false, this is the number of bits-per-pixel for |depth|
  int pixmap_bpp_;
  // if |use_render_| is false, we need the Visual to get the RGB masks.
  void* const visual_;
  // This is the depth of the target window.
  const int visual_depth_;
  // The parent window (probably a GtkDrawingArea) for this backing store.
  const XID root_window_;
  // This is a handle to the server side pixmap which is our backing store.
  XID pixmap_;
  // This is the RENDER picture pointing at |pixmap_|.
  XID picture_;
  // This is a default graphic context, used in XCopyArea
  void* pixmap_gc_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreGtk);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_GTK_H_
