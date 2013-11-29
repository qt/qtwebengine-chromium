// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_FILEAPI_WEBFILEWRITER_BASE_H_
#define WEBKIT_RENDERER_FILEAPI_WEBFILEWRITER_BASE_H_

#include "base/platform_file.h"
#include "third_party/WebKit/public/web/WebFileWriter.h"
#include "url/gurl.h"
#include "webkit/renderer/webkit_storage_renderer_export.h"

namespace WebKit {
class WebFileWriterClient;
class WebURL;
}

namespace fileapi {

class WEBKIT_STORAGE_RENDERER_EXPORT WebFileWriterBase
    : public NON_EXPORTED_BASE(WebKit::WebFileWriter) {
 public:
  WebFileWriterBase(
      const GURL& path, WebKit::WebFileWriterClient* client);
  virtual ~WebFileWriterBase();

  // WebFileWriter implementation
  virtual void truncate(long long length);
  virtual void write(long long position, const WebKit::WebURL& blobURL);
  virtual void cancel();

 protected:
  // This calls DidSucceed() or DidFail() based on the value of |error_code|.
  void DidFinish(base::PlatformFileError error_code);

  void DidWrite(int64 bytes, bool complete);
  void DidSucceed();
  void DidFail(base::PlatformFileError error_code);

  // Derived classes must provide these methods to asynchronously perform
  // the requested operation, and they must call the appropiate DidSomething
  // method upon completion and as progress is made in the Write case.
  virtual void DoTruncate(const GURL& path, int64 offset) = 0;
  virtual void DoWrite(const GURL& path, const GURL& blob_url,
                       int64 offset) = 0;
  virtual void DoCancel() = 0;

 private:
  enum OperationType {
    kOperationNone,
    kOperationWrite,
    kOperationTruncate
  };

  enum CancelState {
    kCancelNotInProgress,
    kCancelSent,
    kCancelReceivedWriteResponse,
  };

  void FinishCancel();

  GURL path_;
  WebKit::WebFileWriterClient* client_;
  OperationType operation_;
  CancelState cancel_state_;
};

}  // namespace fileapi

#endif  // WEBKIT_RENDERER_FILEAPI_WEBFILEWRITER_BASE_H_
