// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/spirv/reader/lower/vector_element_pointer.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"

namespace tint::spirv::reader::lower {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using SpirvReader_VectorElementPointerTest = core::ir::transform::TransformTest;

TEST_F(SpirvReader_VectorElementPointerTest, NonPointerAccess) {
    auto* vec = b.FunctionParam("vec", ty.vec4<u32>());
    auto* foo = b.Function("foo", ty.u32());
    b.Append(foo->Block(), [&] {
        auto* access = b.Access<u32>(vec, 2_u);
        b.Return(foo, access);
    });

    auto* src = R"(
%foo = func():u32 -> %b1 {
  %b1 = block {
    %2:u32 = access %vec, 2u
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, Access_NoIndices) {
    auto* foo = b.Function("foo", ty.vec4<u32>());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access = b.Access<ptr<function, vec4<u32>>>(vec);
        b.Return(foo, b.Load(access));
    });

    auto* src = R"(
%foo = func():vec4<u32> -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, vec4<u32>, read_write> = access %vec
    %4:vec4<u32> = load %3
    ret %4
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, Access_NoIndices_Chain) {
    auto* foo = b.Function("foo", ty.vec4<u32>());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access_1 = b.Access<ptr<function, vec4<u32>>>(vec);
        auto* access_2 = b.Access<ptr<function, vec4<u32>>>(access_1);
        auto* access_3 = b.Access<ptr<function, vec4<u32>>>(access_2);
        b.Return(foo, b.Load(access_3));
    });

    auto* src = R"(
%foo = func():vec4<u32> -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, vec4<u32>, read_write> = access %vec
    %4:ptr<function, vec4<u32>, read_write> = access %3
    %5:ptr<function, vec4<u32>, read_write> = access %4
    %6:vec4<u32> = load %5
    ret %6
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, Access_Component_NoUse) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        b.Access<ptr<function, u32>>(vec, 2_u);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, u32, read_write> = access %vec, 2u
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, Load) {
    auto* foo = b.Function("foo", ty.u32());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access = b.Access<ptr<function, u32>>(vec, 2_u);
        auto* load = b.Load(access);
        b.Return(foo, load);
    });

    auto* src = R"(
%foo = func():u32 -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, u32, read_write> = access %vec, 2u
    %4:u32 = load %3
    ret %4
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():u32 -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:u32 = load_vector_element %vec, 2u
    ret %3
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, Store) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access = b.Access<ptr<function, u32>>(vec, 2_u);
        b.Store(access, 42_u);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, u32, read_write> = access %vec, 2u
    store %3, 42u
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    store_vector_element %vec, 2u, 42u
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, AccessBeforeUse) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access_1 = b.Access<ptr<function, u32>>(vec, 2_u);
        auto* access_2 = b.Access<ptr<function, u32>>(access_1);
        b.Store(access_2, 42_u);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, u32, read_write> = access %vec, 2u
    %4:ptr<function, u32, read_write> = access %3
    store %4, 42u
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    store_vector_element %vec, 2u, 42u
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, MultipleUses) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* vec = b.Var<function, vec4<u32>>("vec");
        auto* access = b.Access<ptr<function, u32>>(vec, 2_u);
        auto* load = b.Load(access);
        auto* add = b.Add<u32>(load, 1_u);
        b.Store(access, add);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:ptr<function, u32, read_write> = access %vec, 2u
    %4:u32 = load %3
    %5:u32 = add %4, 1u
    store %3, %5
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %vec:ptr<function, vec4<u32>, read_write> = var
    %3:u32 = load_vector_element %vec, 2u
    %4:u32 = add %3, 1u
    store_vector_element %vec, 2u, %4
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, ViaMatrix) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* mat = b.Var<function, mat4x4<f32>>("mat");
        auto* access = b.Access<ptr<function, f32>>(mat, 1_u, 2_u);
        b.Store(access, 42_f);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %mat:ptr<function, mat4x4<f32>, read_write> = var
    %3:ptr<function, f32, read_write> = access %mat, 1u, 2u
    store %3, 42.0f
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %mat:ptr<function, mat4x4<f32>, read_write> = var
    %3:ptr<function, vec4<f32>, read_write> = access %mat, 1u
    store_vector_element %3, 2u, 42.0f
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, ViaArray) {
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* arr = b.Var<function, array<vec4<f32>, 4>>("arr");
        auto* access = b.Access<ptr<function, f32>>(arr, 1_u, 2_u);
        b.Store(access, 42_f);
        b.Return(foo);
    });

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %arr:ptr<function, array<vec4<f32>, 4>, read_write> = var
    %3:ptr<function, f32, read_write> = access %arr, 1u, 2u
    store %3, 42.0f
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    %arr:ptr<function, array<vec4<f32>, 4>, read_write> = var
    %3:ptr<function, vec4<f32>, read_write> = access %arr, 1u
    store_vector_element %3, 2u, 42.0f
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, ViaStruct) {
    auto* str_ty = ty.Struct(mod.symbols.New("str"), {{
                                                         mod.symbols.New("vec"),
                                                         ty.vec4<f32>(),
                                                     }});

    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* str = b.Var("str", ty.ptr<function>(str_ty));
        auto* access = b.Access<ptr<function, f32>>(str, 0_u, 2_u);
        b.Store(access, 42_f);
        b.Return(foo);
    });

    auto* src = R"(
str = struct @align(16) {
  vec:vec4<f32> @offset(0)
}

%foo = func():void -> %b1 {
  %b1 = block {
    %str:ptr<function, str, read_write> = var
    %3:ptr<function, f32, read_write> = access %str, 0u, 2u
    store %3, 42.0f
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
str = struct @align(16) {
  vec:vec4<f32> @offset(0)
}

%foo = func():void -> %b1 {
  %b1 = block {
    %str:ptr<function, str, read_write> = var
    %3:ptr<function, vec4<f32>, read_write> = access %str, 0u
    store_vector_element %3, 2u, 42.0f
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_VectorElementPointerTest, DeeplyNested) {
    auto* inner_arr = ty.array(ty.mat4x4<f32>(), 4);
    auto* str_ty = ty.Struct(mod.symbols.New("str"), {{
                                                         mod.symbols.New("inner"),
                                                         inner_arr,
                                                     }});
    auto* outer_arr = ty.array(str_ty, 4);

    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* arr = b.Var("arr", ty.ptr<function>(outer_arr));
        auto* access = b.Access<ptr<function, f32>>(arr, 1_u, 0_u, 3_u, 2_u, 1_u);
        b.Store(access, 42_f);
        b.Return(foo);
    });

    auto* src = R"(
str = struct @align(16) {
  inner:array<mat4x4<f32>, 4> @offset(0)
}

%foo = func():void -> %b1 {
  %b1 = block {
    %arr:ptr<function, array<str, 4>, read_write> = var
    %3:ptr<function, f32, read_write> = access %arr, 1u, 0u, 3u, 2u, 1u
    store %3, 42.0f
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
str = struct @align(16) {
  inner:array<mat4x4<f32>, 4> @offset(0)
}

%foo = func():void -> %b1 {
  %b1 = block {
    %arr:ptr<function, array<str, 4>, read_write> = var
    %3:ptr<function, vec4<f32>, read_write> = access %arr, 1u, 0u, 3u, 2u
    store_vector_element %3, 1u, 42.0f
    ret
  }
}
)";

    Run(VectorElementPointer);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::spirv::reader::lower
