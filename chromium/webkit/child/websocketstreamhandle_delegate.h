// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_DELEGATE_H_
#define WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_DELEGATE_H_

#include "base/strings/string16.h"

class GURL;

namespace blink {
class WebSocketStreamHandle;
}

namespace webkit_glue {

class WebSocketStreamHandleDelegate {
 public:
  WebSocketStreamHandleDelegate() {}

  virtual void WillOpenStream(blink::WebSocketStreamHandle* handle,
                              const GURL& url) {}
  virtual void WillSendData(blink::WebSocketStreamHandle* handle,
                            const char* data, int len) {}

  virtual void DidOpenStream(blink::WebSocketStreamHandle* handle,
                             int max_amount_send_allowed) {}
  virtual void DidSendData(blink::WebSocketStreamHandle* handle,
                           int amount_sent) {}
  virtual void DidReceiveData(blink::WebSocketStreamHandle* handle,
                              const char* data, int len) {}
  virtual void DidClose(blink::WebSocketStreamHandle*) {}
  virtual void DidFail(blink::WebSocketStreamHandle* handle,
                       int error_code,
                       const string16& error_msg) {}

 protected:
  virtual ~WebSocketStreamHandleDelegate() {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_DELEGATE_H_
