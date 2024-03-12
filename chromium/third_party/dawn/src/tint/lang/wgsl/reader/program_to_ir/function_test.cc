// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gmock/gmock.h"
#include "src/tint/lang/core/constant/scalar.h"
#include "src/tint/lang/wgsl/ast/case_selector.h"
#include "src/tint/lang/wgsl/ast/int_literal_expression.h"
#include "src/tint/lang/wgsl/helpers/ir_program_test.h"

namespace tint::wgsl::reader {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using ProgramToIRFunctionTest = helpers::IRProgramTest;

TEST_F(ProgramToIRFunctionTest, EmitFunction_Vertex) {
    Func("test", tint::Empty, ty.vec4<f32>(), Vector{Return(Call<vec4<f32>>(0_f, 0_f, 0_f, 0_f))},
         Vector{Stage(ast::PipelineStage::kVertex)},
         Vector{Builtin(core::BuiltinValue::kPosition)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = @vertex func():vec4<f32> [@position] -> %b1 {
  %b1 = block {
    ret vec4<f32>(0.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_Fragment) {
    Func("test", tint::Empty, ty.void_(), tint::Empty,
         Vector{Stage(ast::PipelineStage::kFragment)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = @fragment func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_Compute) {
    Func("test", tint::Empty, ty.void_(), tint::Empty,
         Vector{Stage(ast::PipelineStage::kCompute), WorkgroupSize(8_i, 4_i, 2_i)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()),
              R"(%test = @compute @workgroup_size(8, 4, 2) func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_Return) {
    Func("test", tint::Empty, ty.vec3<f32>(), Vector{Return(Call<vec3<f32>>(0_f, 0_f, 0_f))},
         tint::Empty);

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = func():vec3<f32> -> %b1 {
  %b1 = block {
    ret vec3<f32>(0.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_UnreachableEnd_ReturnValue) {
    Func("test", tint::Empty, ty.f32(),
         Vector{If(true, Block(Return(0_f)), Else(Block(Return(1_f))))}, tint::Empty);

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = func():f32 -> %b1 {
  %b1 = block {
    if true [t: %b2, f: %b3] {  # if_1
      %b2 = block {  # true
        ret 0.0f
      }
      %b3 = block {  # false
        ret 1.0f
      }
    }
    unreachable
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnPosition) {
    Func("test", tint::Empty, ty.vec4<f32>(), Vector{Return(Call<vec4<f32>>(1_f, 2_f, 3_f, 4_f))},
         Vector{Stage(ast::PipelineStage::kVertex)},
         Vector{Builtin(core::BuiltinValue::kPosition)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = @vertex func():vec4<f32> [@position] -> %b1 {
  %b1 = block {
    ret vec4<f32>(1.0f, 2.0f, 3.0f, 4.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnPositionInvariant) {
    Func("test", tint::Empty, ty.vec4<f32>(), Vector{Return(Call<vec4<f32>>(1_f, 2_f, 3_f, 4_f))},
         Vector{Stage(ast::PipelineStage::kVertex)},
         Vector{Builtin(core::BuiltinValue::kPosition), Invariant()});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()),
              R"(%test = @vertex func():vec4<f32> [@invariant, @position] -> %b1 {
  %b1 = block {
    ret vec4<f32>(1.0f, 2.0f, 3.0f, 4.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnLocation) {
    Func("test", tint::Empty, ty.vec4<f32>(), Vector{Return(Call<vec4<f32>>(1_f, 2_f, 3_f, 4_f))},
         Vector{Stage(ast::PipelineStage::kFragment)}, Vector{Location(1_i)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()),
              R"(%test = @fragment func():vec4<f32> [@location(1)] -> %b1 {
  %b1 = block {
    ret vec4<f32>(1.0f, 2.0f, 3.0f, 4.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnLocation_Interpolate) {
    Func("test", tint::Empty, ty.vec4<f32>(), Vector{Return(Call<vec4<f32>>(1_f, 2_f, 3_f, 4_f))},
         Vector{Stage(ast::PipelineStage::kFragment)},
         Vector{Location(1_i), Interpolate(core::InterpolationType::kLinear,
                                           core::InterpolationSampling::kCentroid)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(
        Disassemble(m.Get()),
        R"(%test = @fragment func():vec4<f32> [@location(1), @interpolate(linear, centroid)] -> %b1 {
  %b1 = block {
    ret vec4<f32>(1.0f, 2.0f, 3.0f, 4.0f)
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnFragDepth) {
    Func("test", tint::Empty, ty.f32(), Vector{Return(1_f)},
         Vector{Stage(ast::PipelineStage::kFragment)},
         Vector{Builtin(core::BuiltinValue::kFragDepth)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = @fragment func():f32 [@frag_depth] -> %b1 {
  %b1 = block {
    ret 1.0f
  }
}
)");
}

TEST_F(ProgramToIRFunctionTest, EmitFunction_ReturnSampleMask) {
    Func("test", tint::Empty, ty.u32(), Vector{Return(1_u)},
         Vector{Stage(ast::PipelineStage::kFragment)},
         Vector{Builtin(core::BuiltinValue::kSampleMask)});

    auto m = Build();
    ASSERT_EQ(m, Success);

    EXPECT_EQ(Disassemble(m.Get()), R"(%test = @fragment func():u32 [@sample_mask] -> %b1 {
  %b1 = block {
    ret 1u
  }
}
)");
}

}  // namespace
}  // namespace tint::wgsl::reader
