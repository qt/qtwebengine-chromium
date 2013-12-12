// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

TEST(QuicHttpUtilsTest, ConvertRequestPriorityToQuicPriority) {
  EXPECT_EQ(0u, ConvertRequestPriorityToQuicPriority(HIGHEST));
  EXPECT_EQ(1u, ConvertRequestPriorityToQuicPriority(MEDIUM));
  EXPECT_EQ(2u, ConvertRequestPriorityToQuicPriority(LOW));
  EXPECT_EQ(3u, ConvertRequestPriorityToQuicPriority(LOWEST));
  EXPECT_EQ(4u, ConvertRequestPriorityToQuicPriority(IDLE));
}

TEST(QuicHttpUtilsTest, ConvertQuicPriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertQuicPriorityToRequestPriority(0));
  EXPECT_EQ(MEDIUM, ConvertQuicPriorityToRequestPriority(1));
  EXPECT_EQ(LOW, ConvertQuicPriorityToRequestPriority(2));
  EXPECT_EQ(LOWEST, ConvertQuicPriorityToRequestPriority(3));
  EXPECT_EQ(IDLE, ConvertQuicPriorityToRequestPriority(4));
  // These are invalid values, but we should still handle them
  // gracefully. TODO(rtenneti): should we test for all possible values of
  // uint32?
  for (int i = 5; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertQuicPriorityToRequestPriority(i));
  }
}

}  // namespace test
}  // namespace net
