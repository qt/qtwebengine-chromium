// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_BRIDGE_H_
#define WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_BRIDGE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace webkit_glue {

class WebSocketStreamHandleBridge
    : public base::RefCountedThreadSafe<WebSocketStreamHandleBridge> {
 public:
  virtual void Connect(const GURL& url) = 0;

  virtual bool Send(const std::vector<char>& data) = 0;

  virtual void Close() = 0;

 protected:
  friend class base::RefCountedThreadSafe<WebSocketStreamHandleBridge>;
  WebSocketStreamHandleBridge() {}
  virtual ~WebSocketStreamHandleBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamHandleBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBSOCKETSTREAMHANDLE_BRIDGE_H_
