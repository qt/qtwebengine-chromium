// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "gn/builder.h"
#include "gn/config.h"
#include "gn/loader.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "gn/toolchain.h"
#include "util/test/test.h"

namespace gn_builder_unittest {

class MockLoader : public Loader {
 public:
  MockLoader() = default;

  // Loader implementation:
  void Load(const SourceFile& file,
            const LocationRange& origin,
            const Label& toolchain_name) override {
    files_.push_back(file);
  }
  void ToolchainLoaded(const Toolchain* toolchain) override {}
  Label GetDefaultToolchain() const override { return Label(); }
  const Settings* GetToolchainSettings(const Label& label) const override {
    return nullptr;
  }
  SourceFile BuildFileForLabel(const Label& label) const override {
    return SourceFile(label.dir().value() + "BUILD.gn");
  }

  bool HasLoadedNone() const { return files_.empty(); }

  // Returns true if one/two loads have been requested and they match the given
  // file(s). This will clear the records so it will be empty for the next call.
  bool HasLoadedOne(const SourceFile& file) {
    if (files_.size() != 1u) {
      files_.clear();
      return false;
    }
    bool match = (files_[0] == file);
    files_.clear();
    return match;
  }
  bool HasLoadedTwo(const SourceFile& a, const SourceFile& b) {
    if (files_.size() != 2u) {
      files_.clear();
      return false;
    }

    bool match = ((files_[0] == a && files_[1] == b) ||
                  (files_[0] == b && files_[1] == a));
    files_.clear();
    return match;
  }
  bool HasLoadedOnce(const SourceFile& f) {
    return count(files_.begin(), files_.end(), f) == 1;
  }

 private:
  ~MockLoader() override = default;

  std::vector<SourceFile> files_;
};

class BuilderTest : public TestWithScheduler {
 public:
  BuilderTest()
      : loader_(new MockLoader),
        builder_(loader_.get()),
        settings_(&build_settings_, std::string()),
        scope_(&settings_) {
    build_settings_.SetBuildDir(SourceDir("//out/"));
    settings_.set_toolchain_label(Label(SourceDir("//tc/"), "default"));
    settings_.set_default_toolchain_label(settings_.toolchain_label());
  }

  Toolchain* DefineToolchain() {
    Toolchain* tc = new Toolchain(&settings_, settings_.toolchain_label());
    TestWithScope::SetupToolchain(tc);
    builder_.ItemDefined(std::unique_ptr<Item>(tc));
    return tc;
  }

 protected:
  scoped_refptr<MockLoader> loader_;
  Builder builder_;
  BuildSettings build_settings_;
  Settings settings_;
  Scope scope_;
};

TEST_F(BuilderTest, BasicDeps) {
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  // Construct a dependency chain: A -> B -> C. Define A first with a
  // forward-reference to B, then C, then B to test the different orders that
  // the dependencies are hooked up.
  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);

  // The builder will take ownership of the pointers.
  Target* a = new Target(&settings_, a_label);
  a->public_deps().push_back(LabelTargetPair(b_label));
  a->set_output_type(Target::EXECUTABLE);
  builder_.ItemDefined(std::unique_ptr<Item>(a));

  // Should have requested that B and the toolchain is loaded.
  EXPECT_TRUE(loader_->HasLoadedTwo(SourceFile("//tc/BUILD.gn"),
                                    SourceFile("//b/BUILD.gn")));

  // Define the toolchain.
  DefineToolchain();
  BuilderRecord* toolchain_record =
      builder_.GetRecord(settings_.toolchain_label());
  ASSERT_TRUE(toolchain_record);
  EXPECT_EQ(BuilderRecord::ITEM_TOOLCHAIN, toolchain_record->type());

  // A should be unresolved with an item
  BuilderRecord* a_record = builder_.GetRecord(a_label);
  EXPECT_TRUE(a_record->item());
  EXPECT_TRUE(a_record->is_defined());
  EXPECT_FALSE(a_record->is_resolved());
  EXPECT_FALSE(a_record->can_resolve());

  // B should be unresolved, have no item, and no deps.
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  EXPECT_FALSE(b_record->item());
  EXPECT_FALSE(b_record->is_defined());
  EXPECT_FALSE(b_record->is_resolved());
  EXPECT_FALSE(b_record->can_resolve());
  EXPECT_TRUE(b_record->all_deps().empty());

  // A should have two deps: B and the toolchain. Only B should be unresolved.
  EXPECT_EQ(2u, a_record->all_deps().size());
  EXPECT_TRUE(a_record->all_deps().contains(toolchain_record));
  EXPECT_TRUE(a_record->all_deps().contains(b_record));

  std::vector<const BuilderRecord*> a_unresolved =
      a_record->GetSortedUnresolvedDeps();
  EXPECT_EQ(1u, a_unresolved.size());
  EXPECT_EQ(a_unresolved[0], b_record);

  // B should be marked as having A waiting on it.
  EXPECT_EQ(1u, b_record->waiting_on_resolution().size());
  EXPECT_TRUE(b_record->waiting_on_resolution().contains(a_record));

  // Add the C target.
  Target* c = new Target(&settings_, c_label);
  c->set_output_type(Target::STATIC_LIBRARY);
  c->visibility().SetPublic();
  builder_.ItemDefined(std::unique_ptr<Item>(c));

  // C only depends on the already-loaded toolchain so we shouldn't have
  // requested anything else.
  EXPECT_TRUE(loader_->HasLoadedNone());

  // Add the B target.
  Target* b = new Target(&settings_, b_label);
  a->public_deps().push_back(LabelTargetPair(c_label));
  b->set_output_type(Target::SHARED_LIBRARY);
  b->visibility().SetPublic();
  builder_.ItemDefined(std::unique_ptr<Item>(b));
  scheduler().Run();

  // B depends only on the already-loaded C and toolchain so we shouldn't have
  // requested anything else.
  EXPECT_TRUE(loader_->HasLoadedNone());

  // All targets should now be resolved.
  BuilderRecord* c_record = builder_.GetRecord(c_label);
  EXPECT_TRUE(a_record->is_resolved());
  EXPECT_TRUE(b_record->is_resolved());
  EXPECT_TRUE(c_record->is_resolved());

  EXPECT_TRUE(a_record->GetSortedUnresolvedDeps().empty());
  EXPECT_TRUE(b_record->GetSortedUnresolvedDeps().empty());
  EXPECT_TRUE(c_record->GetSortedUnresolvedDeps().empty());

  EXPECT_TRUE(a_record->waiting_on_resolution().empty());
  EXPECT_TRUE(b_record->waiting_on_resolution().empty());
  EXPECT_TRUE(c_record->waiting_on_resolution().empty());
}

TEST_F(BuilderTest, SortedUnresolvedDeps) {
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  // Construct a dependency graph with:
  //    A -> B
  //    A -> D
  //    A -> C
  //
  // Ensure that the unresolved list of A is always [B, C, D]
  //
  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);
  Label d_label(SourceDir("//d/"), "d", toolchain_dir, toolchain_name);

  BuilderRecord* a_record = builder_.GetOrCreateRecordForTesting(a_label);
  BuilderRecord* b_record = builder_.GetOrCreateRecordForTesting(b_label);
  BuilderRecord* c_record = builder_.GetOrCreateRecordForTesting(c_label);
  BuilderRecord* d_record = builder_.GetOrCreateRecordForTesting(d_label);

  // Set A's state to Defined as this is required by AddDep() to know
  // which kind of update to perform internally. Don't provide an Item
  // though, as this test doesn't need it.
  a_record->SetDefined(nullptr);

  a_record->AddDep(b_record);
  a_record->AddDep(d_record);
  a_record->AddDep(c_record);

  std::vector<const BuilderRecord*> a_unresolved =
      a_record->GetSortedUnresolvedDeps();
  EXPECT_EQ(3u, a_unresolved.size()) << a_unresolved.size();
  EXPECT_EQ(b_record, a_unresolved[0]);
  EXPECT_EQ(c_record, a_unresolved[1]);
  EXPECT_EQ(d_record, a_unresolved[2]);
}

// Tests that the "should generate" flag is set and propagated properly.
TEST_F(BuilderTest, ShouldGenerate) {
  DefineToolchain();

  // Define a secondary toolchain.
  Settings settings2(&build_settings_, "secondary/");
  Label toolchain_label2(SourceDir("//tc/"), "secondary");
  settings2.set_toolchain_label(toolchain_label2);
  Toolchain* tc2 = new Toolchain(&settings2, toolchain_label2);
  TestWithScope::SetupToolchain(tc2);
  builder_.ItemDefined(std::unique_ptr<Item>(tc2));

  // Construct a dependency chain: A -> B. A is in the default toolchain, B
  // is not.
  Label a_label(SourceDir("//foo/"), "a", settings_.toolchain_label().dir(),
                "a");
  Label b_label(SourceDir("//foo/"), "b", toolchain_label2.dir(),
                toolchain_label2.name());

  // First define B.
  Target* b = new Target(&settings2, b_label);
  b->visibility().SetPublic();
  b->set_output_type(Target::EXECUTABLE);
  builder_.ItemDefined(std::unique_ptr<Item>(b));

  // B should not be marked generated by default.
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  EXPECT_FALSE(b_record->should_generate());

  // Define A with a dependency on B.
  Target* a = new Target(&settings_, a_label);
  a->public_deps().push_back(LabelTargetPair(b_label));
  a->set_output_type(Target::EXECUTABLE);
  builder_.ItemDefined(std::unique_ptr<Item>(a));
  scheduler().Run();

  // A should have the generate bit set since it's in the default toolchain.
  BuilderRecord* a_record = builder_.GetRecord(a_label);
  EXPECT_TRUE(a_record->should_generate());

  // It should have gotten pushed to B.
  EXPECT_TRUE(b_record->should_generate());
}

// Test that "gen_deps" forces targets to be generated.
TEST_F(BuilderTest, GenDeps) {
  DefineToolchain();

  // Define another toolchain
  Settings settings2(&build_settings_, "alternate/");
  Label alt_tc(SourceDir("//tc/"), "alternate");
  settings2.set_toolchain_label(alt_tc);
  Toolchain* tc2 = new Toolchain(&settings2, alt_tc);
  TestWithScope::SetupToolchain(tc2);
  builder_.ItemDefined(std::unique_ptr<Item>(tc2));

  // Construct the dependency chain A -> B -gen-> C -gen-> D where A is the only
  // target in the default toolchain. This should cause all 4 targets to be
  // generated.
  Label a_label(SourceDir("//a/"), "a", settings_.toolchain_label().dir(),
                settings_.toolchain_label().name());
  Label b_label(SourceDir("//b/"), "b", alt_tc.dir(), alt_tc.name());
  Label c_label(SourceDir("//c/"), "c", alt_tc.dir(), alt_tc.name());
  Label d_label(SourceDir("//d/"), "d", alt_tc.dir(), alt_tc.name());

  Target* c = new Target(&settings2, c_label);
  c->set_output_type(Target::EXECUTABLE);
  c->gen_deps().push_back(LabelTargetPair(d_label));
  builder_.ItemDefined(std::unique_ptr<Item>(c));

  Target* b = new Target(&settings2, b_label);
  b->set_output_type(Target::EXECUTABLE);
  b->visibility().SetPublic();  // Allow 'a' to depend on 'b'
  b->gen_deps().push_back(LabelTargetPair(c_label));
  builder_.ItemDefined(std::unique_ptr<Item>(b));

  Target* a = new Target(&settings_, a_label);
  a->set_output_type(Target::EXECUTABLE);
  a->private_deps().push_back(LabelTargetPair(b_label));
  builder_.ItemDefined(std::unique_ptr<Item>(a));

  // At this point, "should generate" should have propogated to C which should
  // request for D to be loaded
  EXPECT_TRUE(loader_->HasLoadedOnce(SourceFile("//d/BUILD.gn")));

  Target* d = new Target(&settings2, d_label);
  d->set_output_type(Target::EXECUTABLE);
  builder_.ItemDefined(std::unique_ptr<Item>(d));
  scheduler().Run();

  BuilderRecord* a_record = builder_.GetRecord(a_label);
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  BuilderRecord* c_record = builder_.GetRecord(c_label);
  BuilderRecord* d_record = builder_.GetRecord(d_label);
  EXPECT_TRUE(a_record->should_generate());
  EXPECT_TRUE(b_record->should_generate());
  EXPECT_TRUE(c_record->should_generate());
  EXPECT_TRUE(d_record->should_generate());
}

// Test that circular dependencies between gen_deps and deps are allowed
TEST_F(BuilderTest, GenDepsCircle) {
  DefineToolchain();
  Settings settings2(&build_settings_, "alternate/");
  Label alt_tc(SourceDir("//tc/"), "alternate");
  settings2.set_toolchain_label(alt_tc);
  Toolchain* tc2 = new Toolchain(&settings2, alt_tc);
  TestWithScope::SetupToolchain(tc2);
  builder_.ItemDefined(std::unique_ptr<Item>(tc2));

  // A is in the default toolchain and lists B as a gen_dep
  // B is in an alternate toolchain and lists A as a normal dep
  Label a_label(SourceDir("//a/"), "a", settings_.toolchain_label().dir(),
                settings_.toolchain_label().name());
  Label b_label(SourceDir("//b/"), "b", alt_tc.dir(), alt_tc.name());

  Target* a = new Target(&settings_, a_label);
  a->gen_deps().push_back(LabelTargetPair(b_label));
  a->set_output_type(Target::EXECUTABLE);
  a->visibility().SetPublic();  // Allow 'b' to depend on 'a'
  builder_.ItemDefined(std::unique_ptr<Item>(a));

  Target* b = new Target(&settings2, b_label);
  b->private_deps().push_back(LabelTargetPair(a_label));
  b->set_output_type(Target::EXECUTABLE);
  builder_.ItemDefined(std::unique_ptr<Item>(b));
  scheduler().Run();

  Err err;
  EXPECT_TRUE(builder_.CheckForBadItems(&err));
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  EXPECT_TRUE(b_record->should_generate());
}

// Tests that configs applied to a config get loaded (bug 536844).
TEST_F(BuilderTest, ConfigLoad) {
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  // Construct a dependency chain: A -> B -> C. Define A first with a
  // forward-reference to B, then C, then B to test the different orders that
  // the dependencies are hooked up.
  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);

  // The builder will take ownership of the pointers.
  Config* a = new Config(&settings_, a_label);
  a->configs().push_back(LabelConfigPair(b_label));
  builder_.ItemDefined(std::unique_ptr<Item>(a));

  // Should have requested that B is loaded.
  EXPECT_TRUE(loader_->HasLoadedOne(SourceFile("//b/BUILD.gn")));
}

// Tests that "validations" dependencies behave correctly:
// 1. They trigger the loading of the validated target's build file (simulating
// cross-directory deps).
// 2. The validator waits for the validation target to be DEFINED, not RESOLVED.
// 3. This allows cycles (A validates B, B depends on A) to resolve without
// error.
TEST_F(BuilderTest, Validations) {
  DefineToolchain();
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);

  // Define A with validatation B.
  auto a = std::make_unique<Target>(&settings_, a_label);
  Target* a_ptr = a.get();
  a_ptr->set_output_type(Target::ACTION);
  a_ptr->visibility().SetPublic();
  a_ptr->validations().push_back(LabelTargetPair(b_label));
  builder_.ItemDefined(std::move(a));

  // Should have requested that B is loaded.
  EXPECT_TRUE(loader_->HasLoadedOne(SourceFile("//b/BUILD.gn")));

  // A should NOT be resolved yet (waiting for B definition).
  BuilderRecord* a_record = builder_.GetRecord(a_label);
  EXPECT_TRUE(a_record);
  EXPECT_FALSE(a_record->is_resolved());

  // Define B. B depends on A.
  auto b = std::make_unique<Target>(&settings_, b_label);
  Target* b_ptr = b.get();
  b_ptr->set_output_type(Target::ACTION);
  b_ptr->visibility().SetPublic();
  b_ptr->private_deps().push_back(LabelTargetPair(a_label));
  builder_.ItemDefined(std::move(b));

  scheduler().Run();

  // Now both should be resolved.
  EXPECT_TRUE(a_record->is_resolved());
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  EXPECT_TRUE(b_record->is_resolved());

  // There should be no cycle.
  Err err;
  EXPECT_TRUE(builder_.CheckForBadItems(&err));
  EXPECT_FALSE(err.has_error()) << "CheckForBadItems error: " << err.message();

  // A should have B in its validations.
  ASSERT_EQ(1u, a_ptr->validations().size());
  EXPECT_EQ(b_ptr, a_ptr->validations()[0].ptr);
}

// Tests that validation dependencies block finalization until resolved.
TEST_F(BuilderTest, ValidationsBlockWriting) {
  DefineToolchain();
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);

  // Define A. A lists B in validations.
  auto a = std::make_unique<Target>(&settings_, a_label);
  a->set_output_type(Target::ACTION);
  a->validations().push_back(LabelTargetPair(b_label));
  a->visibility().SetPublic();
  builder_.ItemDefined(std::move(a));

  // Define B. B depends on C.
  auto b = std::make_unique<Target>(&settings_, b_label);
  b->set_output_type(Target::ACTION);
  b->private_deps().push_back(LabelTargetPair(c_label));
  b->visibility().SetPublic();
  builder_.ItemDefined(std::move(b));

  // C is unresolved (waiting on definition).
  // B is unresolved (waiting on C).
  // A should be RESOLVED (because B is defined, and validations only wait on
  // definition for resolution). BUT A should not be FINALIZED because B
  // is not resolved.

  BuilderRecord* a_record = builder_.GetRecord(a_label);
  BuilderRecord* b_record = builder_.GetRecord(b_label);
  BuilderRecord* c_record = builder_.GetRecord(c_label);

  scheduler().Run();

  EXPECT_FALSE(c_record->is_resolved());
  EXPECT_FALSE(b_record->is_resolved());
  EXPECT_TRUE(a_record->is_resolved());
  EXPECT_FALSE(a_record->is_finalized());

  // Check that B knows A is waiting on it for writing.
  EXPECT_TRUE(b_record->waiting_on_validation_resolution().contains(a_record));
}

// Tests that a target can validate a target that depends on it.
// A -> validations -> B
// B -> deps -> A
TEST_F(BuilderTest, ValidationsWithCycle) {
  DefineToolchain();
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);

  // Define A. A lists B in validations.
  auto a = std::make_unique<Target>(&settings_, a_label);
  a->set_output_type(Target::ACTION);
  a->validations().push_back(LabelTargetPair(b_label));
  a->visibility().SetPublic();
  builder_.ItemDefined(std::move(a));

  // Define B. B depends on A.
  auto b = std::make_unique<Target>(&settings_, b_label);
  b->set_output_type(Target::ACTION);
  b->private_deps().push_back(LabelTargetPair(a_label));
  b->visibility().SetPublic();
  builder_.ItemDefined(std::move(b));

  BuilderRecord* a_record = builder_.GetRecord(a_label);
  BuilderRecord* b_record = builder_.GetRecord(b_label);

  scheduler().Run();

  // Both should be resolved.
  EXPECT_TRUE(a_record->is_resolved());
  EXPECT_TRUE(b_record->is_resolved());

  // There should be no errors (cycle detection passed).
  Err err;
  EXPECT_TRUE(builder_.CheckForBadItems(&err));
  EXPECT_SUCCESS(err);
}

// Tests that if a validation resolves, the target does NOT write if it
// is still waiting on other dependencies to resolve.
// A -> deps -> B
// A -> validations -> C
// Sequence: C finalizes. A should NOT be finalized. B resolves. A is finalized.
TEST_F(BuilderTest, ValidationsPrematureWrite) {
  DefineToolchain();
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);

  // Track written targets.
  std::vector<const BuilderRecord*> written;
  builder_.set_resolved_and_generated_callback(
      [&written](const BuilderRecord* record) { written.push_back(record); });

  // Define A. A depends on B and has validation C.
  auto a = std::make_unique<Target>(&settings_, a_label);
  a->set_output_type(Target::ACTION);
  a->private_deps().push_back(LabelTargetPair(b_label));
  a->validations().push_back(LabelTargetPair(c_label));
  a->visibility().SetPublic();
  builder_.ItemDefined(std::move(a));

  // Define C. C is standalone.
  auto c = std::make_unique<Target>(&settings_, c_label);
  c->set_output_type(Target::ACTION);
  c->visibility().SetPublic();
  builder_.ItemDefined(std::move(c));

  // C should resolve then finalize. A is waiting on B.
  scheduler().Run();

  BuilderRecord* a_record = builder_.GetRecord(a_label);
  BuilderRecord* c_record = builder_.GetRecord(c_label);

  EXPECT_TRUE(c_record->is_resolved());
  EXPECT_TRUE(c_record->is_finalized());
  EXPECT_FALSE(a_record->is_resolved());

  // Only C should be written.
  ASSERT_EQ(1u, written.size());
  EXPECT_EQ(c_record, written[0]);
  EXPECT_TRUE(c_record->is_finalized());
  EXPECT_FALSE(a_record->is_finalized());

  // Define B.
  auto b = std::make_unique<Target>(&settings_, b_label);
  b->set_output_type(Target::ACTION);
  b->visibility().SetPublic();
  builder_.ItemDefined(std::move(b));
  BuilderRecord* b_record = builder_.GetRecord(b_label);

  scheduler().Run();

  EXPECT_TRUE(a_record->is_resolved());
  EXPECT_TRUE(a_record->is_finalized());

  // Order should be C, B, A.
  ASSERT_EQ(3u, written.size());
  EXPECT_EQ(c_record, written[0]);
  EXPECT_EQ(b_record, written[1]);
  EXPECT_EQ(a_record, written[2]);
}

// Tests that RecursiveSetShouldGenerate does not trigger a write callback
// if the target is waiting on validations (can_write() is false).
TEST_F(BuilderTest, RecursiveShouldGenerateWithValidations) {
  DefineToolchain();
  SourceDir toolchain_dir = settings_.toolchain_label().dir();
  std::string toolchain_name = settings_.toolchain_label().name();

  // Set root patterns to something that doesn't match A, B, etc.
  // This ensures they are not generated by default.
  std::vector<LabelPattern> dummy_patterns;
  dummy_patterns.push_back(
      LabelPattern(LabelPattern::MATCH, SourceDir("//c/"), "c", Label()));
  build_settings_.SetRootPatterns(std::move(dummy_patterns));

  Label a_label(SourceDir("//a/"), "a", toolchain_dir, toolchain_name);
  Label b_label(SourceDir("//b/"), "b", toolchain_dir, toolchain_name);
  Label c_label(SourceDir("//c/"), "c", toolchain_dir, toolchain_name);
  Label e_label(SourceDir("//e/"), "e", toolchain_dir, toolchain_name);

  // Track written targets.
  std::vector<const BuilderRecord*> written;
  builder_.set_resolved_and_generated_callback(
      [&written](const BuilderRecord* record) { written.push_back(record); });

  // Define A with validation B.
  auto a = std::make_unique<Target>(&settings_, a_label);
  a->set_output_type(Target::GROUP);
  a->validations().push_back(LabelTargetPair(b_label));
  a->visibility().SetPublic();
  builder_.ItemDefined(std::move(a));
  BuilderRecord* a_record = builder_.GetRecord(a_label);

  // Define B with dependency E delay resolution.
  auto b = std::make_unique<Target>(&settings_, b_label);
  b->set_output_type(Target::ACTION);
  b->private_deps().push_back(LabelTargetPair(e_label));
  b->visibility().SetPublic();
  builder_.ItemDefined(std::move(b));
  BuilderRecord* b_record = builder_.GetRecord(b_label);

  // A resolves (validations don't block resolution) but does not finalize.
  // B waits for E.
  scheduler().Run();

  EXPECT_TRUE(a_record->is_resolved());
  EXPECT_FALSE(a_record->is_finalized());
  EXPECT_FALSE(a_record->should_generate());
  EXPECT_FALSE(b_record->is_resolved());
  EXPECT_TRUE(written.empty());

  // Define C depending on A.
  // C will generate because root pattern matches.
  auto c = std::make_unique<Target>(&settings_, c_label);
  c->set_output_type(Target::GROUP);
  c->public_deps().push_back(LabelTargetPair(a_label));
  c->visibility().SetPublic();
  builder_.ItemDefined(std::move(c));
  BuilderRecord* c_record = builder_.GetRecord(c_label);

  scheduler().Run();

  EXPECT_TRUE(c_record->should_generate());
  EXPECT_TRUE(a_record->should_generate());

  // C cannot be written because A is not finalized.
  EXPECT_TRUE(c_record->is_resolved());
  EXPECT_FALSE(c_record->is_finalized());

  // Nothing could be written for now.
  EXPECT_EQ(written.size(), 0u);

  // Define E to cause B to resolve and write A.
  auto e = std::make_unique<Target>(&settings_, e_label);
  e->set_output_type(Target::ACTION);
  e->visibility().SetPublic();
  builder_.ItemDefined(std::move(e));
  BuilderRecord* e_record = builder_.GetRecord(e_label);

  scheduler().Run();

  // Due to the way the Builder is implemented, here's what happens:
  //
  // E is resolved, notifies B immediately.
  // B is resolved, notifies A immediately.
  // A is finalized, writes, notifies C
  // C writes.
  // A returns from finalization.
  // B returns from resolution.
  // E returns from resolution.
  // E finalizes, notifies B
  // B finalizes.
  ASSERT_EQ(written.size(), 4u);
  EXPECT_EQ(written[0], a_record);
  EXPECT_EQ(written[1], c_record);
  EXPECT_EQ(written[2], e_record);
  EXPECT_EQ(written[3], b_record);
}

}  // namespace gn_builder_unittest
