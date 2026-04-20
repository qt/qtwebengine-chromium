// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/desc_builder.h"

#include "gn/test_with_scope.h"
#include "util/test/test.h"

TEST(DescBuilder, TargetWithValidations) {
  TestWithScope setup;
  Err err;

  Target validation_target(setup.settings(), Label(SourceDir("//foo/"), "val"));
  validation_target.set_output_type(Target::ACTION);
  validation_target.visibility().SetPublic();
  validation_target.SetToolchain(setup.toolchain());
  validation_target.action_values().set_script(SourceFile("//foo/script.py"));
  validation_target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/val.out");
  ASSERT_TRUE(validation_target.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "target"));
  target.set_output_type(Target::GROUP);
  target.visibility().SetPublic();
  target.SetToolchain(setup.toolchain());
  target.validations().push_back(LabelTargetPair(&validation_target));
  ASSERT_TRUE(target.OnResolved(&err));

  std::unique_ptr<base::DictionaryValue> desc =
      DescBuilder::DescriptionForTarget(&target, "", false, false, false);

  base::Value* validations = desc->FindKey("validations");
  ASSERT_TRUE(validations);
  ASSERT_TRUE(validations->is_list());
  ASSERT_EQ(1u, validations->GetList().size());
  EXPECT_EQ("//foo:val()", validations->GetList()[0].GetString());
}
