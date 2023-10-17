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

#ifndef SRC_TINT_IR_TRANSFORM_TEST_HELPER_H_
#define SRC_TINT_IR_TRANSFORM_TEST_HELPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "src/tint/ir/builder.h"
#include "src/tint/ir/disassembler.h"
#include "src/tint/ir/transform/transform.h"
#include "src/tint/ir/validate.h"
#include "src/tint/transform/manager.h"

namespace tint::ir::transform {

/// Helper class for testing IR transforms.
template <typename BASE>
class TransformTestBase : public BASE {
  public:
    /// Transforms the module, using transforms in `TRANSFORMS`.
    /// @param data the optional Transform::DataMap to pass to Transform::Run()
    /// @returns the transform outputs, if any
    template <typename... TRANSFORMS>
    Transform::DataMap Run(const Transform::DataMap& data = {}) {
        tint::transform::Manager manager;
        tint::transform::DataMap outputs;

        // Validate the input IR.
        {
            auto res = ir::Validate(mod);
            EXPECT_TRUE(res) << res.Failure().str();
            if (!res) {
                return outputs;
            }
        }

        // Run the transforms.
        for (auto* transform_ptr : std::initializer_list<Transform*>{new TRANSFORMS()...}) {
            manager.append(std::unique_ptr<Transform>(transform_ptr));
        }
        manager.Run(&mod, data, outputs);

        // Validate the output IR.
        {
            auto res = ir::Validate(mod);
            EXPECT_TRUE(res) << res.Failure().str();
        }

        return outputs;
    }

    /// @returns the transformed module as a disassembled string
    std::string str() {
        ir::Disassembler dis(mod);
        return "\n" + dis.Disassemble();
    }

  protected:
    /// The test IR module.
    ir::Module mod;
    /// The test IR builder.
    ir::Builder b{mod};
    /// The type manager.
    type::Manager& ty{mod.Types()};
};

using TransformTest = TransformTestBase<testing::Test>;

template <typename T>
using TransformTestWithParam = TransformTestBase<testing::TestWithParam<T>>;

}  // namespace tint::ir::transform

#endif  // SRC_TINT_IR_TRANSFORM_TEST_HELPER_H_
