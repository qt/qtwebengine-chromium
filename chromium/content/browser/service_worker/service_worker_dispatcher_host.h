// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class ServiceWorkerContext;

class ServiceWorkerDispatcherHost : public BrowserMessageFilter {
 public:
  explicit ServiceWorkerDispatcherHost(ServiceWorkerContext* context);

  // BrowserIOMessageFilter implementation
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 protected:
  virtual ~ServiceWorkerDispatcherHost();

 private:
  // IPC Message handlers

  void OnRegisterServiceWorker(int32 registry_id,
                            const string16& scope,
                            const GURL& script_url);
  void OnUnregisterServiceWorker(int32 registry_id, const string16& scope);

  scoped_refptr<ServiceWorkerContext> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
