// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

namespace {

class TargetTest : public testing::Test {
 public:
  TargetTest()
      : build_settings_(),
        toolchain_(Label(SourceDir("//tc/"), "tc")),
        settings_(&build_settings_, &toolchain_, std::string()) {
  }
  ~TargetTest() {
  }

 protected:
  BuildSettings build_settings_;
  Toolchain toolchain_;
  Settings settings_;
};

}  // namespace

// Tests that depending on a group is like depending directly on the group's
// deps.
TEST_F(TargetTest, GroupDeps) {
  // Two low-level targets.
  Target x(&settings_, Label(SourceDir("//component/"), "x"));
  Target y(&settings_, Label(SourceDir("//component/"), "y"));

  // Make a group for both x and y.
  Target g(&settings_, Label(SourceDir("//group/"), "g"));
  g.set_output_type(Target::GROUP);
  g.deps().push_back(&x);
  g.deps().push_back(&y);

  // Random placeholder target so we can see the group's deps get inserted at
  // the right place.
  Target b(&settings_, Label(SourceDir("//app/"), "b"));

  // Make a target depending on the group and "b". OnResolved will expand.
  Target a(&settings_, Label(SourceDir("//app/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.deps().push_back(&g);
  a.deps().push_back(&b);
  a.OnResolved();

  // The group's deps should be inserted after the group itself in the deps
  // list, so we should get "g, x, y, b"
  ASSERT_EQ(4u, a.deps().size());
  EXPECT_EQ(&g, a.deps()[0]);
  EXPECT_EQ(&x, a.deps()[1]);
  EXPECT_EQ(&y, a.deps()[2]);
  EXPECT_EQ(&b, a.deps()[3]);
}

// Tests that ldflags are inherited across deps boundaries for static libraries
// but not executables.
TEST_F(TargetTest, LdflagsInheritance) {
  const std::string ldflag("-lfoo");

  // Leaf target with ldflags set.
  Target z(&settings_, Label(SourceDir("//foo/"), "z"));
  z.set_output_type(Target::STATIC_LIBRARY);
  z.config_values().ldflags().push_back(ldflag);
  z.OnResolved();

  // All ldflags should be set when target is resolved.
  ASSERT_EQ(1u, z.all_ldflags().size());
  EXPECT_EQ(ldflag, z.all_ldflags()[0]);

  // Shared library target should inherit the ldflag from the static library
  // and its own. Its own flag should be before the inherited one.
  const std::string second_ldflag("-lbar");
  Target shared(&settings_, Label(SourceDir("//foo/"), "shared"));
  shared.set_output_type(Target::SHARED_LIBRARY);
  shared.config_values().ldflags().push_back(second_ldflag);
  shared.deps().push_back(&z);
  shared.OnResolved();

  ASSERT_EQ(2u, shared.all_ldflags().size());
  EXPECT_EQ(second_ldflag, shared.all_ldflags()[0]);
  EXPECT_EQ(ldflag, shared.all_ldflags()[1]);

  // Executable target shouldn't get either ldflags by depending on shared.
  Target exec(&settings_, Label(SourceDir("//foo/"), "exec"));
  exec.set_output_type(Target::EXECUTABLE);
  exec.deps().push_back(&shared);
  exec.OnResolved();
  EXPECT_EQ(0u, exec.all_ldflags().size());
}

// Test all/direct_dependent_configs inheritance, and
// forward_dependent_configs_from
TEST_F(TargetTest, DependentConfigs) {
  // Set up a dependency chain of a -> b -> c
  Target a(&settings_, Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  Target b(&settings_, Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  Target c(&settings_, Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  a.deps().push_back(&b);
  b.deps().push_back(&c);

  // Normal non-inherited config.
  Config config(Label(SourceDir("//foo/"), "config"));
  c.configs().push_back(&config);

  // All dependent config.
  Config all(Label(SourceDir("//foo/"), "all"));
  c.all_dependent_configs().push_back(&all);

  // Direct dependent config.
  Config direct(Label(SourceDir("//foo/"), "direct"));
  c.direct_dependent_configs().push_back(&direct);

  c.OnResolved();
  b.OnResolved();
  a.OnResolved();

  // B should have gotten both dependent configs from C.
  ASSERT_EQ(2u, b.configs().size());
  EXPECT_EQ(&all, b.configs()[0]);
  EXPECT_EQ(&direct, b.configs()[1]);
  ASSERT_EQ(1u, b.all_dependent_configs().size());
  EXPECT_EQ(&all, b.all_dependent_configs()[0]);

  // A should have just gotten the "all" dependent config from C.
  ASSERT_EQ(1u, a.configs().size());
  EXPECT_EQ(&all, a.configs()[0]);
  EXPECT_EQ(&all, a.all_dependent_configs()[0]);

  // Making an an alternate A and B with B forwarding the direct dependents.
  Target a_fwd(&settings_, Label(SourceDir("//foo/"), "a_fwd"));
  a_fwd.set_output_type(Target::EXECUTABLE);
  Target b_fwd(&settings_, Label(SourceDir("//foo/"), "b_fwd"));
  b_fwd.set_output_type(Target::STATIC_LIBRARY);
  a_fwd.deps().push_back(&b_fwd);
  b_fwd.deps().push_back(&c);
  b_fwd.forward_dependent_configs().push_back(&c);

  b_fwd.OnResolved();
  a_fwd.OnResolved();

  // A_fwd should now have both configs.
  ASSERT_EQ(2u, a_fwd.configs().size());
  EXPECT_EQ(&all, a_fwd.configs()[0]);
  EXPECT_EQ(&direct, a_fwd.configs()[1]);
  ASSERT_EQ(1u, a_fwd.all_dependent_configs().size());
  EXPECT_EQ(&all, a_fwd.all_dependent_configs()[0]);
}
