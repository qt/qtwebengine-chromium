// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace content {

class MediaSourceCreatedObserver {
 public:
  MediaSourceCreatedObserver()
     : result_(false),
       description_(NULL) {
  }

  void OnCreateNativeSourcesComplete(
      blink::WebMediaStream* description,
      bool request_succeeded) {
    result_ = request_succeeded;
    description_ = description;
  }

  blink::WebMediaStream* description() const {
    return description_;
  }
  bool result() const { return result_; }

 private:
  bool result_;
  blink::WebMediaStream* description_;
};

class MediaStreamDependencyFactoryTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
  }

  blink::WebMediaStream CreateWebKitMediaStream(bool audio, bool video) {
    blink::WebVector<blink::WebMediaStreamSource> audio_sources(
        audio ? static_cast<size_t>(1) : 0);
    blink::WebVector<blink::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);
    MediaStreamSourceExtraData::SourceStopCallback dummy_callback;

    if (audio) {
      StreamDeviceInfo info;
      info.device.type = content::MEDIA_DEVICE_AUDIO_CAPTURE;
      info.device.name = "audio";
      info.session_id = 99;
      audio_sources[0].initialize("audio",
                                  blink::WebMediaStreamSource::TypeAudio,
                                  "audio");
      audio_sources[0].setExtraData(
          new MediaStreamSourceExtraData(info, dummy_callback));
      audio_sources_.assign(audio_sources);
    }
    if (video) {
      StreamDeviceInfo info;
      info.device.type = content::MEDIA_DEVICE_VIDEO_CAPTURE;
      info.device.name = "video";
      info.session_id = 98;
      video_sources[0].initialize("video",
                                  blink::WebMediaStreamSource::TypeVideo,
                                  "video");
      video_sources[0].setExtraData(
          new MediaStreamSourceExtraData(info, dummy_callback));
      video_sources_.assign(video_sources);
    }
    blink::WebMediaStream stream_desc;
    blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
        audio_sources.size());
    for (size_t i = 0; i < audio_track_vector.size(); ++i) {
      audio_track_vector[i].initialize(audio_sources[i].id(),
                                       audio_sources[i]);
    }

    blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
        video_sources.size());
    for (size_t i = 0; i < video_track_vector.size(); ++i) {
      video_track_vector[i].initialize(video_sources[i].id(),
                                       video_sources[i]);
    }

    stream_desc.initialize("media stream", audio_track_vector,
                           video_track_vector);
    return stream_desc;
  }

  void CreateNativeSources(blink::WebMediaStream* descriptor) {
    static const int kRenderViewId = 1;

    MediaSourceCreatedObserver observer;
    blink::WebMediaConstraints audio_constraints;
    dependency_factory_->CreateNativeMediaSources(
        kRenderViewId,
        blink::WebMediaConstraints(),
        blink::WebMediaConstraints(),
        descriptor,
        base::Bind(
            &MediaSourceCreatedObserver::OnCreateNativeSourcesComplete,
            base::Unretained(&observer)));

    EXPECT_FALSE(observer.result());
    // Change the state of the created source to live. This should trigger
    // MediaSourceCreatedObserver::OnCreateNativeSourcesComplete
    if (dependency_factory_->last_video_source()) {
      dependency_factory_->last_audio_source()->SetLive();
      dependency_factory_->last_video_source()->SetLive();
    }
    EXPECT_TRUE(observer.result());
    EXPECT_TRUE(observer.description() == descriptor);
  }

  void VerifyMediaStream(const blink::WebMediaStream& stream_desc,
                         size_t num_audio_tracks,
                         size_t num_video_tracks) {
    content::MediaStreamExtraData* extra_data =
        static_cast<content::MediaStreamExtraData*>(stream_desc.extraData());
    ASSERT_TRUE(extra_data && extra_data->stream().get());
    EXPECT_TRUE(extra_data->is_local());
    EXPECT_EQ(num_audio_tracks, extra_data->stream()->GetAudioTracks().size());
    EXPECT_EQ(num_video_tracks, extra_data->stream()->GetVideoTracks().size());
  }

 protected:
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
  blink::WebVector<blink::WebMediaStreamSource> audio_sources_;
  blink::WebVector<blink::WebMediaStreamSource> video_sources_;
};

TEST_F(MediaStreamDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockWebRTCPeerConnectionHandlerClient client_jsep;
  scoped_ptr<blink::WebRTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(&client_jsep));
  EXPECT_TRUE(pc_handler.get() != NULL);
}

TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStream) {
  blink::WebMediaStream stream_desc = CreateWebKitMediaStream(true, true);
  CreateNativeSources(&stream_desc);

  dependency_factory_->CreateNativeLocalMediaStream(&stream_desc);
  VerifyMediaStream(stream_desc, 1, 1);
}

// Test that we don't crash if a MediaStream is created in WebKit with unknown
// sources. This can for example happen if a MediaStream is created with
// remote tracks.
TEST_F(MediaStreamDependencyFactoryTest, CreateNativeMediaStreamWithoutSource) {
  // Create a WebKit MediaStream description.
  blink::WebMediaStreamSource audio_source;
  audio_source.initialize("audio source",
                          blink::WebMediaStreamSource::TypeAudio,
                          "something");
  blink::WebMediaStreamSource video_source;
  video_source.initialize("video source",
                          blink::WebMediaStreamSource::TypeVideo,
                          "something");

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
      static_cast<size_t>(1));
  audio_tracks[0].initialize(audio_source.id(), audio_source);
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
      static_cast<size_t>(1));
  video_tracks[0].initialize(video_source.id(), video_source);

  blink::WebMediaStream stream_desc;
  stream_desc.initialize("new stream", audio_tracks, video_tracks);

  EXPECT_TRUE(dependency_factory_->EnsurePeerConnectionFactory());
  dependency_factory_->CreateNativeLocalMediaStream(&stream_desc);
  VerifyMediaStream(stream_desc, 0, 0);
}

TEST_F(MediaStreamDependencyFactoryTest, AddAndRemoveNativeTrack) {
  blink::WebMediaStream stream_desc = CreateWebKitMediaStream(true, true);
  CreateNativeSources(&stream_desc);

  dependency_factory_->CreateNativeLocalMediaStream(&stream_desc);
  VerifyMediaStream(stream_desc, 1, 1);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream_desc.audioTracks(audio_tracks);
  EXPECT_TRUE(dependency_factory_->RemoveNativeMediaStreamTrack(
      stream_desc, audio_tracks[0]));
  VerifyMediaStream(stream_desc, 0, 1);

  EXPECT_TRUE(dependency_factory_->AddNativeMediaStreamTrack(
      stream_desc, audio_tracks[0]));
  VerifyMediaStream(stream_desc, 1, 1);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream_desc.videoTracks(video_tracks);
  EXPECT_TRUE(dependency_factory_->RemoveNativeMediaStreamTrack(
      stream_desc, video_tracks[0]));
  VerifyMediaStream(stream_desc, 1, 0);

  EXPECT_TRUE(dependency_factory_->AddNativeMediaStreamTrack(
      stream_desc, video_tracks[0]));
  VerifyMediaStream(stream_desc, 1, 1);
}

}  // namespace content
