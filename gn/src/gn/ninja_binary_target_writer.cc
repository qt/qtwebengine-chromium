// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_binary_target_writer.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

#include "base/strings/string_util.h"
#include "gn/builtin_tool.h"
#include "gn/config_values_extractors.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_c_binary_target_writer.h"
#include "gn/ninja_rust_binary_target_writer.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/pool.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/variables.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  return opts;
}

std::vector<const Target*> ExpandModules(const LabelTargetVector& targets) {
  std::vector<const LabelTargetVector*> stack = {&targets};
  std::unordered_set<const Target*> visited;

  std::vector<const Target*> modules;

  while (!stack.empty()) {
    const LabelTargetVector* current = stack.back();
    stack.pop_back();
    for (const auto& pair : *current) {
      const Target* target = pair.ptr;
      if (visited.insert(target).second) {
        if (target->module_type().none()) {
          stack.push_back(&target->public_deps());
          // If you declare `public_deps = ...` on a group, it shows up as a
          // private dep. Probably because groups don't distinguish between
          // public and private deps.
          if (target->output_type() == Target::GROUP) {
            stack.push_back(&target->private_deps());
          }
        } else {
          modules.push_back(target);
        }
      }
    }
  }
  return modules;
}

void WriteModuleMapHeaders(std::ostream& out,
                           const SourceDir& out_dir,
                           const Target::FileList& headers,
                           const Settings* settings) {
  for (const auto& header : headers) {
    if (header.GetType() == SourceFile::SOURCE_H) {
      out << "  textual header \"";
      out << RebasePath(header.value(), out_dir,
                        settings->build_settings()->root_path_utf8());
      out << "\"\n";
    }
  }
}

void WriteModuleDeps(std::ostream& out,
                     const std::vector<const Target*>& deps,
                     const SourceDir& base) {
  for (const auto& dep : deps) {
    auto module_name = dep->module_name();
    auto modulemap = RebasePath(dep->modulemap_file()->value(), base);
    out << "  extern module \"" << module_name << "\" \"" << modulemap
        << "\"\n";
    out << "  use \"" << module_name << "\"\n";
  }
}

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      rule_prefix_(GetNinjaRulePrefixForToolchain(settings_)) {}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() = default;

void NinjaBinaryTargetWriter::Run() {
  if (target_->source_types_used().RustSourceUsed()) {
    NinjaRustBinaryTargetWriter writer(target_, out_);
    writer.SetResolvedTargetData(GetResolvedTargetData());
    writer.SetNinjaOutputs(ninja_outputs_);
    writer.Run();
    return;
  }

  NinjaCBinaryTargetWriter writer(target_, out_);
  writer.SetResolvedTargetData(GetResolvedTargetData());
  writer.SetNinjaOutputs(ninja_outputs_);
  writer.Run();
}

void NinjaBinaryTargetWriter::WritePublicModuleMap(std::ostream& out,
                                                   const SourceDir& out_dir) {
  out << "module \"" << target_->module_name() << "\" {\n";
  if (target_->all_headers_public()) {
    WriteModuleMapHeaders(out, out_dir, target_->sources(), settings_);
  } else {
    WriteModuleMapHeaders(out, out_dir, target_->public_headers(), settings_);
  }
  auto base = target_->modulemap_file()->GetDir();
  auto deps = ExpandModules(target_->public_deps());
  std::ranges::sort(deps, [](const Target* lhs, const Target* rhs) {
    return lhs->module_name() < rhs->module_name();
  });
  WriteModuleDeps(out, deps, base);
  out << "  export *\n}\n";
}

void NinjaBinaryTargetWriter::WritePrivateModuleMap(std::ostream& out,
                                                    const SourceDir& out_dir) {
  auto base = target_->modulemap_file()->GetDir();
  auto module_name = target_->module_name();
  // Though it's not documented, clang special-cases modules suffixed with
  // _Private. Private and public in the context of clang means basically the
  // same thing as in the context of GN.
  out << "module \"" << module_name << "_Private\" {\n";
  if (!target_->all_headers_public()) {
    WriteModuleMapHeaders(out, out_dir, target_->sources(), settings_);
  }
  out << "  extern module \"" << module_name << "\" \""
      << target_->modulemap_file()->GetName() << "\"\n";
  out << "  use \"" << module_name << "\"\n";

  auto deps = ExpandModules(target_->private_deps());
  auto pub_deps = ExpandModules(target_->public_deps());
  deps.insert(deps.end(), pub_deps.begin(), pub_deps.end());
  std::ranges::sort(deps, [](const Target* lhs, const Target* rhs) {
    return lhs->module_name() < rhs->module_name();
  });
  auto dirty = std::ranges::unique(deps);
  deps.erase(dirty.begin(), dirty.end());
  WriteModuleDeps(out, deps, base);
  out << "}\n";
}

std::vector<OutputFile>
NinjaBinaryTargetWriter::WriteInputsStampOrPhonyAndGetDep(
    size_t num_output_uses) const {
  CHECK(target_->toolchain()) << "Toolchain not set on target "
                              << target_->label().GetUserVisibleName(true);

  UniqueVector<const SourceFile*> inputs;
  for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
    for (const auto& input : iter.cur().inputs()) {
      inputs.push_back(&input);
    }
  }

  if (inputs.size() == 0)
    return std::vector<OutputFile>();  // No inputs

  // If we only have one input, return it directly instead of writing a phony
  // target for it.
  if (inputs.size() == 1) {
    return std::vector<OutputFile>{
        OutputFile(settings_->build_settings(), *inputs[0])};
  }

  std::vector<OutputFile> outs;
  for (const SourceFile* source : inputs)
    outs.push_back(OutputFile(settings_->build_settings(), *source));

  // If there are multiple inputs, but the phony target would be referenced only
  // once, don't write it but depend on the inputs directly.
  if (num_output_uses == 1u)
    return outs;

  OutputFile stamp_or_phony;
  std::string tool;
  if (settings_->build_settings()->no_stamp_files()) {
    // Make a phony target. We don't need to worry about an empty phony target,
    // as those would have been peeled off already.
    CHECK(!inputs.empty());
    stamp_or_phony =
        GetBuildDirForTargetAsOutputFile(target_, BuildDirType::PHONY);
    stamp_or_phony.value().append(target_->label().name());
    stamp_or_phony.value().append(".inputs");
    tool = BuiltinTool::kBuiltinToolPhony;
  } else {
    // Make a stamp target.
    stamp_or_phony =
        GetBuildDirForTargetAsOutputFile(target_, BuildDirType::OBJ);
    stamp_or_phony.value().append(target_->label().name());
    stamp_or_phony.value().append(".inputs.stamp");
    tool = GetNinjaRulePrefixForToolchain(settings_) +
           GeneralTool::kGeneralToolStamp;
  }

  out_ << "build ";
  WriteOutput(stamp_or_phony);
  out_ << ": " << tool;

  // File inputs.
  for (const auto* input : inputs) {
    out_ << " ";
    path_output_.WriteFile(out_, *input);
  }

  out_ << std::endl;
  return {stamp_or_phony};
}

NinjaBinaryTargetWriter::ClassifiedDeps
NinjaBinaryTargetWriter::GetClassifiedDeps() const {
  ClassifiedDeps classified_deps;

  const auto& target_deps = resolved().GetTargetDeps(target_);

  // Normal public/private deps.
  for (const Target* dep : target_deps.linked_deps()) {
    ClassifyDependency(dep, &classified_deps);
  }

  // Inherited libraries.
  for (const auto& inherited : resolved().GetInheritedLibraries(target_)) {
    ClassifyDependency(inherited.target(), &classified_deps);
  }

  // Data deps.
  for (const Target* data_dep : target_deps.data_deps())
    classified_deps.non_linkable_deps.push_back(data_dep);

  return classified_deps;
}

void NinjaBinaryTargetWriter::ClassifyDependency(
    const Target* dep,
    ClassifiedDeps* classified_deps) const {
  // Only the following types of outputs have libraries linked into them:
  //  EXECUTABLE
  //  SHARED_LIBRARY
  //  _complete_ STATIC_LIBRARY
  //
  // Child deps of intermediate static libraries get pushed up the
  // dependency tree until one of these is reached, and source sets
  // don't link at all.
  bool can_link_libs = target_->IsFinal();

  if (can_link_libs && dep->builds_swift_module())
    classified_deps->swiftmodule_deps.push_back(dep);

  if (target_->source_types_used().RustSourceUsed() &&
      (target_->output_type() == Target::RUST_LIBRARY ||
       target_->output_type() == Target::STATIC_LIBRARY) &&
      dep->IsLinkable()) {
    // Rust libraries and static libraries aren't final, but need to have the
    // link lines of all transitive deps specified.
    classified_deps->linkable_deps.push_back(dep);
  } else if (dep->output_type() == Target::SOURCE_SET ||
             // If a complete static library depends on an incomplete static
             // library, manually link in the object files of the dependent
             // library as if it were a source set. This avoids problems with
             // braindead tools such as ar which don't properly link dependent
             // static libraries.
             (target_->complete_static_lib() &&
              (dep->output_type() == Target::STATIC_LIBRARY &&
               !dep->complete_static_lib()))) {
    // Source sets have their object files linked into final targets
    // (shared libraries, executables, loadable modules, and complete static
    // libraries). Intermediate static libraries and other source sets
    // just forward the dependency, otherwise the files in the source
    // set can easily get linked more than once which will cause
    // multiple definition errors.
    if (can_link_libs)
      AddSourceSetFiles(dep, &classified_deps->extra_object_files);

    // Add the source set itself as a non-linkable dependency on the current
    // target. This will make sure that anything the source set's phony target
    // depends on (like data deps) are also built before the current target
    // can be complete. Otherwise, these will be skipped since this target
    // will depend only on the source set's object files.
    classified_deps->non_linkable_deps.push_back(dep);
  } else if (target_->complete_static_lib() && dep->IsFinal()) {
    classified_deps->non_linkable_deps.push_back(dep);
  } else if (can_link_libs && dep->IsLinkable()) {
    classified_deps->linkable_deps.push_back(dep);
  } else if (dep->output_type() == Target::CREATE_BUNDLE &&
             dep->bundle_data().is_framework()) {
    classified_deps->framework_deps.push_back(dep);
  } else {
    classified_deps->non_linkable_deps.push_back(dep);
  }
}

void NinjaBinaryTargetWriter::AddSourceSetFiles(
    const Target* source_set,
    UniqueVector<OutputFile>* obj_files) const {
  std::vector<OutputFile> tool_outputs;  // Prevent allocation in loop.

  // Compute object files for all sources. Only link the first output from
  // the tool if there are more than one.
  for (const auto& source : source_set->sources()) {
    const char* tool_name = Tool::kToolNone;
    // Do not add .pcm files as they are not object files linked to final
    // binaries.
    if (source.GetType() != SourceFile::SOURCE_MODULEMAP &&
        source_set->GetOutputFilesForSource(source, &tool_name, &tool_outputs))
      obj_files->push_back(tool_outputs[0]);
  }

  // Swift files may generate one object file per module or one per source file
  // depending on how the compiler is invoked (whole module optimization).
  if (source_set->source_types_used().SwiftSourceUsed()) {
    std::vector<OutputFile> outputs;
    source_set->swift_values().GetOutputs(source_set, &outputs);

    for (const OutputFile& output : outputs) {
      SourceFile output_as_source =
          output.AsSourceFile(source_set->settings()->build_settings());
      if (output_as_source.IsObjectType()) {
        obj_files->push_back(output);
      }
    }
  }

  // Add MSVC precompiled header object files. GCC .gch files are not object
  // files so they are omitted.
  if (source_set->config_values().has_precompiled_headers()) {
    if (source_set->source_types_used().Get(SourceFile::SOURCE_C)) {
      const CTool* tool = source_set->toolchain()->GetToolAsC(CTool::kCToolCc);
      if (tool && tool->precompiled_header_type() == CTool::PCH_MSVC) {
        GetPCHOutputFiles(source_set, CTool::kCToolCc, &tool_outputs);
        obj_files->Append(tool_outputs.begin(), tool_outputs.end());
      }
    }
    if (source_set->source_types_used().Get(SourceFile::SOURCE_CPP)) {
      const CTool* tool = source_set->toolchain()->GetToolAsC(CTool::kCToolCxx);
      if (tool && tool->precompiled_header_type() == CTool::PCH_MSVC) {
        GetPCHOutputFiles(source_set, CTool::kCToolCxx, &tool_outputs);
        obj_files->Append(tool_outputs.begin(), tool_outputs.end());
      }
    }
    if (source_set->source_types_used().Get(SourceFile::SOURCE_M)) {
      const CTool* tool =
          source_set->toolchain()->GetToolAsC(CTool::kCToolObjC);
      if (tool && tool->precompiled_header_type() == CTool::PCH_MSVC) {
        GetPCHOutputFiles(source_set, CTool::kCToolObjC, &tool_outputs);
        obj_files->Append(tool_outputs.begin(), tool_outputs.end());
      }
    }
    if (source_set->source_types_used().Get(SourceFile::SOURCE_MM)) {
      const CTool* tool =
          source_set->toolchain()->GetToolAsC(CTool::kCToolObjCxx);
      if (tool && tool->precompiled_header_type() == CTool::PCH_MSVC) {
        GetPCHOutputFiles(source_set, CTool::kCToolObjCxx, &tool_outputs);
        obj_files->Append(tool_outputs.begin(), tool_outputs.end());
      }
    }
  }
}

void NinjaBinaryTargetWriter::WriteCompilerBuildLine(
    const std::vector<SourceFile>& sources,
    const std::vector<OutputFile>& extra_deps,
    const std::vector<OutputFile>& order_only_deps,
    const Tool* tool,
    const std::vector<OutputFile>& outputs,
    bool can_write_source_info,
    bool restat_output_allowed) {
  out_ << "build";
  WriteOutputs(outputs);

  out_ << ": " << rule_prefix_ << tool->name();
  path_output_.WriteFiles(out_, sources);

  if (!extra_deps.empty() || !tool->inputs().empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, extra_deps);
    if (auto phony = tool->inputs_phony_or_file(rule_prefix_,
                                                *settings_->build_settings())) {
      out_ << " ";
      path_output_.WriteFile(out_, *phony);
    }
  }

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  WriteValidations();
  out_ << std::endl;

  if (!sources.empty() && can_write_source_info) {
    out_ << "  " << "source_file_part = " << sources[0].GetName();
    out_ << std::endl;
    out_ << "  " << "source_name_part = "
         << FindFilenameNoExtension(&sources[0].value());
    out_ << std::endl;
  }

  if (restat_output_allowed) {
    out_ << "  restat = 1" << std::endl;
  }
}

void NinjaBinaryTargetWriter::WriteCustomLinkerFlags(std::ostream& out,
                                                     const Tool* tool) {
  if (tool->AsC() || (tool->AsRust() && tool->AsRust()->MayLink())) {
    // First the ldflags from the target and its config.
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::ldflags,
                                         GetFlagOptions(), out);
  }
}

void NinjaBinaryTargetWriter::WriteLibrarySearchPath(std::ostream& out,
                                                     const Tool* tool) {
  // Write library search paths that have been recursively pushed
  // through the dependency tree.
  const auto& all_lib_dirs = resolved().GetLinkedLibraryDirs(target_);
  if (!all_lib_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput lib_path_output(path_output_.current_dir(),
                               settings_->build_settings()->root_path_utf8(),
                               ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out << " " << tool->lib_dir_switch();
      lib_path_output.WriteDir(out, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }

  const auto& all_framework_dirs = resolved().GetLinkedFrameworkDirs(target_);
  if (!all_framework_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput framework_path_output(
        path_output_.current_dir(),
        settings_->build_settings()->root_path_utf8(), ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_framework_dirs.size(); i++) {
      out << " " << tool->framework_dir_switch();
      framework_path_output.WriteDir(out, all_framework_dirs[i],
                                     PathOutput::DIR_NO_LAST_SLASH);
    }
  }
}

void NinjaBinaryTargetWriter::WriteLinkerFlags(
    std::ostream& out,
    const Tool* tool,
    const SourceFile* optional_def_file) {
  // First any ldflags
  WriteCustomLinkerFlags(out, tool);
  // Then the library search path
  WriteLibrarySearchPath(out, tool);

  if (optional_def_file) {
    out_ << " /DEF:";
    path_output_.WriteFile(out, *optional_def_file);
  }
}

void NinjaBinaryTargetWriter::WriteLibs(std::ostream& out, const Tool* tool) {
  // Libraries that have been recursively pushed through the dependency tree.
  // Since we're passing these on the command line to the linker and not
  // to Ninja, we need to do shell escaping.
  PathOutput lib_path_output(path_output_.current_dir(),
                             settings_->build_settings()->root_path_utf8(),
                             ESCAPE_NINJA_COMMAND);
  EscapeOptions lib_escape_opts;
  lib_escape_opts.mode = ESCAPE_NINJA_COMMAND;
  const auto& all_libs = resolved().GetLinkedLibraries(target_);
  for (size_t i = 0; i < all_libs.size(); i++) {
    const LibFile& lib_file = all_libs[i];
    const std::string& lib_value = lib_file.value();
    if (lib_file.is_source_file()) {
      out << " " << tool->linker_arg();
      lib_path_output.WriteFile(out, lib_file.source_file());
    } else {
      out << " " << tool->lib_switch();
      EscapeStringToStream(out, lib_value, lib_escape_opts);
    }
  }
}

void NinjaBinaryTargetWriter::WriteFrameworks(std::ostream& out,
                                              const Tool* tool) {
  // Frameworks that have been recursively pushed through the dependency tree.
  FrameworksWriter writer(tool->framework_switch());
  const auto& all_frameworks = resolved().GetLinkedFrameworks(target_);
  for (size_t i = 0; i < all_frameworks.size(); i++) {
    writer(all_frameworks[i], out);
  }

  FrameworksWriter weak_writer(tool->weak_framework_switch());
  const auto& all_weak_frameworks = resolved().GetLinkedWeakFrameworks(target_);
  for (size_t i = 0; i < all_weak_frameworks.size(); i++) {
    weak_writer(all_weak_frameworks[i], out);
  }

  if (!tool->weak_library_switch().empty()) {
    WeakLibrariesWriter weak_library_writer(tool->weak_library_switch());
    const auto& all_weak_libraries = resolved().GetLinkedWeakLibraries(target_);
    for (const auto& weak_library : all_weak_libraries) {
      weak_library_writer(weak_library, out);
    }
  }
}

void NinjaBinaryTargetWriter::WriteSwiftModules(
    std::ostream& out,
    const Tool* tool,
    const std::vector<OutputFile>& swiftmodules) {
  // Since we're passing these on the command line to the linker and not
  // to Ninja, we need to do shell escaping.
  PathOutput swiftmodule_path_output(
      path_output_.current_dir(), settings_->build_settings()->root_path_utf8(),
      ESCAPE_NINJA_COMMAND);

  for (const OutputFile& swiftmodule : swiftmodules) {
    out << " " << tool->swiftmodule_switch();
    swiftmodule_path_output.WriteFile(out, swiftmodule);
  }
}

void NinjaBinaryTargetWriter::WritePool(std::ostream& out) {
  if (target_->pool().ptr) {
    out << "  pool = ";
    out << target_->pool().ptr->GetNinjaName(
        settings_->default_toolchain_label());
    out << std::endl;
  }
}

std::vector<OutputFile>
NinjaBinaryTargetWriter::GetOrderOnlyDepsFromNonLinkableDeps(
    const UniqueVector<const Target*>& non_linkable_deps) const {
  std::vector<const Target*> group_stack;
  std::vector<OutputFile> outputs_to_write;
  std::set<std::string> seen_outputs;

  auto add_output = [&](const OutputFile& output) {
    if (seen_outputs.insert(output.value()).second) {
      outputs_to_write.push_back(output);
    }
  };

  auto process_dep = [&](const Target* dep) {
    if (dep->output_type() == Target::GROUP) {
      group_stack.push_back(dep);
    } else if (dep->has_dependency_output()) {
      OutputFile dep_output = dep->dependency_output();
      if (dep->output_type() == Target::SOURCE_SET) {
        dep_output.value().append(".linkdeps");
      }
      add_output(dep_output);
    }
  };

  for (auto* dep : non_linkable_deps) {
    process_dep(dep);
  }

  // Recursively expand dependencies of groups to avoid unnecessary
  // dependencies. If a group depends on a source set, we depend on its
  // .linkdeps instead of the group itself. This prevents including non-object
  // files (like .dwo files) in order-only dependencies. This is crucial for
  // remote linking to avoid uploading unnecessary files, which increases data
  // transfer and could hit file count limits.
  std::set<const Target*> visited_groups;
  while (!group_stack.empty()) {
    const Target* current = group_stack.back();
    group_stack.pop_back();

    if (!visited_groups.insert(current).second)
      continue;

    auto add_deps = [&](const LabelTargetVector& deps) {
      for (const auto& pair : deps) {
        process_dep(pair.ptr);
      }
    };

    add_deps(current->public_deps());
    add_deps(current->private_deps());
    add_deps(current->data_deps());
  }

  return outputs_to_write;
}
