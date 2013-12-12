// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TRANSFORM_OPERATION_H_
#define CC_ANIMATION_TRANSFORM_OPERATION_H_

#include "ui/gfx/transform.h"

namespace gfx {
class BoxF;
}

namespace cc {

struct TransformOperation {
  enum Type {
    TransformOperationTranslate,
    TransformOperationRotate,
    TransformOperationScale,
    TransformOperationSkew,
    TransformOperationPerspective,
    TransformOperationMatrix,
    TransformOperationIdentity
  };

  TransformOperation()
      : type(TransformOperationIdentity) {
  }

  Type type;
  gfx::Transform matrix;

  union {
    SkMScalar perspective_depth;

    struct {
      SkMScalar x, y;
    } skew;

    struct {
      SkMScalar x, y, z;
    } scale;

    struct {
      SkMScalar x, y, z;
    } translate;

    struct {
      struct {
        SkMScalar x, y, z;
      } axis;

      SkMScalar angle;
    } rotate;
  };

  bool IsIdentity() const;
  static bool BlendTransformOperations(const TransformOperation* from,
                                       const TransformOperation* to,
                                       SkMScalar progress,
                                       gfx::Transform* result);

  static bool BlendedBoundsForBox(const gfx::BoxF& box,
                                  const TransformOperation* from,
                                  const TransformOperation* to,
                                  SkMScalar min_progress,
                                  SkMScalar max_progress,
                                  gfx::BoxF* bounds);
};

}  // namespace cc

#endif  // CC_ANIMATION_TRANSFORM_OPERATION_H_
