// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_IMPL_H_
#define WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "third_party/WebKit/public/platform/WebSocketStreamHandle.h"

namespace webkit_glue {

class WebKitPlatformSupportImpl;

class WebSocketStreamHandleImpl
    : public base::SupportsUserData,
      public blink::WebSocketStreamHandle {
 public:
  explicit WebSocketStreamHandleImpl(WebKitPlatformSupportImpl* platform);
  virtual ~WebSocketStreamHandleImpl();

  // WebSocketStreamHandle methods:
  virtual void connect(
      const blink::WebURL& url,
      blink::WebSocketStreamHandleClient* client);
  virtual bool send(const blink::WebData& data);
  virtual void close();

 private:
  class Context;
  scoped_refptr<Context> context_;
  WebKitPlatformSupportImpl* platform_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamHandleImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_IMPL_H_
