// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_CURVE_H_
#define CC_ANIMATION_ANIMATION_CURVE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/transform.h"

namespace gfx {
class BoxF;
}

namespace cc {

class FilterAnimationCurve;
class FloatAnimationCurve;
class TransformAnimationCurve;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
class CC_EXPORT AnimationCurve {
 public:
  enum CurveType { Float, Transform, Filter };

  virtual ~AnimationCurve() {}

  virtual double Duration() const = 0;
  virtual CurveType Type() const = 0;
  virtual scoped_ptr<AnimationCurve> Clone() const = 0;

  const FloatAnimationCurve* ToFloatAnimationCurve() const;
  const TransformAnimationCurve* ToTransformAnimationCurve() const;
  const FilterAnimationCurve* ToFilterAnimationCurve() const;
};

class CC_EXPORT FloatAnimationCurve : public AnimationCurve {
 public:
  virtual ~FloatAnimationCurve() {}

  virtual float GetValue(double t) const = 0;

  // Partial Animation implementation.
  virtual CurveType Type() const OVERRIDE;
};

class CC_EXPORT TransformAnimationCurve : public AnimationCurve {
 public:
  virtual ~TransformAnimationCurve() {}

  virtual gfx::Transform GetValue(double t) const = 0;

  // Sets |bounds| to be the bounding box for the region within which |box|
  // will move during this animation. If this region cannot be computed,
  // returns false.
  virtual bool AnimatedBoundsForBox(const gfx::BoxF& box,
                                    gfx::BoxF* bounds) const = 0;

  // Partial Animation implementation.
  virtual CurveType Type() const OVERRIDE;
};

class CC_EXPORT FilterAnimationCurve : public AnimationCurve {
 public:
  virtual ~FilterAnimationCurve() {}

  virtual FilterOperations GetValue(double t) const = 0;

  // Partial Animation implementation.
  virtual CurveType Type() const OVERRIDE;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_CURVE_H_
