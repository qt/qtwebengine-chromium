// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_HEADER_CHECKER_H_
#define TOOLS_GN_HEADER_CHECKER_H_

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "gn/c_include_iterator.h"
#include "gn/err.h"
#include "gn/hash_table_base.h"
#include "gn/source_dir.h"

class BuildSettings;
class InputFile;
class SourceFile;
class Target;
class WorkerPool;

namespace base {
class FilePath;
}

class HeaderChecker : public base::RefCountedThreadSafe<HeaderChecker> {
 public:
  // Represents a dependency chain.
  struct ChainLink {
    ChainLink() : target(nullptr), is_public(false) {}
    ChainLink(const Target* t, bool p) : target(t), is_public(p) {}

    const Target* target;

    // True when the dependency on this target is public.
    bool is_public;

    // Used for testing.
    bool operator==(const ChainLink& other) const {
      return target == other.target && is_public == other.is_public;
    }
  };
  using Chain = std::vector<ChainLink>;

  // check_generated, if true, will also check generated
  // files. Something that can only be done after running a build that
  // has generated them.
  HeaderChecker(const BuildSettings* build_settings,
                const std::vector<const Target*>& targets,
                bool check_generated,
                bool check_system);

  // Runs the check. The targets in to_check will be checked.
  //
  // This assumes that the current thread already has a message loop. On
  // error, fills the given vector with the errors and returns false. Returns
  // true on success.
  //
  // force_check, if true, will override targets opting out of header checking
  // with "check_includes = false" and will check them anyway.
  bool Run(const std::vector<const Target*>& to_check,
           bool force_check,
           std::vector<Err>* errors);

 private:
  friend class base::RefCountedThreadSafe<HeaderChecker>;
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, IsDependencyOf);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, CheckInclude);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, PublicFirst);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, CheckIncludeAllowCircular);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, CheckIncludeSwiftModule);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, SourceFileForInclude);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest,
                           SourceFileForInclude_FileNotFound);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, Friend);

  ~HeaderChecker();

  // Header checking structures ------------------------------------------------

  // Data for BreadcrumbNode.
  //
  // This class is a trivial type so it can be used in HashTableBase.
  // To implement IsDependencyOf(from_target, to_target), a BFS starting from an
  // arbitrary `from_target` is performed, and a BreadCrumbTable is used to
  // record during the walk, that a given `|target|` is a dependency of
  // `|src_target|`, with `|is_public|` indicating the type of dependency.
  //
  // This table only records the first (src_target->target) dependency during
  // the BFS, since only the shortest dependency path is interesting. This also
  // means that if a target is the dependency of two distinct parents at the
  // same level, only the first parent will be recorded in the table. Consider
  // the following graph:
  //
  // ```
  //     A
  //    / \
  //   B   C
  //    \ /
  //     D
  // ```
  //
  // The BFS will visit nodes in order: A, B, C and D, but will record only the
  // (D, B) edge, not the (D, C) one, even if B->D is private and C->D is
  // public.
  //
  // This information is later used to reconstruct the dependency chain when
  // `to_target` is found by the walk.
  struct BreadcrumbNode {
    const Target* target;
    const Target* src_target;
    bool is_public;

    bool is_null() const { return !target; }
    static bool is_tombstone() { return false; }
    bool is_valid() const { return !is_null(); }
    size_t hash_value() const { return std::hash<const Target*>()(target); }
  };

  struct BreadcrumbTable : public HashTableBase<BreadcrumbNode> {
    using Base = HashTableBase<BreadcrumbNode>;
    using Node = Base::Node;

    // Since we only insert, we don't need to return success/failure.
    // We can also assume that key uniqueness is checked before insertion if
    // necessary, or that we simply overwrite (though BFS usually checks
    // existence first).
    //
    // In IsDependencyOf, we use the return value checking if it was already
    // there. So we need an Insert that returns whether it was new.
    bool Insert(const Target* target,
                const Target* src_target,
                bool is_public) {
      size_t hash = std::hash<const Target*>()(target);
      Node* node = NodeLookup(
          hash, [target](const Node* n) { return n->target == target; });

      if (node->is_valid())
        return false;

      node->target = target;
      node->src_target = src_target;
      node->is_public = is_public;
      UpdateAfterInsert(false);
      return true;
    }

    // Returns the ChainLink for the given target, or a null-target ChainLink if
    // not found. The returned link.target, if not nullptr, is a dependent of
    // the input target that was found during the BFS walk, with dependency
    // type link.is_public.
    ChainLink GetLink(const Target* target) const {
      size_t hash = std::hash<const Target*>()(target);
      const Node* node = NodeLookup(
          hash, [target](const Node* n) { return n->target == target; });

      if (node->is_valid())
        return ChainLink(node->src_target, node->is_public);
      return ChainLink();
    }
  };

  // Store the shortest-dependency-path information for all BFS walks starting
  // from a given `search_from` target.
  //
  // `permitted_breadcrumbs` corresponds to public dependencies only.
  // `any_breadcrumbs` corresponds to all dependencies.
  //
  // Each walk type needs only to be performed once, which is recorded by the
  // corresponding completion flag.
  class ReachabilityCache {
   public:
    ReachabilityCache(const Target* source) : source_target_(source) {}
    ReachabilityCache(const ReachabilityCache&) = delete;
    ReachabilityCache& operator=(const ReachabilityCache&) = delete;

    // Returns true if the given `search_for` target is reachable from
    // `source_target_`.
    //
    // If found, the vector given in `chain` will be filled with the reverse
    // dependency chain from the destination target to the source target.
    //
    // If `permitted` is true, only permitted (public) dependency paths are
    // searched.
    bool SearchForDependencyTo(const Target* search_for,
                               bool permitted,
                               Chain* chain);

    // Conducts a breadth-first search through the dependency graph to find a
    // shortest chain from source_target_.
    void PerformDependencyWalk(bool permitted);

    const Target* source_target() const { return source_target_; }

   private:
    // Reconstructs the shortest dependency chain to the given target if it was
    // found during a previous walk of the given type. Returns true on success.
    bool SearchBreadcrumbs(const Target* search_for,
                           bool permitted,
                           Chain* chain) const;

    const Target* source_target_;

    mutable std::shared_mutex lock_;
    // Breadcrumbs for the shortest permitted path.
    BreadcrumbTable permitted_breadcrumbs_;
    // Breadcrumbs for the shortest path of any type.
    BreadcrumbTable any_breadcrumbs_;

    std::atomic<bool> permitted_complete_ = false;
    std::atomic<bool> any_complete_ = false;
  };

  struct TargetInfo {
    TargetInfo() : target(nullptr), is_public(false), is_generated(false) {}
    TargetInfo(const Target* t, bool is_pub, bool is_gen)
        : target(t), is_public(is_pub), is_generated(is_gen) {}

    const Target* target;

    // True if the file is public in the given target.
    bool is_public;

    // True if this file is generated and won't actually exist on disk.
    bool is_generated;
  };

  using TargetVector = std::vector<TargetInfo>;
  using FileMap = std::map<SourceFile, TargetVector>;
  using PathExistsCallback = std::function<bool(const base::FilePath& path)>;

  // Backend for Run() that takes the list of files to check. The errors_ list
  // will be populate on failure.
  void RunCheckOverFiles(const FileMap& files,
                         bool force_check,
                         WorkerPool* pool);

  void DoWork(const std::vector<const Target*>& targets,
              const SourceFile& file);

  // Adds the sources and public files from the given target to the given map.
  static void AddTargetToFileMap(const Target* target, FileMap* dest);

  // Returns true if the given file is in the output directory.
  bool IsFileInOuputDir(const SourceFile& file) const;

  // Resolves the contents of an include to a SourceFile.
  SourceFile SourceFileForInclude(const IncludeStringWithLocation& include,
                                  const std::vector<SourceDir>& include_dirs,
                                  const InputFile& source_file,
                                  Err* err) const;

  // targets is a list of targets using the source file. They will be used in
  // error messages.
  bool CheckFile(const std::vector<const Target*>& targets,
                 const SourceFile& file,
                 std::vector<Err>* errors) const;

  // Checks that the given file in the given target can include the
  // given include file. If disallowed, adds the error or errors to
  // the errors array.  The range indicates the location of the
  // include in the file for error reporting.
  void CheckInclude(ReachabilityCache& from_target_cache,
                    const InputFile& source_file,
                    const SourceFile& include_file,
                    const LocationRange& range,
                    std::vector<Err>* errors) const;

  // Returns true if the given search_for target is a dependency of
  // the target associated with the given cache.
  //
  // If found, the vector given in "chain" will be filled with the reverse
  // dependency chain from the dest target (chain[0] = search_for) to the src
  // target (chain[chain.size() - 1] = search_from).
  //
  // Chains with permitted dependencies will be considered first. If a
  // permitted match is found, *is_permitted will be set to true. A chain with
  // indirect, non-public dependencies will only be considered if there are no
  // public or direct chains. In this case, *is_permitted will be false.
  //
  // A permitted dependency is a sequence of public dependencies. The first
  // one may be private, since a direct dependency always allows headers to be
  // included.
  bool IsDependencyOf(const Target* search_for,
                      ReachabilityCache& from_target_cache,
                      Chain* chain,
                      bool* is_permitted) const;

  // Makes a very descriptive error message for when an include is disallowed
  // from a given from_target, with a missing dependency to one of the given
  // targets.
  static Err MakeUnreachableError(const InputFile& source_file,
                                  const LocationRange& range,
                                  const Target* from_target,
                                  const TargetVector& targets);

  // Non-locked variables ------------------------------------------------------
  //
  // These are initialized during construction (which happens on one thread)
  // and are not modified after, so any thread can read these without locking.

  const BuildSettings* build_settings_;

  bool check_generated_;

  bool check_system_;

  // Maps source files to targets it appears in (usually just one target).
  FileMap file_map_;

  // Number of tasks posted by RunCheckOverFiles() that haven't completed their
  // execution.
  base::AtomicRefCount task_count_;

  static constexpr size_t kNumShards = 64;
  struct DependencyCacheShard {
    mutable std::shared_mutex lock;
    std::unordered_map<const Target*, std::unique_ptr<ReachabilityCache>> cache;
  };

  // Locked variables ----------------------------------------------------------
  //
  // These are mutable during runtime and require locking.

  mutable std::mutex errors_lock_;

  std::vector<Err> errors_;

  mutable std::array<DependencyCacheShard, kNumShards> dependency_cache_;

  // Returns the cache for the given target, creating it if it doesn't exist.
  ReachabilityCache& GetReachabilityCacheForTarget(const Target* target) const;

  // Separate lock for task count synchronization since std::condition_variable
  // only works with std::unique_lock<std::mutex>.
  std::mutex task_count_lock_;

  // Signaled when |task_count_| becomes zero.
  std::condition_variable task_count_cv_;

  HeaderChecker(const HeaderChecker&) = delete;
  HeaderChecker& operator=(const HeaderChecker&) = delete;
};

#endif  // TOOLS_GN_HEADER_CHECKER_H_
