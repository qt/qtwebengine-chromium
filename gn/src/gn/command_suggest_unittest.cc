// Copyright 2026 The GN Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/commands.h"
#include "gn/setup.h"
#include "gn/switches.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

TEST(Suggest, ResolveModuleName) {
  TestWithScope setup_scope;
  SourceDir current_dir("//");
  Label default_toolchain(SourceDir("//toolchain/"), "default");
  Err err;

  Target target(setup_scope.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_module_name("my_module");

  std::vector<const Target*> all_targets = {&target};

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, default_toolchain,
        "my_module");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected = {
        {&target, commands::ApiScope::kPublic}};
    EXPECT_EQ(expected, results);
    EXPECT_TRUE(ok);
  }

  // Test resolving module name "my_module_Private"
  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, default_toolchain,
        "my_module_Private");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected = {
        {&target, commands::ApiScope::kPrivate}};
    EXPECT_EQ(expected, results);
    EXPECT_TRUE(ok);
  }
}

TEST(Suggest, ResolveTargetName) {
  TestWithScope setup_scope;
  SourceDir current_dir("//");
  Label default_toolchain = setup_scope.toolchain()->label();
  Err err;

  Target target(
      setup_scope.settings(),
      Label(SourceDir("//"), "hello", setup_scope.toolchain()->label().dir(),
            setup_scope.toolchain()->label().name()));
  Target target_gcc(
      setup_scope.settings(),
      Label(SourceDir("//"), "hello", SourceDir("//build/toolchain/"), "gcc"));
  std::vector<const Target*> all_targets = {&target, &target_gcc};

  // Test resolving "//:hello"
  auto [results_label, ok_label] = commands::ResolveSuggestionToTarget(
      setup_scope.build_settings(), all_targets,
      setup_scope.toolchain()->label(), "//:hello");

  std::vector<std::pair<const Target*, commands::ApiScope>> expected_label = {
      {&target, commands::ApiScope::kPublic}};
  EXPECT_EQ(expected_label, results_label);
  EXPECT_TRUE(ok_label);

  // Test resolving "//:hello(//build/toolchain:gcc)"
  auto [results_toolchain, ok_toolchain] = commands::ResolveSuggestionToTarget(
      setup_scope.build_settings(), all_targets, default_toolchain,
      "//:hello(//build/toolchain:gcc)");

  std::vector<std::pair<const Target*, commands::ApiScope>> expected_toolchain =
      {{&target_gcc, commands::ApiScope::kPublic}};
  EXPECT_EQ(expected_toolchain, results_toolchain);
  EXPECT_TRUE(ok_toolchain);
}

TEST(Suggest, ResolveFileName) {
  TestWithScope setup_scope;
  SourceDir current_dir("//");
  Label default_toolchain = setup_scope.toolchain()->label();
  Label current_toolchain(SourceDir("//build/toolchain/"), "gcc");
  Label secondary_toolchain(SourceDir("//build/toolchain/"), "clang");
  Err err;

  // Follow standard practice to create temporary directories in tests.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath root_dir = temp_dir.GetPath();
  setup_scope.build_settings()->SetRootPath(root_dir);

  base::WriteFile(root_dir.AppendASCII("public.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("private.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("implicit_public.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("no_target.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("simple.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("default_toolchain.h"), "", 0);
  base::WriteFile(root_dir.AppendASCII("secondary_toolchain.h"), "", 0);

  Target explicit_target(
      setup_scope.settings(),
      Label(SourceDir("//"), "explicit", current_toolchain.dir(),
            current_toolchain.name()));
  explicit_target.set_all_headers_public(false);
  explicit_target.sources().push_back(SourceFile("//private.h"));
  explicit_target.public_headers().push_back(SourceFile("//public.h"));
  explicit_target.public_headers().push_back(
      SourceFile("//nonexistent_file.h"));

  Target implicit_target(
      setup_scope.settings(),
      Label(SourceDir("//"), "implicit", default_toolchain.dir(),
            default_toolchain.name()));
  implicit_target.set_all_headers_public(true);
  implicit_target.sources().push_back(SourceFile("//implicit_public.h"));
  implicit_target.sources().push_back(SourceFile("//private.cc"));

  Target simple_default(
      setup_scope.settings(),
      Label(SourceDir("//"), "simple", default_toolchain.dir(),
            default_toolchain.name()));
  simple_default.public_headers().push_back(SourceFile("//public.h"));
  simple_default.public_headers().push_back(
      SourceFile("//default_toolchain.h"));

  Target simple_secondary(
      setup_scope.settings(),
      Label(SourceDir("//"), "simple", secondary_toolchain.dir(),
            secondary_toolchain.name()));
  simple_secondary.public_headers().push_back(SourceFile("//public.h"));
  simple_secondary.public_headers().push_back(
      SourceFile("//default_toolchain.h"));
  simple_secondary.public_headers().push_back(
      SourceFile("//secondary_toolchain.h"));

  std::vector<const Target*> all_targets = {&explicit_target, &implicit_target,
                                            &simple_default, &simple_secondary};

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "//public.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected = {
        {&explicit_target, commands::ApiScope::kPublic}};
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected, results);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "../../private.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected = {
        {&explicit_target, commands::ApiScope::kPrivate}};
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected, results);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "//implicit_public.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected = {
        {&implicit_target, commands::ApiScope::kPublic}};
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected, results);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "nonexistent_file.h");
    EXPECT_FALSE(ok);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "//no_target.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected_targets;
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_targets, results);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "//default_toolchain.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected_targets =
        {
            {&simple_secondary, commands::ApiScope::kPublic},
            {&simple_default, commands::ApiScope::kPublic},
        };
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_targets, results);
  }

  {
    auto [results, ok] = commands::ResolveSuggestionToTarget(
        setup_scope.build_settings(), all_targets, current_toolchain,
        "//secondary_toolchain.h");
    std::vector<std::pair<const Target*, commands::ApiScope>> expected_targets =
        {{{&simple_secondary, commands::ApiScope::kPublic}}};
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_targets, results);
  }
}
