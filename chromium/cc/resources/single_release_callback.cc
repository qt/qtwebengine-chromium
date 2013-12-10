// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/single_release_callback.h"

#include "base/callback_helpers.h"
#include "base/logging.h"

namespace cc {

SingleReleaseCallback::SingleReleaseCallback(const ReleaseCallback& callback)
    : has_been_run_(false), callback_(callback) {
  DCHECK(!callback_.is_null())
      << "Use a NULL SingleReleaseCallback for an empty callback.";
}

SingleReleaseCallback::~SingleReleaseCallback() {
  DCHECK(callback_.is_null() || has_been_run_)
      << "SingleReleaseCallback was never run.";
}

void SingleReleaseCallback::Run(unsigned sync_point, bool is_lost) {
  DCHECK(!has_been_run_) << "SingleReleaseCallback was run more than once.";
  has_been_run_ = true;
  callback_.Run(sync_point, is_lost);
}

}  // namespace cc
