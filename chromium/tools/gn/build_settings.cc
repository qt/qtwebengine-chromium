// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/build_settings.h"

#include "tools/gn/filesystem_utils.h"

BuildSettings::BuildSettings()
    : using_external_generator_(false),
      item_tree_(),
      target_manager_(this),
      toolchain_manager_(this) {
}

BuildSettings::~BuildSettings() {
}

void BuildSettings::SetRootPath(const base::FilePath& r) {
  DCHECK(r.value()[r.value().size() - 1] != base::FilePath::kSeparators[0]);
  root_path_ = r;
  root_path_utf8_ = FilePathToUTF8(root_path_);
}

void BuildSettings::SetSecondarySourcePath(const SourceDir& d) {
  secondary_source_path_ = GetFullPath(d);
}

void BuildSettings::SetBuildDir(const SourceDir& d) {
  build_dir_ = d;
  build_to_source_dir_string_ = InvertDir(d);
}

base::FilePath BuildSettings::GetFullPath(const SourceFile& file) const {
  return file.Resolve(root_path_);
}

base::FilePath BuildSettings::GetFullPath(const SourceDir& dir) const {
  return dir.Resolve(root_path_);
}

base::FilePath BuildSettings::GetFullPathSecondary(
    const SourceFile& file) const {
  return file.Resolve(secondary_source_path_);
}

base::FilePath BuildSettings::GetFullPathSecondary(
    const SourceDir& dir) const {
  return dir.Resolve(secondary_source_path_);
}

