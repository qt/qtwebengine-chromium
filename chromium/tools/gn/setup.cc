// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/setup.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/parser.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/trace.h"
#include "tools/gn/value.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

extern const char kDotfile_Help[] =
    ".gn file\n"
    "\n"
    "  When gn starts, it will search the current directory and parent\n"
    "  directories for a file called \".gn\". This indicates the source root.\n"
    "  You can override this detection by using the --root command-line\n"
    "  argument\n"
    "\n"
    "  The .gn file in the source root will be executed. The syntax is the\n"
    "  same as a buildfile, but with very limited build setup-specific\n"
    "  meaning.\n"
    "\n"
    "Variables\n"
    "\n"
    "  buildconfig [required]\n"
    "      Label of the build config file. This file will be used to setup\n"
    "      the build file execution environment for each toolchain.\n"
    "\n"
    "  secondary_source [optional]\n"
    "      Label of an alternate directory tree to find input files. When\n"
    "      searching for a BUILD.gn file (or the build config file discussed\n"
    "      above), the file fill first be looked for in the source root.\n"
    "      If it's not found, the secondary source root will be checked\n"
    "      (which would contain a parallel directory hierarchy).\n"
    "\n"
    "      This behavior is intended to be used when BUILD.gn files can't be\n"
    "      checked in to certain source directories for whaever reason.\n"
    "\n"
    "      The secondary source root must be inside the main source tree.\n"
    "\n"
    "Example .gn file contents\n"
    "\n"
    "  buildconfig = \"//build/config/BUILDCONFIG.gn\"\n"
    "\n"
    "  secondary_source = \"//build/config/temporary_buildfiles/\"\n";

namespace {

// More logging.
const char kSwitchVerbose[] = "v";

// Set build args.
const char kSwitchArgs[] = "args";

// Set root dir.
const char kSwitchRoot[] = "root";

// Enable timing.
const char kTimeSwitch[] = "time";

const char kTracelogSwitch[] = "tracelog";

// Set build output directory.
const char kSwitchBuildOutput[] = "output";

const char kSecondarySource[] = "secondary";

const base::FilePath::CharType kGnFile[] = FILE_PATH_LITERAL(".gn");

base::FilePath FindDotFile(const base::FilePath& current_dir) {
  base::FilePath try_this_file = current_dir.Append(kGnFile);
  if (base::PathExists(try_this_file))
    return try_this_file;

  base::FilePath with_no_slash = current_dir.StripTrailingSeparators();
  base::FilePath up_one_dir = with_no_slash.DirName();
  if (up_one_dir == current_dir)
    return base::FilePath();  // Got to the top.

  return FindDotFile(up_one_dir);
}

// Searches the list of strings, and returns the FilePat corresponding to the
// one ending in the given substring, or the empty path if none match.
base::FilePath GetPathEndingIn(
    const std::vector<base::FilePath::StringType>& list,
    const base::FilePath::StringType ending_in) {
  for (size_t i = 0; i < list.size(); i++) {
    if (EndsWith(list[i], ending_in, true))
      return base::FilePath(list[i]);
  }
  return base::FilePath();
}

// Fins the depot tools directory in the path environment variable and returns
// its value. Returns an empty file path if not found.
//
// We detect the depot_tools path by looking for a directory with depot_tools
// at the end (optionally followed by a separator).
base::FilePath ExtractDepotToolsFromPath() {
#if defined(OS_WIN)
  static const wchar_t kPathVarName[] = L"Path";
  DWORD env_buf_size = GetEnvironmentVariable(kPathVarName, NULL, 0);
  if (env_buf_size == 0)
    return base::FilePath();
  base::string16 path;
  path.resize(env_buf_size);
  GetEnvironmentVariable(kPathVarName, &path[0],
                         static_cast<DWORD>(path.size()));
  path.resize(path.size() - 1);  // Trim off null.

  std::vector<base::string16> components;
  base::SplitString(path, ';', &components);

  base::string16 ending_in1 = L"depot_tools\\";
#else
  static const char kPathVarName[] = "PATH";
  const char* path = getenv(kPathVarName);
  if (!path)
    return base::FilePath();

  std::vector<std::string> components;
  base::SplitString(path, ':', &components);

  std::string ending_in1 = "depot_tools/";
#endif
  base::FilePath::StringType ending_in2 = FILE_PATH_LITERAL("depot_tools");

  base::FilePath found = GetPathEndingIn(components, ending_in1);
  if (!found.empty())
    return found;
  return GetPathEndingIn(components, ending_in2);
}

}  // namespace

Setup::Setup()
    : check_for_bad_items_(true),
      empty_toolchain_(Label()),
      empty_settings_(&empty_build_settings_, &empty_toolchain_, std::string()),
      dotfile_scope_(&empty_settings_) {
}

Setup::~Setup() {
}

bool Setup::DoSetup() {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();

  scheduler_.set_verbose_logging(cmdline->HasSwitch(kSwitchVerbose));
  if (cmdline->HasSwitch(kTimeSwitch) ||
      cmdline->HasSwitch(kTracelogSwitch))
    EnableTracing();

  if (!FillArguments(*cmdline))
    return false;
  if (!FillSourceDir(*cmdline))
    return false;
  if (!RunConfigFile())
    return false;
  if (!FillOtherConfig(*cmdline))
    return false;
  FillPythonPath();

  base::FilePath build_path = cmdline->GetSwitchValuePath(kSwitchBuildOutput);
  if (!build_path.empty()) {
    // We accept either repo paths "//out/Debug" or raw source-root-relative
    // paths "out/Debug".
    std::string build_path_8 = FilePathToUTF8(build_path);
    if (build_path_8.compare(0, 2, "//") != 0)
      build_path_8.insert(0, "//");
    build_settings_.SetBuildDir(SourceDir(build_path_8));
  } else {
    // Default output dir.
    build_settings_.SetBuildDir(SourceDir("//out/gn/"));
  }

  return true;
}

bool Setup::Run() {
  // Load the root build file and start runnung.
  build_settings_.toolchain_manager().StartLoadingUnlocked(
      SourceFile("//BUILD.gn"));
  if (!scheduler_.Run())
    return false;

  Err err;
  if (check_for_bad_items_) {
    err = build_settings_.item_tree().CheckForBadItems();
    if (err.has_error()) {
      err.PrintToStdout();
      return false;
    }
  }

  if (!build_settings_.build_args().VerifyAllOverridesUsed(&err)) {
    err.PrintToStdout();
    return false;
  }

  // Write out tracing and timing if requested.
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kTimeSwitch))
    PrintLongHelp(SummarizeTraces());
  if (cmdline->HasSwitch(kTracelogSwitch))
    SaveTraces(cmdline->GetSwitchValuePath(kTracelogSwitch));

  return true;
}

bool Setup::FillArguments(const CommandLine& cmdline) {
  std::string args = cmdline.GetSwitchValueASCII(kSwitchArgs);
  if (args.empty())
    return true;  // Nothing to set.

  args_input_file_.reset(new InputFile(SourceFile()));
  args_input_file_->SetContents(args);
  args_input_file_->set_friendly_name("the command-line \"--args\" settings");

  Err err;
  args_tokens_ = Tokenizer::Tokenize(args_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  args_root_ = Parser::Parse(args_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  Scope arg_scope(&empty_settings_);
  args_root_->AsBlock()->ExecuteBlockInScope(&arg_scope, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  // Save the result of the command args.
  Scope::KeyValueMap overrides;
  arg_scope.GetCurrentScopeValues(&overrides);
  build_settings_.build_args().AddArgOverrides(overrides);
  return true;
}

bool Setup::FillSourceDir(const CommandLine& cmdline) {
  // Find the .gn file.
  base::FilePath root_path;

  // Prefer the command line args to the config file.
  base::FilePath relative_root_path = cmdline.GetSwitchValuePath(kSwitchRoot);
  if (!relative_root_path.empty()) {
    root_path = base::MakeAbsoluteFilePath(relative_root_path);
    dotfile_name_ = root_path.Append(kGnFile);
  } else {
    base::FilePath cur_dir;
    file_util::GetCurrentDirectory(&cur_dir);
    dotfile_name_ = FindDotFile(cur_dir);
    if (dotfile_name_.empty()) {
      Err(Location(), "Can't find source root.",
          "I could not find a \".gn\" file in the current directory or any "
          "parent,\nand the --root command-line argument was not specified.")
          .PrintToStdout();
      return false;
    }
    root_path = dotfile_name_.DirName();
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using source root", FilePathToUTF8(root_path));
  build_settings_.SetRootPath(root_path);

  return true;
}

void Setup::FillPythonPath() {
#if defined(OS_WIN)
  // We use python from the depot tools which should be on the path. If we
  // converted the python_path to a python_command_line then we could
  // potentially use "cmd.exe /c python.exe" and remove this.
  static const wchar_t kPythonName[] = L"python.exe";
  base::FilePath depot_tools = ExtractDepotToolsFromPath();
  if (!depot_tools.empty()) {
    base::FilePath python =
        depot_tools.Append(L"python_bin").Append(kPythonName);
    if (scheduler_.verbose_logging())
      scheduler_.Log("Using python", FilePathToUTF8(python));
    build_settings_.set_python_path(python);
    return;
  }

  if (scheduler_.verbose_logging()) {
    scheduler_.Log("WARNING", "Could not find depot_tools on path, using "
        "just " + FilePathToUTF8(kPythonName));
  }
#else
  static const char kPythonName[] = "python";
#endif

  build_settings_.set_python_path(base::FilePath(kPythonName));
}

bool Setup::RunConfigFile() {
  if (scheduler_.verbose_logging())
    scheduler_.Log("Got dotfile", FilePathToUTF8(dotfile_name_));

  dotfile_input_file_.reset(new InputFile(SourceFile("//.gn")));
  if (!dotfile_input_file_->Load(dotfile_name_)) {
    Err(Location(), "Could not load dotfile.",
        "The file \"" + FilePathToUTF8(dotfile_name_) + "\" cound't be loaded")
        .PrintToStdout();
    return false;
  }

  Err err;
  dotfile_tokens_ = Tokenizer::Tokenize(dotfile_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_ = Parser::Parse(dotfile_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_->AsBlock()->ExecuteBlockInScope(&dotfile_scope_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  return true;
}

bool Setup::FillOtherConfig(const CommandLine& cmdline) {
  Err err;

  // Secondary source path.
  SourceDir secondary_source;
  if (cmdline.HasSwitch(kSecondarySource)) {
    // Prefer the command line over the config file.
    secondary_source =
        SourceDir(cmdline.GetSwitchValueASCII(kSecondarySource));
  } else {
    // Read from the config file if present.
    const Value* secondary_value =
        dotfile_scope_.GetValue("secondary_source", true);
    if (secondary_value) {
      if (!secondary_value->VerifyTypeIs(Value::STRING, &err)) {
        err.PrintToStdout();
        return false;
      }
      build_settings_.SetSecondarySourcePath(
          SourceDir(secondary_value->string_value()));
    }
  }

  // Build config file.
  const Value* build_config_value =
      dotfile_scope_.GetValue("buildconfig", true);
  if (!build_config_value) {
    Err(Location(), "No build config file.",
        "Your .gn file (\"" + FilePathToUTF8(dotfile_name_) + "\")\n"
        "didn't specify a \"buildconfig\" value.").PrintToStdout();
    return false;
  } else if (!build_config_value->VerifyTypeIs(Value::STRING, &err)) {
    err.PrintToStdout();
    return false;
  }
  build_settings_.set_build_config_file(
      SourceFile(build_config_value->string_value()));

  return true;
}
