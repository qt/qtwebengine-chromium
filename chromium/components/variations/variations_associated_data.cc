// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/singleton.h"
#include "components/variations/metrics_util.h"

namespace chrome_variations {

namespace {

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  typedef std::map<ActiveGroupId, VariationID, ActiveGroupIdCompare>
      GroupToIDMap;

  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return Singleton<GroupMapAccessor>::get();
  }

  // Note that this normally only sets the ID for a group the first time, unless
  // |force| is set to true, in which case it will always override it.
  void AssociateID(IDCollectionKey key,
                   const ActiveGroupId& group_identifier,
                   const VariationID id,
                   const bool force) {
#if !defined(NDEBUG)
    // Validate that all collections with this |group_identifier| have the same
    // associated ID.
    DCHECK_EQ(2, ID_COLLECTION_COUNT);
    IDCollectionKey other_key = GOOGLE_WEB_PROPERTIES;
    if (key == GOOGLE_WEB_PROPERTIES)
      other_key = GOOGLE_UPDATE_SERVICE;
    VariationID other_id = GetID(other_key, group_identifier);
    DCHECK(other_id == EMPTY_ID || other_id == id);
#endif

    base::AutoLock scoped_lock(lock_);

    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    if (force ||
        group_to_id_map->find(group_identifier) == group_to_id_map->end())
      (*group_to_id_map)[group_identifier] = id;
  }

  VariationID GetID(IDCollectionKey key,
                    const ActiveGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    GroupToIDMap::const_iterator it = group_to_id_map->find(group_identifier);
    if (it == group_to_id_map->end())
      return EMPTY_ID;
    return it->second;
  }

  void ClearAllMapsForTesting() {
    base::AutoLock scoped_lock(lock_);

    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      GroupToIDMap* map = GetGroupToIDMap(static_cast<IDCollectionKey>(i));
      DCHECK(map);
      map->clear();
    }
  }

 private:
  friend struct DefaultSingletonTraits<GroupMapAccessor>;

  // Retrieves the GroupToIDMap for |key|.
  GroupToIDMap* GetGroupToIDMap(IDCollectionKey key) {
    return &group_to_id_maps_[key];
  }

  GroupMapAccessor() {
    group_to_id_maps_.resize(ID_COLLECTION_COUNT);
  }
  ~GroupMapAccessor() {}

  base::Lock lock_;
  std::vector<GroupToIDMap> group_to_id_maps_;

  DISALLOW_COPY_AND_ASSIGN(GroupMapAccessor);
};

// Singleton helper class that keeps track of the parameters of all variations
// and ensures access to these is thread-safe.
class VariationsParamAssociator {
 public:
  typedef std::pair<std::string, std::string> VariationKey;
  typedef std::map<std::string, std::string> VariationParams;

  // Retrieve the singleton.
  static VariationsParamAssociator* GetInstance() {
    return Singleton<VariationsParamAssociator>::get();
  }

  bool AssociateVariationParams(const std::string& trial_name,
                                const std::string& group_name,
                                const VariationParams& params) {
    base::AutoLock scoped_lock(lock_);

    if (IsFieldTrialActive(trial_name))
      return false;

    const VariationKey key(trial_name, group_name);
    if (ContainsKey(variation_params_, key))
      return false;

    variation_params_[key] = params;
    return true;
  }

  bool GetVariationParams(const std::string& trial_name,
                          VariationParams* params) {
    base::AutoLock scoped_lock(lock_);

    const std::string group_name =
        base::FieldTrialList::FindFullName(trial_name);
    const VariationKey key(trial_name, group_name);
    if (!ContainsKey(variation_params_, key))
      return false;

    *params = variation_params_[key];
    return true;
  }

  void ClearAllParamsForTesting() {
    base::AutoLock scoped_lock(lock_);
    variation_params_.clear();
  }

 private:
  friend struct DefaultSingletonTraits<VariationsParamAssociator>;

  VariationsParamAssociator() {}
  ~VariationsParamAssociator() {}

  // Tests whether a field trial is active (i.e. group() has been called on it).
  // TODO(asvitkine): Expose this as an API on base::FieldTrial.
  bool IsFieldTrialActive(const std::string& trial_name) {
    base::FieldTrial::ActiveGroups active_groups;
    base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
    for (size_t i = 0; i < active_groups.size(); ++i) {
      if (active_groups[i].trial_name == trial_name)
        return true;
    }
    return false;
  }

  base::Lock lock_;
  std::map<VariationKey, VariationParams> variation_params_;

  DISALLOW_COPY_AND_ASSIGN(VariationsParamAssociator);
};

}  // namespace

ActiveGroupId MakeActiveGroupId(const std::string& trial_name,
                                const std::string& group_name) {
  ActiveGroupId id;
  id.name = metrics::HashName(trial_name);
  id.group = metrics::HashName(group_name);
  return id;
}

void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), id, false);
}

void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), id, true);
}

VariationID GetGoogleVariationID(IDCollectionKey key,
                                 const std::string& trial_name,
                                 const std::string& group_name) {
  return GroupMapAccessor::GetInstance()->GetID(
      key, MakeActiveGroupId(trial_name, group_name));
}

bool AssociateVariationParams(
    const std::string& trial_name,
    const std::string& group_name,
    const std::map<std::string, std::string>& params) {
  return VariationsParamAssociator::GetInstance()->AssociateVariationParams(
      trial_name, group_name, params);
}

bool GetVariationParams(const std::string& trial_name,
                        std::map<std::string, std::string>* params) {
  return VariationsParamAssociator::GetInstance()->GetVariationParams(
      trial_name, params);
}

std::string GetVariationParamValue(const std::string& trial_name,
                                   const std::string& param_name) {
  std::map<std::string, std::string> params;
  if (GetVariationParams(trial_name, &params)) {
    std::map<std::string, std::string>::iterator it = params.find(param_name);
    if (it != params.end())
      return it->second;
  }
  return std::string();
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void ClearAllVariationIDs() {
  GroupMapAccessor::GetInstance()->ClearAllMapsForTesting();
}

void ClearAllVariationParams() {
  VariationsParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

}  // namespace testing

}  // namespace chrome_variations
