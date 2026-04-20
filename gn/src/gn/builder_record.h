// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUILDER_RECORD_H_
#define TOOLS_GN_BUILDER_RECORD_H_

#include <memory>
#include <utility>

#include "gn/item.h"
#include "gn/location.h"
#include "gn/pointer_set.h"

class ParseNode;

// Set this to 1 to enable builder record logging during development.
#define DEBUG_BUILDER_RECORD 0

#if DEBUG_BUILDER_RECORD
#define DEBUG_BUILDER_RECORD_LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else  // !DEBUG_BUILDER_RECORD
#define DEBUG_BUILDER_RECORD_LOG(fmt, ...) \
  do {                                     \
  } while (0)
#endif  // !DEBUG_BUILDER_RECORD

// This class models what GN knows about labelled items when resolving
// all dependencies in the configured graph.
//
// Each BuilderRecord provides information about a given graph item
// label, that is augmented by the Builder class. It can be in one of
// the following states, which are taken sequentially, always in the
// same order.
//
//  Init
//    The record only includes a label, an ItemType and the
//    location where that label was first found in the parse tree
//    (for error reporting). All other fields are empty or should be
//    ignored.
//
//  Defined
//    An Item instance is also associated with the record, however its
//    content is not final, meaning that its Item::OnResolved() method
//    has not been called yet.
//
//  Resolved
//    The record's item's OnResolved() method has been called, and the
//    item's state is now final and immutable.
//
//  Finalized
//    This indicates that the item can be written to the Ninja file.
//
// Records can only change state under certain conditions:
//
//    Init -> Defined:
//      This requires a new Item instance coming from the BUILD.gn
//      evaluator matching the same label. Note that the Item returned
//      by the evaluator is incomplete. In particular its ConfigLabelVector
//      and TargetLabelVector values only contain null pointers, since
//      their labels haven't been resolved yet.
//
//    Defined -> Resolved:
//      This requires that all standard dependencies are themselves
//      in the Resolved state, and that all validations are in the Defined
//      state.
//
//      Once this condition is reached, the builder adjusts all label
//      vectors in the Item to point to the right dependency, then
//      the item's OnResolved() method is called.
//
//      For example Target::OnResolved() performs config propagation,
//      visibility checks, and other computations, then returns the Item
//      in its final state, which cannot include null pointers.
//
//    Resolved -> Finalized:
//      This requires all standard dependencies to be Finalized, and all
//      validation dependencies to be Resolved. Ensuring that the
//      dependency graph from the root to this item is fully immutable.
//
// Every time a record's state is changed, checks are performed to see
// if the state of other dependents can now be changed accordingly.
// For example, a Defined target needs to wait for all its dependencies
// to be Resolved, before changing its own state to Resolved. This is
// tracked by various counters and sets in the records.
//
// The record's should_generate() flag is set to true to indicate that
// a corresponding Ninja build statement should be generated for the item.
// By default, it is set to true for all targets defined in BUILD.gn files
// evaluated in the default toolchain, and only for targets reachable from
// the root in other toolchains.
//
// The Ninja build statement can only be written if the target's record
// is in the Finalized state.
//
// A note on validations:
//
// Validation dependencies are a bit special, because they can introduce
// cycles in the dependency graph. For example, this is valid:
//
//     foo ------validation------> bar
//        ^                        |
//        |                        |
//        +--------deps------------+
//
// In this case, foo can be Resolved as soon as bar is Defined, but
// won't be Finalized until bar is Resolved itself. This ensures that
// bar's output path is known when foo is written to the Ninja file,
// as it is required by the build statement which will look like:
//
//     build foo.output: some_rule |@ bar.output
//
// Ninja doesn't enforce an order for the build statements of foo and
// bar, but computing `bar.output` can only be done when `bar` is
// Resolved.
//
// Another issue is that metadata walks, performed when writing
// the build statement for generated_file() targets, will walk over
// validation dependencies as well. This requires these to be resolved.
//
class BuilderRecord {
 public:
  using BuilderRecordSet = PointerSet<BuilderRecord>;

  enum ItemType : char {
    ITEM_UNKNOWN,
    ITEM_TARGET,
    ITEM_CONFIG,
    ITEM_TOOLCHAIN,
    ITEM_POOL
  };

  enum RecordState : char {
    STATE_INIT,
    STATE_DEFINED,
    STATE_RESOLVED,
    STATE_FINALIZED,
  };

  BuilderRecord(ItemType type,
                const Label& label,
                const ParseNode* originally_referenced_from);

  ItemType type() const { return type_; }
  const Label& label() const { return label_; }

  // Returns a user-ready name for the given type. e.g. "target".
  static const char* GetNameForType(ItemType type);

  // Returns true if the given item is of the given type.
  static bool IsItemOfType(const Item* item, ItemType type);

  // Returns the type enum for the given item.
  static ItemType TypeOfItem(const Item* item);

  Item* item() { return item_.get(); }
  const Item* item() const { return item_.get(); }

  // Indicates from where this item was originally referenced from that caused
  // it to be loaded. For targets for which we encountered the declaration
  // before a reference, this will be the empty range.
  const ParseNode* originally_referenced_from() const {
    return originally_referenced_from_;
  }

  bool should_generate() const { return should_generate_; }
  void set_should_generate(bool sg) { should_generate_ = sg; }

  // Return true if this record is in the Defined state or a later one.
  bool is_defined() const { return state_ >= STATE_DEFINED; }

  // Change the record's state to Defined. This only requires
  // a new Item instance to be provided. After this, the caller
  // should call Add{Dep,GenDep,ValidationDep}() for each
  // dependency type for the Item, then call
  // NotifyDependentsWaitingOnValidationDefinition().
  void SetDefined(std::unique_ptr<Item> item);

  // After a call to SetDefined()call these methods for each dependency
  // based on its type. This is later used to control state changes.
  void AddDep(BuilderRecord* dep_record);
  void AddGenDep(BuilderRecord* dep_record);
  void AddValidationDep(BuilderRecord* dep_record);

  // Iterate over all records waiting on this one to be defined, and call
  // |func| on each one that no longer needs to wait. If |func| returns
  // false, stop and return false.
  template <typename Func>
  bool NotifyDependentsWaitingOnValidationDefinition(Func&& func) {
    BuilderRecordSet waiting_deps =
        std::move(waiting_on_validation_definition_);
    for (auto it = waiting_deps.begin(); it.valid(); ++it) {
      BuilderRecord* waiting = *it;
      if (waiting->OnDefinedValidationDep(this) && !func(waiting))
        return false;
    }
    return true;
  }

  // Returns true if the items has been resolved.
  bool is_resolved() const { return state_ >= STATE_RESOLVED; }

  // Returns true if the item can be resolved. This requires all its
  // standard dependencies to be resolved, and all its validations to
  // be defined.
  bool can_resolve() const {
    return state_ == STATE_DEFINED && unresolved_count_ == 0;
  }

  // Change the record's state to Resolved. This requires can_resolve()
  // to be true.
  //
  // IMPORTANT: Caller should then call Add{Dep,GenDep,ValidationDep}()
  // for each dependency of the current item, then call
  // NotifyDependentsWaitingOnResolution() and
  // NotifyDependentsWaitingOnValidationResolution().
  void SetResolved();

  // Iterate over all records waiting on this one to be resolved, and call
  // |func| on each one that no longer needs to wait. If |func| returns false,
  // stop and return false.
  template <typename Func>
  bool NotifyDependentsWaitingOnResolution(Func&& func) {
    BuilderRecordSet waiting_deps = std::move(waiting_on_resolution_);
    for (auto it = waiting_deps.begin(); it.valid(); ++it) {
      BuilderRecord* waiting = *it;
      if (waiting->OnResolvedDep(this) && !func(waiting))
        return false;
    }
    return true;
  }

  // Iterate over all records waiting on this one to be resolved as a
  // validation, and call |func| on each one that no longer needs to wait. If
  // |func| returns false, stop and return false.
  template <typename Func>
  bool NotifyDependentsWaitingOnValidationResolution(Func&& func) {
    BuilderRecordSet waiting_deps =
        std::move(waiting_on_validation_resolution_);
    for (auto it = waiting_deps.begin(); it.valid(); ++it) {
      BuilderRecord* waiting = *it;
      if (waiting->OnResolvedValidationDep(this) && !func(waiting))
        return false;
    }
    return true;
  }

  // Return true if this record has been finalized.
  bool is_finalized() const { return state_ >= STATE_FINALIZED; }

  // Return true if this record can be finalized.
  bool can_finalize() const {
    return state_ == STATE_RESOLVED && unfinalized_count_ == 0 &&
           unresolved_count_ == 0;
  }

  // Change the state to Finalized. This requires can_finalize() to be true.
  // Callers should then call NotifyDependentsWaitingOnFinalization().
  void SetFinalized();

  // Iterate over all records waiting on this one to be finalized, and call
  // |func| on each one that no longer needs to wait. If |func| returns false,
  // stop and return false.
  template <typename Func>
  bool NotifyDependentsWaitingOnFinalization(Func&& func) {
    BuilderRecordSet waiting_deps = std::move(waiting_on_finalization_);
    for (auto it = waiting_deps.begin(); it.valid(); ++it) {
      BuilderRecord* waiting = *it;
      if (waiting->OnFinalizedDep(this) && !func(waiting))
        return false;
    }
    return true;
  }

  // All records this one is depending on. Note that this includes gen_deps for
  // targets, which can have cycles.
  BuilderRecordSet& all_deps() { return all_deps_; }
  const BuilderRecordSet& all_deps() const { return all_deps_; }

  // Get the set of unresolved records this one depends on,
  // as a list sorted by label.
  std::vector<const BuilderRecord*> GetSortedUnresolvedDeps() const;

  // Records that are waiting on this one to be defined. This is used for
  // "validations" dependencies which don't require the target to be fully
  // resolved, only defined.
  BuilderRecordSet& waiting_on_validation_definition() {
    return waiting_on_validation_definition_;
  }
  const BuilderRecordSet& waiting_on_validation_definition() const {
    return waiting_on_validation_definition_;
  }

  // Records that are waiting on this one to be resolved. This is the other
  // end of the "unresolved deps" arrow for standard dependencies.
  BuilderRecordSet& waiting_on_resolution() { return waiting_on_resolution_; }
  const BuilderRecordSet& waiting_on_resolution() const {
    return waiting_on_resolution_;
  }

  // Records that are waiting on this one to be resolved before they can be
  // written to the ninja file. This is used for "validations" dependencies.
  BuilderRecordSet& waiting_on_validation_resolution() {
    return waiting_on_validation_resolution_;
  }
  const BuilderRecordSet& waiting_on_validation_resolution() const {
    return waiting_on_validation_resolution_;
  }

  // Comparator function used to sort records from their label.
  static bool LabelCompare(const BuilderRecord* a, const BuilderRecord* b) {
    return a->label_ < b->label_;
  }

  // Create debug string describing this record.
  std::string ToDebugString() const;

 private:
  // Called by NotifyDependentsWaitingOnXXX() methods. This should update
  // the counters and return true if the record is no longer waiting for
  // its dependents and can change state.
  bool OnDefinedValidationDep(const BuilderRecord* dep);
  bool OnResolvedDep(const BuilderRecord* dep);
  bool OnResolvedValidationDep(const BuilderRecord* dep);
  bool OnFinalizedDep(const BuilderRecord* dep);

  RecordState state_ = STATE_INIT;
  ItemType type_;
  bool should_generate_ = false;
  Label label_;
  std::unique_ptr<Item> item_;
  const ParseNode* originally_referenced_from_ = nullptr;

  // The number of dependencies preventing this record from being resolved.
  // This is the sum of two numbers:
  //
  // - The number of unresolved standard dependencies this record is
  //   waiting for. For each such dep, dep[record].waiting_on_resolution_
  //   contains 'this'.
  //
  // - The number of undefined validation dependencies this record is
  //   waiting for. For each such dep, dep[record].waiting_on_definition_
  //   contains 'this'.
  //
  size_t unresolved_count_ = 0;

  // The number of dependencies preventing this record from being finalized.
  // This is the sum of two numbers:
  //
  // - The numnber of unfinalized standard dependencies this record is
  //   waiting for. For each such dep, dep[record].waiting_on_finalization_
  //   contains 'this'.
  //
  // - The number of unresolved validation dependencies this record is
  //   waiting for. For each such dep,
  //   dep[record].waiting_on_validation_resolution_ contains |this|.
  size_t unfinalized_count_ = 0;

  BuilderRecordSet all_deps_;
  BuilderRecordSet waiting_on_resolution_;
  BuilderRecordSet waiting_on_finalization_;
  BuilderRecordSet waiting_on_validation_definition_;
  BuilderRecordSet waiting_on_validation_resolution_;

  BuilderRecord(const BuilderRecord&) = delete;
  BuilderRecord& operator=(const BuilderRecord&) = delete;
};

#endif  // TOOLS_GN_BUILDER_RECORD_H_
