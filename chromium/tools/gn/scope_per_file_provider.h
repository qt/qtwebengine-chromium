// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
#define TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/scope.h"
#include "tools/gn/source_file.h"

// ProgrammaticProvider for a scope to provide it with per-file built-in
// variable support.
class ScopePerFileProvider : public Scope::ProgrammaticProvider {
 public:
  ScopePerFileProvider(Scope* scope);
  virtual ~ScopePerFileProvider();

  // ProgrammaticProvider implementation.
  virtual const Value* GetProgrammaticValue(
      const base::StringPiece& ident) OVERRIDE;

 private:
  const Value* GetCurrentToolchain();
  const Value* GetDefaultToolchain();
  const Value* GetPythonPath();
  const Value* GetRootBuildDir();
  const Value* GetRootGenDir();
  const Value* GetRootOutDir();
  const Value* GetTargetGenDir();
  const Value* GetTargetOutDir();

  static std::string GetRootOutputDirWithNoLastSlash(const Settings* settings);
  static std::string GetToolchainOutputDirWithNoLastSlash(
      const Settings* settings);
  static std::string GetToolchainGenDirWithNoLastSlash(
      const Settings* settings);

  std::string GetFileDirWithNoLastSlash() const;

  // All values are lazily created.
  scoped_ptr<Value> current_toolchain_;
  scoped_ptr<Value> default_toolchain_;
  scoped_ptr<Value> python_path_;
  scoped_ptr<Value> root_build_dir_;
  scoped_ptr<Value> root_gen_dir_;
  scoped_ptr<Value> root_out_dir_;
  scoped_ptr<Value> target_gen_dir_;
  scoped_ptr<Value> target_out_dir_;

  DISALLOW_COPY_AND_ASSIGN(ScopePerFileProvider);
};

#endif  // TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
