// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <iterator>

#include "gn/err.h"
#include "gn/label.h"
#include "gn/value.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(Label, Resolve) {
  const struct TestCase {
    const char* cur_dir;
    const char* str;
    bool success;
    const char* expected_dir;
    const char* expected_name;
    const char* expected_toolchain_dir;
    const char* expected_toolchain_name;
  } test_cases[] = {
      {"//chrome/", "", false, "", "", "", ""},
      {"//chrome/", "/", false, "", "", "", ""},
      {"//chrome/", ":", false, "", "", "", ""},
      {"//chrome/", "/:", false, "", "", "", ""},
      {"//chrome/", "blah", true, "//chrome/blah/", "blah", "//t/", "d"},
      {"//chrome/", "blah:bar", true, "//chrome/blah/", "bar", "//t/", "d"},
      // Absolute paths.
      {"//chrome/", "/chrome:bar", true, "/chrome/", "bar", "//t/", "d"},
      {"//chrome/", "/chrome/:bar", true, "/chrome/", "bar", "//t/", "d"},
#if defined(OS_WIN)
      {"//chrome/", "/C:/chrome:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
      {"//chrome/", "/C:/chrome/:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
      {"//chrome/", "C:/chrome:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
#endif
      // Refers to root dir.
      {"//chrome/", "//:bar", true, "//", "bar", "//t/", "d"},
      // Implicit directory
      {"//chrome/", ":bar", true, "//chrome/", "bar", "//t/", "d"},
      {"//chrome/renderer/", ":bar", true, "//chrome/renderer/", "bar", "//t/",
       "d"},
      // Implicit names.
      {"//chrome/", "//base", true, "//base/", "base", "//t/", "d"},
      {"//chrome/", "//base/i18n", true, "//base/i18n/", "i18n", "//t/", "d"},
      {"//chrome/", "//base/i18n:foo", true, "//base/i18n/", "foo", "//t/",
       "d"},
      {"//chrome/", "//", false, "", "", "", ""},
      // Toolchain parsing.
      {"//chrome/", "//chrome:bar(//t:n)", true, "//chrome/", "bar", "//t/",
       "n"},
      {"//chrome/", "//chrome:bar(//t)", true, "//chrome/", "bar", "//t/", "t"},
      {"//chrome/", "//chrome:bar(//t:)", true, "//chrome/", "bar", "//t/",
       "t"},
      {"//chrome/", "//chrome:bar()", true, "//chrome/", "bar", "//t/", "d"},
      {"//chrome/", "//chrome:bar(foo)", true, "//chrome/", "bar",
       "//chrome/foo/", "foo"},
      {"//chrome/", "//chrome:bar(:foo)", true, "//chrome/", "bar", "//chrome/",
       "foo"},
      // TODO(brettw) it might be nice to make this an error:
      //{"//chrome/", "//chrome:bar())", false, "", "", "", "" },
      {"//chrome/", "//chrome:bar(//t:bar(tc))", false, "", "", "", ""},
      {"//chrome/", "//chrome:bar(()", false, "", "", "", ""},
      {"//chrome/", "(t:b)", false, "", "", "", ""},
      {"//chrome/", ":bar(//t/b)", true, "//chrome/", "bar", "//t/b/", "b"},
      {"//chrome/", ":bar(/t/b)", true, "//chrome/", "bar", "/t/b/", "b"},
      {"//chrome/", ":bar(t/b)", true, "//chrome/", "bar", "//chrome/t/b/",
       "b"},
  };

  Label default_toolchain(SourceDir("//t/"), "d");

  for (const auto& test_case : test_cases) {
    std::string location, name;
    Err err;
    Value v(nullptr, Value::STRING);
    v.string_value() = test_case.str;
    Label result =
        Label::Resolve(SourceDir(test_case.cur_dir), std::string_view(),
                       default_toolchain, v, &err);
    EXPECT_EQ(test_case.success, !err.has_error()) << test_case.str;
    if (!err.has_error() && test_case.success) {
      EXPECT_EQ(test_case.expected_dir, result.dir().value()) << test_case.str;
      EXPECT_EQ(test_case.expected_name, result.name()) << test_case.str;
      EXPECT_EQ(test_case.expected_toolchain_dir,
                result.toolchain_dir().value())
          << test_case.str;
      EXPECT_EQ(test_case.expected_toolchain_name, result.toolchain_name())
          << test_case.str;
    }
  }
}

// Tests the case where the path resolves to something above "//". It should get
// converted to an absolute path "/foo/bar".
TEST(Label, ResolveAboveRootBuildDir) {
  Label default_toolchain(SourceDir("//t/"), "d");

  std::string location, name;
  Err err;

  SourceDir cur_dir("//cur/");
  std::string source_root("/foo/bar/baz");

  // No source root given, should not go above the root build dir.
  Label result = Label::Resolve(cur_dir, std::string_view(), default_toolchain,
                                Value(nullptr, "../../..:target"), &err);
  EXPECT_SUCCESS(err);
  EXPECT_EQ("//", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // Source root provided, it should go into that.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "../../..:target"), &err);
  EXPECT_SUCCESS(err);
  EXPECT_EQ("/foo/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // It shouldn't go up higher than the system root.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "../../../../..:target"), &err);
  EXPECT_SUCCESS(err);
  EXPECT_EQ("/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // Test an absolute label that goes above the source root. This currently
  // stops at the source root. It should arguably keep going and produce "/foo/"
  // but this test just makes sure the current behavior isn't regressed by
  // accident.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "//../.."), &err);
  EXPECT_SUCCESS(err);
  EXPECT_EQ("/foo/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("foo", result.name());
}

TEST(Label, GetUserVisibleName) {
  const struct TestCase {
    const SourceDir dir;
    const char* name;
    const SourceDir toolchain_dir;
    const char* toolchain_name;
    bool include_toolchain;
    const char* expected;
  } test_cases[] = {
      // Label in root.
      {SourceDir("//"), "name", SourceDir("//t/"), "tn", false, "//:name"},
      {SourceDir("//"), "name", SourceDir("//t/"), "tn", true,
       "//:name(//t:tn)"},
      // Label in subdir.
      {SourceDir("//dir/"), "name", SourceDir("//t/"), "tn", false,
       "//dir:name"},
      {SourceDir("//dir/"), "name", SourceDir("//t/"), "tn", true,
       "//dir:name(//t:tn)"},
      // Toolchain dir is null.
      {SourceDir("//dir/"), "name", SourceDir(), "", false, "//dir:name"},
      {SourceDir("//dir/"), "name", SourceDir(), "", true, "//dir:name()"},
      // Label dir is null hence label is null.
      {SourceDir(), "name", SourceDir("//t/"), "tn", false, ""},
      {SourceDir(), "name", SourceDir("//t/"), "tn", true, ""},
  };

  for (const auto& test_case : test_cases) {
    Label l(test_case.dir, test_case.name, test_case.toolchain_dir,
            test_case.toolchain_name);
    EXPECT_EQ(test_case.expected,
              l.GetUserVisibleName(test_case.include_toolchain))
        << "dir: " << test_case.dir.value();
  }

  // Also test empty label case.
  EXPECT_EQ("", Label().GetUserVisibleName(false));
  EXPECT_EQ("", Label().GetUserVisibleName(true));
}

TEST(Label, GetUserVisibleNameDefaultToolchain) {
  const struct TestCase {
    const SourceDir dir;
    const char* name;
    const SourceDir toolchain_dir;
    const char* toolchain_name;
    const SourceDir default_toolchain_dir;
    const char* default_toolchain_name;
    const char* expected;
  } test_cases[] = {
      // Matches the default toolchain, so omitted.
      {SourceDir("//d/"), "n", SourceDir("//t/"), "tn", SourceDir("//t/"), "tn",
       "//d:n"},
      // Different (dir doesn't match), so toolchain is printed.
      {SourceDir("//d/"), "n", SourceDir("//t/"), "tn", SourceDir("//x/"), "tn",
       "//d:n(//t:tn)"},
      // Different (name doesn't match), so toolchain is printed.
      {SourceDir("//d/"), "n", SourceDir("//t/"), "tn", SourceDir("//x/"), "yz",
       "//d:n(//t:tn)"},
      // Different (label's toolchain dir is null), so toolchain is printed.
      {SourceDir("//d/"), "n", SourceDir(), "tn", SourceDir("//t/"), "tn",
       "//d:n()"},
      // Different (default toolchain dir is null), so toolchain is printed.
      {SourceDir("//d/"), "n", SourceDir("//t/"), "tn", SourceDir(), "tn",
       "//d:n(//t:tn)"},
      // Label dir is null hence label is null.
      {SourceDir(), "n", SourceDir("//t/"), "tn", SourceDir("//t/"), "tn", ""},
  };

  for (const auto& test_case : test_cases) {
    Label l(test_case.dir, test_case.name, test_case.toolchain_dir,
            test_case.toolchain_name);
    Label dt(test_case.default_toolchain_dir, test_case.default_toolchain_name);
    EXPECT_EQ(test_case.expected, l.GetUserVisibleName(dt))
        << "label: " << l.GetUserVisibleName(true) << " "
        << "default toolchain: " << dt.GetUserVisibleName(true);
  }

  // Also test empty label case.
  EXPECT_EQ("", Label().GetUserVisibleName(Label(SourceDir("//t/"), "tn")));
}
