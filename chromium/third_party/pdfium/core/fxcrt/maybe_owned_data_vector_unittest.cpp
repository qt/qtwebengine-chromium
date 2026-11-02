// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/maybe_owned_data_vector.h"

#include <array>
#include <utility>

#include "core/fxcrt/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace fxcrt {

TEST(MaybeOwnedDataVector, Default) {
  MaybeOwnedDataVector<int> vec;
  EXPECT_FALSE(vec.IsOwned());
  EXPECT_TRUE(vec.span().empty());
}

TEST(MaybeOwnedDataVector, Unowned) {
  std::array<int, 5> data = {1, 2, 3, 4, 5};
  MaybeOwnedDataVector<int> vec(data);
  EXPECT_FALSE(vec.IsOwned());
  EXPECT_EQ(data.data(), vec.span().data());
  EXPECT_THAT(vec.span(), ElementsAre(1, 2, 3, 4, 5));
}

TEST(MaybeOwnedDataVector, Owned) {
  DataVector<int> data = {1, 2, 3, 4, 5};
  MaybeOwnedDataVector<int> vec(std::move(data));
  EXPECT_TRUE(vec.IsOwned());
  EXPECT_THAT(vec.span(), ElementsAre(1, 2, 3, 4, 5));
}

TEST(MaybeOwnedDataVector, ResizeOwned) {
  DataVector<int> data = {1, 2, 3};
  MaybeOwnedDataVector<int> vec(std::move(data));
  EXPECT_TRUE(vec.IsOwned());
  EXPECT_THAT(vec.span(), ElementsAre(1, 2, 3));

  vec.ResizeOwned(5);
  EXPECT_TRUE(vec.IsOwned());
  EXPECT_THAT(vec.span(), ElementsAre(1, 2, 3, 0, 0));
}

TEST(MaybeOwnedDataVector, Copy) {
  DataVector<int> data = {1, 2, 3};
  MaybeOwnedDataVector<int> vec1(std::move(data));
  MaybeOwnedDataVector<int> vec2 = vec1;

  EXPECT_TRUE(vec1.IsOwned());
  EXPECT_TRUE(vec2.IsOwned());
  EXPECT_THAT(vec1.span(), ElementsAre(1, 2, 3));
  EXPECT_THAT(vec2.span(), ElementsAre(1, 2, 3));
  EXPECT_NE(vec1.span().data(), vec2.span().data());
}

TEST(MaybeOwnedDataVector, Move) {
  DataVector<int> data = {1, 2, 3};
  MaybeOwnedDataVector<int> vec1(std::move(data));
  MaybeOwnedDataVector<int> vec2 = std::move(vec1);

  EXPECT_FALSE(vec1.IsOwned());
  EXPECT_TRUE(vec1.span().empty());
  EXPECT_TRUE(vec2.IsOwned());
  EXPECT_THAT(vec2.span(), ElementsAre(1, 2, 3));
}

}  // namespace fxcrt
