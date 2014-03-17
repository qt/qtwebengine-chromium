// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/net_errors.h"


namespace net {
class URLRequest;
}  // namespace net

namespace content {
class ByteStreamWriter;
class ByteStreamReader;
class DownloadRequestHandle;
struct DownloadCreateInfo;

// Forwards data to the download thread.
class CONTENT_EXPORT DownloadResourceHandler
    : public ResourceHandler,
      public base::SupportsWeakPtr<DownloadResourceHandler> {
 public:
  // Size of the buffer used between the DownloadResourceHandler and the
  // downstream receiver of its output.
  static const int kDownloadByteStreamSize;

  // started_cb will be called exactly once on the UI thread.
  // |id| should be invalid if the id should be automatically assigned.
  DownloadResourceHandler(
      uint32 id,
      net::URLRequest* request,
      const DownloadUrlParameters::OnStartedCallback& started_cb,
      scoped_ptr<DownloadSaveInfo> save_info);

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;

  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;

  // Send the download creation information to the download thread.
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;

  // Pass-through implementation.
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  virtual bool OnWillRead(int request_id,
                          scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) OVERRIDE;

  virtual bool OnReadCompleted(int request_id, int bytes_read,
                               bool* defer) OVERRIDE;

  virtual void OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   bool* defer) OVERRIDE;

  // N/A to this flavor of DownloadHandler.
  virtual void OnDataDownloaded(int request_id, int bytes_downloaded) OVERRIDE;

  void PauseRequest();
  void ResumeRequest();
  void CancelRequest();

  std::string DebugString() const;

 private:
  virtual ~DownloadResourceHandler();

  // Arrange for started_cb_ to be called on the UI thread with the
  // below values, nulling out started_cb_.  Should only be called
  // on the IO thread.
  void CallStartedCB(DownloadItem* item, net::Error error);

  uint32 download_id_;
  // This is read only on the IO thread, but may only
  // be called on the UI thread.
  DownloadUrlParameters::OnStartedCallback started_cb_;
  scoped_ptr<DownloadSaveInfo> save_info_;

  // Data flow
  scoped_refptr<net::IOBuffer> read_buffer_;       // From URLRequest.
  scoped_ptr<ByteStreamWriter> stream_writer_; // To rest of system.

  // The following are used to collect stats.
  base::TimeTicks download_start_time_;
  base::TimeTicks last_read_time_;
  base::TimeTicks last_stream_pause_time_;
  base::TimeDelta total_pause_time_;
  size_t last_buffer_size_;
  int64 bytes_read_;

  int pause_count_;
  bool was_deferred_;

  // For DCHECKing
  bool on_response_started_called_;

  static const int kReadBufSize = 32768;  // bytes
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
