// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/scheduler.h"
#include "gn/scope.h"
#include "gn/value.h"

namespace functions {

const char kExpandDirectory[] = "expand_directory";
const char kExpandDirectory_HelpShort[] =
    "expand_directory: Expand a source directory and return files.";
const char kExpandDirectory_Help[] =
    R"(expand_directory: Expand a source directory and return files.

  expand_directory(directory, recursive)

  Returns a list of all files contained within the specified directory.

  Arguments:
    directory: A string representing the directory to search, relative to
               the current BUILD file or source-absolute (starting with "//").
    recursive: A boolean indicating whether to search recursively.

  Returns:
    A list of source-absolute paths representing the files found, sorted
    alphabetically.

  Example:
    files = expand_directory("src/data", true)
)";

namespace {

Value ExpandDirectoryInternal(const ParseNode* function,
                              SourceDir source_path,
                              const base::FilePath& disk_path,
                              bool recursive) {
  auto add_gen_dep = [&](const base::FilePath& path) {
    g_scheduler->AddGenDependency(
        path.StripTrailingSeparators().NormalizePathSeparatorsTo(
            base::FilePath::kSeparators[0]));
  };

  add_gen_dep(disk_path);

  std::string disk_path_utf8 = FilePathToUTF8(disk_path);
  Value files(function, Value::LIST);

  base::FileEnumerator traverser(
      disk_path, recursive,
      base::FileEnumerator::FILES |
          (recursive ? base::FileEnumerator::DIRECTORIES : 0));
  for (base::FilePath current = traverser.Next(); !current.empty();
       current = traverser.Next()) {
    if (traverser.GetInfo().IsDirectory()) {
      add_gen_dep(current);
    } else {
      std::string full = source_path.value() +
                         FilePathToUTF8(current).substr(disk_path_utf8.size());
      NormalizePath(&full);
      files.list_value().emplace_back(function, full);
    }
  }

  std::ranges::sort(files.list_value(), [](const auto& lhs, const auto& rhs) {
    return lhs.string_value() < rhs.string_value();
  });

  return files;
}

}  // namespace

Value RunExpandDirectory(Scope* scope,
                         const FunctionCallNode* function,
                         const std::vector<Value>& args,
                         Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Wrong number of arguments.",
               "expand_directory() takes exactly two arguments");
    return Value();
  }

  if (!InSourceAllowList(
          function,
          scope->settings()->build_settings()->expand_directory_allowlist())) {
    *err = Err(
        function, "Disallowed expand_directory call.",
        "The use of expand_directory is restricted in this build.\n"
        "expand_directory is discouraged because it encourages monolithic \n"
        "build targets with redundant inputs, slowing down the build.\n"
        "\n"
        "The allowed callers of expand_directory is maintained in the "
        "\"//.gn\" file\n"
        "if you need to modify the allowlist.");
    return Value();
  }

  if (!args[0].VerifyTypeIs(Value::STRING, err) ||
      !args[1].VerifyTypeIs(Value::BOOLEAN, err)) {
    return Value();
  }

  std::string root_path = scope->settings()->build_settings()->root_path_utf8();
  SourceDir dir =
      scope->GetSourceDir().ResolveRelativeDir(args[0], err, root_path);
  if (err->has_error())
    return Value();

  bool recursive = args[1].boolean_value();

  base::FilePath dir_path =
      scope->settings()->build_settings()->GetFullPath(dir);

  if (!base::DirectoryExists(dir_path)) {
    *err =
        Err(function, "Directory does not exist: " + FilePathToUTF8(dir_path));
    return Value();
  }

  // This is highly likely to be called once per toolchain per directory.
  // Since this involves a file system scan, it's worth caching.
  struct CacheEntry {
    std::mutex mutex;
    Value result;
  };

  static std::map<std::pair<base::FilePath, bool>, CacheEntry> cache;
  static std::mutex cache_mutex;

  CacheEntry* entry;
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    entry = &cache[{dir_path, recursive}];
  }

  // Now lock the per-entry mutex
  std::lock_guard<std::mutex> lock(entry->mutex);
  if (entry->result.type() != Value::NONE) {
    return entry->result;
  }

  Value result = ExpandDirectoryInternal(function, dir, dir_path, recursive);

  entry->result = result;
  return result;
}

}  // namespace functions
