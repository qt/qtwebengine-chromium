// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_

#include "base/memory/singleton.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/resource_type.h"

namespace appcache {

class AppCacheRequestHandler;
class AppCacheService;

// An interceptor to hijack requests and potentially service them out of
// the appcache.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheInterceptor
    : public net::URLRequest::Interceptor {
 public:
  // Registers a singleton instance with the net library.
  // Should be called early in the IO thread prior to initiating requests.
  static void EnsureRegistered() {
    CHECK(GetInstance());
  }

  // Must be called to make a request eligible for retrieval from an appcache.
  static void SetExtraRequestInfo(net::URLRequest* request,
                                  AppCacheService* service,
                                  int process_id,
                                  int host_id,
                                  ResourceType::Type resource_type);

  // May be called after response headers are complete to retrieve extra
  // info about the response.
  static void GetExtraResponseInfo(net::URLRequest* request,
                                   int64* cache_id,
                                   GURL* manifest_url);

  // Methods to support cross site navigations.
  static void PrepareForCrossSiteTransfer(net::URLRequest* request,
                                          int old_process_id);
  static void CompleteCrossSiteTransfer(net::URLRequest* request,
                                        int new_process_id,
                                        int new_host_id);

  static AppCacheInterceptor* GetInstance();

 protected:
  // Override from net::URLRequest::Interceptor:
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppCacheInterceptor>;

  AppCacheInterceptor();
  virtual ~AppCacheInterceptor();

  static void SetHandler(net::URLRequest* request,
                         AppCacheRequestHandler* handler);
  static AppCacheRequestHandler* GetHandler(net::URLRequest* request);

  DISALLOW_COPY_AND_ASSIGN(AppCacheInterceptor);
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_
