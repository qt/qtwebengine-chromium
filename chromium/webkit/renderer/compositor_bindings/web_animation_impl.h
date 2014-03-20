// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebAnimation.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc { class Animation; }

namespace blink { class WebAnimationCurve; }

namespace webkit {

class WebAnimationImpl : public blink::WebAnimation {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebAnimationImpl(
      const blink::WebAnimationCurve& curve,
      TargetProperty target,
      int animation_id,
      int group_id);
  virtual ~WebAnimationImpl();

  // blink::WebAnimation implementation
  virtual int id();
  virtual TargetProperty targetProperty() const;
  virtual int iterations() const;
  virtual void setIterations(int iterations);
  virtual double startTime() const;
  virtual void setStartTime(double monotonic_time);
  virtual double timeOffset() const;
  virtual void setTimeOffset(double monotonic_time);
  virtual bool alternatesDirection() const;
  virtual void setAlternatesDirection(bool alternates);

  scoped_ptr<cc::Animation> PassAnimation();

 private:
  scoped_ptr<cc::Animation> animation_;

  DISALLOW_COPY_AND_ASSIGN(WebAnimationImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_ANIMATION_IMPL_H_
