// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_MATH_UTIL_H_
#define CC_BASE_MATH_UTIL_H_

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace base { class Value; }

namespace gfx {
class QuadF;
class Rect;
class RectF;
class Transform;
class Vector2dF;
}

namespace cc {

struct HomogeneousCoordinate {
  HomogeneousCoordinate(SkMScalar x, SkMScalar y, SkMScalar z, SkMScalar w) {
    vec[0] = x;
    vec[1] = y;
    vec[2] = z;
    vec[3] = w;
  }

  bool ShouldBeClipped() const { return w() <= 0.0; }

  gfx::PointF CartesianPoint2d() const {
    if (w() == 1.0)
      return gfx::PointF(x(), y());

    // For now, because this code is used privately only by MathUtil, it should
    // never be called when w == 0, and we do not yet need to handle that case.
    DCHECK(w());
    SkMScalar inv_w = 1.0 / w();
    return gfx::PointF(x() * inv_w, y() * inv_w);
  }

  gfx::Point3F CartesianPoint3d() const {
    if (w() == 1)
      return gfx::Point3F(x(), y(), z());

    // For now, because this code is used privately only by MathUtil, it should
    // never be called when w == 0, and we do not yet need to handle that case.
    DCHECK(w());
    SkMScalar inv_w = 1.0 / w();
    return gfx::Point3F(x() * inv_w, y() * inv_w, z() * inv_w);
  }

  SkMScalar x() const { return vec[0]; }
  SkMScalar y() const { return vec[1]; }
  SkMScalar z() const { return vec[2]; }
  SkMScalar w() const { return vec[3]; }

  SkMScalar vec[4];
};

class CC_EXPORT MathUtil {
 public:
  static const double kPiDouble;
  static const float kPiFloat;

  static double Deg2Rad(double deg) { return deg * kPiDouble / 180.0; }
  static double Rad2Deg(double rad) { return rad * 180.0 / kPiDouble; }

  static float Deg2Rad(float deg) { return deg * kPiFloat / 180.0f; }
  static float Rad2Deg(float rad) { return rad * 180.0f / kPiFloat; }

  static float Round(float f) {
    return (f > 0.f) ? std::floor(f + 0.5f) : std::ceil(f - 0.5f);
  }
  static double Round(double d) {
    return (d > 0.0) ? std::floor(d + 0.5) : std::ceil(d - 0.5);
  }

  template <typename T> static T ClampToRange(T value, T min, T max) {
    return std::min(std::max(value, min), max);
  }

  // Background: Existing transform code does not do the right thing in
  // MapRect / MapQuad / ProjectQuad when there is a perspective projection that
  // causes one of the transformed vertices to go to w < 0. In those cases, it
  // is necessary to perform clipping in homogeneous coordinates, after applying
  // the transform, before dividing-by-w to convert to cartesian coordinates.
  //
  // These functions return the axis-aligned rect that encloses the correctly
  // clipped, transformed polygon.
  static gfx::Rect MapClippedRect(const gfx::Transform& transform,
                                  gfx::Rect rect);
  static gfx::RectF MapClippedRect(const gfx::Transform& transform,
                                   const gfx::RectF& rect);
  static gfx::RectF ProjectClippedRect(const gfx::Transform& transform,
                                       const gfx::RectF& rect);

  // Returns an array of vertices that represent the clipped polygon. After
  // returning, indexes from 0 to num_vertices_in_clipped_quad are valid in the
  // clipped_quad array. Note that num_vertices_in_clipped_quad may be zero,
  // which means the entire quad was clipped, and none of the vertices in the
  // array are valid.
  static void MapClippedQuad(const gfx::Transform& transform,
                             const gfx::QuadF& src_quad,
                             gfx::PointF clipped_quad[8],
                             int* num_vertices_in_clipped_quad);

  static gfx::RectF ComputeEnclosingRectOfVertices(gfx::PointF vertices[],
                                                   int num_vertices);
  static gfx::RectF ComputeEnclosingClippedRect(
      const HomogeneousCoordinate& h1,
      const HomogeneousCoordinate& h2,
      const HomogeneousCoordinate& h3,
      const HomogeneousCoordinate& h4);

  // NOTE: These functions do not do correct clipping against w = 0 plane, but
  // they correctly detect the clipped condition via the boolean clipped.
  static gfx::QuadF MapQuad(const gfx::Transform& transform,
                            const gfx::QuadF& quad,
                            bool* clipped);
  static gfx::PointF MapPoint(const gfx::Transform& transform,
                              gfx::PointF point,
                              bool* clipped);
  static gfx::Point3F MapPoint(const gfx::Transform&,
                               const gfx::Point3F&,
                               bool* clipped);
  static gfx::QuadF ProjectQuad(const gfx::Transform& transform,
                                const gfx::QuadF& quad,
                                bool* clipped);
  static gfx::PointF ProjectPoint(const gfx::Transform& transform,
                                  gfx::PointF point,
                                  bool* clipped);

  static gfx::Vector2dF ComputeTransform2dScaleComponents(const gfx::Transform&,
                                                          float fallbackValue);

  // Makes a rect that has the same relationship to input_outer_rect as
  // scale_inner_rect has to scale_outer_rect. scale_inner_rect should be
  // contained within scale_outer_rect, and likewise the rectangle that is
  // returned will be within input_outer_rect at a similar relative, scaled
  // position.
  static gfx::RectF ScaleRectProportional(const gfx::RectF& input_outer_rect,
                                          const gfx::RectF& scale_outer_rect,
                                          const gfx::RectF& scale_inner_rect);

  // Returns the smallest angle between the given two vectors in degrees.
  // Neither vector is assumed to be normalized.
  static float SmallestAngleBetweenVectors(gfx::Vector2dF v1,
                                           gfx::Vector2dF v2);

  // Projects the |source| vector onto |destination|. Neither vector is assumed
  // to be normalized.
  static gfx::Vector2dF ProjectVector(gfx::Vector2dF source,
                                      gfx::Vector2dF destination);

  // Conversion to value.
  static scoped_ptr<base::Value> AsValue(gfx::Size s);
  static scoped_ptr<base::Value> AsValue(gfx::SizeF s);
  static scoped_ptr<base::Value> AsValue(gfx::Rect r);
  static bool FromValue(const base::Value*, gfx::Rect* out_rect);
  static scoped_ptr<base::Value> AsValue(gfx::PointF q);
  static scoped_ptr<base::Value> AsValue(const gfx::QuadF& q);
  static scoped_ptr<base::Value> AsValue(const gfx::RectF& rect);
  static scoped_ptr<base::Value> AsValue(const gfx::Transform& transform);

  // Returns a base::Value representation of the floating point value.
  // If the value is inf, returns max double/float representation.
  static scoped_ptr<base::Value> AsValueSafely(double value);
  static scoped_ptr<base::Value> AsValueSafely(float value);
};

}  // namespace cc

#endif  // CC_BASE_MATH_UTIL_H_
