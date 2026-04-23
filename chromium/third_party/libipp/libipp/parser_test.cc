// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "parser.h"

#include <gtest/gtest.h>

#include "binary_content.h"
#include "ipp_enums.h"

namespace ipp {
namespace {

TEST(SimpleParserLog, Empty) {
  SimpleParserLog log;
  EXPECT_TRUE(log.Errors().empty());
  EXPECT_TRUE(log.CriticalErrors().empty());
}

TEST(SimpleParserLog, AddParserError) {
  SimpleParserLog log;
  log.AddParserError(
      {AttrPath(GroupTag::printer_attributes), ParserCode::kValueInvalidSize});
  ASSERT_EQ(log.Errors().size(), 1);
  EXPECT_EQ(log.Errors()[0].path.AsString(), "printer-attributes");
  EXPECT_EQ(log.Errors()[0].code, ParserCode::kValueInvalidSize);
  EXPECT_TRUE(log.CriticalErrors().empty());
}

TEST(SimpleParserLog, AddParserErrorCritical) {
  SimpleParserLog log;
  log.AddParserError({AttrPath(GroupTag::printer_attributes),
                      ParserCode::kGroupTagWasExpected});
  ASSERT_EQ(log.Errors().size(), 1);
  EXPECT_EQ(log.Errors()[0].path.AsString(), "printer-attributes");
  EXPECT_EQ(log.Errors()[0].code, ParserCode::kGroupTagWasExpected);
  ASSERT_EQ(log.CriticalErrors().size(), 1);
  EXPECT_EQ(log.CriticalErrors()[0].path.AsString(), "printer-attributes");
  EXPECT_EQ(log.CriticalErrors()[0].code, ParserCode::kGroupTagWasExpected);
}

// Parsing the frame with a single dateTime attribute. The attribute has three
// values: the first one has incorrect size, the second one is correct, the last
// one is of type different than dateTime.
TEST(Parser, DateTime) {
  BinaryContent c;
  c.u2(0x0101u);     // version-number = 1.1
  c.u2(0x0000u);     // status-code = successful-ok
  c.u4(1);           // request-id
  c.u1(0x01u);       // group-tag = operation-attributes-tag
  c.u1(0x31);        // value-tag = dateTime
  c.u2(9);           // name-length
  c.s("test-attr");  // name
  c.u2(4);           // value-length
  c.u4(123456789);   // value
  c.u1(0x31);        // value-tag = dateTime
  c.u2(0);           // name-length = 0 (next value in the same attribute)
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
  c.u1(0x44);        // value-tag = keyword
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(8);           // value-length
  c.s("whatever");   // value
  c.u1(0x03u);       // end-of-attributes-tag

  ipp::SimpleParserLog log;
  const ipp::Frame frame = ipp::Parse(c.data.data(), c.data.size(), log);
  ASSERT_EQ(log.Errors().size(), 2);
  EXPECT_EQ(log.Errors()[0].code, ipp::ParserCode::kValueInvalidSize);
  EXPECT_EQ(log.Errors()[1].code, ipp::ParserCode::kValueMismatchTagOmitted);

  ASSERT_EQ(frame.Groups(ipp::GroupTag::operation_attributes).size(), 1);
  const ipp::Collection& coll =
      frame.Groups(ipp::GroupTag::operation_attributes)[0];
  ASSERT_EQ(coll.size(), 1);
  EXPECT_EQ(coll.begin()->Name(), "test-attr");
  EXPECT_EQ(coll.begin()->Tag(), ipp::ValueTag::dateTime);
  EXPECT_EQ(coll.begin()->Size(), 1);

  ipp::DateTime value;
  ASSERT_EQ(coll.begin()->GetValue(0, value), ipp::Code::kOK);
  EXPECT_EQ(value.year, 2002);
  EXPECT_EQ(value.month, 1);
  EXPECT_EQ(value.day, 23);
  EXPECT_EQ(value.hour, 4);
  EXPECT_EQ(value.minutes, 56);
  EXPECT_EQ(value.seconds, 7);
  EXPECT_EQ(value.deci_seconds, 8);
  EXPECT_EQ(value.UTC_direction, '+');
  EXPECT_EQ(value.UTC_hours, 9);
  EXPECT_EQ(value.UTC_minutes, 10);
}

// Parsing the frame with a single resolution attribute. The attribute has three
// values: the first one has incorrect size, the second one is correct, the last
// one is of type different than resolution.
TEST(Parser, Resolution) {
  BinaryContent c;
  c.u2(0x0101u);     // version-number = 1.1
  c.u2(0x0000u);     // status-code = successful-ok
  c.u4(1);           // request-id
  c.u1(0x01u);       // group-tag = operation-attributes-tag
  c.u1(0x32);        // value-tag = resolution
  c.u2(9);           // name-length
  c.s("test-attr");  // name
  c.u2(4);           // value-length
  c.u4(123456789);   // value
  c.u1(0x32);        // value-tag = resolution
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(9);           // value-length
  c.u4(12345);       // resolution.cros-feed-direction
  c.u4(67890);       // resolution.feed-direction
  c.u1(3);           // resolution.units = dots-per-inch
  c.u1(0x44);        // value-tag = keyword
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(8);           // value-length
  c.s("whatever");   // value
  c.u1(0x03u);       // end-of-attributes-tag

  ipp::SimpleParserLog log;
  const ipp::Frame frame = ipp::Parse(c.data.data(), c.data.size(), log);
  ASSERT_EQ(log.Errors().size(), 2);
  EXPECT_EQ(log.Errors()[0].code, ipp::ParserCode::kValueInvalidSize);
  EXPECT_EQ(log.Errors()[1].code, ipp::ParserCode::kValueMismatchTagOmitted);

  ASSERT_EQ(frame.Groups(ipp::GroupTag::operation_attributes).size(), 1);
  const ipp::Collection& coll =
      frame.Groups(ipp::GroupTag::operation_attributes)[0];
  ASSERT_EQ(coll.size(), 1);
  EXPECT_EQ(coll.begin()->Name(), "test-attr");
  EXPECT_EQ(coll.begin()->Tag(), ipp::ValueTag::resolution);
  EXPECT_EQ(coll.begin()->Size(), 1);

  ipp::Resolution value;
  ASSERT_EQ(coll.begin()->GetValue(0, value), ipp::Code::kOK);
  EXPECT_EQ(value.xres, 12345);
  EXPECT_EQ(value.yres, 67890);
  EXPECT_EQ(value.units, ipp::Resolution::Units::kDotsPerInch);
}

// Parsing the frame with a single rangeOfInteger attribute. The attribute has
// four values: the first one has incorrect size, the second one is correct, the
// third one is of type integer and is silently converted to rangeOfInteger, the
// last one is of type different than rangeOfInteger and integer.
TEST(Parser, RangeOfInteger) {
  BinaryContent c;
  c.u2(0x0101u);     // version-number = 1.1
  c.u2(0x0000u);     // status-code = successful-ok
  c.u4(1);           // request-id
  c.u1(0x01u);       // group-tag = operation-attributes-tag
  c.u1(0x33);        // value-tag = rangeOfInteger
  c.u2(9);           // name-length
  c.s("test-attr");  // name
  c.u2(4);           // value-length
  c.u4(123456789);   // value
  c.u1(0x33);        // value-tag = rangeOfInteger
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(8);           // value-length
  c.u4(12345);       // rangeOfInteger.lower-bound
  c.u4(67890);       // rangeOfInteger.upper-bound
  c.u1(0x21);        // value-tag = integer
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(4);           // value-length
  c.u4(1234567890);  // value
  c.u1(0x44);        // value-tag = keyword
  c.u2(0);           // name-length = 0 (next value in the same attribute)
  c.u2(8);           // value-length
  c.s("whatever");   // value
  c.u1(0x03u);       // end-of-attributes-tag

  ipp::SimpleParserLog log;
  const ipp::Frame frame = ipp::Parse(c.data.data(), c.data.size(), log);
  ASSERT_EQ(log.Errors().size(), 2);
  EXPECT_EQ(log.Errors()[0].code, ipp::ParserCode::kValueInvalidSize);
  EXPECT_EQ(log.Errors()[1].code, ipp::ParserCode::kValueMismatchTagOmitted);

  ASSERT_EQ(frame.Groups(ipp::GroupTag::operation_attributes).size(), 1);
  const ipp::Collection& coll =
      frame.Groups(ipp::GroupTag::operation_attributes)[0];
  ASSERT_EQ(coll.size(), 1);
  EXPECT_EQ(coll.begin()->Name(), "test-attr");
  EXPECT_EQ(coll.begin()->Tag(), ipp::ValueTag::rangeOfInteger);
  EXPECT_EQ(coll.begin()->Size(), 2);

  ipp::RangeOfInteger value;
  ASSERT_EQ(coll.begin()->GetValue(0, value), ipp::Code::kOK);
  EXPECT_EQ(value.min_value, 12345);
  EXPECT_EQ(value.max_value, 67890);
  ASSERT_EQ(coll.begin()->GetValue(1, value), ipp::Code::kOK);
  EXPECT_EQ(value.min_value, 1234567890);
  EXPECT_EQ(value.max_value, 1234567890);
}

}  // namespace
}  // namespace ipp
