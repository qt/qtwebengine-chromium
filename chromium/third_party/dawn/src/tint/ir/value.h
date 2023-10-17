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

#ifndef SRC_TINT_IR_VALUE_H_
#define SRC_TINT_IR_VALUE_H_

#include "src/tint/type/type.h"
#include "src/tint/utils/castable.h"
#include "src/tint/utils/hashset.h"

// Forward declarations
namespace tint::ir {
class Instruction;
}  // namespace tint::ir

namespace tint::ir {

/// A specific usage of a Value in the IR.
struct Usage {
    /// The instruction that is using the value;
    Instruction* instruction = nullptr;
    /// The index of the operand that is the value being used.
    size_t operand_index = 0u;

    /// A specialization of utils::Hasher for Usage.
    struct Hasher {
        /// @param u the usage to hash
        /// @returns a hash of the usage
        inline std::size_t operator()(const Usage& u) const {
            return utils::Hash(u.instruction, u.operand_index);
        }
    };

    /// An equality helper for Usage.
    /// @param other the usage to compare against
    /// @returns true if the two usages are equal
    bool operator==(const Usage& other) const {
        return instruction == other.instruction && operand_index == other.operand_index;
    }
};

/// Value in the IR.
class Value : public utils::Castable<Value> {
  public:
    /// Destructor
    ~Value() override;

    /// @returns the type of the value
    virtual const type::Type* Type() { return nullptr; }

    /// Destroys the Value. Once called, the Value must not be used again.
    /// The Value must not be in use by any instruction.
    virtual void Destroy();

    /// @returns true if the Value has not been destroyed with Destroy()
    bool Alive() const { return alive_; }

    /// Adds a usage of this value.
    /// @param u the usage
    void AddUsage(Usage u) { uses_.Add(u); }

    /// Remove a usage of this value.
    /// @param u the usage
    void RemoveUsage(Usage u) { uses_.Remove(u); }

    /// @returns the set of usages of this value. An instruction may appear multiple times if it
    /// uses the value for multiple different operands.
    const utils::Hashset<Usage, 4, Usage::Hasher>& Usages() { return uses_; }

    /// Replace all uses of the value.
    /// @param replacer a function which returns a replacement for a given use
    void ReplaceAllUsesWith(std::function<Value*(Usage use)> replacer);

    /// Replace all uses of the value.
    /// @param replacement the replacement value
    void ReplaceAllUsesWith(Value* replacement);

  protected:
    /// Constructor
    Value();

  private:
    utils::Hashset<Usage, 4, Usage::Hasher> uses_;
    bool alive_ = true;
};
}  // namespace tint::ir

#endif  // SRC_TINT_IR_VALUE_H_
