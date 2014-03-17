// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_

#include <string>

#include "content/renderer/media/media_stream_dispatcher.h"
#include "url/gurl.h"

namespace content {

// This class is a mock implementation of MediaStreamDispatcher.
class MockMediaStreamDispatcher : public MediaStreamDispatcher {
 public:
  MockMediaStreamDispatcher();
  virtual ~MockMediaStreamDispatcher();

  virtual void GenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      const StreamOptions& components,
      const GURL& url) OVERRIDE;
  virtual void CancelGenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>&
          event_handler) OVERRIDE;
  virtual void StopStreamDevice(const StreamDeviceInfo& device_info) OVERRIDE;
  virtual bool IsStream(const std::string& label) OVERRIDE;
  virtual int video_session_id(const std::string& label, int index) OVERRIDE;
  virtual int audio_session_id(const std::string& label, int index) OVERRIDE;

  int request_id() const { return request_id_; }
  int request_stream_counter() const { return request_stream_counter_; }
  void IncrementSessionId() { ++session_id_; }

  int stop_audio_device_counter() const { return stop_audio_device_counter_; }
  int stop_video_device_counter() const { return stop_video_device_counter_; }

  const std::string& stream_label() const { return stream_label_;}
  StreamDeviceInfoArray audio_array() const { return audio_array_; }
  StreamDeviceInfoArray video_array() const { return video_array_; }

 private:
  int request_id_;
  base::WeakPtr<MediaStreamDispatcherEventHandler> event_handler_;
  int request_stream_counter_;
  int stop_audio_device_counter_;
  int stop_video_device_counter_;

  std::string stream_label_;
  int session_id_;
  StreamDeviceInfoArray audio_array_;
  StreamDeviceInfoArray video_array_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
