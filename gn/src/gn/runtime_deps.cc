// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/runtime_deps.h"

#include <optional>
#include <sstream>
#include <unordered_map>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "gn/build_settings.h"
#include "gn/builder.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/loader.h"
#include "gn/output_file.h"
#include "gn/scheduler.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"
#include "gn/switches.h"
#include "gn/target.h"
#include "gn/trace.h"

namespace {

using RuntimeDepsVector = std::vector<std::pair<OutputFile, const Target*>>;

// Returns an OutputFile given a string that looks like a source.
std::string SourceAsOutputFile(const std::string& str, const Target* source) {
  return RebasePath(str, source->settings()->build_settings()->build_dir(),
                    source->settings()->build_settings()->root_path_utf8());
}

// Runs `on_file` for each output file and target. To avoid duplicate traversals
// of targets, the set of targets that have been found so far is passed. The
// "value" of the seen_targets map is a boolean indicating if the seen dep was a
// data dep (true = data_dep). data deps add more stuff, so we will want to
// revisit a target if it's a data dependency and we've previously only seen it
// as a regular dep. `on_file` may be called more than once for the same output
// file.
template <typename F>
void RecursiveCollectRuntimeDeps(
    const Target* target,
    bool is_target_data_dep,
    F&& on_file,
    std::unordered_map<const Target*, bool>* seen_targets) {
  auto [found_seen_target, inserted] =
      seen_targets->try_emplace(target, is_target_data_dep);
  if (!inserted) {
    // Already visited.
    if (found_seen_target->second || !is_target_data_dep) {
      // Already visited as a data dep, or the current dep is not a data
      // dep so visiting again will be a no-op.
      return;
    }
    // In the else case, the previously seen target was a regular dependency
    // and we'll now process it as a data dependency.
    found_seen_target->second = is_target_data_dep;
  }

  // Add the main output file for executables, shared libraries, and
  // loadable modules.
  if (target->output_type() == Target::EXECUTABLE ||
      target->output_type() == Target::LOADABLE_MODULE ||
      target->output_type() == Target::SHARED_LIBRARY) {
    for (const auto& runtime_output : target->runtime_outputs())
      on_file(runtime_output.value(), target);
  }

  // Add all data files.
  for (const auto& file : target->data())
    on_file(SourceAsOutputFile(file, target), target);

  // Actions/copy have all outputs considered when the're a data dep.
  if (is_target_data_dep && (target->output_type() == Target::ACTION ||
                             target->output_type() == Target::ACTION_FOREACH ||
                             target->output_type() == Target::COPY_FILES)) {
    std::vector<SourceFile> outputs;
    target->action_values().GetOutputsAsSourceFiles(target, &outputs);
    for (const auto& output_file : outputs)
      on_file(SourceAsOutputFile(output_file.value(), target), target);
  }

  // Data dependencies.
  for (const auto& dep_pair : target->data_deps()) {
    RecursiveCollectRuntimeDeps(dep_pair.ptr, true, on_file, seen_targets);
  }

  // Do not recurse into bundle targets. A bundle's dependencies should be
  // copied into the bundle itself for run-time access.
  if (target->output_type() == Target::CREATE_BUNDLE) {
    SourceDir bundle_root_dir =
        target->bundle_data().GetBundleRootDirOutputAsDir(target->settings());
    on_file(SourceAsOutputFile(bundle_root_dir.value(), target), target);
    return;
  }

  // Non-data dependencies (both public and private).
  for (const auto& dep_pair : target->GetDeps(Target::DEPS_LINKED)) {
    if (dep_pair.ptr->output_type() == Target::EXECUTABLE)
      continue;  // Skip executables that aren't data deps.
    if (dep_pair.ptr->output_type() == Target::SHARED_LIBRARY &&
        (target->output_type() == Target::ACTION ||
         target->output_type() == Target::ACTION_FOREACH)) {
      // Skip shared libraries that action depends on,
      // unless it were listed in data deps.
      continue;
    }
    RecursiveCollectRuntimeDeps(dep_pair.ptr, false, on_file, seen_targets);
  }
}

// Streams the output file for all runtime deps of `target` to `out`.
void StreamRuntimeDeps(const Target* target, std::ostream& out) {
  std::unordered_map<const Target*, bool> seen_targets;

  // The initial target is not considered a data dependency so that actions's
  // outputs (if the current target is an action) are not automatically
  // considered data deps.
  auto on_file = [&out](const std::string& output_file,
                        const Target* target) -> void {
    out << output_file << std::endl;
  };
  RecursiveCollectRuntimeDeps(target, false, on_file, &seen_targets);
}

bool CollectRuntimeDepsFromFlag(const BuildSettings* build_settings,
                                const Builder& builder,
                                RuntimeDepsVector* files_to_write,
                                Err* err) {
  std::string deps_target_list_file =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueString(
          switches::kRuntimeDepsListFile);

  if (deps_target_list_file.empty())
    return true;

  std::string list_contents;
  ScopedTrace load_trace(TraceItem::TRACE_FILE_LOAD, deps_target_list_file);
  if (!base::ReadFileToString(UTF8ToFilePath(deps_target_list_file),
                              &list_contents)) {
    *err = Err(Location(),
               std::string("File for --") + switches::kRuntimeDepsListFile +
                   " doesn't exist.",
               "The file given was \"" + deps_target_list_file + "\"");
    return false;
  }
  load_trace.Done();

  SourceDir root_dir("//");
  Label default_toolchain_label = builder.loader()->GetDefaultToolchain();
  for (const auto& line : base::SplitString(
           list_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (line.empty())
      continue;
    Label label =
        Label::Resolve(root_dir, build_settings->root_path_utf8(),
                       default_toolchain_label, Value(nullptr, line), err);
    if (err->has_error())
      return false;

    const Item* item = builder.GetItem(label);
    const Target* target = item ? item->AsTarget() : nullptr;
    if (!target) {
      *err =
          Err(Location(),
              "The label \"" + label.GetUserVisibleName(true) +
                  "\" isn't a target.",
              "When reading the line:\n  " + line +
                  "\n"
                  "from the --" +
                  switches::kRuntimeDepsListFile + "=" + deps_target_list_file);
      return false;
    }

    std::optional<OutputFile> output_file;
    const char extension[] = ".runtime_deps";
    if (target->output_type() == Target::SHARED_LIBRARY ||
        target->output_type() == Target::LOADABLE_MODULE) {
      // Force the first output for shared-library-type linker outputs since
      // the dependency output files might not be the main output.
      CHECK(!target->computed_outputs().empty());
      output_file.emplace(target->computed_outputs()[0].value() + extension);
    } else if (target->has_dependency_output_file()) {
      output_file.emplace(target->dependency_output_file().value() + extension);
    } else {
      // If there is no dependency_output_file, this target's dependency output
      // is either a phony alias or was elided entirely (due to lack of real
      // inputs). In either case, there is no file to add an additional
      // extension to, so we should compute our own name in the OBJ BuildDir.
      output_file = GetBuildDirForTargetAsOutputFile(target, BuildDirType::OBJ);
      output_file->value().append(target->GetComputedOutputName());
      output_file->value().append(extension);
    }
    if (output_file)
      files_to_write->emplace_back(*output_file, target);
  }
  return true;
}

bool WriteRuntimeDepsFile(const OutputFile& output_file,
                          const Target* target,
                          Err* err) {
  SourceFile output_as_source =
      output_file.AsSourceFile(target->settings()->build_settings());
  base::FilePath data_deps_file =
      target->settings()->build_settings()->GetFullPath(output_as_source);

  StringOutputBuffer storage;
  std::ostream contents(&storage);
  StreamRuntimeDeps(target, contents);

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, output_as_source.value());
  return storage.WriteToFileIfChanged(data_deps_file, err);
}

}  // namespace

const char kRuntimeDeps_Help[] =
    R"(Runtime dependencies

  Runtime dependencies of a target are exposed via the "runtime_deps" category
  of "gn desc" (see "gn help desc") or they can be written at build generation
  time via write_runtime_deps(), or --runtime-deps-list-file (see "gn help
  --runtime-deps-list-file").

  To a first approximation, the runtime dependencies of a target are the set of
  "data" files, data directories, and the shared libraries from all transitive
  dependencies. Executables, shared libraries, and loadable modules are
  considered runtime dependencies of themselves.

Executables

  Executable targets and those executable targets' transitive dependencies are
  not considered unless that executable is listed in "data_deps". Otherwise, GN
  assumes that the executable (and everything it requires) is a build-time
  dependency only.

Actions and copies

  Action and copy targets that are listed as "data_deps" will have all of their
  outputs and data files considered as runtime dependencies. Action and copy
  targets that are "deps" or "public_deps" will have only their data files
  considered as runtime dependencies. These targets can list an output file in
  both the "outputs" and "data" lists to force an output file as a runtime
  dependency in all cases.

  The different rules for deps and data_deps are to express build-time (deps)
  vs. run-time (data_deps) outputs. If GN counted all build-time copy steps as
  data dependencies, there would be a lot of extra stuff, and if GN counted all
  run-time dependencies as regular deps, the build's parallelism would be
  unnecessarily constrained.

  This rule can sometimes lead to unintuitive results. For example, given the
  three targets:
    A  --[data_deps]-->  B  --[deps]-->  ACTION
  GN would say that A does not have runtime deps on the result of the ACTION,
  which is often correct. But the purpose of the B target might be to collect
  many actions into one logic unit, and the "data"-ness of A's dependency is
  lost. Solutions:

   - List the outputs of the action in its data section (if the results of
     that action are always runtime files).
   - Have B list the action in data_deps (if the outputs of the actions are
     always runtime files).
   - Have B list the action in both deps and data deps (if the outputs might be
     used in both contexts and you don't care about unnecessary entries in the
     list of files required at runtime).
   - Split B into run-time and build-time versions with the appropriate "deps"
     for each.

Static libraries and source sets

  The results of static_library or source_set targets are not considered
  runtime dependencies since these are assumed to be intermediate targets only.
  If you need to list a static library as a runtime dependency, you can
  manually compute the .a/.lib file name for the current platform and list it
  in the "data" list of a target (possibly on the static library target
  itself).

Multiple outputs

  Linker tools can specify which of their outputs should be considered when
  computing the runtime deps by setting runtime_outputs. If this is unset on
  the tool, the default will be the first output only.
)";

RuntimeDepsVector ComputeRuntimeDeps(const Target* target) {
  RuntimeDepsVector result;
  std::unordered_map<const Target*, bool> seen_targets;

  auto on_file = [&result](const std::string& output_file,
                           const Target* target) {
    result.emplace_back(OutputFile(output_file), target);
  };
  // The initial target is not considered a data dependency so that actions's
  // outputs (if the current target is an action) are not automatically
  // considered data deps.
  RecursiveCollectRuntimeDeps(target, false, on_file, &seen_targets);
  return result;
}

bool WriteRuntimeDepsFilesIfNecessary(const BuildSettings* build_settings,
                                      const Builder& builder) {
  RuntimeDepsVector files_to_write;
  Err err;
  if (!CollectRuntimeDepsFromFlag(build_settings, builder, &files_to_write,
                                  &err)) {
    err.PrintToStdout();
    return false;
  }
  for (auto& entry : files_to_write) {
    g_scheduler->ScheduleWork(
        [output_file = std::move(entry.first), target = entry.second]() {
          Err err;
          if (!WriteRuntimeDepsFile(output_file, target, &err)) {
            g_scheduler->FailWithError(err);
          }
        });
  }

  // Files scheduled by write_runtime_deps.
  for (const Target* target : g_scheduler->GetWriteRuntimeDepsTargets()) {
    g_scheduler->ScheduleWork(
        [output_file = target->write_runtime_deps_output(), target]() {
          Err err;
          if (!WriteRuntimeDepsFile(output_file, target, &err)) {
            g_scheduler->FailWithError(err);
          }
        });
  }

  return g_scheduler->Run();
}
