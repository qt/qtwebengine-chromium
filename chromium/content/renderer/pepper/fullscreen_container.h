// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_FULLSCREEN_CONTAINER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_FULLSCREEN_CONTAINER_IMPL_H_

namespace blink {
class WebLayer;
struct WebCursorInfo;
struct WebRect;
}  // namespace blink

namespace content {

// This class is like a lightweight WebPluginContainer for fullscreen PPAPI
// plugins, that only handles painting.
class FullscreenContainer {
 public:
  // Invalidates the full plugin region.
  virtual void Invalidate() = 0;

  // Invalidates a partial region of the plugin.
  virtual void InvalidateRect(const blink::WebRect&) = 0;

  // Scrolls a partial region of the plugin in the given direction.
  virtual void ScrollRect(int dx, int dy, const blink::WebRect&) = 0;

  // Destroys the fullscreen window. This also destroys the FullscreenContainer
  // instance.
  virtual void Destroy() = 0;

  // Notifies the container that the mouse cursor has changed.
  virtual void DidChangeCursor(const blink::WebCursorInfo& cursor) = 0;

  virtual void SetLayer(blink::WebLayer* layer) = 0;

 protected:
  virtual ~FullscreenContainer() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_FULLSCREEN_CONTAINER_IMPL_H_
