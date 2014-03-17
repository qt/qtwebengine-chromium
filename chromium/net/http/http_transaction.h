// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_H_
#define NET_HTTP_HTTP_TRANSACTION_H_

#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

class AuthCredentials;
class BoundNetLog;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class X509Certificate;

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpTransaction {
 public:
  // Stops any pending IO and destroys the transaction object.
  virtual ~HttpTransaction() {}

  // Starts the HTTP transaction (i.e., sends the HTTP request).
  //
  // Returns OK if the transaction could be started synchronously, which means
  // that the request was served from the cache.  ERR_IO_PENDING is returned to
  // indicate that the CompletionCallback will be notified once response info is
  // available or if an IO error occurs.  Any other return value indicates that
  // the transaction could not be started.
  //
  // Regardless of the return value, the caller is expected to keep the
  // request_info object alive until Destroy is called on the transaction.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Start(const HttpRequestInfo* request_info,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) = 0;

  // Restarts the HTTP transaction, ignoring the last error.  This call can
  // only be made after a call to Start (or RestartIgnoringLastError) failed.
  // Once Read has been called, this method cannot be called.  This method is
  // used, for example, to continue past various SSL related errors.
  //
  // Not all errors can be ignored using this method.  See error code
  // descriptions for details about errors that can be ignored.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int RestartIgnoringLastError(const CompletionCallback& callback) = 0;

  // Restarts the HTTP transaction with a client certificate.
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     const CompletionCallback& callback) = 0;

  // Restarts the HTTP transaction with authentication credentials.
  virtual int RestartWithAuth(const AuthCredentials& credentials,
                              const CompletionCallback& callback) = 0;

  // Returns true if auth is ready to be continued. Callers should check
  // this value anytime Start() completes: if it is true, the transaction
  // can be resumed with RestartWithAuth(L"", L"", callback) to resume
  // the automatic auth exchange. This notification gives the caller a
  // chance to process the response headers from all of the intermediate
  // restarts needed for authentication.
  virtual bool IsReadyToRestartForAuth() = 0;

  // Once response info is available for the transaction, response data may be
  // read by calling this method.
  //
  // Response data is copied into the given buffer and the number of bytes
  // copied is returned.  ERR_IO_PENDING is returned if response data is not
  // yet available.  The CompletionCallback is notified when the data copy
  // completes, and it is passed the number of bytes that were successfully
  // copied.  Or, if a read error occurs, the CompletionCallback is notified of
  // the error.  Any other negative return value indicates that the transaction
  // could not be read.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  // If the operation is not completed immediately, the transaction must acquire
  // a reference to the provided buffer.
  //
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) = 0;

  // Stops further caching of this request by the HTTP cache, if there is any.
  virtual void StopCaching() = 0;

  // Gets the full request headers sent to the server.  This is guaranteed to
  // work only if Start returns success and the underlying transaction supports
  // it.  (Right now, this is only network transactions, not cache ones.)
  //
  // Returns true and overwrites headers if it can get the request headers;
  // otherwise, returns false and does not modify headers.
  virtual bool GetFullRequestHeaders(HttpRequestHeaders* headers) const = 0;

  // Called to tell the transaction that we have successfully reached the end
  // of the stream. This is equivalent to performing an extra Read() at the end
  // that should return 0 bytes. This method should not be called if the
  // transaction is busy processing a previous operation (like a pending Read).
  //
  // DoneReading may also be called before the first Read() to notify that the
  // entire response body is to be ignored (e.g., in a redirect).
  virtual void DoneReading() = 0;

  // Returns the response info for this transaction or NULL if the response
  // info is not available.
  virtual const HttpResponseInfo* GetResponseInfo() const = 0;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const = 0;

  // Returns the upload progress in bytes.  If there is no upload data,
  // zero will be returned.  This does not include the request headers.
  virtual UploadProgress GetUploadProgress() const = 0;

  // Populates all of load timing, except for request start times and receive
  // headers time.
  // |load_timing_info| must have all null times when called.  Returns false and
  // does not modify |load_timing_info| if there's no timing information to
  // provide.
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const = 0;

  // Called when the priority of the parent job changes.
  virtual void SetPriority(RequestPriority priority) = 0;

  // Set the WebSocketHandshakeStreamBase::CreateHelper to be used for the
  // request.  Only relevant to WebSocket transactions. Must be called before
  // Start(). Ownership of |create_helper| remains with the caller.
  virtual void SetWebSocketHandshakeStreamCreateHelper(
      WebSocketHandshakeStreamBase::CreateHelper* create_helper) = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_H_
