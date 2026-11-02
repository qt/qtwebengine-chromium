// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/raw_ref.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(RawRefTest, SizeIsSameAsRawPointer) {
#if !defined(BUILD_WITH_CHROMIUM)
  // In standalone builds, raw_ref must be zero-overhead.
  EXPECT_EQ(sizeof(raw_ref<int>), sizeof(int*));
#endif
}

TEST(RawRefTest, ConstructFromReference) {
  int val = 42;
  raw_ref<int> r(val);
  EXPECT_EQ(&r.get(), &val);
  EXPECT_EQ(*r, 42);
}

TEST(RawRefTest, CopyAndMove) {
  int val = 42;
  raw_ref<int> r1(val);

  // Copy
  raw_ref<int> r2 = r1;
  EXPECT_EQ(&r2.get(), &val);
  EXPECT_EQ(&r1.get(), &val);

  // Move
  raw_ref<int> r3 = std::move(r2);
  EXPECT_EQ(&r3.get(), &val);
}

TEST(RawRefTest, Assignment) {
  int val1 = 42;
  int val2 = 84;
  raw_ref<int> r1(val1);
  raw_ref<int> r2(val2);

  r1 = r2;
  EXPECT_EQ(&r1.get(), &val2);

  r1 = std::move(r2);
  EXPECT_EQ(&r1.get(), &val2);
}

TEST(RawRefTest, DereferenceAndMemberAccess) {
  struct Foo {
    int x = 42;
  };
  Foo foo;
  raw_ref<Foo> r(foo);
  EXPECT_EQ(r->x, 42);
  EXPECT_EQ((*r).x, 42);
  EXPECT_EQ(r.get().x, 42);
}

TEST(RawRefTest, Comparison) {
  int val1 = 42;
  int val2 = 84;
  raw_ref<int> r1(val1);
  raw_ref<int> r1_copy(val1);
  raw_ref<int> r2(val2);

  EXPECT_TRUE(r1 == r1);
  EXPECT_TRUE(r1 == r1_copy);
  EXPECT_FALSE(r1 == r2);
  EXPECT_TRUE(r1 != r2);
}

struct Base {
  virtual ~Base() = default;
  int base_val = 1;
};

struct Derived : public Base {
  int derived_val = 2;
};

TEST(RawRefTest, Upcasting) {
  Derived derived;
  raw_ref<Derived> r_derived(derived);

  // Copy construct Derived -> Base
  raw_ref<Base> r_base(r_derived);
  EXPECT_EQ(&r_base.get(), &derived);
  EXPECT_EQ(r_base->base_val, 1);

  // Move construct Derived -> Base
  raw_ref<Derived> r_derived_move(derived);
  raw_ref<Base> r_base_move(std::move(r_derived_move));
  EXPECT_EQ(&r_base_move.get(), &derived);

  // Assignment Derived -> Base
  Derived derived2;
  raw_ref<Derived> r_derived2(derived2);
  r_base = r_derived2;
  EXPECT_EQ(&r_base.get(), &derived2);

  // Move assignment Derived -> Base
  Derived derived3;
  raw_ref<Derived> r_derived3(derived3);
  r_base = std::move(r_derived3);
  EXPECT_EQ(&r_base.get(), &derived3);
}

}  // namespace
}  // namespace openscreen
