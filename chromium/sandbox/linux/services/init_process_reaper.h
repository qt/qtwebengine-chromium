// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_
#define SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_

#include "base/callback_forward.h"

namespace sandbox {

// The current process will fork(). The parent will become a process reaper
// like init(1). The child will continue normally (after this function
// returns).
// If not NULL, |post_fork_parent_callback| will run in the parent almost
// immediately after fork().
// Since this function calls fork(), it's very important that the caller has
// only one thread running.
bool CreateInitProcessReaper(base::Closure* post_fork_parent_callback);

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_
