// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/copy_target_generator.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

CopyTargetGenerator::CopyTargetGenerator(Target* target,
                                         Scope* scope,
                                         const Token& function_token,
                                         Err* err)
    : TargetGenerator(target, scope, function_token, err) {
}

CopyTargetGenerator::~CopyTargetGenerator() {
}

void CopyTargetGenerator::DoRun() {
  target_->set_output_type(Target::COPY_FILES);

  FillExternal();
  if (err_->has_error())
    return;
  FillSources();
  if (err_->has_error())
    return;
  FillOutputs();
  if (err_->has_error())
    return;

  if (target_->sources().empty()) {
    *err_ = Err(function_token_, "Empty sources for copy command.",
        "You have to specify at least one file to copy in the \"sources\".");
    return;
  }
  if (target_->script_values().outputs().size() != 1) {
    *err_ = Err(function_token_, "Copy command must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the copy\n(see \"gn help copy\"). If there are "
        "multiple sources to copy, use source expansion\n(see \"gn help "
        "source_expansion\").");
    return;
  }

  SetToolchainDependency();
}
