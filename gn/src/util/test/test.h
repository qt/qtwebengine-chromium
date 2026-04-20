// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_TEST_TEST_H_
#define UTIL_TEST_TEST_H_

#include <string.h>

#include <concepts>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"

// This is a minimal googletest-like testing framework. It's originally derived
// from Ninja's src/test.h. You might prefer that one if you have different
// tradeoffs (in particular, if you don't need to stream message to assertion
// failures, Ninja's is a bit simpler.)
namespace testing {

class Test {
 public:
  Test() : failed_(false) {}
  virtual ~Test() {}
  virtual void SetUp() {}
  virtual void TearDown() {}
  virtual void Run() = 0;

  bool Failed() const { return failed_; }

 private:
  friend class TestResult;

  bool failed_;
};

extern testing::Test* g_current_test;

class TestResult {
 public:
  TestResult(bool condition, const char* error)
      : condition_(condition), error_(error) {
    if (!condition)
      g_current_test->failed_ = true;
  }

  operator bool() const { return condition_; }
  const char* error() const { return error_; }

 private:
  bool condition_;
  const char* error_;
};

class Message {
 public:
  Message() {}
  ~Message() { printf("%s\n\n", ss_.str().c_str()); }

  template <typename T>
  inline Message& operator<<(const T& val) {
    ss_ << val;
    return *this;
  }

 private:
  std::stringstream ss_;
};

class AssertHelper {
 public:
  AssertHelper(const char* file, int line, const TestResult& test_result)
      : file_(file), line_(line), error_(test_result.error()) {}

  void operator=(const Message& message) const {
    printf("\n*** FAILURE %s:%d: %s\n", file_, line_, error_);
  }

 private:
  const char* file_;
  int line_;
  const char* error_;
};

std::string Indent(std::string_view value);
std::string DiffStrings(std::string_view expected, std::string_view actual);

std::string Pretty(bool value);

// Explicitly write this for enum, because otherwise it tries to cast enums
// to bools.
template <typename T>
  requires std::is_enum_v<T>
std::string Pretty(T value) {
  return "Enum value " +
         std::to_string(static_cast<std::underlying_type_t<T>>(value));
}

template <typename T>
  requires std::is_convertible_v<T, std::string>
std::string Pretty(const T& value) {
  return value;
}

template <typename T>
  requires std::is_arithmetic_v<T>
std::string Pretty(T value) {
  return std::to_string(value);
}

// Order matters for these templates.
// The requires clause requires that the clause can already be met.
// The TLDR is that you must order them so that your inner types come first.
// For example, since we put T* before vector<T>, we support vector<T*> but not
// vector<T>*.

// Pointer and options should come first because we pretty much universally
// point to fixed types rather than templates.
template <typename T>
  requires requires(T t) { Pretty(t); }
std::string Pretty(const T* value) {
  if (value == nullptr) {
    return "NULL";
  }
  auto pretty = Pretty(*value);
  return base::StringPrintf("%p -> %.*s", value,
                            static_cast<int>(pretty.size()), pretty.data());
}

template <typename T>
  requires requires(T t) { Pretty(t); }
std::string Pretty(const std::unique_ptr<T>& value) {
  return "unique_ptr " + Pretty(value.get());
}

template <typename T>
  requires requires(T t) { Pretty(t); }
std::string Pretty(const std::optional<T>& value) {
  if (!value) {
    return "nullopt";
  }
  return "Optional(" + Pretty(*value) + ")";
}

// Containers of pairs are far more common than pairs of containers.
template <typename T, typename U>
  requires requires(T t) { Pretty(t); } && requires(U u) { Pretty(u); }
std::string Pretty(const std::pair<T, U>& value) {
  std::stringstream ss;
  ss << "Pair(\n";
  ss << Indent(Pretty(value.first)) << ",\n";
  ss << Indent(Pretty(value.second)) << ",\n";
  ss << ")";
  return ss.str();
}

// Containers are usually the outer layer, so they come last.
template <typename T>
  requires requires(T t) { Pretty(t); }
std::string Pretty(const std::vector<T>& value) {
  std::stringstream ss;
  ss << "[\n";
  for (const auto& v : value) {
    ss << Indent(Pretty(v)) << ",\n";
  }
  ss << "]";
  return ss.str();
}

template <typename T, typename U>
std::string TryDiffStrings(const T& expected, const U& actual) {
  if constexpr (requires {
                  Pretty(expected);
                  Pretty(actual);
                }) {
    auto expected_pretty = Pretty(expected);
    auto actual_pretty = Pretty(actual);
    // Ensure git diff doesn't complain about missing newlines.
    if (!expected_pretty.ends_with("\n") && !actual_pretty.ends_with("\n")) {
      return DiffStrings(expected_pretty + "\n", actual_pretty + "\n");
    }
    return DiffStrings(expected_pretty, actual_pretty);
  } else {
    return "";
  }
}

}  // namespace testing

void RegisterTest(testing::Test* (*)(), const char*);

#define TEST_F_(x, y, name)                                                    \
  struct y : public x {                                                        \
    static testing::Test* Create() { return testing::g_current_test = new y; } \
    virtual void Run();                                                        \
  };                                                                           \
  struct Register##y {                                                         \
    Register##y() { RegisterTest(y::Create, name); }                           \
  };                                                                           \
  Register##y g_register_##y;                                                  \
  void y::Run()

#define TEST_F(x, y) TEST_F_(x, x##y, #x "." #y)
#define TEST(x, y) TEST_F_(testing::Test, x##y, #x "." #y)

#define FRIEND_TEST(x, y) friend class x##y

// Some compilers emit a warning if nested "if" statements are followed by an
// "else" statement and braces are not used to explicitly disambiguate the
// "else" binding.  This leads to problems with code like:
//
//   if (something)
//     ASSERT_TRUE(condition) << "Some message";
#define TEST_AMBIGUOUS_ELSE_BLOCKER_ \
  switch (0)                         \
  case 0:                            \
  default:

#define TEST_ASSERT_(expression, on_failure)                  \
  TEST_AMBIGUOUS_ELSE_BLOCKER_                                \
  if (const ::testing::TestResult test_result = (expression)) \
    ;                                                         \
  else                                                        \
    on_failure(test_result)

#define TEST_NONFATAL_FAILURE_(message) \
  ::testing::AssertHelper(__FILE__, __LINE__, message) = ::testing::Message()

#define TEST_FATAL_FAILURE_(message)                            \
  return ::testing::AssertHelper(__FILE__, __LINE__, message) = \
             ::testing::Message()

#define EXPECT_EQ(a, b)                                        \
  TEST_AMBIGUOUS_ELSE_BLOCKER_                                 \
  if (const ::testing::TestResult test_result =                \
          ::testing::TestResult(a == b, #a " == " #b))         \
    ;                                                          \
  else                                                         \
    ::testing::AssertHelper(__FILE__, __LINE__, test_result) = \
        ::testing::Message() << ::testing::TryDiffStrings(a, b)

#define EXPECT_NE(a, b)                                     \
  TEST_ASSERT_(::testing::TestResult(a != b, #a " != " #b), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_LT(a, b)                                   \
  TEST_ASSERT_(::testing::TestResult(a < b, #a " < " #b), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_GT(a, b)                                   \
  TEST_ASSERT_(::testing::TestResult(a > b, #a " > " #b), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_LE(a, b)                                     \
  TEST_ASSERT_(::testing::TestResult(a <= b, #a " <= " #b), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_GE(a, b)                                     \
  TEST_ASSERT_(::testing::TestResult(a >= b, #a " >= " #b), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_TRUE(a)                                          \
  TEST_ASSERT_(::testing::TestResult(static_cast<bool>(a), #a), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_FALSE(a)                                          \
  TEST_ASSERT_(::testing::TestResult(!static_cast<bool>(a), #a), \
               TEST_NONFATAL_FAILURE_)

#define EXPECT_STREQ(a, b)                                                \
  TEST_ASSERT_(::testing::TestResult(strcmp(a, b) == 0, #a " str== " #b), \
               TEST_NONFATAL_FAILURE_)

#define ASSERT_EQ(a, b)                                               \
  TEST_AMBIGUOUS_ELSE_BLOCKER_                                        \
  if (const ::testing::TestResult test_result =                       \
          ::testing::TestResult(a == b, #a " == " #b))                \
    ;                                                                 \
  else                                                                \
    return ::testing::AssertHelper(__FILE__, __LINE__, test_result) = \
               ::testing::Message() << ::testing::TryDiffStrings(a, b)

#define ASSERT_NE(a, b) \
  TEST_ASSERT_(::testing::TestResult(a != b, #a " != " #b), TEST_FATAL_FAILURE_)

#define ASSERT_LT(a, b) \
  TEST_ASSERT_(::testing::TestResult(a < b, #a " < " #b), TEST_FATAL_FAILURE_)

#define ASSERT_GT(a, b) \
  TEST_ASSERT_(::testing::TestResult(a > b, #a " > " #b), TEST_FATAL_FAILURE_)

#define ASSERT_LE(a, b) \
  TEST_ASSERT_(::testing::TestResult(a <= b, #a " <= " #b), TEST_FATAL_FAILURE_)

#define ASSERT_GE(a, b) \
  TEST_ASSERT_(::testing::TestResult(a >= b, #a " >= " #b), TEST_FATAL_FAILURE_)

#define ASSERT_TRUE(a)                                          \
  TEST_ASSERT_(::testing::TestResult(static_cast<bool>(a), #a), \
               TEST_FATAL_FAILURE_)

#define ASSERT_FALSE(a)                                          \
  TEST_ASSERT_(::testing::TestResult(!static_cast<bool>(a), #a), \
               TEST_FATAL_FAILURE_)

#define ASSERT_STREQ(a, b)                                                \
  TEST_ASSERT_(::testing::TestResult(strcmp(a, b) == 0, #a " str== " #b), \
               TEST_FATAL_FAILURE_)

#define EXPECT_SUCCESS(err)                                       \
  TEST_AMBIGUOUS_ELSE_BLOCKER_                                    \
  if (const auto& test_err = (err); !test_err.has_error())        \
    ;                                                             \
  else                                                            \
    TEST_NONFATAL_FAILURE_(                                       \
        ::testing::TestResult(false, "EXPECT_SUCCESS(" #err ")")) \
        << test_err.message()

#define ASSERT_SUCCESS(err)                                       \
  TEST_AMBIGUOUS_ELSE_BLOCKER_                                    \
  if (const auto& test_err = (err); !test_err.has_error())        \
    ;                                                             \
  else                                                            \
    TEST_FATAL_FAILURE_(                                          \
        ::testing::TestResult(false, "ASSERT_SUCCESS(" #err ")")) \
        << test_err.message()

#endif  // UTIL_TEST_TEST_H_
