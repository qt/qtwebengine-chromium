// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/input_file.h"
#include "gn/parse_tree.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

class ExpandDirectoryTest : public TestWithScheduler {
 protected:
  ExpandDirectoryTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    setup.build_settings()->SetRootPath(temp_dir_.GetPath());
  }

  base::ScopedTempDir temp_dir_;
  TestWithScope setup;

  std::unique_ptr<FunctionCallNode> SetupDefaultDirAndFunction(
      InputFile* input_file,
      base::FilePath dir) {
    auto file1 = dir.AppendASCII("file1.txt");
    auto file2 = dir.AppendASCII("file2.txt");
    auto sub_dir = dir.AppendASCII("sub");
    auto file3 = sub_dir.AppendASCII("file3.txt");

    EXPECT_TRUE(base::CreateDirectory(sub_dir));
    EXPECT_TRUE(WriteFile(file1, "content1", nullptr));
    EXPECT_TRUE(WriteFile(file2, "content2", nullptr));
    EXPECT_TRUE(WriteFile(file3, "content3", nullptr));

    Location location(input_file, 1, 1);
    Token token(location, Token::IDENTIFIER, "expand_directory");
    auto function = std::make_unique<FunctionCallNode>();
    function->set_function(token);

    auto args = std::make_unique<ListNode>();
    args->set_begin_token(token);
    args->set_end(std::make_unique<EndNode>(token));
    function->set_args(std::move(args));

    auto allowlist = std::make_unique<SourceFileSet>();
    allowlist->insert(input_file->name());
    setup.build_settings()->set_expand_directory_allowlist(
        std::move(allowlist));
    return function;
  }
};

TEST_F(ExpandDirectoryTest, Recursive) {
  auto dir_path = temp_dir_.GetPath().AppendASCII("foo").AppendASCII("bar");
  auto input_file = InputFile(SourceFile("//BUILD.gn"));
  auto function = SetupDefaultDirAndFunction(&input_file, dir_path);

  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), function.get(),
      {Value(nullptr, "//foo/bar"), Value(nullptr, true)}, &err);
  ASSERT_SUCCESS(err);

  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 3);
  EXPECT_EQ(result.list_value()[0].string_value(), "//foo/bar/file1.txt");
  EXPECT_EQ(result.list_value()[1].string_value(), "//foo/bar/file2.txt");
  EXPECT_EQ(result.list_value()[2].string_value(), "//foo/bar/sub/file3.txt");

  std::vector<base::FilePath> deps = scheduler().GetGenDependencies();
  EXPECT_TRUE(std::ranges::find(deps, dir_path) != deps.end())
      << FilePathToUTF8(dir_path);
  auto sub = dir_path.AppendASCII("sub");
  EXPECT_TRUE(std::ranges::find(deps, sub) != deps.end())
      << FilePathToUTF8(sub);
}

TEST_F(ExpandDirectoryTest, NonRecursive) {
  auto dir_path = temp_dir_.GetPath().AppendASCII("foo").AppendASCII("bar");
  auto input_file = InputFile(SourceFile("//foo/BUILD.gn"));
  auto function = SetupDefaultDirAndFunction(&input_file, dir_path);
  setup.scope()->set_source_dir(SourceDir("//foo/"));

  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), function.get(),
      {Value(nullptr, "bar"), Value(nullptr, false)}, &err);
  ASSERT_SUCCESS(err);
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 2);
  EXPECT_EQ(result.list_value()[0].string_value(), "//foo/bar/file1.txt");
  EXPECT_EQ(result.list_value()[1].string_value(), "//foo/bar/file2.txt");

  std::vector<base::FilePath> deps = scheduler().GetGenDependencies();
  EXPECT_TRUE(std::ranges::find(deps, dir_path) != deps.end());
  EXPECT_TRUE(std::ranges::find(deps, dir_path.AppendASCII("sub")) ==
              deps.end());
}

TEST_F(ExpandDirectoryTest, EmptyDir) {
  std::string dir_str = FilePathToUTF8(temp_dir_.GetPath());

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function, {Value(nullptr, dir_str), Value(nullptr, true)},
      &err);
  ASSERT_SUCCESS(err);
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 0);
}

TEST_F(ExpandDirectoryTest, NonExistentDir) {
  base::FilePath non_existent = temp_dir_.GetPath().AppendASCII("non_existent");

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function,
      {Value(nullptr, FilePathToUTF8(non_existent)), Value(nullptr, true)},
      &err);
  EXPECT_TRUE(err.has_error());
}

TEST_F(ExpandDirectoryTest, Allowlist) {
  InputFile input_file(SourceFile("//BUILD.gn"));
  Location location(&input_file, 1, 1);
  Token token(location, Token::IDENTIFIER, "expand_directory");
  FunctionCallNode function;
  function.set_function(token);

  auto args = std::make_unique<ListNode>();
  args->set_begin_token(token);
  args->set_end(std::make_unique<EndNode>(token));
  function.set_args(std::move(args));

  // No allowlist
  {
    Err err;
    Value result = functions::RunExpandDirectory(
        setup.scope(), &function,
        {Value(nullptr, FilePathToUTF8(temp_dir_.GetPath())),
         Value(nullptr, true)},
        &err);
    EXPECT_TRUE(err.has_error());
  }

  // Empty allowlist
  auto allowlist_owned = std::make_unique<SourceFileSet>();
  auto allowlist = allowlist_owned.get();
  setup.build_settings()->set_expand_directory_allowlist(
      std::move(allowlist_owned));
  allowlist->insert(SourceFile("//foo.gni"));
  {
    Err err;
    Value result = functions::RunExpandDirectory(
        setup.scope(), &function,
        {Value(nullptr, FilePathToUTF8(temp_dir_.GetPath())),
         Value(nullptr, true)},
        &err);
    EXPECT_TRUE(err.has_error());
  }
}
