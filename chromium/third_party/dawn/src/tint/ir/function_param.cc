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

#include "src/tint/ir/function_param.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::FunctionParam);

namespace tint::ir {

FunctionParam::FunctionParam(const type::Type* ty) : type_(ty) {
    TINT_ASSERT(IR, ty != nullptr);
}

FunctionParam::~FunctionParam() = default;

utils::StringStream& operator<<(utils::StringStream& out, enum FunctionParam::Builtin value) {
    switch (value) {
        case FunctionParam::Builtin::kVertexIndex:
            out << "vertex_index";
            break;
        case FunctionParam::Builtin::kInstanceIndex:
            out << "instance_index";
            break;
        case FunctionParam::Builtin::kPosition:
            out << "position";
            break;
        case FunctionParam::Builtin::kFrontFacing:
            out << "front_facing";
            break;
        case FunctionParam::Builtin::kLocalInvocationId:
            out << "local_invocation_id";
            break;
        case FunctionParam::Builtin::kLocalInvocationIndex:
            out << "local_invocation_index";
            break;
        case FunctionParam::Builtin::kGlobalInvocationId:
            out << "global_invocation_id";
            break;
        case FunctionParam::Builtin::kWorkgroupId:
            out << "workgroup_id";
            break;
        case FunctionParam::Builtin::kNumWorkgroups:
            out << "num_workgroups";
            break;
        case FunctionParam::Builtin::kSampleIndex:
            out << "sample_index";
            break;
        case FunctionParam::Builtin::kSampleMask:
            out << "sample_mask";
            break;
    }
    return out;
}

}  // namespace tint::ir
