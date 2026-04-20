// Copyright 2026 The GN Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "gn/commands.h"
#include "gn/filesystem_utils.h"
#include "gn/item.h"
#include "gn/setup.h"
#include "gn/standard_out.h"
#include "gn/target.h"

namespace commands {

const char kSuggest[] = "suggest";
const char kSuggest_HelpShort[] =
    "suggest: Suggest fixes to build graph based on includes.";
const char kSuggest_Help[] =
    R"(suggest: Suggest fixes to build graph based on includes.

  gn suggest <out_dir> includer1=included1 includer2=included2...

  Where each includer or included is either:
  * A label
  * A module name (usually the same as the label)
  * A file path relative to the build directory
  * An absolute file path (eg. "//foo/bar.txt")

  Eg. gn suggest out_dir path/to/target.cc=foo/bar.h

  Will print a suggestion like:
  Request: path/to/target.cc wants to depend on foo/bar.h
  Suggestion: add deps = [ "//foo:bar" ] to "//path/to:target" (defined in //path/to/BUILD.gn:1234)
)";

constexpr std::string_view kPrivateSuffix = "_Private";

namespace {
// Determines whether a source file is in either the public or private API of a
// target.
std::optional<commands::ApiScope> DepKind(const Target* target,
                                          const SourceFile& file) {
  for (const auto& source : target->sources()) {
    if (source == file) {
      return target->all_headers_public() &&
                     file.GetType() == SourceFile::SOURCE_H
                 ? commands::ApiScope::kPublic
                 : commands::ApiScope::kPrivate;
    }
  }
  for (const auto& header : target->public_headers()) {
    if (header == file) {
      return commands::ApiScope::kPublic;
    }
  }
  return std::nullopt;
}

// Finds all targets that use a file as a source from a specific toolchain and
// adds them to results. Checks every toolchain if current_toolchain is null.
bool AddToolchainSources(
    const std::vector<const Target*>& all_targets,
    const Label* current_toolchain,
    const SourceFile& file,
    std::vector<std::pair<const Target*, commands::ApiScope>>& results) {
  for (const Target* target : all_targets) {
    if (!current_toolchain ||
        target->label().GetToolchainLabel() == *current_toolchain) {
      if (auto dep_kind = DepKind(target, file); dep_kind.has_value()) {
        results.emplace_back(target, *dep_kind);
      }
    }
  }
  return !results.empty();
}

SourceFile ResolveFilePath(const BuildSettings* build_settings,
                           std::string_view input) {
  if (input.starts_with("//")) {
    SourceFile file = SourceFile(input);
    if (base::PathExists(build_settings->GetFullPath(file))) {
      return file;
    }
    return SourceFile();
  }
  // Resolve relative to the output directory.
  // This is because the user is most likely running this based on an error
  // message from clang, which gives paths relative to the output directory to
  // be unambiguous.
  Err err;
  SourceFile file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, std::string(input)), &err);
  if (!err.has_error() && base::PathExists(build_settings->GetFullPath(file))) {
    return file;
  }
  return SourceFile();
}

constexpr auto kLabelLike = TextDecoration::DECORATION_GREEN;

void OutputSuggestion(std::string_view message) {
  OutputString("Suggestion: ", TextDecoration::DECORATION_BLUE);
  OutputString(message);
}

void OutputWarning(std::string_view message = "") {
  OutputString("Warning: ", TextDecoration::DECORATION_YELLOW);
  OutputString(message);
}

void OutputError(std::string_view message = "") {
  OutputString("Error: ", TextDecoration::DECORATION_RED);
  OutputString(message);
}

void OutputQuoted(std::string_view message) {
  OutputString("\"", kLabelLike);
  OutputString(message, kLabelLike);
  OutputString("\"", kLabelLike);
}

void OutputDefinition(const Target* target) {
  OutputString(":", kLabelLike);
  OutputString(target->label().name(), kLabelLike);
  OutputString(" (defined at ");
  OutputString(target->user_friendly_location().Describe(false), kLabelLike);
  OutputString(")");
}

}  // namespace

// Resolves an input to a list of targets, and whether each are private.
// The input can be:
// * A module name for a target
// * A target label
// * A file path, which attempts to resolve to:
//   * Targets defined in the current toolchain that contain the file
//   * Targets defined in the default toolchain that contain the file
//   * Targets defined in any toolchain that contain the file
std::pair<std::vector<std::pair<const Target*, commands::ApiScope>>, bool>
ResolveSuggestionToTarget(const BuildSettings* build_settings,
                          const std::vector<const Target*>& all_targets,
                          const Label& current_toolchain,
                          std::string_view input) {
  auto sort_results = [](auto& vec) {
    std::sort(vec.begin(), vec.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.first->label() < rhs.first->label();
    });
  };
  std::vector<std::pair<const Target*, commands::ApiScope>> results;
  std::string_view module_name = input;
  commands::ApiScope is_private = commands::ApiScope::kPublic;
  if (module_name.ends_with(kPrivateSuffix)) {
    is_private = commands::ApiScope::kPrivate;
    module_name.remove_suffix(kPrivateSuffix.size());
  }

  // Try to resolve as a module name.
  for (const Target* target : all_targets) {
    if (target->module_name() == module_name) {
      results.emplace_back(target, is_private);
    }
  }
  if (!results.empty()) {
    sort_results(results);
    return {results, true};
  }

  // If that doesn't work, try to resolve as an absolute target label.
  if (input.starts_with("//") && input.find(':') != std::string_view::npos) {
    Err err;
    Label want;
    Value input_value(nullptr, std::string(input));
    want = Label::Resolve(SourceDir("//"), build_settings->root_path_utf8(),
                          current_toolchain, input_value, &err);
    if (!err.has_error()) {
      for (const Target* target : all_targets) {
        if (target->label() == want) {
          results.emplace_back(target, is_private);
          // We know each label corresponds to exactly one target, so we don't
          // need to keep going.
          return {results, true};
        }
      }
    }
  }

  // If that doesn't work, try to resolve as a file path.
  SourceFile file = ResolveFilePath(build_settings, input);
  if (file.is_null()) {
    return {results, false};
  }

  // If we see //foo(:toolchain) request bar.h, prefer //:bar(:toolchain)
  // over other toolchains.
  if (!AddToolchainSources(all_targets, &current_toolchain, file, results)) {
    AddToolchainSources(all_targets, nullptr, file, results);
  }
  sort_results(results);
  return {results, true};
}

bool OutputSuggestions(const std::vector<const Target*>& all_targets,
                       Setup* setup,
                       std::string_view includer_name,
                       std::string_view included_name) {
  Label current_toolchain = setup->loader()->default_toolchain_label();
  auto OutputTarget = [&current_toolchain](const Target* target) {
    OutputString(target->label().GetUserVisibleName(current_toolchain),
                 kLabelLike);
  };

  auto OutputInsertionHint = [&](std::string_view key, std::string_view value,
                                 const Target* target) {
    OutputSuggestion("Add ");
    OutputString(key);
    OutputString(" = [ ");
    OutputQuoted(value);
    OutputString(" ] to ");
    OutputDefinition(target);
    if (current_toolchain != setup->loader()->default_toolchain_label()) {
      OutputString(" for toolchain ");
      OutputString(
          target->label().GetToolchainLabel().GetUserVisibleName(false),
          kLabelLike);
    }
    OutputString("\n");
  };

  auto ResolveSuggestion = [&](std::string_view value) {
    const auto& [targets, ok] = ResolveSuggestionToTarget(
        &setup->build_settings(), all_targets, current_toolchain, value);
    if (!ok) {
      OutputError();
      if (value.starts_with("//")) {
        OutputString("Could not find target or file ");
        OutputQuoted(value);
      } else {
        OutputString("Unable to find ");
        OutputQuoted(value);
        OutputString(" in either the output or source root directories\n");
      }
    }
    return std::make_pair(targets, ok);
  };

  const auto& [includer_targets, includer_ok] =
      ResolveSuggestion(includer_name);
  if (!includer_ok)
    return false;

  if (includer_targets.empty()) {
    OutputError();
    OutputQuoted(includer_name);
    OutputString(" did not resolve to any targets\n");
    return false;
  } else if (includer_targets.size() > 1) {
    OutputError();
    OutputQuoted(includer_name);
    OutputString(" resolved to multiple targets\n");
    for (const auto& [target, is_private] : includer_targets) {
      OutputString("* ");
      OutputTarget(target);
      OutputString("\n");
    }
    return false;
  }
  const auto& [includer, dep_kind] = includer_targets.front();
  current_toolchain = includer->label().GetToolchainLabel();

  const char* dep_field =
      (dep_kind == commands::ApiScope::kPrivate) ? "deps" : "public_deps";

  const auto& [targets, ok] = ResolveSuggestion(included_name);
  if (!ok)
    return false;

  // We've passed the errors phase. At this point, everything is valid input.
  // Includer is a single target, and included is a valid target, or a file
  // that exists on disk.

  if (targets.empty()) {
    OutputQuoted(included_name);
    OutputString(" is not in the headers of any targets.\n");
    OutputSuggestion("Add ");
    OutputQuoted(included_name);
    OutputString(" to a target's public headers");
    return true;
  }

  std::set<Label> labels_without_toolchain;
  for (const auto& [target, _] : targets) {
    labels_without_toolchain.insert(target->label().GetWithNoToolchain());
  }
  if (labels_without_toolchain.size() == 1 &&
      targets.front().first->label().GetToolchainLabel() != current_toolchain) {
    // The resolution requires that if //:bar(:toolchain1) contained bar.h, we
    // would have returned no targets from any other toolchain. Thus, we now
    // have:
    // //:foo(:toolchain1) including bar.h -> //:bar(:toolchain2),
    // //:bar(:toolchain3)
    OutputQuoted(included_name);
    OutputString(" is defined in ");
    OutputString(labels_without_toolchain.begin()->GetUserVisibleName(false),
                 kLabelLike);
    OutputString(", but not in the toolchain ");
    OutputString(current_toolchain.GetUserVisibleName(false), kLabelLike);
    OutputString("\n");
    OutputInsertionHint("public", included_name, targets.front().first);
    return true;
  }

  if (targets.size() > 1) {
    OutputWarning();
    OutputQuoted(included_name);
    OutputString(" is ambiguous because it belongs to multiple targets:\n");
    for (const auto& [target, _] : targets) {
      OutputString("* ");
      OutputTarget(target);
      OutputString("\n");
    }
    OutputSuggestion(
        "Create a source_set target for the common headers and sources and "
        "have all of the above targets depend on that.");
    OutputInsertionHint(dep_field, "$NEW_SOURCE_SET", includer);
    return true;
  }

  const auto& [included, included_dep_kind] = targets.front();
  if (included_dep_kind == commands::ApiScope::kPrivate) {
    OutputWarning();
    OutputQuoted(included_name);
    OutputString(" is in the private API of ");
    OutputTarget(included);
    OutputSuggestion("Move ");
    OutputQuoted(included_name);
    OutputString(" from `sources` to `public` in ");
    OutputDefinition(included);
  }

  // TODO: There are a bunch of optimizations we can perform here to make better
  // suggestions. They may be considered in the future. Some initial thoughts
  // include:
  // * Check the visibility of includer -> included
  //   * If it is not visible:
  //     * Find a group target that exposes included's headers
  //     * Fall back to suggesting adding visibility
  // * Check if included transitively depends on includer. Suggest ways to break
  // the loop.

  // Note: if we have a toolchain mismatch, we already returned, so the
  // toolchains must match.
  OutputInsertionHint(
      dep_field,
      // Output a relative label if possible.
      included->label().dir() == includer->label().dir()
          ? ":" + included->label().name()
          : included->label().GetUserVisibleName(current_toolchain),
      includer);
  return true;
}

int RunSuggest(const std::vector<std::string>& args) {
  if (args.size() <= 1) {
    OutputError("gn suggest requires arguments. See \"gn help suggest\"\n");
    return 1;
  }

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false) || !setup->Run())
    return 1;

  std::vector<const Target*> all_targets =
      setup->builder().GetAllResolvedTargets();

  bool success = true;
  for (size_t i = 1; i < args.size(); i++) {
    if (i != 1) {
      OutputString("\n");
    }
    std::vector<std::string_view> pair = base::SplitStringPiece(
        args[i], "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (pair.size() != 2) {
      OutputError("Invalid pair: " + args[i] + "\n");
      return 1;
    }
    const auto& includer = pair[0];
    const auto& included = pair[1];

    OutputString("Request: ", TextDecoration::DECORATION_MAGENTA);
    OutputQuoted(includer);
    OutputString(" wants to depend on ");
    OutputQuoted(included);
    OutputString(":\n");

    success &= OutputSuggestions(all_targets, setup, includer, included);
  }

  return success ? 0 : 1;
}

}  // namespace commands
