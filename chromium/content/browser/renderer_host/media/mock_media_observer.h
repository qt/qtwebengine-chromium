// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "content/public/browser/media_observer.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMediaObserver : public MediaObserver {
 public:
  MockMediaObserver();
  virtual ~MockMediaObserver();

  MOCK_METHOD1(OnAudioCaptureDevicesChanged,
               void(const MediaStreamDevices& devices));
  MOCK_METHOD1(OnVideoCaptureDevicesChanged,
                 void(const MediaStreamDevices& devices));
  MOCK_METHOD5(OnMediaRequestStateChanged,
               void(int render_process_id, int render_view_id,
                    int page_request_id,
                    const MediaStreamDevice& device,
                    const MediaRequestState state));
  MOCK_METHOD6(OnAudioStreamPlayingChanged,
               void(int render_process_id,
                    int render_view_id,
                    int stream_id,
                    bool is_playing,
                    float power_dbfs,
                    bool clipped));
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_MEDIA_OBSERVER_H_
