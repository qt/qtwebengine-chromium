// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

class SkBitmap;

namespace blink {
class WebFrame;
class WebURLResponse;
}

namespace content {

class ResourceFetcher;

// A resource fetcher that returns all (differently-sized) frames in
// an image. Useful for favicons.
class MultiResolutionImageResourceFetcher {
 public:
  typedef base::Callback<void(MultiResolutionImageResourceFetcher*,
                              const std::vector<SkBitmap>&)> Callback;

  MultiResolutionImageResourceFetcher(
      const GURL& image_url,
      blink::WebFrame* frame,
      int id,
      blink::WebURLRequest::TargetType target_type,
      const Callback& callback);

  virtual ~MultiResolutionImageResourceFetcher();

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

  // HTTP status code upon fetch completion.
  int http_status_code() const { return http_status_code_; }

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const blink::WebURLResponse& response,
                          const std::string& data);

  Callback callback_;

  // Unique identifier for the request.
  const int id_;

  // HTTP status code upon fetch completion.
  int http_status_code_;

  // URL of the image.
  const GURL image_url_;

  // Does the actual download.
  scoped_ptr<ResourceFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(MultiResolutionImageResourceFetcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
