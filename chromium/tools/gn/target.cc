// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target.h"

#include "base/bind.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/scheduler.h"

namespace {

typedef std::set<const Config*> ConfigSet;

void TargetResolvedThunk(const base::Callback<void(const Target*)>& cb,
                         const Target* t) {
  cb.Run(t);
}

// Merges the dependent configs from the given target to the given config list.
// The unique_configs list is used for de-duping so values already added will
// not be added again.
void MergeDirectDependentConfigsFrom(const Target* from_target,
                                     ConfigSet* unique_configs,
                                     std::vector<const Config*>* dest) {
  const std::vector<const Config*>& direct =
      from_target->direct_dependent_configs();
  for (size_t i = 0; i < direct.size(); i++) {
    if (unique_configs->find(direct[i]) == unique_configs->end()) {
      unique_configs->insert(direct[i]);
      dest->push_back(direct[i]);
    }
  }
}

// Like MergeDirectDependentConfigsFrom above except does the "all dependent"
// ones. This additionally adds all configs to the all_dependent_configs_ of
// the dest target given in *all_dest.
void MergeAllDependentConfigsFrom(const Target* from_target,
                                  ConfigSet* unique_configs,
                                  std::vector<const Config*>* dest,
                                  std::vector<const Config*>* all_dest) {
  const std::vector<const Config*>& all =
      from_target->all_dependent_configs();
  for (size_t i = 0; i < all.size(); i++) {
    // Always add it to all_dependent_configs_ since it might not be in that
    // list even if we've seen it applied to this target before. This may
    // introduce some duplicates in all_dependent_configs_, but those will
    // we removed when they're actually applied to a target.
    all_dest->push_back(all[i]);
    if (unique_configs->find(all[i]) == unique_configs->end()) {
      // One we haven't seen yet, also apply it to ourselves.
      dest->push_back(all[i]);
      unique_configs->insert(all[i]);
    }
  }
}

}  // namespace

Target::Target(const Settings* settings, const Label& label)
    : Item(label),
      settings_(settings),
      output_type_(UNKNOWN),
      hard_dep_(false),
      external_(false),
      generated_(false),
      generator_function_(NULL) {
}

Target::~Target() {
}

// static
const char* Target::GetStringForOutputType(OutputType type) {
  switch (type) {
    case UNKNOWN:
      return "Unknown";
    case GROUP:
      return "Group";
    case EXECUTABLE:
      return "Executable";
    case SHARED_LIBRARY:
      return "Shared library";
    case STATIC_LIBRARY:
      return "Static library";
    case COPY_FILES:
      return "Copy";
    case CUSTOM:
      return "Custom";
    default:
      return "";
  }
}

Target* Target::AsTarget() {
  return this;
}

const Target* Target::AsTarget() const {
  return this;
}

void Target::OnResolved() {
  DCHECK(output_type_ != UNKNOWN);

  // Convert any groups we depend on to just direct dependencies on that
  // group's deps. We insert the new deps immediately after the group so that
  // the ordering is preserved. We need to keep the original group so that any
  // flags, etc. that it specifies itself are applied to us.
  size_t original_deps_size = deps_.size();
  for (size_t i = 0; i < original_deps_size; i++) {
    const Target* dep = deps_[i];
    if (dep->output_type_ == GROUP) {
      deps_.insert(deps_.begin() + i + 1, dep->deps_.begin(), dep->deps_.end());
      i += dep->deps_.size();
    }
  }

  // Only add each config once. First remember the target's configs.
  ConfigSet unique_configs;
  for (size_t i = 0; i < configs_.size(); i++)
    unique_configs.insert(configs_[i]);

  // Copy our own dependent configs to the list of configs applying to us.
  for (size_t i = 0; i < all_dependent_configs_.size(); i++) {
    const Config* cur = all_dependent_configs_[i];
    if (unique_configs.find(cur) == unique_configs.end()) {
      unique_configs.insert(cur);
      configs_.push_back(cur);
    }
  }
  for (size_t i = 0; i < direct_dependent_configs_.size(); i++) {
    const Config* cur = direct_dependent_configs_[i];
    if (unique_configs.find(cur) == unique_configs.end()) {
      unique_configs.insert(cur);
      configs_.push_back(cur);
    }
  }

  // Copy our own ldflags to the final set. This will be from our target and
  // all of our configs. We do this for ldflags because it must get inherited
  // through the dependency tree (other flags don't work this way).
  for (ConfigValuesIterator iter(this); !iter.done(); iter.Next()) {
    all_ldflags_.append(iter.cur().ldflags().begin(),
                        iter.cur().ldflags().end());
  }

  if (output_type_ != GROUP) {
    // Don't pull target info like libraries and configs from dependencies into
    // a group target. When A depends on a group G, the G's dependents will
    // be treated as direct dependencies of A, so this is unnecessary and will
    // actually result in duplicated settings (since settings will also be
    // pulled from G to A in case G has configs directly on it).
    PullDependentTargetInfo(&unique_configs);
  }

  // Mark as resolved.
  if (!settings_->build_settings()->target_resolved_callback().is_null()) {
    g_scheduler->ScheduleWork(base::Bind(&TargetResolvedThunk,
        settings_->build_settings()->target_resolved_callback(),
        this));
  }
}

bool Target::HasBeenGenerated() const {
  return generated_;
}

void Target::SetGenerated(const Token* token) {
  DCHECK(!generated_);
  generated_ = true;
  generator_function_ = token;
}

bool Target::IsLinkable() const {
  return output_type_ == STATIC_LIBRARY || output_type_ == SHARED_LIBRARY;
}

void Target::PullDependentTargetInfo(std::set<const Config*>* unique_configs) {
  // Gather info from our dependents we need.
  for (size_t dep_i = 0; dep_i < deps_.size(); dep_i++) {
    const Target* dep = deps_[dep_i];
    MergeAllDependentConfigsFrom(dep, unique_configs, &configs_,
                                 &all_dependent_configs_);
    MergeDirectDependentConfigsFrom(dep, unique_configs, &configs_);

    // Direct dependent libraries.
    if (dep->output_type() == STATIC_LIBRARY ||
        dep->output_type() == SHARED_LIBRARY)
      inherited_libraries_.insert(dep);

    // Inherited libraries and flags are inherited across static library
    // boundaries.
    if (dep->output_type() != SHARED_LIBRARY &&
        dep->output_type() != EXECUTABLE) {
      const std::set<const Target*> inherited = dep->inherited_libraries();
      for (std::set<const Target*>::const_iterator i = inherited.begin();
           i != inherited.end(); ++i)
        inherited_libraries_.insert(*i);

      // Inherited system libraries.
      all_ldflags_.append(dep->all_ldflags());
    }
  }

  // Forward direct dependent configs if requested.
  for (size_t dep = 0; dep < forward_dependent_configs_.size(); dep++) {
    const Target* from_target = forward_dependent_configs_[dep];

    // The forward_dependent_configs_ must be in the deps already, so we
    // don't need to bother copying to our configs, only forwarding.
    DCHECK(std::find(deps_.begin(), deps_.end(), from_target) !=
           deps_.end());
    const std::vector<const Config*>& direct =
        from_target->direct_dependent_configs();
    for (size_t i = 0; i < direct.size(); i++)
      direct_dependent_configs_.push_back(direct[i]);
  }
}
