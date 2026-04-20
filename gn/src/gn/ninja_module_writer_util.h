// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_
#define TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_

#include <compare>
#include <optional>
#include <set>
#include <string>

#include "gn/output_file.h"
#include "gn/path_output.h"

class ResolvedTargetData;
class SourceFile;
class Target;

struct ClangModuleDep {
  ClangModuleDep(const SourceFile* modulemap,
                 const std::string& module_name,
                 std::optional<OutputFile> pcm,
                 bool is_self);

  std::strong_ordering operator<=>(const ClangModuleDep& other) const;
  bool operator==(const ClangModuleDep& other) const = default;
  void Write(std::ostream& out,
             const PathOutput& path_output,
             bool include_self) const;

  // The input module.modulemap source file.
  const SourceFile* modulemap;

  // The internal module name.
  std::string module_name;

  // The compiled version of the module.
  // Will be nullopt if the modulemap is purely textual.
  std::optional<OutputFile> pcm;

  // Is this the module for the current target.
  bool is_self;
};

// Gathers information about all module dependencies for a given target.
std::set<ClangModuleDep> GetModuleDepsInformation(
    const Target* target,
    const ResolvedTargetData& resolved);

#endif  // TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_
