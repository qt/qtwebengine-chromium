// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_HELPER_H_
#define TOOLS_GN_NINJA_HELPER_H_

#include <iosfwd>
#include <string>

#include "tools/gn/filesystem_utils.h"
#include "tools/gn/output_file.h"
#include "tools/gn/target.h"

class BuildSettings;
class SourceDir;
class SourceFile;
class Target;

// NinjaHelper -----------------------------------------------------------------

class NinjaHelper {
 public:
  NinjaHelper(const BuildSettings* build_settings);
  ~NinjaHelper();

  // Ends in a slash.
  std::string GetTopleveOutputDir() const;

  // Ends in a slash.
  std::string GetTargetOutputDir(const Target* target) const;

  // Example: "base/base.ninja". The string version will not be escaped, and
  // will always have slashes for path separators.
  OutputFile GetNinjaFileForTarget(const Target* target) const;

  // Returns the name of the root .ninja file for the given toolchain.
  OutputFile GetNinjaFileForToolchain(const Settings* settings) const;

  // Given a source file relative to the source root, returns the output
  // filename.
  OutputFile GetOutputFileForSource(const Target* target,
                                    const SourceFile& source,
                                    SourceFileType type) const;

  // Returns the filename produced by the given output.
  //
  // Some targets make multiple files (like a .dll and an import library). This
  // function returns the name of the file other targets should depend on and
  // link to (so in this example, the import library).
  OutputFile GetTargetOutputFile(const Target* target) const;

  // Returns the prefix for rules on the given toolchain. We need this to
  // disambiguate a given rule for each toolchain.
  std::string GetRulePrefix(const Toolchain* toolchain) const;

  // Returns the name of the rule name for the given toolchain and file/target
  // type.  Returns the empty string for source files with no command.
  std::string GetRuleForSourceType(const Settings* settings,
                                   const Toolchain* toolchain,
                                   SourceFileType type) const;
  std::string GetRuleForTargetType(const Toolchain* toolchain,
                                   Target::OutputType target_type) const;

  // Returns the relative directory in either slashes or the system separator
  // from the ninja directory (e.g. "out/Debug") to the source root (e.g.
  // "../.."). It has no terminating slash.
  const std::string& build_to_src_no_last_slash() const {
    return build_to_src_no_last_slash_;
  }
  const std::string& build_to_src_system_no_last_slash() const {
    return build_to_src_system_no_last_slash_;
  }

 private:
  const BuildSettings* build_settings_;

  std::string build_to_src_no_last_slash_;
  std::string build_to_src_system_no_last_slash_;

  DISALLOW_COPY_AND_ASSIGN(NinjaHelper);
};

#endif  // TOOLS_GN_NINJA_HELPER_H_
