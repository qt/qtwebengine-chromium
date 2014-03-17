// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/single_object_invalidation_set.h"

#include "base/values.h"
#include "sync/notifier/invalidation_util.h"

namespace syncer {

SingleObjectInvalidationSet::SingleObjectInvalidationSet() {}

SingleObjectInvalidationSet::~SingleObjectInvalidationSet() {}

void SingleObjectInvalidationSet::Insert(const Invalidation& invalidation) {
  invalidations_.insert(invalidation);
}

void SingleObjectInvalidationSet::InsertAll(
    const SingleObjectInvalidationSet& other) {
  invalidations_.insert(other.begin(), other.end());
}

void SingleObjectInvalidationSet::Clear() {
  invalidations_.clear();
}

bool SingleObjectInvalidationSet::StartsWithUnknownVersion() const {
  return !invalidations_.empty() &&
      invalidations_.begin()->is_unknown_version();
}

size_t SingleObjectInvalidationSet::GetSize() const {
  return invalidations_.size();
}

bool SingleObjectInvalidationSet::IsEmpty() const {
  return invalidations_.empty();
}

namespace {

struct InvalidationComparator {
  bool operator()(const Invalidation& inv1, const Invalidation& inv2) {
    return inv1.Equals(inv2);
  }
};

}  // namespace

bool SingleObjectInvalidationSet::operator==(
    const SingleObjectInvalidationSet& other) const {
  return std::equal(invalidations_.begin(),
                    invalidations_.end(),
                    other.invalidations_.begin(),
                    InvalidationComparator());
}

SingleObjectInvalidationSet::const_iterator
SingleObjectInvalidationSet::begin() const {
  return invalidations_.begin();
}

SingleObjectInvalidationSet::const_iterator
SingleObjectInvalidationSet::end() const {
  return invalidations_.end();
}

SingleObjectInvalidationSet::const_reverse_iterator
SingleObjectInvalidationSet::rbegin() const {
  return invalidations_.rbegin();
}

SingleObjectInvalidationSet::const_reverse_iterator
SingleObjectInvalidationSet::rend() const {
  return invalidations_.rend();
}

const Invalidation& SingleObjectInvalidationSet::back() const {
  return *invalidations_.rbegin();
}

scoped_ptr<base::ListValue> SingleObjectInvalidationSet::ToValue() const {
  scoped_ptr<base::ListValue> value(new ListValue);
  for (InvalidationsSet::const_iterator it = invalidations_.begin();
       it != invalidations_.end(); ++it) {
    value->Append(it->ToValue().release());
  }
  return value.Pass();
}

bool SingleObjectInvalidationSet::ResetFromValue(
    const base::ListValue& list) {
  for (size_t i = 0; i < list.GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (!list.GetDictionary(i, &dict)) {
      DLOG(WARNING) << "Could not find invalidation at index " << i;
      return false;
    }
    scoped_ptr<Invalidation> invalidation = Invalidation::InitFromValue(*dict);
    if (!invalidation) {
      DLOG(WARNING) << "Failed to parse invalidation at index " << i;
      return false;
    }
    invalidations_.insert(*invalidation);
  }
  return true;
}

}  // namespace syncer
