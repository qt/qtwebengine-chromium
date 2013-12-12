// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/page_scale_animation.h"

#include <math.h>

#include "base/logging.h"
#include "cc/animation/timing_function.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector2d_conversions.h"

namespace {

// This takes a viewport-relative vector and returns a vector whose values are
// between 0 and 1, representing the percentage position within the viewport.
gfx::Vector2dF NormalizeFromViewport(gfx::Vector2dF denormalized,
                                     gfx::SizeF viewport_size) {
  return gfx::ScaleVector2d(denormalized,
                            1.f / viewport_size.width(),
                            1.f / viewport_size.height());
}

gfx::Vector2dF DenormalizeToViewport(gfx::Vector2dF normalized,
                                     gfx::SizeF viewport_size) {
  return gfx::ScaleVector2d(normalized,
                            viewport_size.width(),
                            viewport_size.height());
}

gfx::Vector2dF InterpolateBetween(gfx::Vector2dF start,
                                  gfx::Vector2dF end,
                                  float interp) {
  return start + gfx::ScaleVector2d(end - start, interp);
}

}  // namespace

namespace cc {

scoped_ptr<PageScaleAnimation> PageScaleAnimation::Create(
    gfx::Vector2dF start_scroll_offset,
    float start_page_scale_factor,
    gfx::SizeF viewport_size,
    gfx::SizeF root_layer_size,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(new PageScaleAnimation(start_scroll_offset,
                                                start_page_scale_factor,
                                                viewport_size,
                                                root_layer_size,
                                                timing_function.Pass()));
}

PageScaleAnimation::PageScaleAnimation(
    gfx::Vector2dF start_scroll_offset,
    float start_page_scale_factor,
    gfx::SizeF viewport_size,
    gfx::SizeF root_layer_size,
    scoped_ptr<TimingFunction> timing_function)
    : start_page_scale_factor_(start_page_scale_factor),
      target_page_scale_factor_(0.f),
      start_scroll_offset_(start_scroll_offset),
      start_anchor_(),
      target_anchor_(),
      viewport_size_(viewport_size),
      root_layer_size_(root_layer_size),
      start_time_(-1.0),
      duration_(0.0),
      timing_function_(timing_function.Pass()) {}

PageScaleAnimation::~PageScaleAnimation() {}

void PageScaleAnimation::ZoomTo(gfx::Vector2dF target_scroll_offset,
                                float target_page_scale_factor,
                                double duration) {
  target_page_scale_factor_ = target_page_scale_factor;
  target_scroll_offset_ = target_scroll_offset;
  ClampTargetScrollOffset();
  duration_ = duration;

  if (start_page_scale_factor_ == target_page_scale_factor) {
    start_anchor_ = start_scroll_offset_;
    target_anchor_ = target_scroll_offset;
    return;
  }

  // For uniform-looking zooming, infer an anchor from the start and target
  // viewport rects.
  InferTargetAnchorFromScrollOffsets();
  start_anchor_ = target_anchor_;
}

void PageScaleAnimation::ZoomWithAnchor(gfx::Vector2dF anchor,
                                        float target_page_scale_factor,
                                        double duration) {
  start_anchor_ = anchor;
  target_page_scale_factor_ = target_page_scale_factor;
  duration_ = duration;

  // We start zooming out from the anchor tapped by the user. But if
  // the target scale is impossible to attain without hitting the root layer
  // edges, then infer an anchor that doesn't collide with the edges.
  // We will interpolate between the two anchors during the animation.
  InferTargetScrollOffsetFromStartAnchor();
  ClampTargetScrollOffset();

  if (start_page_scale_factor_ == target_page_scale_factor_) {
    target_anchor_ = start_anchor_;
    return;
  }
  InferTargetAnchorFromScrollOffsets();
}

void PageScaleAnimation::InferTargetScrollOffsetFromStartAnchor() {
  gfx::Vector2dF normalized = NormalizeFromViewport(
      start_anchor_ - start_scroll_offset_, StartViewportSize());
  target_scroll_offset_ =
      start_anchor_ - DenormalizeToViewport(normalized, TargetViewportSize());
}

void PageScaleAnimation::InferTargetAnchorFromScrollOffsets() {
  // The anchor is the point which is at the same normalized relative position
  // within both start viewport rect and target viewport rect. For example, a
  // zoom-in double-tap to a perfectly centered rect will have normalized
  // anchor (0.5, 0.5), while one to a rect touching the bottom-right of the
  // screen will have normalized anchor (1.0, 1.0). In other words, it obeys
  // the equations:
  // anchor = start_size * normalized + start_offset
  // anchor = target_size * normalized + target_offset
  // where both anchor and normalized begin as unknowns. Solving
  // for the normalized, we get the following:
  float width_scale =
      1.f / (TargetViewportSize().width() - StartViewportSize().width());
  float height_scale =
      1.f / (TargetViewportSize().height() - StartViewportSize().height());
  gfx::Vector2dF normalized = gfx::ScaleVector2d(
      start_scroll_offset_ - target_scroll_offset_, width_scale, height_scale);
  target_anchor_ =
      target_scroll_offset_ + DenormalizeToViewport(normalized,
                                                    TargetViewportSize());
}

void PageScaleAnimation::ClampTargetScrollOffset() {
  gfx::Vector2dF max_scroll_offset =
      gfx::RectF(root_layer_size_).bottom_right() -
      gfx::RectF(TargetViewportSize()).bottom_right();
  target_scroll_offset_.SetToMax(gfx::Vector2dF());
  target_scroll_offset_.SetToMin(max_scroll_offset);
}

gfx::SizeF PageScaleAnimation::StartViewportSize() const {
  return gfx::ScaleSize(viewport_size_, 1.f / start_page_scale_factor_);
}

gfx::SizeF PageScaleAnimation::TargetViewportSize() const {
  return gfx::ScaleSize(viewport_size_, 1.f / target_page_scale_factor_);
}

gfx::SizeF PageScaleAnimation::ViewportSizeAt(float interp) const {
  return gfx::ScaleSize(viewport_size_, 1.f / PageScaleFactorAt(interp));
}

bool PageScaleAnimation::IsAnimationStarted() const {
  return start_time_ >= 0;
}

void PageScaleAnimation::StartAnimation(double time) {
  DCHECK_GT(0, start_time_);
  start_time_ = time;
}

gfx::Vector2dF PageScaleAnimation::ScrollOffsetAtTime(double time) const {
  DCHECK_GE(start_time_, 0);
  return ScrollOffsetAt(InterpAtTime(time));
}

float PageScaleAnimation::PageScaleFactorAtTime(double time) const {
  DCHECK_GE(start_time_, 0);
  return PageScaleFactorAt(InterpAtTime(time));
}

bool PageScaleAnimation::IsAnimationCompleteAtTime(double time) const {
  DCHECK_GE(start_time_, 0);
  return time >= end_time();
}

float PageScaleAnimation::InterpAtTime(double time) const {
  DCHECK_GE(start_time_, 0);
  DCHECK_GE(time, start_time_);
  if (IsAnimationCompleteAtTime(time))
    return 1.f;

  const double normalized_time = (time - start_time_) / duration_;
  return timing_function_->GetValue(normalized_time);
}

gfx::Vector2dF PageScaleAnimation::ScrollOffsetAt(float interp) const {
  if (interp <= 0.f)
    return start_scroll_offset_;
  if (interp >= 1.f)
    return target_scroll_offset_;

  return AnchorAt(interp) - ViewportRelativeAnchorAt(interp);
}

gfx::Vector2dF PageScaleAnimation::AnchorAt(float interp) const {
  // Interpolate from start to target anchor in absolute space.
  return InterpolateBetween(start_anchor_, target_anchor_, interp);
}

gfx::Vector2dF PageScaleAnimation::ViewportRelativeAnchorAt(
    float interp) const {
  // Interpolate from start to target anchor in normalized space.
  gfx::Vector2dF start_normalized =
      NormalizeFromViewport(start_anchor_ - start_scroll_offset_,
                            StartViewportSize());
  gfx::Vector2dF target_normalized =
      NormalizeFromViewport(target_anchor_ - target_scroll_offset_,
                            TargetViewportSize());
  gfx::Vector2dF interp_normalized =
      InterpolateBetween(start_normalized, target_normalized, interp);

  return DenormalizeToViewport(interp_normalized, ViewportSizeAt(interp));
}

float PageScaleAnimation::PageScaleFactorAt(float interp) const {
  if (interp <= 0.f)
    return start_page_scale_factor_;
  if (interp >= 1.f)
    return target_page_scale_factor_;

  // Linearly interpolate the magnitude in log scale.
  float diff = target_page_scale_factor_ / start_page_scale_factor_;
  float log_diff = log(diff);
  log_diff *= interp;
  diff = exp(log_diff);
  return start_page_scale_factor_ * diff;
}

}  // namespace cc
