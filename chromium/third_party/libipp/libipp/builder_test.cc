// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "builder.h"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "binary_content.h"
#include "frame.h"

// Build the frame with a single dateTime attribute.
TEST(Builder, DateTime) {
  ipp::Frame frame(ipp::Status::successful_ok, ipp::Version::_1_1,
                   /*request_id=*/1,
                   /*set_localization_en_us_and_status_message=*/false);
  ipp::CollsView::iterator coll;
  ASSERT_EQ(frame.AddGroup(ipp::GroupTag::operation_attributes, coll),
            ipp::Code::kOK);
  ipp::DateTime value;
  value.year = 2002;
  value.month = 1;
  value.day = 23;
  value.hour = 4;
  value.minutes = 56;
  value.seconds = 7;
  value.deci_seconds = 8;
  value.UTC_direction = '+';
  value.UTC_hours = 9;
  value.UTC_minutes = 10;
  ASSERT_EQ(coll->AddAttr("test-attr", value), ipp::Code::kOK);

  BinaryContent c;
  c.u2(0x0101u);     // version-number = 1.1
  c.u2(0x0000u);     // status-code = successful-ok
  c.u4(1);           // request-id
  c.u1(0x01u);       // group-tag = operation-attributes-tag
  c.u1(0x31);        // value-tag = dateTime
  c.u2(9);           // name-length
  c.s("test-attr");  // name
  c.u2(11);          // value-length
  c.u2(2002);        // dateTime.year
  c.u1(1);           // dateTime.month
  c.u1(23);          // dateTime.day
  c.u1(4);           // dateTime.hour
  c.u1(56);          // dateTime.minutes
  c.u1(7);           // dateTime.seconds
  c.u1(8);           // dateTime.deci-seconds
  c.u1('+');         // dateTime.direction-from-UTC
  c.u1(9);           // dateTime.hours-from-UTC
  c.u1(10);          // dateTime.minutes-from-UTC
  c.u1(0x03u);       // end-of-attributes-tag

  std::vector<uint8_t> buf(1024, 0);
  EXPECT_EQ(ipp::BuildBinaryFrame(frame, buf.data(), 1024), c.data.size());
  buf.resize(c.data.size());
  EXPECT_EQ(buf, c.data);
}

// Build the frame with a single resolution attribute.
TEST(Builder, Resolution) {
  ipp::Frame frame(ipp::Status::successful_ok, ipp::Version::_1_1,
                   /*request_id=*/1,
                   /*set_localization_en_us_and_status_message=*/false);
  ipp::CollsView::iterator coll;
  ASSERT_EQ(frame.AddGroup(ipp::GroupTag::operation_attributes, coll),
            ipp::Code::kOK);
  ipp::Resolution value;
  value.xres = 1234567890;
  value.yres = 234567890;
  value.units = ipp::Resolution::Units::kDotsPerCentimeter;
  ASSERT_EQ(coll->AddAttr("test-attr", value), ipp::Code::kOK);

  BinaryContent c;
  c.u2(0x0101u);     // version-number = 1.1
  c.u2(0x0000u);     // status-code = successful-ok
  c.u4(1);           // request-id
  c.u1(0x01u);       // group-tag = operation-attributes-tag
  c.u1(0x32);        // value-tag = resolution
  c.u2(9);           // name-length
  c.s("test-attr");  // name
  c.u2(9);           // value-length
  c.u4(1234567890);  // resolution.cros-feed-direction
  c.u4(234567890);   // resolution.feed-direction
  c.u1(4);           // resolution.units = dots-per-centimeter
  c.u1(0x03u);       // end-of-attributes-tag

  std::vector<uint8_t> buf(1024, 0);
  EXPECT_EQ(ipp::BuildBinaryFrame(frame, buf.data(), 1024), c.data.size());
  buf.resize(c.data.size());
  EXPECT_EQ(buf, c.data);
}

// Build the frame with a single rangeOfInteger attribute.
TEST(Builder, RangeOfInteger) {
  ipp::Frame frame(ipp::Status::successful_ok, ipp::Version::_1_1,
                   /*request_id=*/1,
                   /*set_localization_en_us_and_status_message=*/false);
  ipp::CollsView::iterator coll;
  ASSERT_EQ(frame.AddGroup(ipp::GroupTag::operation_attributes, coll),
            ipp::Code::kOK);
  ipp::RangeOfInteger value;
  value.min_value = -1234567890;
  value.max_value = 1234567890;
  ASSERT_EQ(coll->AddAttr("test-attr", value), ipp::Code::kOK);

  BinaryContent c;
  c.u2(0x0101u);      // version-number = 1.1
  c.u2(0x0000u);      // status-code = successful-ok
  c.u4(1);            // request-id
  c.u1(0x01u);        // group-tag = operation-attributes-tag
  c.u1(0x33);         // value-tag = rangeOfInteger
  c.u2(9);            // name-length
  c.s("test-attr");   // name
  c.u2(8);            // value-length
  c.u4(-1234567890);  // rangeOfInteger.lower-bound
  c.u4(1234567890);   // rangeOfInteger.upper-bound
  c.u1(0x03u);        // end-of-attributes-tag

  std::vector<uint8_t> buf(1024, 0);
  EXPECT_EQ(ipp::BuildBinaryFrame(frame, buf.data(), 1024), c.data.size());
  buf.resize(c.data.size());
  EXPECT_EQ(buf, c.data);
}
