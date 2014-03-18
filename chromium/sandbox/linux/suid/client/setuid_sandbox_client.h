// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_
#define SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

namespace base { class Environment; }

namespace sandbox {

// Helper class to use the setuid sandbox. This class is to be used both
// before launching the setuid helper and after being executed through the
// setuid helper.
//
// A typical use would be:
// 1. The browser calls SetupLaunchEnvironment()
// 2. The browser launches a renderer through the setuid sandbox.
// 3. The renderer requests being chroot-ed through ChrootMe() and
//    requests other sandboxing status via the status functions.
class SetuidSandboxClient {
 public:
  // All instantation should go through this factory method.
  static class SetuidSandboxClient* Create();
  ~SetuidSandboxClient();

  // Ask the setuid helper over the setuid sandbox IPC channel to chroot() us
  // to an empty directory.
  // Will only work if we have been launched through the setuid helper.
  bool ChrootMe();
  // When a new PID namespace is created, the process with pid == 1 should
  // assume the role of init.
  // See sandbox/linux/services/init_process_reaper.h for more information
  // on this API.
  bool CreateInitProcessReaper(base::Closure* post_fork_parent_callback);

  // Did we get launched through an up to date setuid binary ?
  bool IsSuidSandboxUpToDate() const;
  // Did we get launched through the setuid helper ?
  bool IsSuidSandboxChild() const;
  // Did the setuid helper create a new PID namespace ?
  bool IsInNewPIDNamespace() const;
  // Did the setuid helper create a new network namespace ?
  bool IsInNewNETNamespace() const;
  // Are we done and fully sandboxed ?
  bool IsSandboxed() const;

  // Set-up the environment. This should be done prior to launching the setuid
  // helper.
  void SetupLaunchEnvironment();

 private:
  // Holds the environment. Will never be NULL.
  base::Environment* env_;
  bool sandboxed_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(SetuidSandboxClient);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_

