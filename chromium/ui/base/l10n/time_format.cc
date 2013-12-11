// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/time_format.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ui_strings.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/plurfmt.h"
#include "third_party/icu/source/i18n/unicode/plurrule.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_plurals.h"

using base::Time;
using base::TimeDelta;

namespace {

static const char kFallbackFormatSuffixShort[] = "}";
static const char kFallbackFormatSuffixLeft[] = " left}";
static const char kFallbackFormatSuffixAgo[] = " ago}";

// Contains message IDs for various time units and pluralities.
struct MessageIDs {
  // There are 4 different time units and 6 different pluralities.
  int ids[4][6];
};

// Message IDs for different time formats.
static const MessageIDs kTimeShortMessageIDs = { {
  {
    IDS_TIME_SECS_DEFAULT, IDS_TIME_SEC_SINGULAR, IDS_TIME_SECS_ZERO,
    IDS_TIME_SECS_TWO, IDS_TIME_SECS_FEW, IDS_TIME_SECS_MANY
  },
  {
    IDS_TIME_MINS_DEFAULT, IDS_TIME_MIN_SINGULAR, IDS_TIME_MINS_ZERO,
    IDS_TIME_MINS_TWO, IDS_TIME_MINS_FEW, IDS_TIME_MINS_MANY
  },
  {
    IDS_TIME_HOURS_DEFAULT, IDS_TIME_HOUR_SINGULAR, IDS_TIME_HOURS_ZERO,
    IDS_TIME_HOURS_TWO, IDS_TIME_HOURS_FEW, IDS_TIME_HOURS_MANY
  },
  {
    IDS_TIME_DAYS_DEFAULT, IDS_TIME_DAY_SINGULAR, IDS_TIME_DAYS_ZERO,
    IDS_TIME_DAYS_TWO, IDS_TIME_DAYS_FEW, IDS_TIME_DAYS_MANY
  }
} };

static const MessageIDs kTimeRemainingMessageIDs = { {
  {
    IDS_TIME_REMAINING_SECS_DEFAULT, IDS_TIME_REMAINING_SEC_SINGULAR,
    IDS_TIME_REMAINING_SECS_ZERO, IDS_TIME_REMAINING_SECS_TWO,
    IDS_TIME_REMAINING_SECS_FEW, IDS_TIME_REMAINING_SECS_MANY
  },
  {
    IDS_TIME_REMAINING_MINS_DEFAULT, IDS_TIME_REMAINING_MIN_SINGULAR,
    IDS_TIME_REMAINING_MINS_ZERO, IDS_TIME_REMAINING_MINS_TWO,
    IDS_TIME_REMAINING_MINS_FEW, IDS_TIME_REMAINING_MINS_MANY
  },
  {
    IDS_TIME_REMAINING_HOURS_DEFAULT, IDS_TIME_REMAINING_HOUR_SINGULAR,
    IDS_TIME_REMAINING_HOURS_ZERO, IDS_TIME_REMAINING_HOURS_TWO,
    IDS_TIME_REMAINING_HOURS_FEW, IDS_TIME_REMAINING_HOURS_MANY
  },
  {
    IDS_TIME_REMAINING_DAYS_DEFAULT, IDS_TIME_REMAINING_DAY_SINGULAR,
    IDS_TIME_REMAINING_DAYS_ZERO, IDS_TIME_REMAINING_DAYS_TWO,
    IDS_TIME_REMAINING_DAYS_FEW, IDS_TIME_REMAINING_DAYS_MANY
  }
} };

static const MessageIDs kTimeRemainingLongMessageIDs = { {
  {
    IDS_TIME_REMAINING_SECS_DEFAULT, IDS_TIME_REMAINING_SEC_SINGULAR,
    IDS_TIME_REMAINING_SECS_ZERO, IDS_TIME_REMAINING_SECS_TWO,
    IDS_TIME_REMAINING_SECS_FEW, IDS_TIME_REMAINING_SECS_MANY
  },
  {
    IDS_TIME_REMAINING_LONG_MINS_DEFAULT, IDS_TIME_REMAINING_LONG_MIN_SINGULAR,
    IDS_TIME_REMAINING_LONG_MINS_ZERO, IDS_TIME_REMAINING_LONG_MINS_TWO,
    IDS_TIME_REMAINING_LONG_MINS_FEW, IDS_TIME_REMAINING_LONG_MINS_MANY
  },
  {
    IDS_TIME_REMAINING_HOURS_DEFAULT, IDS_TIME_REMAINING_HOUR_SINGULAR,
    IDS_TIME_REMAINING_HOURS_ZERO, IDS_TIME_REMAINING_HOURS_TWO,
    IDS_TIME_REMAINING_HOURS_FEW, IDS_TIME_REMAINING_HOURS_MANY
  },
  {
    IDS_TIME_REMAINING_DAYS_DEFAULT, IDS_TIME_REMAINING_DAY_SINGULAR,
    IDS_TIME_REMAINING_DAYS_ZERO, IDS_TIME_REMAINING_DAYS_TWO,
    IDS_TIME_REMAINING_DAYS_FEW, IDS_TIME_REMAINING_DAYS_MANY
  }
} };

static const MessageIDs kTimeDurationLongMessageIDs = { {
  {
    IDS_TIME_DURATION_LONG_SECS_DEFAULT, IDS_TIME_DURATION_LONG_SEC_SINGULAR,
    IDS_TIME_DURATION_LONG_SECS_ZERO, IDS_TIME_DURATION_LONG_SECS_TWO,
    IDS_TIME_DURATION_LONG_SECS_FEW, IDS_TIME_DURATION_LONG_SECS_MANY
  },
  {
    IDS_TIME_DURATION_LONG_MINS_DEFAULT, IDS_TIME_DURATION_LONG_MIN_SINGULAR,
    IDS_TIME_DURATION_LONG_MINS_ZERO, IDS_TIME_DURATION_LONG_MINS_TWO,
    IDS_TIME_DURATION_LONG_MINS_FEW, IDS_TIME_DURATION_LONG_MINS_MANY
  },
  {
    IDS_TIME_HOURS_DEFAULT, IDS_TIME_HOUR_SINGULAR,
    IDS_TIME_HOURS_ZERO, IDS_TIME_HOURS_TWO,
    IDS_TIME_HOURS_FEW, IDS_TIME_HOURS_MANY
  },
  {
    IDS_TIME_DAYS_DEFAULT, IDS_TIME_DAY_SINGULAR,
    IDS_TIME_DAYS_ZERO, IDS_TIME_DAYS_TWO,
    IDS_TIME_DAYS_FEW, IDS_TIME_DAYS_MANY
  }
} };

static const MessageIDs kTimeElapsedMessageIDs = { {
  {
    IDS_TIME_ELAPSED_SECS_DEFAULT, IDS_TIME_ELAPSED_SEC_SINGULAR,
    IDS_TIME_ELAPSED_SECS_ZERO, IDS_TIME_ELAPSED_SECS_TWO,
    IDS_TIME_ELAPSED_SECS_FEW, IDS_TIME_ELAPSED_SECS_MANY
  },
  {
    IDS_TIME_ELAPSED_MINS_DEFAULT, IDS_TIME_ELAPSED_MIN_SINGULAR,
    IDS_TIME_ELAPSED_MINS_ZERO, IDS_TIME_ELAPSED_MINS_TWO,
    IDS_TIME_ELAPSED_MINS_FEW, IDS_TIME_ELAPSED_MINS_MANY
  },
  {
    IDS_TIME_ELAPSED_HOURS_DEFAULT, IDS_TIME_ELAPSED_HOUR_SINGULAR,
    IDS_TIME_ELAPSED_HOURS_ZERO, IDS_TIME_ELAPSED_HOURS_TWO,
    IDS_TIME_ELAPSED_HOURS_FEW, IDS_TIME_ELAPSED_HOURS_MANY
  },
  {
    IDS_TIME_ELAPSED_DAYS_DEFAULT, IDS_TIME_ELAPSED_DAY_SINGULAR,
    IDS_TIME_ELAPSED_DAYS_ZERO, IDS_TIME_ELAPSED_DAYS_TWO,
    IDS_TIME_ELAPSED_DAYS_FEW, IDS_TIME_ELAPSED_DAYS_MANY
  }
} };

// Different format types.
enum FormatType {
  FORMAT_SHORT,
  FORMAT_REMAINING,
  FORMAT_REMAINING_LONG,
  FORMAT_DURATION_LONG,
  FORMAT_ELAPSED,
};

class TimeFormatter {
  public:
    const std::vector<icu::PluralFormat*>& formatter(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return short_formatter_.get();
        case FORMAT_REMAINING:
          return time_left_formatter_.get();
        case FORMAT_REMAINING_LONG:
          return time_left_long_formatter_.get();
        case FORMAT_DURATION_LONG:
          return time_duration_long_formatter_.get();
        case FORMAT_ELAPSED:
          return time_elapsed_formatter_.get();
        default:
          NOTREACHED();
          return short_formatter_.get();
      }
    }
  private:
    static const MessageIDs& GetMessageIDs(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kTimeShortMessageIDs;
        case FORMAT_REMAINING:
          return kTimeRemainingMessageIDs;
        case FORMAT_REMAINING_LONG:
          return kTimeRemainingLongMessageIDs;
        case FORMAT_DURATION_LONG:
          return kTimeDurationLongMessageIDs;
        case FORMAT_ELAPSED:
          return kTimeElapsedMessageIDs;
        default:
          NOTREACHED();
          return kTimeShortMessageIDs;
      }
    }

    static const char* GetFallbackFormatSuffix(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kFallbackFormatSuffixShort;
        case FORMAT_REMAINING:
        case FORMAT_REMAINING_LONG:
          return kFallbackFormatSuffixLeft;
        case FORMAT_ELAPSED:
          return kFallbackFormatSuffixAgo;
        default:
          NOTREACHED();
          return kFallbackFormatSuffixShort;
      }
    }

    TimeFormatter() {
      BuildFormats(FORMAT_SHORT, &short_formatter_);
      BuildFormats(FORMAT_REMAINING, &time_left_formatter_);
      BuildFormats(FORMAT_REMAINING_LONG, &time_left_long_formatter_);
      BuildFormats(FORMAT_DURATION_LONG, &time_duration_long_formatter_);
      BuildFormats(FORMAT_ELAPSED, &time_elapsed_formatter_);
    }
    ~TimeFormatter() {
    }
    friend struct base::DefaultLazyInstanceTraits<TimeFormatter>;

    ScopedVector<icu::PluralFormat> short_formatter_;
    ScopedVector<icu::PluralFormat> time_left_formatter_;
    ScopedVector<icu::PluralFormat> time_left_long_formatter_;
    ScopedVector<icu::PluralFormat> time_duration_long_formatter_;
    ScopedVector<icu::PluralFormat> time_elapsed_formatter_;
    static void BuildFormats(FormatType format_type,
                             ScopedVector<icu::PluralFormat>* time_formats);
    static icu::PluralFormat* createFallbackFormat(
        const icu::PluralRules& rules, int index, FormatType format_type);

    DISALLOW_COPY_AND_ASSIGN(TimeFormatter);
};

static base::LazyInstance<TimeFormatter> g_time_formatter =
    LAZY_INSTANCE_INITIALIZER;

void TimeFormatter::BuildFormats(
    FormatType format_type, ScopedVector<icu::PluralFormat>* time_formats) {
  const MessageIDs& message_ids = GetMessageIDs(format_type);

  for (int i = 0; i < 4; ++i) {
    icu::UnicodeString pattern;
    std::vector<int> ids;
    for (size_t j = 0; j < arraysize(message_ids.ids[i]); ++j) {
      ids.push_back(message_ids.ids[i][j]);
    }
    scoped_ptr<icu::PluralFormat> format = l10n_util::BuildPluralFormat(ids);
    if (format) {
      time_formats->push_back(format.release());
    } else {
      scoped_ptr<icu::PluralRules> rules(l10n_util::BuildPluralRules());
      time_formats->push_back(createFallbackFormat(*rules, i, format_type));
    }
  }
}

// Create a hard-coded fallback plural format. This will never be called
// unless translators make a mistake.
icu::PluralFormat* TimeFormatter::createFallbackFormat(
    const icu::PluralRules& rules, int index, FormatType format_type) {
  const icu::UnicodeString kUnits[4][2] = {
    { UNICODE_STRING_SIMPLE("sec"), UNICODE_STRING_SIMPLE("secs") },
    { UNICODE_STRING_SIMPLE("min"), UNICODE_STRING_SIMPLE("mins") },
    { UNICODE_STRING_SIMPLE("hour"), UNICODE_STRING_SIMPLE("hours") },
    { UNICODE_STRING_SIMPLE("day"), UNICODE_STRING_SIMPLE("days") }
  };
  icu::UnicodeString suffix(GetFallbackFormatSuffix(format_type), -1, US_INV);
  icu::UnicodeString pattern;
  if (rules.isKeyword(UNICODE_STRING_SIMPLE("one"))) {
    pattern += UNICODE_STRING_SIMPLE("one{# ") + kUnits[index][0] + suffix;
  }
  pattern += UNICODE_STRING_SIMPLE(" other{# ") + kUnits[index][1] + suffix;
  UErrorCode err = U_ZERO_ERROR;
  icu::PluralFormat* format = new icu::PluralFormat(rules, pattern, err);
  DCHECK(U_SUCCESS(err));
  return format;
}

string16 FormatTimeImpl(const TimeDelta& delta, FormatType format_type) {
  if (delta.ToInternalValue() < 0) {
    NOTREACHED() << "Negative duration";
    return string16();
  }

  int number;

  const std::vector<icu::PluralFormat*>& formatters =
    g_time_formatter.Get().formatter(format_type);

  UErrorCode error = U_ZERO_ERROR;
  icu::UnicodeString time_string;
  // Less than a minute gets "X seconds left"
  if (delta.ToInternalValue() < Time::kMicrosecondsPerMinute) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerSecond);
    time_string = formatters[0]->format(number, error);

  // Less than 1 hour gets "X minutes left".
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerHour) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerMinute);
    time_string = formatters[1]->format(number, error);

  // Less than 1 day remaining gets "X hours left"
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerDay) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerHour);
    time_string = formatters[2]->format(number, error);

  // Anything bigger gets "X days left"
  } else {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerDay);
    time_string = formatters[3]->format(number, error);
  }

  // With the fallback added, this should never fail.
  DCHECK(U_SUCCESS(error));
  int capacity = time_string.length() + 1;
  DCHECK_GT(capacity, 1);
  string16 result;
  time_string.extract(static_cast<UChar*>(WriteInto(&result, capacity)),
                      capacity, error);
  DCHECK(U_SUCCESS(error));
  return result;
}

}  // namespace

namespace ui {

// static
string16 TimeFormat::TimeElapsed(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_ELAPSED);
}

// static
string16 TimeFormat::TimeRemaining(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_REMAINING);
}

// static
string16 TimeFormat::TimeRemainingLong(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_REMAINING_LONG);
}

// static
string16 TimeFormat::TimeRemainingShort(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_SHORT);
}

// static
string16 TimeFormat::TimeDurationLong(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_DURATION_LONG);
}

// static
string16 TimeFormat::RelativeDate(
    const Time& time,
    const Time* optional_midnight_today) {
  Time midnight_today = optional_midnight_today ? *optional_midnight_today :
      Time::Now().LocalMidnight();
  TimeDelta day = TimeDelta::FromMicroseconds(Time::kMicrosecondsPerDay);
  Time tomorrow = midnight_today + day;
  Time yesterday = midnight_today - day;
  if (time >= tomorrow)
    return string16();
  else if (time >= midnight_today)
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_TODAY);
  else if (time >= yesterday)
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_YESTERDAY);
  return string16();
}

}  // namespace ui
