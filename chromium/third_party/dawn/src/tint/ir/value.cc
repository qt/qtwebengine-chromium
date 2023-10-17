// Copyright 2022 The Tint Authors.
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

#include "src/tint/ir/value.h"

#include "src/tint/ir/constant.h"
#include "src/tint/ir/instruction.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::Value);

namespace tint::ir {

Value::Value() = default;

Value::~Value() = default;

void Value::Destroy() {
    TINT_ASSERT(IR, Alive());
    TINT_ASSERT(IR, Usages().Count() == 0);
    alive_ = false;
}

void Value::ReplaceAllUsesWith(std::function<Value*(Usage use)> replacer) {
    while (!uses_.IsEmpty()) {
        auto& use = *uses_.begin();
        auto* replacement = replacer(use);
        use.instruction->SetOperand(use.operand_index, replacement);
    }
}

void Value::ReplaceAllUsesWith(Value* replacement) {
    while (!uses_.IsEmpty()) {
        auto& use = *uses_.begin();
        use.instruction->SetOperand(use.operand_index, replacement);
    }
}

}  // namespace tint::ir
