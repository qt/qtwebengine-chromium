// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/builder_record.h"

#include "gn/item.h"

BuilderRecord::BuilderRecord(ItemType type,
                             const Label& label,
                             const ParseNode* originally_referenced_from)
    : type_(type),
      label_(label),
      originally_referenced_from_(originally_referenced_from) {}

// static
const char* BuilderRecord::GetNameForType(ItemType type) {
  switch (type) {
    case ITEM_TARGET:
      return "target";
    case ITEM_CONFIG:
      return "config";
    case ITEM_TOOLCHAIN:
      return "toolchain";
    case ITEM_POOL:
      return "pool";
    case ITEM_UNKNOWN:
    default:
      return "unknown";
  }
}

// static
bool BuilderRecord::IsItemOfType(const Item* item, ItemType type) {
  switch (type) {
    case ITEM_TARGET:
      return !!item->AsTarget();
    case ITEM_CONFIG:
      return !!item->AsConfig();
    case ITEM_TOOLCHAIN:
      return !!item->AsToolchain();
    case ITEM_POOL:
      return !!item->AsPool();
    case ITEM_UNKNOWN:
    default:
      return false;
  }
}

// static
BuilderRecord::ItemType BuilderRecord::TypeOfItem(const Item* item) {
  if (item->AsTarget())
    return ITEM_TARGET;
  if (item->AsConfig())
    return ITEM_CONFIG;
  if (item->AsToolchain())
    return ITEM_TOOLCHAIN;
  if (item->AsPool())
    return ITEM_POOL;

  NOTREACHED();
  return ITEM_UNKNOWN;
}

void BuilderRecord::SetDefined(std::unique_ptr<Item> item) {
  DCHECK(state_ == STATE_INIT);
  state_ = STATE_DEFINED;
  item_ = std::move(item);
}

void BuilderRecord::AddDep(BuilderRecord* dep) {
  DCHECK(state_ == STATE_DEFINED);
  all_deps_.add(dep);
  if (!dep->is_resolved()) {
    // This cannot be resolved yet if any of its standard dependencies is not
    // resolved.
    if (dep->waiting_on_resolution_.add(this)) {
      unresolved_count_++;
      DEBUG_BUILDER_RECORD_LOG("-- AddDep waiting_on_resolution %s -> %s\n",
                               dep->ToDebugString().c_str(),
                               this->ToDebugString().c_str());
    }
  }
  if (!dep->is_finalized()) {
    // Finalization of the current record is blocked until the
    // dependency is finalized.
    if (dep->waiting_on_finalization_.add(this)) {
      unfinalized_count_++;
      DEBUG_BUILDER_RECORD_LOG("-- AddDep waiting_on_finalization %s -> %s\n",
                               dep->ToDebugString().c_str(),
                               this->ToDebugString().c_str());
    }
  }
}

void BuilderRecord::AddGenDep(BuilderRecord* dep) {
  DCHECK(state_ == STATE_DEFINED);
  all_deps_.insert(dep);
}

void BuilderRecord::AddValidationDep(BuilderRecord* dep) {
  DCHECK(state_ == STATE_DEFINED);
  // Validation dependencies are treated differently than normal dependencies:
  // 1. They are added to all_deps_ for graph traversal (should_generate
  //    propagation) and error checking.
  // 2. They block resolution ONLY if they are undefined (!item). This allows
  //    cycles (A is validated by B, B depends on A) to resolve.
  // 3. They block writing if they are unresolved. This prevents race conditions
  //    where we might write the ninja file before the validation output path
  //    is computed.
  all_deps_.add(dep);
  if (!dep->is_defined()) {
    // The record cannot be resolved yet if any of its validation dependencies
    // is not defined.
    if (dep->waiting_on_validation_definition_.add(this)) {
      unresolved_count_++;
      DEBUG_BUILDER_RECORD_LOG(
          "-- AddValidationDep waiting_on_validation_definition %s -> %s\n",
          dep->ToDebugString().c_str(), this->ToDebugString().c_str());
    }
  }
  if (!dep->is_resolved()) {
    // This record cannot be finalized if any of its validation deps is not
    // resolved.
    if (dep->waiting_on_validation_resolution_.add(this)) {
      unfinalized_count_++;
      DEBUG_BUILDER_RECORD_LOG(
          "-- AddValidationDep waiting_on_validation_resolution %s -> %s\n",
          dep->ToDebugString().c_str(), this->ToDebugString().c_str());
    }
  }
}

bool BuilderRecord::OnDefinedValidationDep(const BuilderRecord* dep) {
  DCHECK(all_deps_.contains(const_cast<BuilderRecord*>(dep)));
  DCHECK(unresolved_count_ > 0);
  bool result = (--unresolved_count_ == 0);
  DEBUG_BUILDER_RECORD_LOG("-- OnDefinedValidationDep %s -> %s result=%d\n",
                           dep->ToDebugString().c_str(),
                           this->ToDebugString().c_str(), result);
  return result;
}

void BuilderRecord::SetResolved() {
  DCHECK(can_resolve());
  state_ = STATE_RESOLVED;
}

bool BuilderRecord::OnResolvedDep(const BuilderRecord* dep) {
  DCHECK(all_deps_.contains(const_cast<BuilderRecord*>(dep)));
  DCHECK(unresolved_count_ > 0);
  bool result = (--unresolved_count_ == 0);
  DEBUG_BUILDER_RECORD_LOG("-- OnResolvedDep %s -> %s result=%d\n",
                           dep->ToDebugString().c_str(),
                           this->ToDebugString().c_str(), result);
  return result;
}

bool BuilderRecord::OnResolvedValidationDep(const BuilderRecord* dep) {
  DCHECK(all_deps_.contains(const_cast<BuilderRecord*>(dep)));
  DCHECK(unfinalized_count_ > 0);
  bool result = (--unfinalized_count_ == 0);
  DEBUG_BUILDER_RECORD_LOG("-- OnResolvedValidationDep %s -> %s result=%d\n",
                           dep->ToDebugString().c_str(),
                           this->ToDebugString().c_str(), result);
  return result;
}

void BuilderRecord::SetFinalized() {
  DCHECK(can_finalize());
  state_ = STATE_FINALIZED;
}

bool BuilderRecord::OnFinalizedDep(const BuilderRecord* dep) {
  DCHECK(all_deps_.contains(const_cast<BuilderRecord*>(dep)));
  DCHECK(unfinalized_count_ > 0);
  bool result = (--unfinalized_count_ == 0);
  DEBUG_BUILDER_RECORD_LOG("-- OnFinalizedDep %s -> %s result=%d\n",
                           dep->ToDebugString().c_str(),
                           this->ToDebugString().c_str(), result);
  return result;
}

std::vector<const BuilderRecord*> BuilderRecord::GetSortedUnresolvedDeps()
    const {
  std::vector<const BuilderRecord*> result;
  for (auto it = all_deps_.begin(); it.valid(); ++it) {
    BuilderRecord* dep = *it;
    if (dep->waiting_on_resolution_.contains(
            const_cast<BuilderRecord*>(this)) ||
        dep->waiting_on_validation_definition_.contains(
            const_cast<BuilderRecord*>(this)))
      result.push_back(dep);
  }
  std::sort(result.begin(), result.end(), LabelCompare);
  return result;
}

#if DEBUG_BUILDER_RECORD
std::string BuilderRecord::ToDebugString() const {
  std::string result = label_.GetUserVisibleName(false);
  result += " (";
  switch (state_) {
    case STATE_INIT:
      result += "INIT";
      break;
    case STATE_DEFINED:
      result += "DEFINED";
      break;
    case STATE_RESOLVED:
      result += "RESOLVED";
      break;
    case STATE_FINALIZED:
      result += "FINALIZED";
      break;
    default:
      result += "UNKNOWN";
      break;
  }
  result += " unresolved=" + std::to_string(unresolved_count_);
  result += " unfinalized=" + std::to_string(unfinalized_count_);
  if (should_generate_)
    result += " generated";
  result += ")";
  return result;
}
#endif  // DEBUG_BUILDER_RECORD