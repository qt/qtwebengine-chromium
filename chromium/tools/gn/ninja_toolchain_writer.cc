// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_toolchain_writer.h"

#include <fstream>

#include "base/file_util.h"
#include "base/strings/stringize_macros.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/item_node.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/trace.h"

NinjaToolchainWriter::NinjaToolchainWriter(
    const Settings* settings,
    const std::vector<const Target*>& targets,
    const std::set<std::string>& skip_files,
    std::ostream& out)
    : settings_(settings),
      targets_(targets),
      skip_files_(skip_files),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   ESCAPE_NINJA, true),
      helper_(settings->build_settings()) {
}

NinjaToolchainWriter::~NinjaToolchainWriter() {
}

void NinjaToolchainWriter::Run() {
  WriteRules();
  WriteSubninjas();
}

// static
bool NinjaToolchainWriter::RunAndWriteFile(
    const Settings* settings,
    const std::vector<const Target*>& targets,
    const std::set<std::string>& skip_files) {
  NinjaHelper helper(settings->build_settings());
  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForToolchain(settings).GetSourceFile(
          settings->build_settings())));
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, FilePathToUTF8(ninja_file));

  file_util::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaToolchainWriter gen(settings, targets, skip_files, file);
  gen.Run();
  return true;
}

void NinjaToolchainWriter::WriteRules() {
  const Toolchain* tc = settings_->toolchain();
  std::string indent("  ");

  NinjaHelper helper(settings_->build_settings());
  std::string rule_prefix = helper.GetRulePrefix(tc);

  for (int i = Toolchain::TYPE_NONE + 1; i < Toolchain::TYPE_NUMTYPES; i++) {
    Toolchain::ToolType tool_type = static_cast<Toolchain::ToolType>(i);
    const Toolchain::Tool& tool = tc->GetTool(tool_type);
    if (tool.empty())
      continue;

    out_ << "rule " << rule_prefix << Toolchain::ToolTypeToName(tool_type)
         << std::endl;

    #define WRITE_ARG(name) \
      if (!tool.name.empty()) \
        out_ << indent << "  " STRINGIZE(name) " = " << tool.name << std::endl;
    WRITE_ARG(command);
    WRITE_ARG(depfile);
    WRITE_ARG(deps);
    WRITE_ARG(description);
    WRITE_ARG(pool);
    WRITE_ARG(restat);
    WRITE_ARG(rspfile);
    WRITE_ARG(rspfile_content);
    #undef WRITE_ARG
  }
  out_ << std::endl;
}

void NinjaToolchainWriter::WriteSubninjas() {
  // Write subninja commands for each generated target.
  for (size_t i = 0; i < targets_.size(); i++) {
    if (!targets_[i]->item_node()->should_generate() ||
        (targets_[i]->settings()->build_settings()->using_external_generator()
         && targets_[i]->external()))
      continue;

    OutputFile ninja_file = helper_.GetNinjaFileForTarget(targets_[i]);
    if (skip_files_.find(ninja_file.value()) != skip_files_.end())
      continue;

    out_ << "subninja ";
    path_output_.WriteFile(out_, ninja_file);
    out_ << std::endl;
  }
  out_ << std::endl;
}
