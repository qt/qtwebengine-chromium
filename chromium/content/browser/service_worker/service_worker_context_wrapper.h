// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_context.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class ServiceWorkerContextCore;

// A refcounted wrapper class for our core object. Higher level content lib
// classes keep references to this class on mutliple threads. The inner core
// instance is strictly single threaded and is not refcounted, the core object
// is what is used internally in the service worker lib.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : NON_EXPORTED_BASE(public ServiceWorkerContext),
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper> {
 public:
  ServiceWorkerContextWrapper();

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            quota::QuotaManagerProxy* quota_manager_proxy);
  void Shutdown();

  // The core context is only for use on the IO thread.
  ServiceWorkerContextCore* context();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWrapper>;
  virtual ~ServiceWorkerContextWrapper();

  scoped_ptr<ServiceWorkerContextCore> context_core_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
