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

#ifndef SRC_TINT_IR_EXIT_IF_H_
#define SRC_TINT_IR_EXIT_IF_H_

#include "src/tint/ir/exit.h"
#include "src/tint/utils/castable.h"

// Forward declarations
namespace tint::ir {
class If;
}  // namespace tint::ir

namespace tint::ir {

/// A exit if instruction.
class ExitIf : public utils::Castable<ExitIf, Exit> {
  public:
    /// The base offset in Operands() for the args
    static constexpr size_t kArgsOperandOffset = 0;

    /// Constructor
    /// @param i the if being exited
    /// @param args the branch arguments
    explicit ExitIf(ir::If* i, utils::VectorRef<Value*> args = utils::Empty);
    ~ExitIf() override;

    /// Re-associates the exit with the given if instruction
    /// @param i the new If to exit from
    void SetIf(ir::If* i);

    /// @returns the if being exited
    ir::If* If();
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_EXIT_IF_H_
