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

#include "src/tint/writer/spirv/ir/test_helper_ir.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::spirv {
namespace {

TEST_F(SpvGeneratorImplTest, Switch_Basic) {
    auto* func = b.Function("foo", ty.void_());

    auto* swtch = b.Switch(42_i);

    auto* def_case = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector()});
    def_case->Append(b.ExitSwitch(swtch));

    func->StartTarget()->Append(swtch);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %8 None
OpSwitch %6 %5
%5 = OpLabel
OpBranch %8
%8 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_MultipleCases) {
    auto* func = b.Function("foo", ty.void_());

    auto* swtch = b.Switch(42_i);

    auto* case_a = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)}});
    case_a->Append(b.ExitSwitch(swtch));

    auto* case_b = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.ExitSwitch(swtch));

    auto* def_case = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector()});
    def_case->Append(b.ExitSwitch(swtch));

    func->StartTarget()->Append(swtch);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %10 None
OpSwitch %6 %5 1 %8 2 %9
%8 = OpLabel
OpBranch %10
%9 = OpLabel
OpBranch %10
%5 = OpLabel
OpBranch %10
%10 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_MultipleSelectorsPerCase) {
    auto* func = b.Function("foo", ty.void_());

    auto* swtch = b.Switch(42_i);

    auto* case_a = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)},
                                               ir::Switch::CaseSelector{b.Constant(3_i)}});
    case_a->Append(b.ExitSwitch(swtch));

    auto* case_b = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)},
                                               ir::Switch::CaseSelector{b.Constant(4_i)}});
    case_b->Append(b.ExitSwitch(swtch));

    auto* def_case = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(5_i)},
                                                 ir::Switch::CaseSelector()});
    def_case->Append(b.ExitSwitch(swtch));

    func->StartTarget()->Append(swtch);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %10 None
OpSwitch %6 %5 1 %8 3 %8 2 %9 4 %9 5 %5
%8 = OpLabel
OpBranch %10
%9 = OpLabel
OpBranch %10
%5 = OpLabel
OpBranch %10
%10 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_AllCasesReturn) {
    auto* func = b.Function("foo", ty.void_());

    auto* swtch = b.Switch(42_i);

    auto* case_a = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)}});
    case_a->Append(b.Return(func));

    auto* case_b = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.Return(func));

    auto* def_case = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector()});
    def_case->Append(b.Return(func));

    func->StartTarget()->Append(swtch);
    func->StartTarget()->Append(b.Unreachable());

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %10 None
OpSwitch %6 %5 1 %8 2 %9
%8 = OpLabel
OpReturn
%9 = OpLabel
OpReturn
%5 = OpLabel
OpReturn
%10 = OpLabel
OpUnreachable
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_ConditionalBreak) {
    auto* func = b.Function("foo", ty.void_());

    auto* swtch = b.Switch(42_i);

    auto* cond_break = b.If(true);
    cond_break->True()->Append(b.ExitSwitch(swtch));
    cond_break->False()->Append(b.ExitIf(cond_break));

    auto* case_a = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)}});
    case_a->Append(cond_break);
    case_a->Append(b.Return(func));

    auto* def_case = b.Case(swtch, utils::Vector{ir::Switch::CaseSelector()});
    def_case->Append(b.ExitSwitch(swtch));

    func->StartTarget()->Append(swtch);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%13 = OpTypeBool
%12 = OpConstantTrue %13
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %9 None
OpSwitch %6 %5 1 %8
%8 = OpLabel
OpSelectionMerge %10 None
OpBranchConditional %12 %11 %10
%11 = OpLabel
OpBranch %9
%10 = OpLabel
OpReturn
%5 = OpLabel
OpBranch %9
%9 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_Phi_SingleValue) {
    auto* func = b.Function("foo", ty.i32());

    auto* s = b.Switch(42_i);
    s->SetResults(b.InstructionResult(ty.i32()));
    auto* case_a = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)},
                                           ir::Switch::CaseSelector{nullptr}});
    case_a->Append(b.ExitSwitch(s, 10_i));

    auto* case_b = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.ExitSwitch(s, 20_i));

    func->StartTarget()->Append(s);
    func->StartTarget()->Append(b.Return(func, s));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeInt 32 1
%3 = OpTypeFunction %2
%6 = OpConstant %2 42
%10 = OpConstant %2 10
%11 = OpConstant %2 20
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %8 None
OpSwitch %6 %5 1 %5 2 %7
%5 = OpLabel
OpBranch %8
%7 = OpLabel
OpBranch %8
%8 = OpLabel
%9 = OpPhi %2 %10 %5 %11 %7
OpReturnValue %9
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_Phi_SingleValue_CaseReturn) {
    auto* func = b.Function("foo", ty.i32());

    auto* s = b.Switch(42_i);
    s->SetResults(b.InstructionResult(ty.i32()));
    auto* case_a = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)},
                                           ir::Switch::CaseSelector{nullptr}});
    case_a->Append(b.Return(func, 10_i));

    auto* case_b = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.ExitSwitch(s, 20_i));

    func->StartTarget()->Append(s);
    func->StartTarget()->Append(b.Return(func, s));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeInt 32 1
%3 = OpTypeFunction %2
%6 = OpConstant %2 42
%9 = OpConstant %2 10
%11 = OpConstant %2 20
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %8 None
OpSwitch %6 %5 1 %5 2 %7
%5 = OpLabel
OpReturnValue %9
%7 = OpLabel
OpBranch %8
%8 = OpLabel
%10 = OpPhi %2 %11 %7
OpReturnValue %10
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_Phi_MultipleValue_0) {
    auto* func = b.Function("foo", ty.i32());

    auto* s = b.Switch(42_i);
    s->SetResults(b.InstructionResult(ty.i32()), b.InstructionResult(ty.bool_()));
    auto* case_a = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)},
                                           ir::Switch::CaseSelector{nullptr}});
    case_a->Append(b.ExitSwitch(s, 10_i, true));

    auto* case_b = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.ExitSwitch(s, 20_i, false));

    func->StartTarget()->Append(s);
    func->StartTarget()->Append(b.Return(func, s->Result(0)));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeInt 32 1
%3 = OpTypeFunction %2
%6 = OpConstant %2 42
%10 = OpConstant %2 10
%11 = OpConstant %2 20
%12 = OpTypeBool
%14 = OpConstantTrue %12
%15 = OpConstantFalse %12
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %8 None
OpSwitch %6 %5 1 %5 2 %7
%5 = OpLabel
OpBranch %8
%7 = OpLabel
OpBranch %8
%8 = OpLabel
%9 = OpPhi %2 %10 %5 %11 %7
%13 = OpPhi %12 %14 %5 %15 %7
OpReturnValue %9
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Switch_Phi_MultipleValue_1) {
    auto* func = b.Function("foo", ty.bool_());

    auto* s = b.Switch(b.Constant(42_i));
    s->SetResults(b.InstructionResult(ty.i32()), b.InstructionResult(ty.bool_()));
    auto* case_a = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(1_i)},
                                           ir::Switch::CaseSelector{nullptr}});
    case_a->Append(b.ExitSwitch(s, 10_i, true));

    auto* case_b = b.Case(s, utils::Vector{ir::Switch::CaseSelector{b.Constant(2_i)}});
    case_b->Append(b.ExitSwitch(s, 20_i, false));

    func->StartTarget()->Append(s);
    func->StartTarget()->Append(b.Return(func, s->Result(1)));

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeBool
%3 = OpTypeFunction %2
%7 = OpTypeInt 32 1
%6 = OpConstant %7 42
%11 = OpConstant %7 10
%12 = OpConstant %7 20
%14 = OpConstantTrue %2
%15 = OpConstantFalse %2
%1 = OpFunction %2 None %3
%4 = OpLabel
OpSelectionMerge %9 None
OpSwitch %6 %5 1 %5 2 %8
%5 = OpLabel
OpBranch %9
%8 = OpLabel
OpBranch %9
%9 = OpLabel
%10 = OpPhi %7 %11 %5 %12 %8
%13 = OpPhi %2 %14 %5 %15 %8
OpReturnValue %13
OpFunctionEnd
)");
}

}  // namespace
}  // namespace tint::writer::spirv
