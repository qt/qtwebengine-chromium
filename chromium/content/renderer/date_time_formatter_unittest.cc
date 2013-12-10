// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "content/renderer/date_time_formatter.h"
#include "content/renderer/renderer_date_time_picker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class RendererDateTimePickerTest {
};

TEST(RendererDateTimePickerTest, TestParserValidStringInputs) {
  WebKit::WebDateTimeChooserParams params;
  params.currentValue = "2010-07";
  params.type = WebKit::WebDateTimeInputTypeMonth;
  DateTimeFormatter sut(params);
  EXPECT_EQ(2010, sut.GetYear());

  // Month field is 0 based
  EXPECT_EQ(6, sut.GetMonth());

  // Month input defaults to the first day of the month (1 based)
  EXPECT_EQ(1, sut.GetDay());
  EXPECT_EQ(0, sut.GetHour());
  EXPECT_EQ(0, sut.GetMinute());
  EXPECT_EQ(0, sut.GetSecond());
  EXPECT_EQ(2010, sut.GetWeekYear());
  EXPECT_EQ(26, sut.GetWeek());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_MONTH, sut.GetType());

  params.currentValue = "2012-05-25";
  params.type = WebKit::WebDateTimeInputTypeDate;
  DateTimeFormatter sut2(params);
  EXPECT_EQ(2012, sut2.GetYear());
  EXPECT_EQ(4, sut2.GetMonth());
  EXPECT_EQ(25, sut2.GetDay());
  EXPECT_EQ(0, sut2.GetHour());
  EXPECT_EQ(0, sut2.GetMinute());
  EXPECT_EQ(0, sut2.GetSecond());
  EXPECT_EQ(2012, sut2.GetWeekYear());
  EXPECT_EQ(21, sut2.GetWeek());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_DATE, sut2.GetType());

  params.currentValue = "2013-05-21T12:15";
  params.type = WebKit::WebDateTimeInputTypeDateTimeLocal;
  DateTimeFormatter sut3(params);
  EXPECT_EQ(2013, sut3.GetYear());
  EXPECT_EQ(4, sut3.GetMonth());
  EXPECT_EQ(21, sut3.GetDay());
  EXPECT_EQ(12, sut3.GetHour());
  EXPECT_EQ(15, sut3.GetMinute());
  EXPECT_EQ(0, sut3.GetSecond());
  EXPECT_EQ(2013, sut3.GetWeekYear());
  EXPECT_EQ(21, sut3.GetWeek());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL, sut3.GetType());

  params.currentValue = "2013-W15";
  params.type = WebKit::WebDateTimeInputTypeWeek;
  DateTimeFormatter sut4(params);
  EXPECT_EQ(2013, sut4.GetYear());
  EXPECT_EQ(3, sut4.GetMonth());
  EXPECT_EQ(7, sut4.GetDay());
  EXPECT_EQ(0, sut4.GetHour());
  EXPECT_EQ(0, sut4.GetMinute());
  EXPECT_EQ(0, sut4.GetSecond());
  EXPECT_EQ(2013, sut4.GetWeekYear());
  EXPECT_EQ(15, sut4.GetWeek());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_WEEK, sut4.GetType());

  params.currentValue = "12:15";
  params.type = WebKit::WebDateTimeInputTypeTime;
  DateTimeFormatter sut5(params);
  EXPECT_EQ(12, sut5.GetHour());
  EXPECT_EQ(15, sut5.GetMinute());
  EXPECT_EQ(0, sut5.GetSecond());
  EXPECT_EQ(0, sut5.GetMilli());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TIME, sut5.GetType());

  params.currentValue = "12:15:02";
  params.type = WebKit::WebDateTimeInputTypeTime;
  DateTimeFormatter sut6(params);
  EXPECT_EQ(12, sut6.GetHour());
  EXPECT_EQ(15, sut6.GetMinute());
  EXPECT_EQ(02, sut6.GetSecond());
  EXPECT_EQ(0, sut6.GetMilli());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TIME, sut6.GetType());

  params.currentValue = "12:15:02.1";
  params.type = WebKit::WebDateTimeInputTypeTime;
  DateTimeFormatter sut7(params);
  EXPECT_EQ(12, sut7.GetHour());
  EXPECT_EQ(15, sut7.GetMinute());
  EXPECT_EQ(02, sut7.GetSecond());
  EXPECT_EQ(100, sut7.GetMilli());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TIME, sut7.GetType());

  params.currentValue = "12:15:02.123";
  params.type = WebKit::WebDateTimeInputTypeTime;
  DateTimeFormatter sut8(params);
  EXPECT_EQ(12, sut8.GetHour());
  EXPECT_EQ(15, sut8.GetMinute());
  EXPECT_EQ(02, sut8.GetSecond());
  EXPECT_EQ(123, sut8.GetMilli());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TIME, sut8.GetType());
}


TEST(RendererDateTimePickerTest, TestParserInvalidStringInputs) {

  // Random non parsable text
  WebKit::WebDateTimeChooserParams params;
  params.currentValue = "<script injection";
  params.type = WebKit::WebDateTimeInputTypeMonth;
  DateTimeFormatter sut(params);
  EXPECT_EQ(0, sut.GetYear());
  EXPECT_EQ(0, sut.GetMonth());
  EXPECT_EQ(0, sut.GetDay());
  EXPECT_EQ(0, sut.GetHour());
  EXPECT_EQ(0, sut.GetMinute());
  EXPECT_EQ(0, sut.GetSecond());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, sut.GetType());

  // unimplemented type
  params.currentValue = "week 23";
  params.type = WebKit::WebDateTimeInputTypeWeek; // Not implemented
  DateTimeFormatter sut2(params);
  EXPECT_EQ(0, sut2.GetYear());
  EXPECT_EQ(0, sut2.GetMonth());
  EXPECT_EQ(0, sut2.GetDay());
  EXPECT_EQ(0, sut2.GetHour());
  EXPECT_EQ(0, sut2.GetMinute());
  EXPECT_EQ(0, sut2.GetSecond());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, sut2.GetType());

  // type is a subset of pattern
  params.currentValue = "2012-05-25";
  params.type = WebKit::WebDateTimeInputTypeDateTimeLocal;
  DateTimeFormatter sut3(params);
  EXPECT_EQ(0, sut3.GetYear());
  EXPECT_EQ(0, sut3.GetMonth());
  EXPECT_EQ(0, sut3.GetDay());
  EXPECT_EQ(0, sut3.GetHour());
  EXPECT_EQ(0, sut3.GetMinute());
  EXPECT_EQ(0, sut3.GetSecond());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, sut3.GetType());

  // type is a superset of pattern
  params.currentValue = "2013-05-21T12:15";
  params.type = WebKit::WebDateTimeInputTypeMonth;
  DateTimeFormatter sut4(params);
  EXPECT_EQ(2013, sut4.GetYear());
  EXPECT_EQ(4, sut4.GetMonth());
  EXPECT_EQ(1, sut4.GetDay());
  EXPECT_EQ(0, sut4.GetHour());
  EXPECT_EQ(0, sut4.GetMinute());
  EXPECT_EQ(0, sut4.GetSecond());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_MONTH, sut4.GetType());
}


TEST(RendererDateTimePickerTest, TestParserValidDateInputs) {
  DateTimeFormatter sut(
      ui::TEXT_INPUT_TYPE_MONTH, 2012, 11, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("2012-12", sut.GetFormattedValue());

  DateTimeFormatter sut2(
      ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL, 2013, 3, 23, 15, 47, 0, 0, 0, 0);
  EXPECT_EQ("2013-04-23T15:47", sut2.GetFormattedValue());

  DateTimeFormatter sut3(
      ui::TEXT_INPUT_TYPE_WEEK, 0, 0, 0, 0, 0, 0, 0, 2012, 2);
  EXPECT_EQ("2012-W02", sut3.GetFormattedValue());
}

TEST(RendererDateTimePickerTest, TestParserValidTimeInputs) {
  DateTimeFormatter sut(
      ui::TEXT_INPUT_TYPE_TIME, 0, 0, 0, 12, 15, 0, 0, 0, 0);
  EXPECT_EQ("12:15", sut.GetFormattedValue());

  DateTimeFormatter sut2(
      ui::TEXT_INPUT_TYPE_TIME, 0, 0, 0, 12, 15, 02, 0, 0, 0);
  EXPECT_EQ("12:15:02", sut2.GetFormattedValue());

  DateTimeFormatter sut3(
      ui::TEXT_INPUT_TYPE_TIME, 0, 0, 0, 12, 15, 02, 123, 0, 0);
  EXPECT_EQ("12:15:02.123", sut3.GetFormattedValue());
}

TEST(RendererDateTimePickerTest, TestParserInvalidDateInputs) {
  DateTimeFormatter sut(ui::TEXT_INPUT_TYPE_WEEK, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("", sut.GetFormattedValue());

  DateTimeFormatter sut2(
      ui::TEXT_INPUT_TYPE_NONE, 2013, 3, 23, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("", sut2.GetFormattedValue());

  DateTimeFormatter sut3(
      ui::TEXT_INPUT_TYPE_NONE, 2013, 14, 32, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("", sut3.GetFormattedValue());

  DateTimeFormatter sut4(ui::TEXT_INPUT_TYPE_DATE, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("", sut4.GetFormattedValue());

  DateTimeFormatter sut5(ui::TEXT_INPUT_TYPE_TIME, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("", sut5.GetFormattedValue());

  DateTimeFormatter sut6(
      ui::TEXT_INPUT_TYPE_PASSWORD, 23, 0, 0, 0, 5, 0, 0, 0, 0);
  EXPECT_EQ("", sut6.GetFormattedValue());

  DateTimeFormatter sut7(ui::TEXT_INPUT_TYPE_MAX, 23, 0, 0, 0, 5, 0, 0, 0, 0);
  EXPECT_EQ("", sut7.GetFormattedValue());

  DateTimeFormatter sut8(
      static_cast<ui::TextInputType>(10000), 23, 0, 0, 0, 5, 0, 0, 0, 0);
  EXPECT_EQ("", sut8.GetFormattedValue());
}
} // namespace content
