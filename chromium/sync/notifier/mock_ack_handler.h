// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_MOCK_ACK_HANDLER_H_
#define SYNC_NOTIFIER_MOCK_ACK_HANDLER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/ack_handler.h"

namespace syncer {

class Invalidation;

// This AckHandler implementation colaborates with the FakeInvalidationService
// to enable unit tests to assert that invalidations are being acked properly.
class SYNC_EXPORT MockAckHandler
  : public AckHandler,
    public base::SupportsWeakPtr<MockAckHandler> {
 public:
  MockAckHandler();
  virtual ~MockAckHandler();

  // Sets up some internal state to track this invalidation, and modifies it so
  // that its Acknowledge() and Drop() methods will route back to us.
  void RegisterInvalidation(Invalidation* invalidation);

  // No one was listening for this invalidation, so no one will receive it or
  // ack it.  We keep track of it anyway to let tests make assertions about it.
  void RegisterUnsentInvalidation(Invalidation* invalidation);

  // Returns true if the specified invalidaition has been delivered, but has not
  // been acknowledged yet.
  bool IsUnacked(const Invalidation& invalidation) const;

  // Returns true if the specified invalidation was never delivered.
  bool IsUnsent(const Invalidation& invalidation) const;

  // Implementation of AckHandler.
  virtual void Acknowledge(
      const invalidation::ObjectId& id,
      const AckHandle& handle) OVERRIDE;
  virtual void Drop(
      const invalidation::ObjectId& id,
      const AckHandle& handle) OVERRIDE;

 private:
  typedef std::vector<syncer::Invalidation> InvalidationVector;

  WeakHandle<AckHandler> WeakHandleThis();

  InvalidationVector unsent_invalidations_;
  InvalidationVector unacked_invalidations_;
  InvalidationVector acked_invalidations_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_MOCK_ACK_HANDLER_H_
