// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_UNACKED_INVALIDATION_SET_H_
#define SYNC_NOTIFIER_UNACKED_INVALIDATION_SET_H_

#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_util.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {

namespace test_util {
class UnackedInvalidationSetEqMatcher;
}  // test_util

class SingleObjectInvalidationSet;
class ObjectIdInvalidationMap;
class AckHandle;

// Manages the set of invalidations that are awaiting local acknowledgement for
// a particular ObjectId.  This set of invalidations will be persisted across
// restarts, though this class is not directly responsible for that.
class SYNC_EXPORT UnackedInvalidationSet {
 public:
  static const size_t kMaxBufferedInvalidations;

  UnackedInvalidationSet(invalidation::ObjectId id);
  ~UnackedInvalidationSet();

  // Returns the ObjectID of the invalidations this class is tracking.
  const invalidation::ObjectId& object_id() const;

  // Adds a new invalidation to the set awaiting acknowledgement.
  void Add(const Invalidation& invalidation);

  // Adds many new invalidations to the set awaiting acknowledgement.
  void AddSet(const SingleObjectInvalidationSet& invalidations);

  // Exports the set of invalidations awaiting acknowledgement as an
  // ObjectIdInvalidationMap.  Each of these invalidations will be associated
  // with the given |ack_handler|.
  //
  // The contents of the UnackedInvalidationSet are not directly modified by
  // this procedure, but the AckHandles stored in those exported invalidations
  // are likely to end up back here in calls to Acknowledge() or Drop().
  void ExportInvalidations(WeakHandle<AckHandler> ack_handler,
                           ObjectIdInvalidationMap* out) const;

  // Removes all stored invalidations from this object.
  void Clear();

  // Indicates that a handler has registered to handle these invalidations.
  //
  // Registrations with the invalidations server persist across restarts, but
  // registrations from InvalidationHandlers to the InvalidationService are not.
  // In the time immediately after a restart, it's possible that the server
  // will send us invalidations, and we won't have a handler to send them to.
  //
  // The SetIsRegistered() call indicates that this period has come to an end.
  // There is now a handler that can receive these invalidations.  Once this
  // function has been called, the kMaxBufferedInvalidations limit will be
  // ignored.  It is assumed that the handler will manage its own buffer size.
  void SetHandlerIsRegistered();

  // Indicates that the handler has now unregistered itself.
  //
  // This causes the object to resume enforcement of the
  // kMaxBufferedInvalidations limit.
  void SetHandlerIsUnregistered();

  // Given an AckHandle belonging to one of the contained invalidations, finds
  // the invalidation and drops it from the list.  It is considered to be
  // acknowledged, so there is no need to continue maintaining its state.
  void Acknowledge(const AckHandle& handle);

  // Given an AckHandle belonging to one of the contained invalidations, finds
  // the invalidation, drops it from the list, and adds additional state to
  // indicate that this invalidation has been lost without being acted on.
  void Drop(const AckHandle& handle);

  scoped_ptr<base::DictionaryValue> ToValue() const;
  bool ResetFromValue(const base::DictionaryValue& value);

 private:
  // Allow this test helper to have access to our internals.
  friend class test_util::UnackedInvalidationSetEqMatcher;

  typedef std::set<Invalidation, InvalidationVersionLessThan> InvalidationsSet;

  bool ResetListFromValue(const base::ListValue& value);

  // Limits the list size to the given maximum.  This function will correctly
  // update this class' internal data to indicate if invalidations have been
  // dropped.
  void Truncate(size_t max_size);

  bool registered_;
  invalidation::ObjectId object_id_;
  InvalidationsSet invalidations_;
};

typedef std::map<invalidation::ObjectId,
                 UnackedInvalidationSet,
                 ObjectIdLessThan> UnackedInvalidationsMap;

}  // namespace syncer

#endif  // SYNC_NOTIFIER_UNACKED_INVALIDATION_SET_H_
