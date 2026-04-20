// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_module_writer_util.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/strings/stringprintf.h"
#include "gn/resolved_target_data.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

ClangModuleDep::ClangModuleDep(const SourceFile* modulemap,
                               const std::string& module_name,
                               std::optional<OutputFile> pcm,
                               bool is_self)
    : modulemap(modulemap),
      module_name(module_name),
      pcm(std::move(pcm)),
      is_self(is_self) {}

std::strong_ordering ClangModuleDep::operator<=>(
    const ClangModuleDep& other) const {
  // Sort by (module name, modulemap path, module file path)
  if (auto cmp = module_name <=> other.module_name; cmp != 0)
    return cmp;
  if (modulemap && other.modulemap) {
    if (auto cmp = *modulemap <=> *other.modulemap; cmp != 0)
      return cmp;
  } else {
    if (auto cmp = modulemap <=> other.modulemap; cmp != 0)
      return cmp;
  }
  // std::optional doesn't support <=> on older versions of mac.
  if (pcm.has_value() && other.pcm.has_value()) {
    return *pcm <=> *other.pcm;
  } else {
    return pcm.has_value() <=> other.pcm.has_value();
  }
}

std::set<ClangModuleDep> GetModuleDepsInformation(
    const Target* target,
    const ResolvedTargetData& resolved) {
  std::set<ClangModuleDep> ret;

  auto add_if_new = [&ret](const Target* t, bool is_self,
                           bool has_generated_modulemap) {
    if (!t->module_type().test(Target::HAS_MODULEMAP))
      return;

    std::optional<OutputFile> pcm = std::nullopt;
    auto modulemap = t->modulemap_file();
    CHECK(modulemap != nullptr);
    if (!t->module_type().test(Target::MODULEMAP_IS_TEXTUAL)) {
      const char* tool_type;
      std::vector<OutputFile> modulemap_outputs;
      CHECK(t->GetOutputFilesForSource(*modulemap, &tool_type,
                                       &modulemap_outputs));
      CHECK(modulemap_outputs.size() == 1u);
      pcm = std::move(modulemap_outputs[0]);
    }

    ret.emplace(
        // If we have a generated modulemap, the modulemap should contain
        // "extern module" declarations, so we don't need to declare
        // -fmodule-map-file for the dependencies.
        has_generated_modulemap ? nullptr : modulemap, t->module_name(), pcm,
        is_self);
  };

  // Generated modulemaps always generate private modulemaps as well.
  bool has_generated_modulemap =
      target->module_type().test(Target::MODULEMAP_IS_GENERATED);
  if (has_generated_modulemap) {
    // Add the private modulemap as a dependency.
    ret.emplace(target->private_modulemap_file(),
                base::StringPrintf("%s_Private", target->module_name().c_str()),
                std::nullopt, true);
  } else {
    add_if_new(target, true, has_generated_modulemap);
  }

  for (const auto& pair : resolved.GetModuleDepsInformation(target))
    add_if_new(pair.target(), false, has_generated_modulemap);

  return ret;
}

void ClangModuleDep::Write(std::ostream& out,
                           const PathOutput& path_output,
                           bool include_self) const {
  if (modulemap) {
    out << " -fmodule-map-file=";
    path_output.WriteFile(out, *modulemap);
  }
  if (pcm && (include_self || !is_self)) {
    out << " -fmodule-file=" << module_name << "=";
    path_output.WriteFile(out, *pcm);
  }
}