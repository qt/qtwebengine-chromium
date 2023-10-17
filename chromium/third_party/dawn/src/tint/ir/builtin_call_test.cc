// Copyright 2023 The Tint Authors.
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

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "src/tint/ir/block_param.h"
#include "src/tint/ir/ir_test_helper.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT
using IR_BuiltinCallTest = IRTestHelper;

TEST_F(IR_BuiltinCallTest, Usage) {
    auto* arg1 = b.Constant(1_u);
    auto* arg2 = b.Constant(2_u);
    auto* builtin = b.Call(mod.Types().f32(), builtin::Function::kAbs, arg1, arg2);

    EXPECT_THAT(arg1->Usages(), testing::UnorderedElementsAre(Usage{builtin, 0u}));
    EXPECT_THAT(arg2->Usages(), testing::UnorderedElementsAre(Usage{builtin, 1u}));
}

TEST_F(IR_BuiltinCallTest, Result) {
    auto* arg1 = b.Constant(1_u);
    auto* arg2 = b.Constant(2_u);
    auto* builtin = b.Call(mod.Types().f32(), builtin::Function::kAbs, arg1, arg2);

    EXPECT_TRUE(builtin->HasResults());
    EXPECT_FALSE(builtin->HasMultiResults());
    EXPECT_TRUE(builtin->Result()->Is<InstructionResult>());
    EXPECT_EQ(builtin->Result()->Source(), builtin);
}

TEST_F(IR_BuiltinCallTest, Fail_NullType) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            b.Call(nullptr, builtin::Function::kAbs);
        },
        "");
}

TEST_F(IR_BuiltinCallTest, Fail_NoneFunction) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            b.Call(mod.Types().f32(), builtin::Function::kNone);
        },
        "");
}

TEST_F(IR_BuiltinCallTest, Fail_TintMaterializeFunction) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            b.Call(mod.Types().f32(), builtin::Function::kTintMaterialize);
        },
        "");
}

}  // namespace
}  // namespace tint::ir
