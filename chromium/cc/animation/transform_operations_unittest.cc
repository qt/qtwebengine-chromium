// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/memory/scoped_vector.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/box_f.h"
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
    expected.ApplyPerspectiveDepth(500 +
                                   0.5 * std::numeric_limits<SkMScalar>::max());

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
    expected.ApplyPerspectiveDepth(500 +
                                   0.5 * std::numeric_limits<SkMScalar>::max());

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
  expected.ApplyPerspectiveDepth(250);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations1.Blend(operations2, -0.5));

  expected.MakeIdentity();
  expected.ApplyPerspectiveDepth(1250);

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
