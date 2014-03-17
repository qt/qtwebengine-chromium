// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/time_util.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace google_apis {
namespace util {

namespace {

const char kNullTimeString[] = "null";

bool ParseTimezone(const base::StringPiece& timezone,
                   bool ahead,
                   int* out_offset_to_utc_in_minutes) {
  DCHECK(out_offset_to_utc_in_minutes);

  std::vector<base::StringPiece> parts;
  int num_of_token = Tokenize(timezone, ":", &parts);

  int hour = 0;
  if (!base::StringToInt(parts[0], &hour))
    return false;

  int minute = 0;
  if (num_of_token > 1 && !base::StringToInt(parts[1], &minute))
    return false;

  *out_offset_to_utc_in_minutes = (hour * 60 + minute) * (ahead ? +1 : -1);
  return true;
}

}  // namespace

bool GetTimeFromString(const base::StringPiece& raw_value,
                       base::Time* parsed_time) {
  base::StringPiece date;
  base::StringPiece time_and_tz;
  base::StringPiece time;
  base::Time::Exploded exploded = {0};
  bool has_timezone = false;
  int offset_to_utc_in_minutes = 0;

  // Splits the string into "date" part and "time" part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(raw_value, "T", &parts) != 2)
      return false;
    date = parts[0];
    time_and_tz = parts[1];
  }

  // Parses timezone suffix on the time part if available.
  {
    std::vector<base::StringPiece> parts;
    if (time_and_tz[time_and_tz.size() - 1] == 'Z') {
      // Timezone is 'Z' (UTC)
      has_timezone = true;
      offset_to_utc_in_minutes = 0;
      time = time_and_tz;
      time.remove_suffix(1);
    } else if (Tokenize(time_and_tz, "+", &parts) == 2) {
      // Timezone is "+hh:mm" format
      if (!ParseTimezone(parts[1], true, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else if (Tokenize(time_and_tz, "-", &parts) == 2) {
      // Timezone is "-hh:mm" format
      if (!ParseTimezone(parts[1], false, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else {
      // No timezone (uses local timezone)
      time = time_and_tz;
    }
  }

  // Parses the date part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(date, "-", &parts) != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Parses the time part.
  {
    std::vector<base::StringPiece> parts;
    int num_of_token = Tokenize(time, ":", &parts);
    if (num_of_token != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.hour) ||
        !base::StringToInt(parts[1], &exploded.minute)) {
      return false;
    }

    std::vector<base::StringPiece> seconds_parts;
    int num_of_seconds_token = Tokenize(parts[2], ".", &seconds_parts);
    if (num_of_seconds_token >= 3)
      return false;

    if (!base::StringToInt(seconds_parts[0], &exploded.second))
        return false;

    // Only accept milli-seconds (3-digits).
    if (num_of_seconds_token > 1 &&
        seconds_parts[1].length() == 3 &&
        !base::StringToInt(seconds_parts[1], &exploded.millisecond)) {
      return false;
    }
  }

  exploded.day_of_week = 0;
  if (!exploded.HasValidValues())
    return false;

  if (has_timezone) {
    *parsed_time = base::Time::FromUTCExploded(exploded);
    if (offset_to_utc_in_minutes != 0)
      *parsed_time -= base::TimeDelta::FromMinutes(offset_to_utc_in_minutes);
  } else {
    *parsed_time = base::Time::FromLocalExploded(exploded);
  }

  return true;
}

std::string FormatTimeAsString(const base::Time& time) {
  if (time.is_null())
    return kNullTimeString;

  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

std::string FormatTimeAsStringLocaltime(const base::Time& time) {
  if (time.is_null())
    return kNullTimeString;

  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

}  // namespace util
}  // namespace google_apis
