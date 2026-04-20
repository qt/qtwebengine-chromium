// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "gn/exec_process.h"
#include "gn/standard_out.h"
#include "util/test/test.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace testing {
Test* g_current_test;

std::string Pretty(bool value) {
  return value ? "true" : "false";
}

std::string Indent(std::string_view value) {
  std::stringstream ss;
  ss << "  ";
  for (auto c : value) {
    switch (c) {
      case '\n':
        ss << c;
        ss << "  ";
        break;
      default:
        ss << c;
    }
  }
  return ss.str();
}

std::string DiffStrings(std::string_view expected, std::string_view actual) {
  if (expected == actual) {
    return "Different values with the same representation:\n" +
           std::string(expected);
  }
  base::ScopedTempDir temp_dir;
  auto fallback = [&]() {
    return base::StringPrintf(
        "Expected:\n%.*s\n\nActual:\n%.*s", static_cast<int>(expected.size()),
        expected.data(), static_cast<int>(actual.size()), actual.data());
  };
  if (!temp_dir.CreateUniqueTempDir()) {
    return fallback();
  }

  base::FilePath expected_path = temp_dir.GetPath().AppendASCII("expected.txt");
  base::FilePath actual_path = temp_dir.GetPath().AppendASCII("actual.txt");

  if (base::WriteFile(expected_path, expected.data(), expected.size()) !=
      static_cast<int>(expected.size())) {
    return fallback();
  }
  if (base::WriteFile(actual_path, actual.data(), actual.size()) !=
      static_cast<int>(actual.size())) {
    return fallback();
  }

  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.SetParseSwitches(false);
  cmdline.SetProgram(base::FilePath(FILE_PATH_LITERAL("git")));
  cmdline.AppendArg("diff");
  cmdline.AppendArg("--no-index");
  if (::IsColorEnabled()) {
    cmdline.AppendArg("--color");
  }
  cmdline.AppendArgPath(expected_path);
  cmdline.AppendArgPath(actual_path);

  std::string output;
  std::string stderr_output;
  int exit_code = 0;
  ::internal::ExecProcess(cmdline, base::FilePath(FILE_PATH_LITERAL(".")),
                          &output, &stderr_output, &exit_code);
  if (output.empty()) {
    return fallback();
  }
  return output;
}

}  // namespace testing

struct RegisteredTest {
  testing::Test* (*factory)();
  const char* name;
  bool should_run;
};

// This can't be a vector because tests call RegisterTest from static
// initializers and the order static initializers run it isn't specified. So
// the vector constructor isn't guaranteed to run before all of the
// RegisterTest() calls.
static RegisteredTest tests[10000];
static int ntests;

void RegisterTest(testing::Test* (*factory)(), const char* name) {
  tests[ntests].factory = factory;
  tests[ntests++].name = name;
}

namespace {

bool PatternMatchesString(const char* pattern, const char* str) {
  switch (*pattern) {
    case '\0':
    case '-':
    case ':':
      return *str == '\0';
    case '*':
      return (*str != '\0' && PatternMatchesString(pattern, str + 1)) ||
             PatternMatchesString(pattern + 1, str);
    default:
      return *pattern == *str && PatternMatchesString(pattern + 1, str + 1);
  }
}

bool PatternListMatchString(const char* pattern, const char* str) {
  const char* const colon = strchr(pattern, ':');
  if (PatternMatchesString(pattern, str))
    return true;

  if (!colon)
    return false;

  return PatternListMatchString(colon + 1, str);
}

bool TestMatchesFilter(const char* test, const char* filter) {
  // Split --gtest_filter at '-' into positive and negative filters.
  const char* const dash = strchr(filter, '-');
  const char* pos =
      dash == filter ? "*" : filter;  // Treat '-test1' as '*-test1'
  const char* neg = dash ? dash + 1 : "";
  return PatternListMatchString(pos, test) &&
         !PatternListMatchString(neg, test);
}

#if defined(OS_WIN)
struct ScopedEnableVTEscapeProcessing {
  ScopedEnableVTEscapeProcessing() {
    console_ = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(console_, &csbi) &&
        GetConsoleMode(console_, &original_mode_)) {
      SetConsoleMode(console_, original_mode_ |
                                   ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                   DISABLE_NEWLINE_AUTO_RETURN);
    } else {
      console_ = INVALID_HANDLE_VALUE;
    }
  }

  ~ScopedEnableVTEscapeProcessing() {
    if (is_valid())
      SetConsoleMode(console_, original_mode_);
  }

  bool is_valid() const { return console_ != INVALID_HANDLE_VALUE; }

  HANDLE console_;
  DWORD original_mode_;
};
#endif

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

#if defined(OS_WIN)
  ScopedEnableVTEscapeProcessing enable_vt_processing;
#endif
  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

  int tests_started = 0;

  const char* test_filter = "*";
  for (int i = 1; i < argc; ++i) {
    const char kTestFilterPrefix[] = "--gtest_filter=";
    if (strncmp(argv[i], kTestFilterPrefix, strlen(kTestFilterPrefix)) == 0) {
      test_filter = &argv[i][strlen(kTestFilterPrefix)];
    }
  }

  int num_active_tests = 0;
  for (int i = 0; i < ntests; i++) {
    tests[i].should_run = TestMatchesFilter(tests[i].name, test_filter);
    if (tests[i].should_run) {
      ++num_active_tests;
    }
  }

  const char* prefix = "";
  const char* suffix = "\n";
#if defined(OS_WIN)
  if (enable_vt_processing.is_valid())
#else
  // When run from Xcode, the console returns "true" to isatty(1) but it
  // does not interprets ANSI escape sequence resulting in difficult to
  // read output. There is no portable way to detect if the console is
  // Xcode's console (term is set to xterm or xterm-256colors) but Xcode
  // sets the __XCODE_BUILT_PRODUCTS_DIR_PATHS environment variable. Use
  // this as a proxy to detect that the console does not interpret the
  // ANSI sequences correctly.
  if (isatty(1) && getenv("__XCODE_BUILT_PRODUCTS_DIR_PATHS") == NULL)
#endif
  {
    prefix = "\r";
    suffix = "\x1B[K";
  }
  bool passed = true;
  for (int i = 0; i < ntests; i++) {
    if (!tests[i].should_run)
      continue;

    ++tests_started;
    testing::Test* test = tests[i].factory();
    printf("%s[%d/%d] %s%s", prefix, tests_started, num_active_tests,
           tests[i].name, suffix);
    test->SetUp();
    test->Run();
    test->TearDown();
    if (test->Failed())
      passed = false;
    delete test;
  }

  printf("\n%s\n", passed ? "PASSED" : "FAILED");
  fflush(stdout);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
