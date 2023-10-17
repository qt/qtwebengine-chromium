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

#ifndef SRC_TINT_IR_FUNCTION_PARAM_H_
#define SRC_TINT_IR_FUNCTION_PARAM_H_

#include <utility>

#include "src/tint/ir/binding_point.h"
#include "src/tint/ir/location.h"
#include "src/tint/ir/value.h"
#include "src/tint/utils/castable.h"
#include "src/tint/utils/vector.h"

namespace tint::ir {

/// A function parameter in the IR.
class FunctionParam : public utils::Castable<FunctionParam, Value> {
  public:
    /// Builtin attribute
    enum class Builtin {
        /// Builtin Vertex index
        kVertexIndex,
        /// Builtin Instance index
        kInstanceIndex,
        /// Builtin Position
        kPosition,
        /// Builtin FrontFacing
        kFrontFacing,
        /// Builtin Local invocation id
        kLocalInvocationId,
        /// Builtin Local invocation index
        kLocalInvocationIndex,
        /// Builtin Global invocation id
        kGlobalInvocationId,
        /// Builtin Workgroup id
        kWorkgroupId,
        /// Builtin Num workgroups
        kNumWorkgroups,
        /// Builtin Sample index
        kSampleIndex,
        /// Builtin Sample mask
        kSampleMask,
    };

    /// Constructor
    /// @param type the type of the var
    explicit FunctionParam(const type::Type* type);
    ~FunctionParam() override;

    /// @returns the type of the var
    const type::Type* Type() override { return type_; }

    /// Sets the builtin information. Note, it is currently an error if the builtin is already set.
    /// @param val the builtin to set
    void SetBuiltin(FunctionParam::Builtin val) {
        TINT_ASSERT(IR, !builtin_.has_value());
        builtin_ = val;
    }
    /// @returns the builtin set for the parameter
    std::optional<FunctionParam::Builtin> Builtin() { return builtin_; }

    /// Sets the parameter as invariant
    /// @param val the value to set for invariant
    void SetInvariant(bool val) { invariant_ = val; }
    /// @returns true if parameter is invariant
    bool Invariant() { return invariant_; }

    /// Sets the location
    /// @param loc the location value
    /// @param interpolation if the location interpolation settings
    void SetLocation(uint32_t loc, std::optional<builtin::Interpolation> interpolation) {
        location_ = {loc, interpolation};
    }
    /// @returns the location if `Attributes` contains `kLocation`
    std::optional<struct Location> Location() { return location_; }

    /// Sets the binding point
    /// @param group the group
    /// @param binding the binding
    void SetBindingPoint(uint32_t group, uint32_t binding) { binding_point_ = {group, binding}; }
    /// @returns the binding points if `Attributes` contains `kBindingPoint`
    std::optional<struct BindingPoint> BindingPoint() { return binding_point_; }

  private:
    const type::Type* type_ = nullptr;
    std::optional<enum FunctionParam::Builtin> builtin_;
    std::optional<struct Location> location_;
    std::optional<struct BindingPoint> binding_point_;
    bool invariant_ = false;
};

utils::StringStream& operator<<(utils::StringStream& out, enum FunctionParam::Builtin value);

}  // namespace tint::ir

#endif  // SRC_TINT_IR_FUNCTION_PARAM_H_
