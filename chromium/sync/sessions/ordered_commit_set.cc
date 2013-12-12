// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/ordered_commit_set.h"

#include <algorithm>

#include "base/logging.h"

namespace syncer {
namespace sessions {

OrderedCommitSet::OrderedCommitSet(const ModelSafeRoutingInfo& routes)
    : routes_(routes) {
}

OrderedCommitSet::~OrderedCommitSet() {}

void OrderedCommitSet::AddCommitItem(const int64 metahandle,
                                     ModelType type) {
  if (!HaveCommitItem(metahandle)) {
    inserted_metahandles_.insert(metahandle);
    metahandle_order_.push_back(metahandle);
    projections_[GetGroupForModelType(type, routes_)].push_back(
        inserted_metahandles_.size() - 1);
    types_.push_back(type);
    types_in_list_.Put(type);
  }
}

void OrderedCommitSet::AddCommitItems(
    const std::vector<int64> metahandles,
    ModelType type) {
  for (std::vector<int64>::const_iterator it = metahandles.begin();
       it != metahandles.end(); ++it) {
    AddCommitItem(*it, type);
  }
}

const OrderedCommitSet::Projection& OrderedCommitSet::GetCommitIdProjection(
    ModelSafeGroup group) const {
  Projections::const_iterator i = projections_.find(group);
  DCHECK(i != projections_.end());
  return i->second;
}

void OrderedCommitSet::Append(const OrderedCommitSet& other) {
  for (size_t i = 0; i < other.Size(); ++i) {
    CommitItem item = other.GetCommitItemAt(i);
    AddCommitItem(item.meta, item.group);
  }
}

void OrderedCommitSet::AppendReverse(const OrderedCommitSet& other) {
  for (int i = other.Size() - 1; i >= 0; i--) {
    CommitItem item = other.GetCommitItemAt(i);
    AddCommitItem(item.meta, item.group);
  }
}

void OrderedCommitSet::Truncate(size_t max_size) {
  if (max_size < metahandle_order_.size()) {
    for (size_t i = max_size; i < metahandle_order_.size(); ++i) {
      inserted_metahandles_.erase(metahandle_order_[i]);
    }

    // Some projections may refer to indices that are getting chopped.
    // Since projections are in increasing order, it's easy to fix. Except
    // that you can't erase(..) using a reverse_iterator, so we use binary
    // search to find the chop point.
    Projections::iterator it = projections_.begin();
    for (; it != projections_.end(); ++it) {
      // For each projection, chop off any indices larger than or equal to
      // max_size by looking for max_size using binary search.
      Projection& p = it->second;
      Projection::iterator element = std::lower_bound(p.begin(), p.end(),
        max_size);
      if (element != p.end())
        p.erase(element, p.end());
    }
    metahandle_order_.resize(max_size);
    types_.resize(max_size);
  }
}

void OrderedCommitSet::Clear() {
  inserted_metahandles_.clear();
  metahandle_order_.clear();
  for (Projections::iterator it = projections_.begin();
       it != projections_.end(); ++it) {
    it->second.clear();
  }
  types_.clear();
  types_in_list_.Clear();
}

OrderedCommitSet::CommitItem OrderedCommitSet::GetCommitItemAt(
    const size_t position) const {
  DCHECK(position < Size());
  CommitItem return_item = {metahandle_order_[position],
      types_[position]};
  return return_item;
}

bool OrderedCommitSet::HasBookmarkCommitId() const {
  ModelSafeRoutingInfo::const_iterator group = routes_.find(BOOKMARKS);
  if (group == routes_.end())
    return false;
  Projections::const_iterator proj = projections_.find(group->second);
  if (proj == projections_.end())
    return false;
  DCHECK_LE(proj->second.size(), types_.size());
  for (size_t i = 0; i < proj->second.size(); i++) {
    if (types_[proj->second[i]] == BOOKMARKS)
      return true;
  }
  return false;
}

void OrderedCommitSet::operator=(const OrderedCommitSet& other) {
  inserted_metahandles_ = other.inserted_metahandles_;
  metahandle_order_ = other.metahandle_order_;
  projections_ = other.projections_;
  types_ = other.types_;
  routes_ = other.routes_;
}

}  // namespace sessions
}  // namespace syncer

