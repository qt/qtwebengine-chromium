// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/value.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#endif

namespace functions {

namespace {

#if defined(OS_WIN)
bool ExecProcess(const CommandLine& cmdline,
                 const base::FilePath& startup_dir,
                 std::string* std_out,
                 std::string* std_err,
                 int* exit_code) {
  SECURITY_ATTRIBUTES sa_attr;
  // Set the bInheritHandle flag so pipe handles are inherited.
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  // Create the pipe for the child process's STDOUT.
  HANDLE out_read = NULL;
  HANDLE out_write = NULL;
  if (!CreatePipe(&out_read, &out_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }
  base::win::ScopedHandle scoped_out_read(out_read);
  base::win::ScopedHandle scoped_out_write(out_write);

  // Create the pipe for the child process's STDERR.
  HANDLE err_read = NULL;
  HANDLE err_write = NULL;
  if (!CreatePipe(&err_read, &err_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }
  base::win::ScopedHandle scoped_err_read(err_read);
  base::win::ScopedHandle scoped_err_write(err_write);

  // Ensure the read handle to the pipe for STDOUT/STDERR is not inherited.
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }
  if (!SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }

  base::FilePath::StringType cmdline_str(cmdline.GetCommandLineString());

  base::win::ScopedProcessInformation proc_info;
  STARTUPINFO start_info = { 0 };

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdOutput = out_write;
  // Keep the normal stdin.
  start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  // FIXME(brettw) set stderr here when we actually read it below.
  //start_info.hStdError = err_write;
  start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // Create the child process.
  if (!CreateProcess(NULL,
                     &cmdline_str[0],
                     NULL, NULL,
                     TRUE,  // Handles are inherited.
                     0, NULL,
                     startup_dir.value().c_str(),
                     &start_info, proc_info.Receive())) {
    return false;
  }

  // Close our writing end of pipes now. Otherwise later read would not be able
  // to detect end of child's output.
  scoped_out_write.Close();
  scoped_err_write.Close();

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 1024;
  char buffer[kBufferSize];

  // FIXME(brettw) read from stderr here! This is complicated because we want
  // to read both of them at the same time, probably need overlapped I/O.
  // Also uncomment start_info code above.
  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(out_read, buffer, kBufferSize, &bytes_read, NULL);
    if (!success || bytes_read == 0)
      break;
    std_out->append(buffer, bytes_read);
  }

  // Let's wait for the process to finish.
  WaitForSingleObject(proc_info.process_handle(), INFINITE);

  DWORD dw_exit_code;
  GetExitCodeProcess(proc_info.process_handle(), &dw_exit_code);
  *exit_code = static_cast<int>(dw_exit_code);

  return true;
}
#else
bool ExecProcess(const CommandLine& cmdline,
                 const base::FilePath& startup_dir,
                 std::string* std_out,
                 std::string* std_err,
                 int* exit_code) {
  //NOTREACHED() << "Implement me.";
  return false;
}
#endif

}  // namespace

const char kExecScript[] = "exec_script";
const char kExecScript_Help[] =
    "exec_script: Synchronously run a script and return the output.\n"
    "\n"
    "  exec_script(filename, arguments, input_conversion,\n"
    "              [file_dependencies])\n"
    "\n"
    "  Runs the given script, returning the stdout of the script. The build\n"
    "  generation will fail if the script does not exist or returns a nonzero\n"
    "  exit code.\n"
    "\n"
    "Arguments:\n"
    "\n"
    "  filename:\n"
    "      File name of python script to execute, relative to the build file.\n"
    "\n"
    "  arguments:\n"
    "      A list of strings to be passed to the script as arguments.\n"
    "\n"
    "  input_conversion:\n"
    "      Controls how the file is read and parsed.\n"
    "      See \"gn help input_conversion\".\n"
    "\n"
    "  dependencies:\n"
    "      (Optional) A list of files that this script reads or otherwise\n"
    "      depends on. These dependencies will be added to the build result\n"
    "      such that if any of them change, the build will be regenerated and\n"
    "      the script will be re-run.\n"
    "\n"
    "      The script itself will be an implicit dependency so you do not\n"
    "      need to list it.\n"
    "\n"
    "Example:\n"
    "\n"
    "  all_lines = exec_script(\"myscript.py\", [some_input], \"list lines\",\n"
    "                          [\"data_file.txt\"])\n";

Value RunExecScript(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  if (args.size() != 3 && args.size() != 4) {
    *err = Err(function->function(), "Wrong number of args to write_file",
               "I expected three or four arguments.");
    return Value();
  }

  const Settings* settings = scope->settings();
  const BuildSettings* build_settings = settings->build_settings();
  const SourceDir& cur_dir = SourceDirForFunctionCall(function);

  // Find the python script to run.
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  SourceFile script_source =
      cur_dir.ResolveRelativeFile(args[0].string_value());
  base::FilePath script_path = build_settings->GetFullPath(script_source);
  if (!build_settings->secondary_source_path().empty() &&
      !base::PathExists(script_path)) {
    // Fall back to secondary source root when the file doesn't exist.
    script_path = build_settings->GetFullPathSecondary(script_source);
  }

  // Add all dependencies of this script, including the script itself, to the
  // build deps.
  g_scheduler->AddGenDependency(script_source);
  if (args.size() == 4) {
    const Value& deps_value = args[3];
    if (!deps_value.VerifyTypeIs(Value::LIST, err))
      return Value();

    for (size_t i = 0; i < deps_value.list_value().size(); i++) {
      if (!deps_value.list_value()[0].VerifyTypeIs(Value::STRING, err))
        return Value();
      g_scheduler->AddGenDependency(cur_dir.ResolveRelativeFile(
          deps_value.list_value()[0].string_value()));
    }
  }

  // Make the command line.
  const base::FilePath& python_path = build_settings->python_path();
  CommandLine cmdline(python_path);
  cmdline.AppendArgPath(script_path);

  const Value& script_args = args[1];
  if (!script_args.VerifyTypeIs(Value::LIST, err))
    return Value();
  for (size_t i = 0; i < script_args.list_value().size(); i++) {
    if (!script_args.list_value()[i].VerifyTypeIs(Value::STRING, err))
      return Value();
    cmdline.AppendArg(script_args.list_value()[i].string_value());
  }

  // Execute the process.
  // TODO(brettw) set the environment block.
  std::string output;
  std::string stderr_output;  // TODO(brettw) not hooked up, see above.
  int exit_code = 0;
  if (!ExecProcess(cmdline, build_settings->GetFullPath(cur_dir),
                   &output, &stderr_output, &exit_code)) {
    *err = Err(function->function(), "Could not execute python.",
        "I was trying to execute \"" + FilePathToUTF8(python_path) + "\".");
    return Value();
  }

  // TODO(brettw) maybe we need stderr also for reasonable stack dumps.
  if (exit_code != 0) {
    std::string msg =
        std::string("I was running \"") + FilePathToUTF8(script_path) + "\"\n"
        "and it returned " + base::IntToString(exit_code);
    if (!output.empty())
      msg += " and printed out:\n\n" + output;
    else
      msg += ".";
    *err = Err(function->function(), "Script returned non-zero exit code.",
               msg);
    return Value();
  }

  return ConvertInputToValue(output, function, args[2], err);
}

}  // namespace functions
