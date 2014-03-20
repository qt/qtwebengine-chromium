// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/box_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/vector3d_f.h"

namespace cc {
namespace {

TEST(TransformOperationTest, TransformTypesAreUnique) {
  ScopedVector<TransformOperations> transforms;

  TransformOperations* to_add = new TransformOperations();
  to_add->AppendTranslate(1, 0, 0);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 2);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(2, 2, 2);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(1, 0);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendPerspective(800);
  transforms.push_back(to_add);

  for (size_t i = 0; i < transforms.size(); ++i) {
    for (size_t j = 0; j < transforms.size(); ++j) {
      bool matches_type = transforms[i]->MatchesTypes(*transforms[j]);
      EXPECT_TRUE((i == j && matches_type) || !matches_type);
    }
  }
}

TEST(TransformOperationTest, MatchTypesSameLength) {
  TransformOperations translates;
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);

  TransformOperations skews;
  skews.AppendSkew(0, 2);
  skews.AppendSkew(0, 2);
  skews.AppendSkew(0, 2);

  TransformOperations translates2;
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);

  TransformOperations translates3 = translates2;

  EXPECT_FALSE(translates.MatchesTypes(skews));
  EXPECT_TRUE(translates.MatchesTypes(translates2));
  EXPECT_TRUE(translates.MatchesTypes(translates3));
}

TEST(TransformOperationTest, MatchTypesDifferentLength) {
  TransformOperations translates;
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);

  TransformOperations skews;
  skews.AppendSkew(2, 0);
  skews.AppendSkew(2, 0);

  TransformOperations translates2;
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);

  EXPECT_FALSE(translates.MatchesTypes(skews));
  EXPECT_FALSE(translates.MatchesTypes(translates2));
}

void GetIdentityOperations(ScopedVector<TransformOperations>* operations) {
  TransformOperations* to_add = new TransformOperations();
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendTranslate(0, 0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendTranslate(0, 0, 0);
  to_add->AppendTranslate(0, 0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(1, 1, 1);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(1, 1, 1);
  to_add->AppendScale(1, 1, 1);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(0, 0);
  to_add->AppendSkew(0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 0);
  to_add->AppendRotate(0, 0, 1, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendMatrix(gfx::Transform());
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendMatrix(gfx::Transform());
  to_add->AppendMatrix(gfx::Transform());
  operations->push_back(to_add);
}

TEST(TransformOperationTest, IdentityAlwaysMatches) {
  ScopedVector<TransformOperations> operations;
  GetIdentityOperations(&operations);

  for (size_t i = 0; i < operations.size(); ++i) {
    for (size_t j = 0; j < operations.size(); ++j)
      EXPECT_TRUE(operations[i]->MatchesTypes(*operations[j]));
  }
}

TEST(TransformOperationTest, ApplyTranslate) {
  SkMScalar x = 1;
  SkMScalar y = 2;
  SkMScalar z = 3;
  TransformOperations operations;
  operations.AppendTranslate(x, y, z);
  gfx::Transform expected;
  expected.Translate3d(x, y, z);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyRotate) {
  SkMScalar x = 1;
  SkMScalar y = 2;
  SkMScalar z = 3;
  SkMScalar degrees = 80;
  TransformOperations operations;
  operations.AppendRotate(x, y, z, degrees);
  gfx::Transform expected;
  expected.RotateAbout(gfx::Vector3dF(x, y, z), degrees);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyScale) {
  SkMScalar x = 1;
  SkMScalar y = 2;
  SkMScalar z = 3;
  TransformOperations operations;
  operations.AppendScale(x, y, z);
  gfx::Transform expected;
  expected.Scale3d(x, y, z);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplySkew) {
  SkMScalar x = 1;
  SkMScalar y = 2;
  TransformOperations operations;
  operations.AppendSkew(x, y);
  gfx::Transform expected;
  expected.SkewX(x);
  expected.SkewY(y);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyPerspective) {
  SkMScalar depth = 800;
  TransformOperations operations;
  operations.AppendPerspective(depth);
  gfx::Transform expected;
  expected.ApplyPerspectiveDepth(depth);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyMatrix) {
  SkMScalar dx = 1;
  SkMScalar dy = 2;
  SkMScalar dz = 3;
  gfx::Transform expected_matrix;
  expected_matrix.Translate3d(dx, dy, dz);
  TransformOperations matrix_transform;
  matrix_transform.AppendMatrix(expected_matrix);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_matrix, matrix_transform.Apply());
}

TEST(TransformOperationTest, ApplyOrder) {
  SkMScalar sx = 2;
  SkMScalar sy = 4;
  SkMScalar sz = 8;

  SkMScalar dx = 1;
  SkMScalar dy = 2;
  SkMScalar dz = 3;

  TransformOperations operations;
  operations.AppendScale(sx, sy, sz);
  operations.AppendTranslate(dx, dy, dz);

  gfx::Transform expected_scale_matrix;
  expected_scale_matrix.Scale3d(sx, sy, sz);

  gfx::Transform expected_translate_matrix;
  expected_translate_matrix.Translate3d(dx, dy, dz);

  gfx::Transform expected_combined_matrix = expected_scale_matrix;
  expected_combined_matrix.PreconcatTransform(expected_translate_matrix);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_combined_matrix, operations.Apply());
}

TEST(TransformOperationTest, BlendOrder) {
  SkMScalar sx1 = 2;
  SkMScalar sy1 = 4;
  SkMScalar sz1 = 8;

  SkMScalar dx1 = 1;
  SkMScalar dy1 = 2;
  SkMScalar dz1 = 3;

  SkMScalar sx2 = 4;
  SkMScalar sy2 = 8;
  SkMScalar sz2 = 16;

  SkMScalar dx2 = 10;
  SkMScalar dy2 = 20;
  SkMScalar dz2 = 30;

  TransformOperations operations_from;
  operations_from.AppendScale(sx1, sy1, sz1);
  operations_from.AppendTranslate(dx1, dy1, dz1);

  TransformOperations operations_to;
  operations_to.AppendScale(sx2, sy2, sz2);
  operations_to.AppendTranslate(dx2, dy2, dz2);

  gfx::Transform scale_from;
  scale_from.Scale3d(sx1, sy1, sz1);
  gfx::Transform translate_from;
  translate_from.Translate3d(dx1, dy1, dz1);

  gfx::Transform scale_to;
  scale_to.Scale3d(sx2, sy2, sz2);
  gfx::Transform translate_to;
  translate_to.Translate3d(dx2, dy2, dz2);

  SkMScalar progress = 0.25f;

  gfx::Transform blended_scale = scale_to;
  blended_scale.Blend(scale_from, progress);

  gfx::Transform blended_translate = translate_to;
  blended_translate.Blend(translate_from, progress);

  gfx::Transform expected = blended_scale;
  expected.PreconcatTransform(blended_translate);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

static void CheckProgress(SkMScalar progress,
                          const gfx::Transform& from_matrix,
                          const gfx::Transform& to_matrix,
                          const TransformOperations& from_transform,
                          const TransformOperations& to_transform) {
  gfx::Transform expected_matrix = to_matrix;
  expected_matrix.Blend(from_matrix, progress);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_matrix, to_transform.Blend(from_transform, progress));
}

TEST(TransformOperationTest, BlendProgress) {
  SkMScalar sx = 2;
  SkMScalar sy = 4;
  SkMScalar sz = 8;
  TransformOperations operations_from;
  operations_from.AppendScale(sx, sy, sz);

  gfx::Transform matrix_from;
  matrix_from.Scale3d(sx, sy, sz);

  sx = 4;
  sy = 8;
  sz = 16;
  TransformOperations operations_to;
  operations_to.AppendScale(sx, sy, sz);

  gfx::Transform matrix_to;
  matrix_to.Scale3d(sx, sy, sz);

  CheckProgress(-1, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0.25f, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0.5f, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(1, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(2, matrix_from, matrix_to, operations_from, operations_to);
}

TEST(TransformOperationTest, BlendWhenTypesDoNotMatch) {
  SkMScalar sx1 = 2;
  SkMScalar sy1 = 4;
  SkMScalar sz1 = 8;

  SkMScalar dx1 = 1;
  SkMScalar dy1 = 2;
  SkMScalar dz1 = 3;

  SkMScalar sx2 = 4;
  SkMScalar sy2 = 8;
  SkMScalar sz2 = 16;

  SkMScalar dx2 = 10;
  SkMScalar dy2 = 20;
  SkMScalar dz2 = 30;

  TransformOperations operations_from;
  operations_from.AppendScale(sx1, sy1, sz1);
  operations_from.AppendTranslate(dx1, dy1, dz1);

  TransformOperations operations_to;
  operations_to.AppendTranslate(dx2, dy2, dz2);
  operations_to.AppendScale(sx2, sy2, sz2);

  gfx::Transform from;
  from.Scale3d(sx1, sy1, sz1);
  from.Translate3d(dx1, dy1, dz1);

  gfx::Transform to;
  to.Translate3d(dx2, dy2, dz2);
  to.Scale3d(sx2, sy2, sz2);

  SkMScalar progress = 0.25f;

  gfx::Transform expected = to;
  expected.Blend(from, progress);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithSameAxis) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 0);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 0, 2, 360);

  SkMScalar progress = 0.5f;

  gfx::Transform expected;
  expected.RotateAbout(gfx::Vector3dF(0, 0, 1), 180);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithSameAxisInDifferentDirection) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 180);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 0, -1, 180);

  SkMScalar progress = 0.5f;

  gfx::Transform expected;

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithDifferentAxes) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 175);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 1, 0, 175);

  SkMScalar progress = 0.5f;
  gfx::Transform matrix_from;
  matrix_from.RotateAbout(gfx::Vector3dF(0, 0, 1), 175);

  gfx::Transform matrix_to;
  matrix_to.RotateAbout(gfx::Vector3dF(0, 1, 0), 175);

  gfx::Transform expected = matrix_to;
  expected.Blend(matrix_from, progress);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, BlendRotationFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendRotate(0, 0, 1, 360);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.RotateAbout(gfx::Vector3dF(0, 0, 1), 180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = -0.5f;

    expected.MakeIdentity();
    expected.RotateAbout(gfx::Vector3dF(0, 0, 1), -180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = 1.5f;

    expected.MakeIdentity();
    expected.RotateAbout(gfx::Vector3dF(0, 0, 1), 540);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendTranslationFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendTranslate(2, 2, 2);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.Translate3d(1, 1, 1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = -0.5f;

    expected.MakeIdentity();
    expected.Translate3d(-1, -1, -1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = 1.5f;

    expected.MakeIdentity();
    expected.Translate3d(3, 3, 3);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendScaleFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendScale(3, 3, 3);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.Scale3d(2, 2, 2);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = -0.5f;

    expected.MakeIdentity();
    expected.Scale3d(0, 0, 0);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = 1.5f;

    expected.MakeIdentity();
    expected.Scale3d(4, 4, 4);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendSkewFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendSkew(2, 2);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.SkewX(1);
    expected.SkewY(1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = -0.5f;

    expected.MakeIdentity();
    expected.SkewX(-1);
    expected.SkewY(-1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));

    progress = 1.5f;

    expected.MakeIdentity();
    expected.SkewX(3);
    expected.SkewY(3);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendPerspectiveFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendPerspective(1000);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.ApplyPerspectiveDepth(2000);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendRotationToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendRotate(0, 0, 1, 360);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.RotateAbout(gfx::Vector3dF(0, 0, 1), 180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendTranslationToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendTranslate(2, 2, 2);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.Translate3d(1, 1, 1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendScaleToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendScale(3, 3, 3);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.Scale3d(2, 2, 2);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendSkewToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendSkew(2, 2);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.SkewX(1);
    expected.SkewY(1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendPerspectiveToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendPerspective(1000);

    SkMScalar progress = 0.5f;

    gfx::Transform expected;
    expected.ApplyPerspectiveDepth(2000);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, ExtrapolatePerspectiveBlending) {
  TransformOperations operations1;
  operations1.AppendPerspective(1000);

  TransformOperations operations2;
  operations2.AppendPerspective(500);

  gfx::Transform expected;
  expected.ApplyPerspectiveDepth(400);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations1.Blend(operations2, -0.5));

  expected.MakeIdentity();
  expected.ApplyPerspectiveDepth(2000);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations1.Blend(operations2, 1.5));
}

TEST(TransformOperationTest, ExtrapolateMatrixBlending) {
  gfx::Transform transform1;
  transform1.Translate3d(1, 1, 1);
  TransformOperations operations1;
  operations1.AppendMatrix(transform1);

  gfx::Transform transform2;
  transform2.Translate3d(3, 3, 3);
  TransformOperations operations2;
  operations2.AppendMatrix(transform2);

  gfx::Transform expected;
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations1.Blend(operations2, 1.5));

  expected.Translate3d(4, 4, 4);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations1.Blend(operations2, -0.5));
}

TEST(TransformOperationTest, BlendedBoundsWhenTypesDoNotMatch) {
  TransformOperations operations_from;
  operations_from.AppendScale(2.0, 4.0, 8.0);
  operations_from.AppendTranslate(1.0, 2.0, 3.0);

  TransformOperations operations_to;
  operations_to.AppendTranslate(10.0, 20.0, 30.0);
  operations_to.AppendScale(4.0, 8.0, 16.0);

  gfx::BoxF box(1.f, 1.f, 1.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = 0.f;
  SkMScalar max_progress = 1.f;

  EXPECT_FALSE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
}

TEST(TransformOperationTest, BlendedBoundsForIdentity) {
  TransformOperations operations_from;
  operations_from.AppendIdentity();
  TransformOperations operations_to;
  operations_to.AppendIdentity();

  gfx::BoxF box(1.f, 2.f, 3.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = 0.f;
  SkMScalar max_progress = 1.f;

  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(box.ToString(), bounds.ToString());
}

TEST(TransformOperationTest, BlendedBoundsForTranslate) {
  TransformOperations operations_from;
  operations_from.AppendTranslate(3.0, -4.0, 2.0);
  TransformOperations operations_to;
  operations_to.AppendTranslate(7.0, 4.0, -2.0);

  gfx::BoxF box(1.f, 2.f, 3.f, 4.f, 4.f, 4.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = -0.5f;
  SkMScalar max_progress = 1.5f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(2.f, -6.f, -1.f, 12.f, 20.f, 12.f).ToString(),
            bounds.ToString());

  min_progress = 0.f;
  max_progress = 1.f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(4.f, -2.f, 1.f, 8.f, 12.f, 8.f).ToString(),
            bounds.ToString());

  TransformOperations identity;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
        box, identity, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, 1.f, 11.f, 8.f, 6.f).ToString(),
            bounds.ToString());

  EXPECT_TRUE(identity.BlendedBoundsForBox(
        box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, -2.f, 3.f, 7.f, 8.f, 6.f).ToString(),
            bounds.ToString());
}

TEST(TransformOperationTest, BlendedBoundsForScale) {
  TransformOperations operations_from;
  operations_from.AppendScale(3.0, 0.5, 2.0);
  TransformOperations operations_to;
  operations_to.AppendScale(7.0, 4.0, -2.0);

  gfx::BoxF box(1.f, 2.f, 3.f, 4.f, 4.f, 4.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = -0.5f;
  SkMScalar max_progress = 1.5f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, -7.5f, -28.f, 44.f, 42.f, 56.f).ToString(),
            bounds.ToString());

  min_progress = 0.f;
  max_progress = 1.f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(3.f, 1.f, -14.f, 32.f, 23.f, 28.f).ToString(),
            bounds.ToString());

  TransformOperations identity;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
        box, identity, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -14.f, 34.f, 22.f, 21.f).ToString(),
            bounds.ToString());

  EXPECT_TRUE(identity.BlendedBoundsForBox(
        box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 1.f, 3.f, 14.f, 5.f, 11.f).ToString(),
            bounds.ToString());
}

TEST(TransformOperationTest, BlendedBoundsWithZeroScale) {
  TransformOperations zero_scale;
  zero_scale.AppendScale(0.0, 0.0, 0.0);
  TransformOperations non_zero_scale;
  non_zero_scale.AppendScale(2.0, -4.0, 5.0);

  gfx::BoxF box(1.f, 2.f, 3.f, 4.f, 4.f, 4.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = 0.f;
  SkMScalar max_progress = 1.f;
  EXPECT_TRUE(zero_scale.BlendedBoundsForBox(
      box, non_zero_scale, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(0.f, -24.f, 0.f, 10.f, 24.f, 35.f).ToString(),
            bounds.ToString());

  EXPECT_TRUE(non_zero_scale.BlendedBoundsForBox(
      box, zero_scale, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(0.f, -24.f, 0.f, 10.f, 24.f, 35.f).ToString(),
            bounds.ToString());

  EXPECT_TRUE(zero_scale.BlendedBoundsForBox(
      box, zero_scale, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF().ToString(), bounds.ToString());
}

TEST(TransformOperationTest, BlendedBoundsForRotationTrivial) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0.f, 0.f, 1.f, 0.f);
  TransformOperations operations_to;
  operations_to.AppendRotate(0.f, 0.f, 1.f, 360.f);

  float sqrt_2 = sqrt(2.f);
  gfx::BoxF box(
      -sqrt_2, -sqrt_2, 0.f, sqrt_2, sqrt_2, 0.f);
  gfx::BoxF bounds;

  // Since we're rotating 360 degrees, any box with dimensions between 0 and
  // 2 * sqrt(2) should give the same result.
  float sizes[] = { 0.f, 0.1f, sqrt_2, 2.f * sqrt_2 };
  for (size_t i = 0; i < arraysize(sizes); ++i) {
    box.set_size(sizes[i], sizes[i], 0.f);
    SkMScalar min_progress = 0.f;
    SkMScalar max_progress = 1.f;
    EXPECT_TRUE(operations_to.BlendedBoundsForBox(
        box, operations_from, min_progress, max_progress, &bounds));
    EXPECT_EQ(gfx::BoxF(-2.f, -2.f, 0.f, 4.f, 4.f, 0.f).ToString(),
              bounds.ToString());
  }
}

TEST(TransformOperationTest, BlendedBoundsForRotationAllExtrema) {
  // If the normal is out of the plane, we can have up to 6 extrema (a min/max
  // in each dimension) between the endpoints of the arc. This test ensures that
  // we consider all 6.
  TransformOperations operations_from;
  operations_from.AppendRotate(1.f, 1.f, 1.f, 30.f);
  TransformOperations operations_to;
  operations_to.AppendRotate(1.f, 1.f, 1.f, 390.f);

  gfx::BoxF box(1.f, 0.f, 0.f, 0.f, 0.f, 0.f);
  gfx::BoxF bounds;

  float min = -1.f / 3.f;
  float max = 1.f;
  float size = max - min;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, 0.f, 1.f, &bounds));
  EXPECT_EQ(gfx::BoxF(min, min, min, size, size, size).ToString(),
            bounds.ToString());
}

TEST(TransformOperationTest, BlendedBoundsForRotationDifferentAxes) {
  // We can handle rotations about a single axis. If the axes are different,
  // we revert to matrix interpolation for which inflated bounds cannot be
  // computed.
  TransformOperations operations_from;
  operations_from.AppendRotate(1.f, 1.f, 1.f, 30.f);
  TransformOperations operations_to_same;
  operations_to_same.AppendRotate(1.f, 1.f, 1.f, 390.f);
  TransformOperations operations_to_opposite;
  operations_to_opposite.AppendRotate(-1.f, -1.f, -1.f, 390.f);
  TransformOperations operations_to_different;
  operations_to_different.AppendRotate(1.f, 3.f, 1.f, 390.f);

  gfx::BoxF box(1.f, 0.f, 0.f, 0.f, 0.f, 0.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(operations_to_same.BlendedBoundsForBox(
      box, operations_from, 0.f, 1.f, &bounds));
  EXPECT_TRUE(operations_to_opposite.BlendedBoundsForBox(
      box, operations_from, 0.f, 1.f, &bounds));
  EXPECT_FALSE(operations_to_different.BlendedBoundsForBox(
      box, operations_from, 0.f, 1.f, &bounds));
}

TEST(TransformOperationTest, BlendedBoundsForRotationPointOnAxis) {
  // Checks that if the point to rotate is sitting on the axis of rotation, that
  // it does not get affected.
  TransformOperations operations_from;
  operations_from.AppendRotate(1.f, 1.f, 1.f, 30.f);
  TransformOperations operations_to;
  operations_to.AppendRotate(1.f, 1.f, 1.f, 390.f);

  gfx::BoxF box(1.f, 1.f, 1.f, 0.f, 0.f, 0.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, 0.f, 1.f, &bounds));
  EXPECT_EQ(box.ToString(), bounds.ToString());
}

// This would have been best as anonymous structs, but |arraysize| does not get
// along with anonymous structs (and using ARRAYSIZE_UNSAFE seemed like a worse
// option).
struct ProblematicAxisTest {
  float x;
  float y;
  float z;
  gfx::BoxF expected;
};

TEST(TransformOperationTest, BlendedBoundsForRotationProblematicAxes) {
  // Zeros in the components of the axis of rotation turned out to be tricky to
  // deal with in practice. This function tests some potentially problematic
  // axes to ensure sane behavior.

  // Some common values used in the expected boxes.
  float dim1 = 0.292893f;
  float dim2 = sqrt(2.f);
  float dim3 = 2.f * dim2;

  ProblematicAxisTest tests[] = {
    { 0.f, 0.f, 0.f, gfx::BoxF(1.f, 1.f, 1.f, 0.f, 0.f, 0.f) },
    { 1.f, 0.f, 0.f, gfx::BoxF(1.f, -dim2, -dim2, 0.f, dim3, dim3) },
    { 0.f, 1.f, 0.f, gfx::BoxF(-dim2, 1.f, -dim2, dim3, 0.f, dim3) },
    { 0.f, 0.f, 1.f, gfx::BoxF(-dim2, -dim2, 1.f, dim3, dim3, 0.f) },
    { 1.f, 1.f, 0.f, gfx::BoxF(dim1, dim1, -1.f, dim2, dim2, 2.f) },
    { 0.f, 1.f, 1.f, gfx::BoxF(-1.f, dim1, dim1, 2.f, dim2, dim2) },
    { 1.f, 0.f, 1.f, gfx::BoxF(dim1, -1.f, dim1, dim2, 2.f, dim2) }
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    float x = tests[i].x;
    float y = tests[i].y;
    float z = tests[i].z;
    TransformOperations operations_from;
    operations_from.AppendRotate(x, y, z, 0.f);
    TransformOperations operations_to;
    operations_to.AppendRotate(x, y, z, 360.f);
    gfx::BoxF box(1.f, 1.f, 1.f, 0.f, 0.f, 0.f);
    gfx::BoxF bounds;

    EXPECT_TRUE(operations_to.BlendedBoundsForBox(
        box, operations_from, 0.f, 1.f, &bounds));
    EXPECT_EQ(tests[i].expected.ToString(), bounds.ToString());
  }
}

// These would have been best as anonymous structs, but |arraysize| does not get
// along with anonymous structs (and using ARRAYSIZE_UNSAFE seemed like a worse
// option).
struct TestAxis {
  float x;
  float y;
  float z;
};

struct TestAngles {
  float theta_from;
  float theta_to;
};

struct TestProgress {
  float min_progress;
  float max_progress;
};

static void ExpectBoxesApproximatelyEqual(const gfx::BoxF& lhs,
                                          const gfx::BoxF& rhs,
                                          float tolerance) {
  EXPECT_NEAR(lhs.x(), rhs.x(), tolerance);
  EXPECT_NEAR(lhs.y(), rhs.y(), tolerance);
  EXPECT_NEAR(lhs.z(), rhs.z(), tolerance);
  EXPECT_NEAR(lhs.width(), rhs.width(), tolerance);
  EXPECT_NEAR(lhs.height(), rhs.height(), tolerance);
  EXPECT_NEAR(lhs.depth(), rhs.depth(), tolerance);
}

static void EmpiricallyTestBounds(const TransformOperations& from,
                                  const TransformOperations& to,
                                  SkMScalar min_progress,
                                  SkMScalar max_progress,
                                  bool test_containment_only) {
  gfx::BoxF box(200.f, 500.f, 100.f, 100.f, 300.f, 200.f);
  gfx::BoxF bounds;
  EXPECT_TRUE(
      to.BlendedBoundsForBox(box, from, min_progress, max_progress, &bounds));

  bool first_time = true;
  gfx::BoxF empirical_bounds;
  static const size_t kNumSteps = 10;
  for (size_t step = 0; step < kNumSteps; ++step) {
    float t = step / (kNumSteps - 1.f);
    t = gfx::Tween::FloatValueBetween(t, min_progress, max_progress);
    gfx::Transform partial_transform = to.Blend(from, t);
    gfx::BoxF transformed = box;
    partial_transform.TransformBox(&transformed);

    if (first_time) {
      empirical_bounds = transformed;
      first_time = false;
    } else {
      empirical_bounds.Union(transformed);
    }
  }

  if (test_containment_only) {
    gfx::BoxF unified_bounds = bounds;
    unified_bounds.Union(empirical_bounds);
    // Convert to the screen space rects these boxes represent.
    gfx::Rect bounds_rect = ToEnclosingRect(
        gfx::RectF(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
    gfx::Rect unified_bounds_rect =
        ToEnclosingRect(gfx::RectF(unified_bounds.x(),
                                   unified_bounds.y(),
                                   unified_bounds.width(),
                                   unified_bounds.height()));
    EXPECT_EQ(bounds_rect.ToString(), unified_bounds_rect.ToString());
  } else {
    // Our empirical estimate will be a little rough since we're only doing
    // 100 samples.
    static const float kTolerance = 1e-2f;
    ExpectBoxesApproximatelyEqual(empirical_bounds, bounds, kTolerance);
  }
}

static void EmpiricallyTestBoundsEquality(const TransformOperations& from,
                                          const TransformOperations& to,
                                          SkMScalar min_progress,
                                          SkMScalar max_progress) {
  EmpiricallyTestBounds(from, to, min_progress, max_progress, false);
}

static void EmpiricallyTestBoundsContainment(const TransformOperations& from,
                                             const TransformOperations& to,
                                             SkMScalar min_progress,
                                             SkMScalar max_progress) {
  EmpiricallyTestBounds(from, to, min_progress, max_progress, true);
}

TEST(TransformOperationTest, BlendedBoundsForRotationEmpiricalTests) {
  // Sets up various axis angle combinations, computes the bounding box and
  // empirically tests that the transformed bounds are indeed contained by the
  // computed bounding box.

  TestAxis axes[] = {
    { 1.f, 1.f, 1.f },
    { -1.f, -1.f, -1.f },
    { -1.f, 2.f, 3.f },
    { 1.f, -2.f, 3.f },
    { 1.f, 2.f, -3.f },
    { 0.f, 0.f, 0.f },
    { 1.f, 0.f, 0.f },
    { 0.f, 1.f, 0.f },
    { 0.f, 0.f, 1.f },
    { 1.f, 1.f, 0.f },
    { 0.f, 1.f, 1.f },
    { 1.f, 0.f, 1.f },
    { -1.f, 0.f, 0.f },
    { 0.f, -1.f, 0.f },
    { 0.f, 0.f, -1.f },
    { -1.f, -1.f, 0.f },
    { 0.f, -1.f, -1.f },
    { -1.f, 0.f, -1.f }
  };

  TestAngles angles[] = {
    { 5.f, 10.f },
    { 10.f, 5.f },
    { 0.f, 360.f },
    { 20.f, 180.f },
    { -20.f, -180.f },
    { 180.f, -220.f },
    { 220.f, 320.f }
  };

  // We can go beyond the range [0, 1] (the bezier might slide out of this range
  // at either end), but since the first and last knots are at (0, 0) and (1, 1)
  // we will never go within it, so these tests are sufficient.
  TestProgress progress[] = {
    { 0.f, 1.f },
    { -.25f, 1.25f },
  };

  for (size_t i = 0; i < arraysize(axes); ++i) {
    for (size_t j = 0; j < arraysize(angles); ++j) {
      for (size_t k = 0; k < arraysize(progress); ++k) {
        float x = axes[i].x;
        float y = axes[i].y;
        float z = axes[i].z;
        TransformOperations operations_from;
        operations_from.AppendRotate(x, y, z, angles[j].theta_from);
        TransformOperations operations_to;
        operations_to.AppendRotate(x, y, z, angles[j].theta_to);
        EmpiricallyTestBoundsContainment(operations_from,
                                         operations_to,
                                         progress[k].min_progress,
                                         progress[k].max_progress);
      }
    }
  }
}

TEST(TransformOperationTest, PerspectiveMatrixAndTransformBlendingEquivalency) {
    TransformOperations from_operations;
    from_operations.AppendPerspective(200);

    TransformOperations to_operations;
    to_operations.AppendPerspective(1000);

    gfx::Transform from_transform;
    from_transform.ApplyPerspectiveDepth(200);

    gfx::Transform to_transform;
    to_transform.ApplyPerspectiveDepth(1000);

    static const int steps = 20;
    for (int i = 0; i < steps; ++i) {
      double progress = static_cast<double>(i) / (steps - 1);

      gfx::Transform blended_matrix = to_transform;
      EXPECT_TRUE(blended_matrix.Blend(from_transform, progress));

      gfx::Transform blended_transform =
          to_operations.Blend(from_operations, progress);

      EXPECT_TRANSFORMATION_MATRIX_EQ(blended_matrix, blended_transform);
    }
}

struct TestPerspectiveDepths {
  float from_depth;
  float to_depth;
};

TEST(TransformOperationTest, BlendedBoundsForPerspective) {
  TestPerspectiveDepths perspective_depths[] = {
    { 600.f, 400.f },
    { 800.f, 1000.f },
    { 800.f, std::numeric_limits<float>::infinity() },
  };

  TestProgress progress[] = {
    { 0.f, 1.f },
    { -0.1f, 1.1f },
  };

  for (size_t i = 0; i < arraysize(perspective_depths); ++i) {
    for (size_t j = 0; j < arraysize(progress); ++j) {
      TransformOperations operations_from;
      operations_from.AppendPerspective(perspective_depths[i].from_depth);
      TransformOperations operations_to;
      operations_to.AppendPerspective(perspective_depths[i].to_depth);
      EmpiricallyTestBoundsEquality(operations_from,
                                    operations_to,
                                    progress[j].min_progress,
                                    progress[j].max_progress);
    }
  }
}

struct TestSkews {
  float from_x;
  float from_y;
  float to_x;
  float to_y;
};

TEST(TransformOperationTest, BlendedBoundsForSkew) {
  TestSkews skews[] = {
    { 1.f, 0.5f, 0.5f, 1.f },
    { 2.f, 1.f, 0.5f, 0.5f },
  };

  TestProgress progress[] = {
    { 0.f, 1.f },
    { -0.1f, 1.1f },
  };

  for (size_t i = 0; i < arraysize(skews); ++i) {
    for (size_t j = 0; j < arraysize(progress); ++j) {
      TransformOperations operations_from;
      operations_from.AppendSkew(skews[i].from_x, skews[i].from_y);
      TransformOperations operations_to;
      operations_to.AppendSkew(skews[i].to_x, skews[i].to_y);
      EmpiricallyTestBoundsEquality(operations_from,
                                    operations_to,
                                    progress[j].min_progress,
                                    progress[j].max_progress);
    }
  }
}

TEST(TransformOperationTest, BlendedBoundsForSequence) {
  TransformOperations operations_from;
  operations_from.AppendTranslate(2.0, 4.0, -1.0);
  operations_from.AppendScale(-1.0, 2.0, 3.0);
  operations_from.AppendTranslate(1.0, -5.0, 1.0);
  TransformOperations operations_to;
  operations_to.AppendTranslate(6.0, -2.0, 3.0);
  operations_to.AppendScale(-3.0, -2.0, 5.0);
  operations_to.AppendTranslate(13.0, -1.0, 5.0);

  gfx::BoxF box(1.f, 2.f, 3.f, 4.f, 4.f, 4.f);
  gfx::BoxF bounds;

  SkMScalar min_progress = -0.5f;
  SkMScalar max_progress = 1.5f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(-57.f, -59.f, -1.f, 76.f, 112.f, 80.f).ToString(),
            bounds.ToString());

  min_progress = 0.f;
  max_progress = 1.f;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
      box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(-32.f, -25.f, 7.f, 42.f, 44.f, 48.f).ToString(),
            bounds.ToString());

  TransformOperations identity;
  EXPECT_TRUE(operations_to.BlendedBoundsForBox(
        box, identity, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(-33.f, -13.f, 3.f, 57.f, 19.f, 52.f).ToString(),
            bounds.ToString());

  EXPECT_TRUE(identity.BlendedBoundsForBox(
        box, operations_from, min_progress, max_progress, &bounds));
  EXPECT_EQ(gfx::BoxF(-7.f, -3.f, 2.f, 15.f, 23.f, 20.f).ToString(),
            bounds.ToString());
}

}  // namespace
}  // namespace cc
