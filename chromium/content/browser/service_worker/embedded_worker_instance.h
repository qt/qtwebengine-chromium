// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class EmbeddedWorkerRegistry;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
class CONTENT_EXPORT EmbeddedWorkerInstance {
 public:
  enum Status {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
  };

  ~EmbeddedWorkerInstance();

  // Starts the worker. It is invalid to call this when the worker is
  // not in STOPPED status.
  // This returns false if starting a worker fails immediately, e.g. when
  // IPC couldn't be sent to the worker or no process was available.
  bool Start(int64 service_worker_version_id,
             const GURL& script_url);

  // Stops the worker. It is invalid to call this when the worker is
  // not in STARTING or RUNNING status.
  // This returns false if stopping a worker fails immediately, e.g. when
  // IPC couldn't be sent to the worker.
  bool Stop();

  // Add or remove |process_id| to the internal process set where this
  // worker can be started.
  void AddProcessReference(int process_id);
  void ReleaseProcessReference(int process_id);

  int embedded_worker_id() const { return embedded_worker_id_; }
  Status status() const { return status_; }
  int process_id() const { return process_id_; }
  int thread_id() const { return thread_id_; }

 private:
  friend class EmbeddedWorkerRegistry;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);

  typedef std::map<int, int> ProcessRefMap;

  // Constructor is called via EmbeddedWorkerRegistry::CreateWorker().
  // This instance holds a ref of |registry|.
  EmbeddedWorkerInstance(EmbeddedWorkerRegistry* registry,
                         int embedded_worker_id);

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually started on |thread_id| in the
  // child process.
  // This will change the internal status from STARTING to RUNNING.
  void OnStarted(int thread_id);

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually stopped in the child process.
  // This will change the internal status from STARTING or RUNNING to
  // STOPPED.
  void OnStopped();

  // Chooses a process to start this worker and populate process_id_.
  // Returns false when no process is available.
  bool ChooseProcess();

  scoped_refptr<EmbeddedWorkerRegistry> registry_;
  const int embedded_worker_id_;
  Status status_;

  // Current running information. -1 indicates the worker is not running.
  int process_id_;
  int thread_id_;

  ProcessRefMap process_refs_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
