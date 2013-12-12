// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_

#include <deque>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>
#include <QuartzCore/QuartzCore.h>

#include "base/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/video_frame.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"

class IOSurfaceSupport;
class SkBitmap;

namespace gfx {
class Rect;
}

namespace content {

class CompositingIOSurfaceContext;
class CompositingIOSurfaceShaderPrograms;
class CompositingIOSurfaceTransformer;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostViewMac;

// This class manages an OpenGL context and IOSurface for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurface.
class CompositingIOSurfaceMac {
 public:
  // Returns NULL if IOSurface support is missing or GL APIs fail. Specify in
  // |order| the desired ordering relationship of the surface to the containing
  // window.
  static CompositingIOSurfaceMac* Create(
      const scoped_refptr<CompositingIOSurfaceContext>& context);
  ~CompositingIOSurfaceMac();

  // Set IOSurface that will be drawn on the next NSView drawRect.
  bool SetIOSurface(uint64 io_surface_handle,
                    const gfx::Size& size,
                    float scale_factor,
                    const ui::LatencyInfo& latency_info);

  // Get the CGL renderer ID currently associated with this context.
  int GetRendererID();

  // Blit the IOSurface at the upper-left corner of the of the specified
  // window_size. If the window size is larger than the IOSurface, the
  // remaining right and bottom edges will be white. |scaleFactor| is 1
  // in normal views, 2 in HiDPI views.  |frame_subscriber| listens to
  // this draw event and provides output buffer for copying this frame into.
  bool DrawIOSurface(const gfx::Size& window_size,
                     float window_scale_factor,
                     RenderWidgetHostViewFrameSubscriber* frame_subscriber,
                     bool using_core_animation);

  // Copy the data of the "live" OpenGL texture referring to this IOSurfaceRef
  // into |out|. The copied region is specified with |src_pixel_subrect| and
  // the data is transformed so that it fits in |dst_pixel_size|.
  // |src_pixel_subrect| and |dst_pixel_size| are not in DIP but in pixel.
  // Caller must ensure that |out| is allocated to dimensions that match
  // dst_pixel_size, with no additional padding.
  // |callback| is invoked when the operation is completed or failed.
  // Do no call this method again before |callback| is invoked.
  void CopyTo(const gfx::Rect& src_pixel_subrect,
              const gfx::Size& dst_pixel_size,
              const base::Callback<void(bool, const SkBitmap&)>& callback);

  // Transfer the contents of the surface to an already-allocated YV12
  // VideoFrame, and invoke a callback to indicate success or failure.
  void CopyToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Unref the IOSurface and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void UnrefIOSurface();

  bool HasIOSurface() { return !!io_surface_.get(); }

  const gfx::Size& pixel_io_surface_size() const {
    return pixel_io_surface_size_;
  }
  // In cocoa view units / DIPs.
  const gfx::Size& dip_io_surface_size() const { return dip_io_surface_size_; }
  float scale_factor() const { return scale_factor_; }

  bool is_vsync_disabled() const;

  void SetContext(
      const scoped_refptr<CompositingIOSurfaceContext>& new_context);

  const scoped_refptr<CompositingIOSurfaceContext>& context() {
    return context_;
  }

  // Get vsync scheduling parameters.
  // |interval_numerator/interval_denominator| equates to fractional number of
  // seconds between vsyncs.
  void GetVSyncParameters(base::TimeTicks* timebase,
                          uint32* interval_numerator,
                          uint32* interval_denominator);

  // Returns true if asynchronous readback is supported on this system.
  bool IsAsynchronousReadbackSupported();

 private:
  friend CVReturn DisplayLinkCallback(CVDisplayLinkRef,
                                      const CVTimeStamp*,
                                      const CVTimeStamp*,
                                      CVOptionFlags,
                                      CVOptionFlags*,
                                      void*);

  // Vertex structure for use in glDraw calls.
  struct SurfaceVertex {
    SurfaceVertex() : x_(0.0f), y_(0.0f), tx_(0.0f), ty_(0.0f) { }
    void set(float x, float y, float tx, float ty) {
      x_ = x;
      y_ = y;
      tx_ = tx;
      ty_ = ty;
    }
    void set_position(float x, float y) {
      x_ = x;
      y_ = y;
    }
    void set_texcoord(float tx, float ty) {
      tx_ = tx;
      ty_ = ty;
    }
    float x_;
    float y_;
    float tx_;
    float ty_;
  };

  // Counter-clockwise verts starting from upper-left corner (0, 0).
  struct SurfaceQuad {
    void set_size(gfx::Size vertex_size, gfx::Size texcoord_size) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      float vw = static_cast<float>(vertex_size.width());
      float vh = static_cast<float>(vertex_size.height());
      float tw = static_cast<float>(texcoord_size.width());
      float th = static_cast<float>(texcoord_size.height());
      verts_[0].set(0.0f, 0.0f, 0.0f, th);
      verts_[1].set(0.0f, vh, 0.0f, 0.0f);
      verts_[2].set(vw, vh, tw, 0.0f);
      verts_[3].set(vw, 0.0f, tw, th);
    }
    void set_rect(float x1, float y1, float x2, float y2) {
      verts_[0].set_position(x1, y1);
      verts_[1].set_position(x1, y2);
      verts_[2].set_position(x2, y2);
      verts_[3].set_position(x2, y1);
    }
    void set_texcoord_rect(float tx1, float ty1, float tx2, float ty2) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      verts_[0].set_texcoord(tx1, ty2);
      verts_[1].set_texcoord(tx1, ty1);
      verts_[2].set_texcoord(tx2, ty1);
      verts_[3].set_texcoord(tx2, ty2);
    }
    SurfaceVertex verts_[4];
  };

  // Keeps track of states and buffers for readback of IOSurface.
  //
  // TODO(miu): Major code refactoring is badly needed!  To be done in a
  // soon-upcoming change.  For now, we blatantly violate the style guide with
  // respect to struct vs. class usage:
  struct CopyContext {
    explicit CopyContext(const scoped_refptr<CompositingIOSurfaceContext>& ctx);
    ~CopyContext();

    // Delete any references to owned OpenGL objects.  This must be called
    // within the OpenGL context just before destruction.
    void ReleaseCachedGLObjects();

    // The following two methods assume |num_outputs| has been set, and are
    // being called within the OpenGL context.
    void PrepareReadbackFramebuffers();
    void PrepareForAsynchronousReadback();

    const scoped_ptr<CompositingIOSurfaceTransformer> transformer;
    GLenum output_readback_format;
    int num_outputs;
    GLuint output_textures[3];  // Not owned.
    // Note: For YUV, the |output_texture_sizes| widths are in terms of 4-byte
    // quads, not pixels.
    gfx::Size output_texture_sizes[3];
    GLuint frame_buffers[3];
    GLuint pixel_buffers[3];
    GLuint fence;  // When non-zero, doing an asynchronous copy.
    int cycles_elapsed;
    base::Callback<bool(const void*, int)> map_buffer_callback;
    base::Callback<void(bool)> done_callback;
  };

  CompositingIOSurfaceMac(
      IOSurfaceSupport* io_surface_support,
      const scoped_refptr<CompositingIOSurfaceContext>& context);

  void SetupCVDisplayLink();

  // If this IOSurface has moved to a different window, use that window's
  // GL context (if multiple visible windows are using the same GL context
  // then call to setView call can stall and prevent reaching 60fps).
  void SwitchToContextOnNewWindow(NSView* view,
                                  int window_number);

  bool IsVendorIntel();

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTexture(uint64 io_surface_handle);

  void UnrefIOSurfaceWithContextCurrent();

  void DrawQuad(const SurfaceQuad& quad);

  // Called on display-link thread.
  void DisplayLinkTick(CVDisplayLinkRef display_link,
                       const CVTimeStamp* time);

  void CalculateVsyncParametersLockHeld(const CVTimeStamp* time);

  // Prevent from spinning on CGLFlushDrawable when it fails to throttle to
  // VSync frequency.
  void RateLimitDraws();

  void StartOrContinueDisplayLink();
  void StopDisplayLink();

  // Copy current frame to |target| video frame. This method must be called
  // within a CGL context. Returns a callback that should be called outside
  // of the CGL context.
  // If |called_within_draw| is true this method is called within a drawing
  // operations. This allow certain optimizations.
  base::Closure CopyToVideoFrameWithinContext(
      const gfx::Rect& src_subrect,
      bool called_within_draw,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Common GPU-readback copy path.  Only one of |bitmap_output| or
  // |video_frame_output| may be specified: Either ARGB is written to
  // |bitmap_output| or letter-boxed YV12 is written to |video_frame_output|.
  base::Closure CopyToSelectedOutputWithinContext(
      const gfx::Rect& src_pixel_subrect,
      const gfx::Rect& dst_pixel_rect,
      bool called_within_draw,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output,
      const base::Callback<void(bool)>& done_callback);

  // TODO(hclam): These two methods should be static.
  void AsynchronousReadbackForCopy(
      const gfx::Rect& dst_pixel_rect,
      bool called_within_draw,
      CopyContext* copy_context,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output);
  bool SynchronousReadbackForCopy(
      const gfx::Rect& dst_pixel_rect,
      CopyContext* copy_context,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output);

  // Scan the list of started asynchronous copies and test if each one has
  // completed. If |block_until_finished| is true, then block until all
  // pending copies are finished.
  void CheckIfAllCopiesAreFinished(bool block_until_finished);
  void CheckIfAllCopiesAreFinishedWithinContext(
      bool block_until_finished,
      std::vector<base::Closure>* done_callbacks);

  void FailAllCopies();
  void DestroyAllCopyContextsWithinContext();

  // Check for GL errors and store the result in error_. Only return new
  // errors
  GLenum GetAndSaveGLError();

  gfx::Rect IntersectWithIOSurface(const gfx::Rect& rect) const;

  // Cached pointer to IOSurfaceSupport Singleton.
  IOSurfaceSupport* io_surface_support_;

  // GL context, and parameters for context sharing. This may change when
  // moving between windows, but will never be NULL.
  scoped_refptr<CompositingIOSurfaceContext> context_;

  // IOSurface data.
  uint64 io_surface_handle_;
  base::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The width and height of the io surface.
  gfx::Size pixel_io_surface_size_;  // In pixels.
  gfx::Size dip_io_surface_size_;  // In view / density independent pixels.
  float scale_factor_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // A pool of CopyContexts with OpenGL objects ready for re-use.  Prefer to
  // pull one from the pool before creating a new CopyContext.
  std::vector<CopyContext*> copy_context_pool_;

  // CopyContexts being used for in-flight copy operations.
  std::deque<CopyContext*> copy_requests_;

  // Timer for finishing a copy operation.
  base::Timer finish_copy_timer_;

  // CVDisplayLink for querying Vsync timing info and throttling swaps.
  CVDisplayLinkRef display_link_;

  // Timer for stopping display link after a timeout with no swaps.
  base::DelayTimer<CompositingIOSurfaceMac> display_link_stop_timer_;

  // Lock for sharing data between UI thread and display-link thread.
  base::Lock lock_;

  // Vsync timing data.
  base::TimeTicks vsync_timebase_;
  uint32 vsync_interval_numerator_;
  uint32 vsync_interval_denominator_;

  bool initialized_is_intel_;
  bool is_intel_;
  GLint screen_;

  // Error saved by GetAndSaveGLError
  GLint gl_error_;

  ui::LatencyInfo latency_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
