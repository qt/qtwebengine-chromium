// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/raw_ptr.h"

#include <memory>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(RawPtrTest, SizeIsSameAsRawPointer) {
#if !defined(BUILD_WITH_CHROMIUM)
  // In standalone builds, raw_ptr must be zero-overhead.
  EXPECT_EQ(sizeof(raw_ptr<int>), sizeof(int*));
#endif
}

TEST(RawPtrTest, DefaultConstructorInitializesToNull) {
  raw_ptr<int> p;
  EXPECT_EQ(p.get(), nullptr);
  EXPECT_FALSE(p);
}

TEST(RawPtrTest, NullptrConstructorInitializesToNull) {
  raw_ptr<int> p = nullptr;
  EXPECT_EQ(p.get(), nullptr);
  EXPECT_FALSE(p);
}

TEST(RawPtrTest, ConstructFromRawPointer) {
  int val = 42;
  raw_ptr<int> p = &val;
  EXPECT_EQ(p.get(), &val);
  EXPECT_TRUE(p);
  EXPECT_EQ(*p, 42);
}

TEST(RawPtrTest, CopyAndMove) {
  int val = 42;
  raw_ptr<int> p1 = &val;

  // Copy
  raw_ptr<int> p2 = p1;
  EXPECT_EQ(p2.get(), &val);
  EXPECT_EQ(p1.get(), &val);

  // Move
  raw_ptr<int> p3 = std::move(p2);
  EXPECT_EQ(p3.get(), &val);
  EXPECT_EQ(p2.get(), nullptr);
}

TEST(RawPtrTest, Assignment) {
  int val1 = 42;
  int val2 = 84;
  raw_ptr<int> p1 = &val1;
  raw_ptr<int> p2 = &val2;

  p1 = p2;
  EXPECT_EQ(p1.get(), &val2);

  p1 = nullptr;
  EXPECT_EQ(p1.get(), nullptr);

  p1 = &val1;
  EXPECT_EQ(p1.get(), &val1);
}

TEST(RawPtrTest, SelfAssignment) {
  int val = 42;
  raw_ptr<int> p = &val;

  // Self-copy-assignment
  raw_ptr<int>* p_ptr = &p;
  p = *p_ptr;
  EXPECT_EQ(p.get(), &val);

  // Self-move-assignment
  p = std::move(*p_ptr);
  EXPECT_EQ(p.get(), &val);
}

#if !defined(BUILD_WITH_CHROMIUM) && !defined(MEMORY_SANITIZER) && \
    !defined(__SANITIZE_MEMORY__)
TEST(RawPtrTest, NullsOnDestruction) {
  alignas(raw_ptr<int>) char buffer[sizeof(raw_ptr<int>)];
  int val = 42;
  auto* p = new (buffer) raw_ptr<int>(&val);
  EXPECT_EQ(p->get(), &val);
  p->~raw_ptr();
  int* ptr_in_buffer = *reinterpret_cast<int**>(buffer);
  EXPECT_EQ(ptr_in_buffer, nullptr);
}
#endif

TEST(RawPtrTest, DereferenceAndMemberAccess) {
  struct Foo {
    int x = 42;
  };
  Foo foo;
  raw_ptr<Foo> p = &foo;
  EXPECT_EQ(p->x, 42);
  EXPECT_EQ((*p).x, 42);
}

TEST(RawPtrTest, Comparison) {
  int val1 = 42;
  int val2 = 84;
  raw_ptr<int> p1 = &val1;
  raw_ptr<int> p2 = &val2;
  raw_ptr<int> null_p = nullptr;

  EXPECT_TRUE(p1 == p1);
  EXPECT_FALSE(p1 == p2);
  EXPECT_TRUE(p1 != p2);

  EXPECT_TRUE(p1 == &val1);
  EXPECT_TRUE(&val1 == p1);
  EXPECT_FALSE(p1 == &val2);
  EXPECT_FALSE(&val2 == p1);

  EXPECT_TRUE(null_p == nullptr);
  EXPECT_TRUE(nullptr == null_p);
  EXPECT_FALSE(p1 == nullptr);
  EXPECT_FALSE(nullptr == p1);
}

struct Base {
  virtual ~Base() = default;
  int base_val = 1;
};

struct Derived : public Base {
  int derived_val = 2;
};

TEST(RawPtrTest, Upcasting) {
  Derived derived;
  raw_ptr<Derived> p_derived = &derived;

  // Copy construct Derived -> Base
  raw_ptr<Base> p_base(p_derived);
  EXPECT_EQ(p_base.get(), &derived);
  EXPECT_EQ(p_base->base_val, 1);

  // Assignment Derived -> Base
  raw_ptr<Base> p_base2;
  p_base2 = p_derived;
  EXPECT_EQ(p_base2.get(), &derived);

  // Move construct Derived -> Base
  raw_ptr<Derived> p_derived_move = &derived;
  raw_ptr<Base> p_base_move(std::move(p_derived_move));
  EXPECT_EQ(p_base_move.get(), &derived);
  EXPECT_EQ(p_derived_move.get(), nullptr);

  // Move assignment Derived -> Base
  raw_ptr<Derived> p_derived_move2 = &derived;
  raw_ptr<Base> p_base_move2;
  p_base_move2 = std::move(p_derived_move2);
  EXPECT_EQ(p_base_move2.get(), &derived);
  EXPECT_EQ(p_derived_move2.get(), nullptr);
}

TEST(RawPtrTest, RelationalOperators) {
  int arr[2] = {42, 84};
  raw_ptr<int> p0 = &arr[0];
  raw_ptr<int> p1 = &arr[1];

  EXPECT_TRUE(p0 < p1);
  EXPECT_TRUE(p0 <= p1);
  EXPECT_TRUE(p1 > p0);
  EXPECT_TRUE(p1 >= p0);

  EXPECT_FALSE(p1 < p0);
  EXPECT_FALSE(p1 <= p0);
  EXPECT_FALSE(p0 > p1);
  EXPECT_FALSE(p0 >= p1);

  EXPECT_TRUE(p0 < &arr[1]);
  EXPECT_TRUE(p0 <= &arr[1]);
  EXPECT_TRUE(&arr[1] > p0);
  EXPECT_TRUE(&arr[1] >= p0);
}

TEST(RawPtrTest, VoidPointer) {
  int val = 42;
  raw_ptr<void> p = &val;
  EXPECT_EQ(p.get(), &val);
  EXPECT_TRUE(p);

  raw_ptr<const void> cp = &val;
  EXPECT_EQ(cp.get(), &val);
}

TEST(RawPtrTest, StlHooks) {
  int a = 1;
  int b = 2;
  raw_ptr<int> pa = &a;
  raw_ptr<int> pb = &b;

  std::set<raw_ptr<int>, std::less<>> ptr_set;
  ptr_set.insert(pa);
  ptr_set.insert(pb);
  EXPECT_EQ(ptr_set.size(), 2u);
  EXPECT_TRUE(ptr_set.find(pa) != ptr_set.end());
  EXPECT_TRUE(ptr_set.find(&a) != ptr_set.end());

  std::unordered_set<raw_ptr<int>> ptr_uset;
  ptr_uset.insert(pa);
  ptr_uset.insert(pb);
  EXPECT_EQ(ptr_uset.size(), 2u);
  EXPECT_TRUE(ptr_uset.find(pa) != ptr_uset.end());

  EXPECT_EQ(std::to_address(pa), &a);
}

}  // namespace
}  // namespace openscreen
