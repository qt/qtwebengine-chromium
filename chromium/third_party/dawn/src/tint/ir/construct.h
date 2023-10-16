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

#ifndef SRC_TINT_IR_CONSTRUCT_H_
#define SRC_TINT_IR_CONSTRUCT_H_

#include "src/tint/ir/call.h"
#include "src/tint/symbol_table.h"
#include "src/tint/type/type.h"
#include "src/tint/utils/castable.h"
#include "src/tint/utils/string_stream.h"

namespace tint::ir {

/// A constructor instruction in the IR.
class Construct : public utils::Castable<Construct, Call> {
  public:
    /// Constructor
    /// @param result the result value
    /// @param args the constructor arguments
    Construct(Value* result, utils::VectorRef<Value*> args);
    Construct(const Construct& instr) = delete;
    Construct(Construct&& instr) = delete;
    ~Construct() override;

    Construct& operator=(const Construct& instr) = delete;
    Construct& operator=(Construct&& instr) = delete;

    /// Write the instruction to the given stream
    /// @param out the stream to write to
    /// @returns the stream
    utils::StringStream& ToString(utils::StringStream& out) const override;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_CONSTRUCT_H_
