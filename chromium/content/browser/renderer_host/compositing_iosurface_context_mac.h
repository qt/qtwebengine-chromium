// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_

#import <AppKit/NSOpenGL.h>
#include <OpenGL/OpenGL.h>
#include <map>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace content {

class CompositingIOSurfaceShaderPrograms;

class CompositingIOSurfaceContext
    : public base::RefCounted<CompositingIOSurfaceContext> {
 public:
  enum { kOffscreenContextWindowNumber = -2 };

  // Get or create a GL context for the specified window with the specified
  // surface ordering. Share these GL contexts as much as possible because
  // creating and destroying them can be expensive
  // http://crbug.com/180463
  static scoped_refptr<CompositingIOSurfaceContext> Get(int window_number);

  // Mark that all the currently existing GL contexts shouldn't be returned
  // anymore by Get, but rather, new contexts should be created. This is
  // called as a precaution when unexpected GL errors occur.
  static void MarkExistingContextsAsNotShareable();

  CompositingIOSurfaceShaderPrograms* shader_program_cache() const {
    return shader_program_cache_.get();
  }
  NSOpenGLContext* nsgl_context() const { return nsgl_context_; }
  CGLContextObj cgl_context() const { return cgl_context_; }
  bool is_vsync_disabled() const { return is_vsync_disabled_; }
  int window_number() const { return window_number_; }

  bool IsVendorIntel();

 private:
  friend class base::RefCounted<CompositingIOSurfaceContext>;

  CompositingIOSurfaceContext(
      int window_number,
      NSOpenGLContext* nsgl_context,
      CGLContextObj clg_context,
      bool is_vsync_disabled_,
      scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache);
  ~CompositingIOSurfaceContext();

  int window_number_;
  base::scoped_nsobject<NSOpenGLContext> nsgl_context_;
  CGLContextObj cgl_context_; // weak, backed by |nsgl_context_|
  bool is_vsync_disabled_;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache_;
  bool can_be_shared_;

  bool initialized_is_intel_;
  bool is_intel_;
  GLint screen_;

  // The global map from window number and window ordering to
  // context data.
  typedef std::map<int, CompositingIOSurfaceContext*> WindowMap;
  static base::LazyInstance<WindowMap> window_map_;
  static WindowMap* window_map();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
