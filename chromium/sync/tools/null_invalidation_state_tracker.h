// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_
#define SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "sync/notifier/invalidation_state_tracker.h"

namespace syncer {

class NullInvalidationStateTracker
    : public base::SupportsWeakPtr<NullInvalidationStateTracker>,
      public InvalidationStateTracker {
 public:
  NullInvalidationStateTracker();
  virtual ~NullInvalidationStateTracker();

  virtual void SetInvalidatorClientId(const std::string& data) OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;

  virtual std::string GetBootstrapData() const OVERRIDE;
  virtual void SetBootstrapData(const std::string& data) OVERRIDE;

  virtual void SetSavedInvalidations(
      const UnackedInvalidationsMap& states) OVERRIDE;
  virtual UnackedInvalidationsMap GetSavedInvalidations() const OVERRIDE;

  virtual void Clear() OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_
