// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_REGISTRY_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_REGISTRY_H_

#include <string>

#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"

namespace content {

class MockMediaStreamRegistry : public MediaStreamRegistryInterface {
 public:
  explicit MockMediaStreamRegistry(MockMediaStreamDependencyFactory* factory);

  void Init(const std::string& stream_label);
  bool AddVideoTrack(const std::string& track_id);
  virtual blink::WebMediaStream GetMediaStream(const std::string& url)
      OVERRIDE;
  const blink::WebMediaStream test_stream() const;

 private:
  MockMediaStreamDependencyFactory* factory_;
  blink::WebMediaStream test_stream_;
  std::string stream_url_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_REGISTRY_H_
