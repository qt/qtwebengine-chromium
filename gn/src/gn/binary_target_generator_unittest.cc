// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/binary_target_generator.h"
#include "gn/err.h"
#include "gn/scheduler.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

using BinaryTargetGeneratorTest = TestWithScheduler;

TEST_F(BinaryTargetGeneratorTest, NonModuleTarget) {
  TestWithScope setup;
  Scope::ItemVector items_;
  setup.scope()->set_item_collector(&items_);
  setup.scope()->set_source_dir(SourceDir("//test/"));

  TestParseInput input(
      R"(static_library("foo") {
           generate_modulemap = "textual"
           sources = [ "//foo.c" ]
         })");
  ASSERT_SUCCESS(input);

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_SUCCESS(err);

  ASSERT_EQ(1u, items_.size());
  Target* target = items_[0]->AsTarget();
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->module_type().none());
}

TEST_F(BinaryTargetGeneratorTest, GeneratedModuleMapAllPublic) {
  TestWithScope setup;
  Scope::ItemVector items_;
  setup.scope()->set_item_collector(&items_);
  setup.scope()->set_source_dir(SourceDir("//test/"));

  TestParseInput input(
      R"(static_library("foo") {
           generate_modulemap = "textual"
           sources = [ "//foo.cc", "//foo.h" ]
         })");
  ASSERT_SUCCESS(input);

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_SUCCESS(err);

  ASSERT_EQ(1u, items_.size());
  Target* target = items_[0]->AsTarget();
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->module_type().test(Target::HAS_MODULEMAP));
  EXPECT_TRUE(target->module_type().test(Target::MODULEMAP_IS_GENERATED));
  EXPECT_TRUE(target->module_type().test(Target::MODULEMAP_IS_TEXTUAL));
}

TEST_F(BinaryTargetGeneratorTest, GeneratedModuleMap) {
  TestWithScope setup;
  Scope::ItemVector items_;
  setup.scope()->set_item_collector(&items_);
  setup.scope()->set_source_dir(SourceDir("//test/"));

  TestParseInput input(
      R"(static_library("foo") {
           generate_modulemap = "textual"
           sources = [ "//foo.cc" ]
           public = ["//foo.h"]
         })");
  ASSERT_SUCCESS(input);

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_SUCCESS(err);

  ASSERT_EQ(1u, items_.size());
  Target* target = items_[0]->AsTarget();
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->module_type().test(Target::HAS_MODULEMAP));
  EXPECT_TRUE(target->module_type().test(Target::MODULEMAP_IS_GENERATED));
  EXPECT_TRUE(target->module_type().test(Target::MODULEMAP_IS_TEXTUAL));
}
