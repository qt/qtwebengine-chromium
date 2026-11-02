// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/bytestring_pool.h"

#include "core/fxcrt/bytestring.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fxcrt {

TEST(ByteStringPool, Basic) {
  ByteStringPool pool;

  ByteString null1;
  ByteString null2;
  ByteString goats1("goats");
  ByteString goats2("goats");

  // Underlying storage, if non-null, is not shared.
  EXPECT_TRUE(null1.IsEmpty());
  EXPECT_TRUE(null2.IsEmpty());
  EXPECT_NE(goats1.c_str(), goats2.c_str());

  ByteString interned_null1 = pool.Intern(null1);
  ByteString interned_null2 = pool.Intern(null2);
  ByteString interned_goats1 = pool.Intern(goats1);
  ByteString interned_goats2 = pool.Intern(goats2);

  // Strings are logically equal after being interned.
  EXPECT_EQ(null1, interned_null1);
  EXPECT_EQ(null2, interned_null2);
  EXPECT_EQ(goats1, interned_goats1);
  EXPECT_EQ(goats2, interned_goats2);

  // Interned underlying storage, if non-null, belongs to first seen.
  EXPECT_TRUE(interned_null1.IsEmpty());
  EXPECT_TRUE(interned_null2.IsEmpty());
  EXPECT_EQ(goats1.c_str(), interned_goats1.c_str());
  EXPECT_EQ(goats1.c_str(), interned_goats2.c_str());
}

}  // namespace fxcrt
