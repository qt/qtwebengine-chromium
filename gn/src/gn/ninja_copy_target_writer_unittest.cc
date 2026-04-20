// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "gn/ninja_copy_target_writer.h"
#include "gn/substitution_list.h"
#include "gn/target.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

// Tests multiple files with an output pattern and no toolchain dependency.
TEST(NinjaCopyTargetWriter, Run) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt\n"
      "build input2.out: copy ../../foo/input2.txt\n"
      "\n"
      "build phony/foo/bar: phony input1.out input2.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

// Tests a single file with no output pattern.
TEST(NinjaCopyTargetWriter, ToolchainDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/output.out");

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build output.out: copy ../../foo/input1.txt\n"
      "\n"
      "build phony/foo/bar: phony output.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

TEST(NinjaCopyTargetWriter, OrderOnlyDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);
  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");
  target.config_values().inputs().push_back(SourceFile("//foo/script.py"));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt || ../../foo/script.py\n"
      "\n"
      "build phony/foo/bar: phony input1.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

TEST(NinjaCopyTargetWriter, DataDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);
  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  Target data_dep(setup.settings(), Label(SourceDir("//foo/"), "datadep"));
  data_dep.set_output_type(Target::ACTION);
  data_dep.visibility().SetPublic();
  data_dep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(data_dep.OnResolved(&err));

  target.data_deps().push_back(LabelTargetPair(&data_dep));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt || phony/foo/datadep\n"
      "\n"
      "build phony/foo/bar: phony input1.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

TEST(NinjaCopyTargetWriter, NoSourcesInOutputs) {
  Err err;
  TestWithScope setup;
  setup.build_settings()->set_no_stamp_files(true);

  // First with a single action / output / copy
  {
    Target action1(setup.settings(), Label(SourceDir("//foo/"), "action1"));
    action1.set_output_type(Target::ACTION);
    action1.visibility().SetPublic();
    action1.SetToolchain(setup.toolchain());
    action1.action_values().outputs() =
        SubstitutionList::MakeForTest("//out/Debug/action1.out");
    ASSERT_TRUE(action1.OnResolved(&err));

    Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
    target.set_output_type(Target::COPY_FILES);
    target.sources().push_back(
        action1.computed_outputs()[0].AsSourceFile(setup.build_settings()));
    target.SetToolchain(setup.toolchain());
    target.private_deps().push_back(LabelTargetPair(&action1));
    target.action_values().outputs() =
        SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.copy");
    ASSERT_TRUE(target.OnResolved(&err));

    std::ostringstream out;
    std::vector<OutputFile> ninja_outputs;
    NinjaCopyTargetWriter writer(&target, out);
    writer.SetNinjaOutputs(&ninja_outputs);
    writer.Run();

    const char expected_linux[] =
        "build action1.copy: copy action1.out || phony/foo/action1\n"
        "\n"
        "build phony/foo/bar: phony action1.copy\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected_linux, out_str);

    EXPECT_EQ(2u, ninja_outputs.size());
    EXPECT_EQ(ninja_outputs[0].value(), "action1.copy");
    EXPECT_EQ(ninja_outputs[1].value(), "phony/foo/bar");
  }

  // Second, with two actions / outputs / copies, which is what trigerred
  // the bug in https://gn.issues.chromium.org/448860851
  {
    Target action1(setup.settings(), Label(SourceDir("//foo/"), "action1"));
    action1.set_output_type(Target::ACTION);
    action1.visibility().SetPublic();
    action1.SetToolchain(setup.toolchain());
    action1.action_values().outputs() =
        SubstitutionList::MakeForTest("//out/Debug/action1.out");
    ASSERT_TRUE(action1.OnResolved(&err));

    Target action2(setup.settings(), Label(SourceDir("//foo/"), "action2"));
    action2.set_output_type(Target::ACTION);
    action2.visibility().SetPublic();
    action2.SetToolchain(setup.toolchain());
    action2.action_values().outputs() =
        SubstitutionList::MakeForTest("//out/Debug/action2.out");
    ASSERT_TRUE(action2.OnResolved(&err));

    Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
    target.set_output_type(Target::COPY_FILES);
    target.sources().push_back(
        action1.computed_outputs()[0].AsSourceFile(setup.build_settings()));
    target.sources().push_back(
        action2.computed_outputs()[0].AsSourceFile(setup.build_settings()));
    target.SetToolchain(setup.toolchain());
    target.private_deps().push_back(LabelTargetPair(&action1));
    target.private_deps().push_back(LabelTargetPair(&action2));
    target.action_values().outputs() =
        SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.copy");
    ASSERT_TRUE(target.OnResolved(&err));

    std::ostringstream out;
    std::vector<OutputFile> ninja_outputs;
    NinjaCopyTargetWriter writer(&target, out);
    writer.SetNinjaOutputs(&ninja_outputs);
    writer.Run();

    const char expected_linux[] =
        "build phony/foo/bar.inputdeps: phony phony/foo/action1 "
        "phony/foo/action2\n"
        "build action1.copy: copy action1.out || phony/foo/bar.inputdeps\n"
        "build action2.copy: copy action2.out || phony/foo/bar.inputdeps\n"
        "\n"
        "build phony/foo/bar: phony action1.copy action2.copy\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected_linux, out_str);

    EXPECT_EQ(3u, ninja_outputs.size());
    EXPECT_EQ(ninja_outputs[0].value(), "action1.copy");
    EXPECT_EQ(ninja_outputs[1].value(), "action2.copy");
    EXPECT_EQ(ninja_outputs[2].value(), "phony/foo/bar");
  }
}

// Tests that validation dependencies are correctly written for a copy target.
TEST(NinjaCopyTargetWriter, CopyWithValidations) {
  Err err;
  TestWithScope setup;

  Target validation_target(setup.settings(), Label(SourceDir("//foo/"), "val"));
  validation_target.set_output_type(Target::ACTION);
  validation_target.visibility().SetPublic();
  validation_target.action_values().set_script(SourceFile("//foo/script.py"));
  validation_target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/val.out");
  validation_target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(validation_target.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);
  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");
  target.validations().push_back(LabelTargetPair(&validation_target));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt |@ phony/foo/val\n"
      "\n"
      "build phony/foo/bar: phony input1.out |@ phony/foo/val\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}
