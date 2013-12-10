// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target_generator.h"

#include "tools/gn/binary_target_generator.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/copy_target_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/group_target_generator.h"
#include "tools/gn/item_node.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/script_target_generator.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

TargetGenerator::TargetGenerator(Target* target,
                                 Scope* scope,
                                 const Token& function_token,
                                 Err* err)
    : target_(target),
      scope_(scope),
      function_token_(function_token),
      err_(err) {
}

TargetGenerator::~TargetGenerator() {
}

void TargetGenerator::Run() {
  // All target types use these.
  FillDependentConfigs();
  FillData();
  FillDependencies();

  // To type-specific generation.
  DoRun();

  // Mark the target as complete.
  if (!err_->has_error()) {
    target_->SetGenerated(&function_token_);
    GetBuildSettings()->target_manager().TargetGenerationComplete(
        target_->label(), err_);
  }
}

// static
void TargetGenerator::GenerateTarget(Scope* scope,
                                     const Token& function_token,
                                     const std::vector<Value>& args,
                                     const std::string& output_type,
                                     Err* err) {
  // Name is the argument to the function.
  if (args.size() != 1u || args[0].type() != Value::STRING) {
    *err = Err(function_token,
        "Target generator requires one string argument.",
        "Otherwise I'm not sure what to call this target.");
    return;
  }

  // The location of the target is the directory name with no slash at the end.
  // FIXME(brettw) validate name.
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  Label label(scope->GetSourceDir(), args[0].string_value(),
              toolchain_label.dir(), toolchain_label.name());

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Generating target", label.GetUserVisibleName(true));

  Target* target =
      scope->settings()->build_settings()->target_manager().GetTarget(
          label, function_token.range(), NULL, err);
  if (err->has_error())
    return;

  // Create and call out to the proper generator.
  if (output_type == functions::kCopy) {
    CopyTargetGenerator generator(target, scope, function_token, err);
    generator.Run();
  } else if (output_type == functions::kCustom) {
    ScriptTargetGenerator generator(target, scope, function_token, err);
    generator.Run();
  } else if (output_type == functions::kExecutable) {
    BinaryTargetGenerator generator(target, scope, function_token,
                                    Target::EXECUTABLE, err);
    generator.Run();
  } else if (output_type == functions::kGroup) {
    GroupTargetGenerator generator(target, scope, function_token, err);
    generator.Run();
  } else if (output_type == functions::kSharedLibrary) {
    BinaryTargetGenerator generator(target, scope, function_token,
                                    Target::SHARED_LIBRARY, err);
    generator.Run();
  } else if (output_type == functions::kStaticLibrary) {
    BinaryTargetGenerator generator(target, scope, function_token,
                                    Target::STATIC_LIBRARY, err);
    generator.Run();
  } else {
    *err = Err(function_token, "Not a known output type",
               "I am very confused.");
  }
}

const BuildSettings* TargetGenerator::GetBuildSettings() const {
  return scope_->settings()->build_settings();
}

void TargetGenerator::FillSources() {
  const Value* value = scope_->GetValue(variables::kSources, true);
  if (!value)
    return;

  Target::FileList dest_sources;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_sources, err_))
    return;
  target_->swap_in_sources(&dest_sources);
}

void TargetGenerator::FillSourcePrereqs() {
  const Value* value = scope_->GetValue(variables::kSourcePrereqs, true);
  if (!value)
    return;

  Target::FileList dest_reqs;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_reqs, err_))
    return;
  target_->swap_in_source_prereqs(&dest_reqs);
}

void TargetGenerator::FillConfigs() {
  FillGenericConfigs(variables::kConfigs, &Target::swap_in_configs);
}

void TargetGenerator::FillDependentConfigs() {
  FillGenericConfigs(variables::kAllDependentConfigs,
                     &Target::swap_in_all_dependent_configs);
  FillGenericConfigs(variables::kDirectDependentConfigs,
                     &Target::swap_in_direct_dependent_configs);
}

void TargetGenerator::FillData() {
  // TODO(brettW) hook this up to the constant when we have cleaned up
  // how data files are used.
  const Value* value = scope_->GetValue(variables::kData, true);
  if (!value)
    return;

  Target::FileList dest_data;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_data, err_))
    return;
  target_->swap_in_data(&dest_data);
}

void TargetGenerator::FillDependencies() {
  FillGenericDeps(variables::kDeps, &Target::swap_in_deps);
  FillGenericDeps(variables::kDatadeps, &Target::swap_in_datadeps);

  // This is a list of dependent targets to have their configs fowarded, so
  // it goes here rather than in FillConfigs.
  FillForwardDependentConfigs();

  FillHardDep();
}

void TargetGenerator::FillHardDep() {
  const Value* hard_dep_value = scope_->GetValue(variables::kHardDep, true);
  if (!hard_dep_value)
    return;
  if (!hard_dep_value->VerifyTypeIs(Value::BOOLEAN, err_))
    return;
  target_->set_hard_dep(hard_dep_value->boolean_value());
}

void TargetGenerator::FillExternal() {
  const Value* value = scope_->GetValue(variables::kExternal, true);
  if (!value)
    return;
  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return;
  target_->set_external(value->boolean_value());
}

void TargetGenerator::FillOutputs() {
  const Value* value = scope_->GetValue(variables::kOutputs, true);
  if (!value)
    return;

  Target::FileList outputs;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &outputs, err_))
    return;

  // Validate that outputs are in the output dir.
  CHECK(outputs.size() == value->list_value().size());
  for (size_t i = 0; i < outputs.size(); i++) {
    if (!EnsureStringIsInOutputDir(
            GetBuildSettings()->build_dir(),
            outputs[i].value(), value->list_value()[i], err_))
      return;
  }
  target_->script_values().swap_in_outputs(&outputs);
}

void TargetGenerator::SetToolchainDependency() {
  // TODO(brettw) currently we lock separately for each config, dep, and
  // toolchain we add which is bad! Do this in one lock.
  ItemTree* tree = &GetBuildSettings()->item_tree();
  base::AutoLock lock(tree->lock());
  ItemNode* tc_node =
      tree->GetExistingNodeLocked(ToolchainLabelForScope(scope_));
  target_->item_node()->AddDependency(
      GetBuildSettings(), function_token_.range(), tc_node, err_);
}

void TargetGenerator::FillGenericConfigs(
    const char* var_name,
    void (Target::*setter)(std::vector<const Config*>*)) {
  const Value* value = scope_->GetValue(var_name, true);
  if (!value)
    return;

  std::vector<Label> labels;
  if (!ExtractListOfLabels(*value, scope_->GetSourceDir(),
                           ToolchainLabelForScope(scope_), &labels, err_))
    return;

  std::vector<const Config*> dest_configs;
  dest_configs.resize(labels.size());
  for (size_t i = 0; i < labels.size(); i++) {
    dest_configs[i] = Config::GetConfig(
        scope_->settings(),
        value->list_value()[i].origin()->GetRange(),
        labels[i], target_, err_);
    if (err_->has_error())
      return;
  }
  (target_->*setter)(&dest_configs);
}

void TargetGenerator::FillGenericDeps(
    const char* var_name,
    void (Target::*setter)(std::vector<const Target*>*)) {
  const Value* value = scope_->GetValue(var_name, true);
  if (!value)
    return;

  std::vector<Label> labels;
  if (!ExtractListOfLabels(*value, scope_->GetSourceDir(),
                           ToolchainLabelForScope(scope_), &labels, err_))
    return;

  std::vector<const Target*> dest_deps;
  dest_deps.resize(labels.size());
  for (size_t i = 0; i < labels.size(); i++) {
    dest_deps[i] = GetBuildSettings()->target_manager().GetTarget(
        labels[i], value->list_value()[i].origin()->GetRange(), target_, err_);
    if (err_->has_error())
      return;
  }

  (target_->*setter)(&dest_deps);
}

void TargetGenerator::FillForwardDependentConfigs() {
  const Value* value = scope_->GetValue(
      variables::kForwardDependentConfigsFrom, true);
  if (!value)
    return;

  std::vector<Label> labels;
  if (!ExtractListOfLabels(*value, scope_->GetSourceDir(),
                           ToolchainLabelForScope(scope_), &labels, err_))
    return;

  const std::vector<const Target*>& deps = target_->deps();

  // We currently assume that the list is very small and do a brute-force
  // search in the deps for the labeled target. This could be optimized.
  std::vector<const Target*> forward_from_list;
  for (size_t label_index = 0; label_index < labels.size(); label_index++) {
    const Target* forward_from = NULL;
    for (size_t dep_index = 0; dep_index < deps.size(); dep_index++) {
      if (deps[dep_index]->label() == labels[label_index]) {
        forward_from = deps[dep_index];
        break;
      }
    }
    if (!forward_from) {
      *err_ = Err(value->list_value()[label_index],
          "Can't forward from this target.",
          "forward_dependent_configs_from must contain a list of labels that\n"
          "must all appear in the deps of the same target.");
      return;
    }

    forward_from_list.push_back(forward_from);
  }

  target_->swap_in_forward_dependent_configs(&forward_from_list);
}





