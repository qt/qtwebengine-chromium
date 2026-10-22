// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_LOGGING_H_
#define TOOLS_CDDL_LOGGING_H_

#include <string_view>

#define CHECK(condition) (condition) ? (void)0 : Logger::Abort(#condition)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))

namespace Logger {

// Writes a log to stderr.
void Log(std::string_view message);

// Writes an error to stderr.
void Error(std::string_view message);

// Aborts the program after logging the condition that caused the
// CHECK-failure.
[[noreturn]] void Abort(const char* condition);

}  // namespace Logger

#endif  // TOOLS_CDDL_LOGGING_H_
