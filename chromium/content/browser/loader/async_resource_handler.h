// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceBuffer;
class ResourceContext;
class ResourceDispatcherHostImpl;
class ResourceMessageFilter;
class SharedIOBuffer;

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class AsyncResourceHandler : public ResourceHandler,
                             public ResourceMessageDelegate {
 public:
  AsyncResourceHandler(ResourceMessageFilter* filter,
                       ResourceContext* resource_context,
                       net::URLRequest* request,
                       ResourceDispatcherHostImpl* rdh);
  virtual ~AsyncResourceHandler();

  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnDataDownloaded(int request_id,
                                int bytes_downloaded) OVERRIDE;

 private:
  // IPC message handlers:
  void OnFollowRedirect(int request_id,
                        bool has_new_first_party_for_cookies,
                        const GURL& new_first_party_for_cookies);
  void OnDataReceivedACK(int request_id);

  bool EnsureResourceBufferIsInitialized();
  void ResumeIfDeferred();

  scoped_refptr<ResourceBuffer> buffer_;
  scoped_refptr<ResourceMessageFilter> filter_;
  ResourceContext* resource_context_;
  net::URLRequest* request_;
  ResourceDispatcherHostImpl* rdh_;

  // Number of messages we've sent to the renderer that we haven't gotten an
  // ACK for. This allows us to avoid having too many messages in flight.
  int pending_data_count_;

  int allocation_size_;

  bool did_defer_;

  bool has_checked_for_sufficient_resources_;
  bool sent_received_response_msg_;
  bool sent_first_data_msg_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
