// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBURLLOADER_IMPL_H_
#define WEBKIT_CHILD_WEBURLLOADER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "webkit/child/webkit_child_export.h"

namespace webkit_glue {

class WebKitPlatformSupportImpl;
struct ResourceResponseInfo;

class WebURLLoaderImpl : public WebKit::WebURLLoader {
 public:
  explicit WebURLLoaderImpl(WebKitPlatformSupportImpl* platform);
  virtual ~WebURLLoaderImpl();

  static WebKit::WebURLError CreateError(const WebKit::WebURL& unreachable_url,
                                         int reason);
  WEBKIT_CHILD_EXPORT static void PopulateURLResponse(
      const GURL& url,
      const ResourceResponseInfo& info,
      WebKit::WebURLResponse* response);

  // WebURLLoader methods:
  virtual void loadSynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLResponse& response,
      WebKit::WebURLError& error,
      WebKit::WebData& data);
  virtual void loadAsynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool value);
  virtual void didChangePriority(WebKit::WebURLRequest::Priority new_priority);

 private:
  class Context;
  scoped_refptr<Context> context_;
  WebKitPlatformSupportImpl* platform_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBURLLOADER_IMPL_H_
