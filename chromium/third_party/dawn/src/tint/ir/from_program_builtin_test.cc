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
#include "src/tint/ast/case_selector.h"
#include "src/tint/ast/int_literal_expression.h"
#include "src/tint/constant/scalar.h"
#include "src/tint/ir/program_test_helper.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT

using IR_FromProgramBuiltinTest = ProgramTestHelper;

TEST_F(IR_FromProgramBuiltinTest, EmitExpression_Builtin) {
    auto i = GlobalVar("i", builtin::AddressSpace::kPrivate, Expr(1_f));
    auto* expr = Call("asin", i);
    WrapInFunction(expr);

    auto m = Build();
    ASSERT_TRUE(m) << (!m ? m.Failure() : "");

    EXPECT_EQ(Disassemble(m.Get()), R"(# Root block
%b1 = block {
  %i:ptr<private, f32, read_write> = var, 1.0f
}

%test_function = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:f32 = load %i
    %tint_symbol:f32 = asin %3
    ret
  }
}
)");
}

}  // namespace
}  // namespace tint::ir
