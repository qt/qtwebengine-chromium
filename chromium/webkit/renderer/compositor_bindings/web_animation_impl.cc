// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_animation_impl.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_id_provider.h"
#include "third_party/WebKit/public/platform/WebAnimation.h"
#include "third_party/WebKit/public/platform/WebAnimationCurve.h"
#include "webkit/renderer/compositor_bindings/web_filter_animation_curve_impl.h"
#include "webkit/renderer/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/renderer/compositor_bindings/web_transform_animation_curve_impl.h"

using cc::Animation;
using cc::AnimationIdProvider;

using blink::WebAnimation;
using blink::WebAnimationCurve;

namespace webkit {

WebAnimationImpl::WebAnimationImpl(const WebAnimationCurve& web_curve,
                                   TargetProperty target_property,
                                   int animation_id,
                                   int group_id) {
  if (!animation_id)
    animation_id = AnimationIdProvider::NextAnimationId();
  if (!group_id)
    group_id = AnimationIdProvider::NextGroupId();

  WebAnimationCurve::AnimationCurveType curve_type = web_curve.type();
  scoped_ptr<cc::AnimationCurve> curve;
  switch (curve_type) {
    case WebAnimationCurve::AnimationCurveTypeFloat: {
      const WebFloatAnimationCurveImpl* float_curve_impl =
          static_cast<const WebFloatAnimationCurveImpl*>(&web_curve);
      curve = float_curve_impl->CloneToAnimationCurve();
      break;
    }
    case WebAnimationCurve::AnimationCurveTypeTransform: {
      const WebTransformAnimationCurveImpl* transform_curve_impl =
          static_cast<const WebTransformAnimationCurveImpl*>(&web_curve);
      curve = transform_curve_impl->CloneToAnimationCurve();
      break;
    }
    case WebAnimationCurve::AnimationCurveTypeFilter: {
      const WebFilterAnimationCurveImpl* filter_curve_impl =
          static_cast<const WebFilterAnimationCurveImpl*>(&web_curve);
      curve = filter_curve_impl->CloneToAnimationCurve();
      break;
    }
  }
  animation_ = Animation::Create(
      curve.Pass(),
      animation_id,
      group_id,
      static_cast<cc::Animation::TargetProperty>(target_property));
}

WebAnimationImpl::~WebAnimationImpl() {}

int WebAnimationImpl::id() { return animation_->id(); }

blink::WebAnimation::TargetProperty WebAnimationImpl::targetProperty() const {
  return static_cast<WebAnimationImpl::TargetProperty>(
      animation_->target_property());
}

int WebAnimationImpl::iterations() const { return animation_->iterations(); }

void WebAnimationImpl::setIterations(int n) { animation_->set_iterations(n); }

double WebAnimationImpl::startTime() const { return animation_->start_time(); }

void WebAnimationImpl::setStartTime(double monotonic_time) {
  animation_->set_start_time(monotonic_time);
}

double WebAnimationImpl::timeOffset() const {
  return animation_->time_offset();
}

void WebAnimationImpl::setTimeOffset(double monotonic_time) {
  animation_->set_time_offset(monotonic_time);
}

bool WebAnimationImpl::alternatesDirection() const {
  return animation_->alternates_direction();
}

void WebAnimationImpl::setAlternatesDirection(bool alternates) {
  animation_->set_alternates_direction(alternates);
}

scoped_ptr<cc::Animation> WebAnimationImpl::PassAnimation() {
  animation_->set_needs_synchronized_start_time(true);
  return animation_.Pass();
}

#define COMPILE_ASSERT_MATCHING_ENUMS(webkit_name, cc_name)                    \
    COMPILE_ASSERT(static_cast<int>(webkit_name) == static_cast<int>(cc_name), \
                   mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    WebAnimation::TargetPropertyTransform, Animation::Transform);
COMPILE_ASSERT_MATCHING_ENUMS(
    WebAnimation::TargetPropertyOpacity, Animation::Opacity);
COMPILE_ASSERT_MATCHING_ENUMS(
    WebAnimation::TargetPropertyFilter, Animation::Filter);

}  // namespace webkit
