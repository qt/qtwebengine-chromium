// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/base/scoped_ref_ptr.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::IceCandidateCollection;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;

namespace content {

template <class V>
static typename V::iterator FindTrack(V* vector,
                                      const std::string& track_id) {
  typename V::iterator it = vector->begin();
  for (; it != vector->end(); ++it) {
    if ((*it)->id() == track_id) {
      break;
    }
  }
  return it;
};

class MockMediaStream : public webrtc::MediaStreamInterface {
 public:
  explicit MockMediaStream(const std::string& label)
      : label_(label),
        observer_(NULL) {
  }
  virtual bool AddTrack(AudioTrackInterface* track) OVERRIDE {
    audio_track_vector_.push_back(track);
    if (observer_)
      observer_->OnChanged();
    return true;
  }
  virtual bool AddTrack(VideoTrackInterface* track) OVERRIDE {
    video_track_vector_.push_back(track);
    if (observer_)
      observer_->OnChanged();
    return true;
  }
  virtual bool RemoveTrack(AudioTrackInterface* track) OVERRIDE {
    AudioTrackVector::iterator it = FindTrack(&audio_track_vector_,
                                              track->id());
    if (it == audio_track_vector_.end())
      return false;
    audio_track_vector_.erase(it);
    if (observer_)
      observer_->OnChanged();
    return true;
  }
  virtual bool RemoveTrack(VideoTrackInterface* track) OVERRIDE {
    VideoTrackVector::iterator it = FindTrack(&video_track_vector_,
                                              track->id());
    if (it == video_track_vector_.end())
      return false;
    video_track_vector_.erase(it);
    if (observer_)
      observer_->OnChanged();
    return true;
  }
  virtual std::string label() const OVERRIDE { return label_; }
  virtual AudioTrackVector GetAudioTracks() OVERRIDE {
    return audio_track_vector_;
  }
  virtual VideoTrackVector GetVideoTracks() OVERRIDE {
    return video_track_vector_;
  }
  virtual talk_base::scoped_refptr<AudioTrackInterface>
      FindAudioTrack(const std::string& track_id) OVERRIDE {
    AudioTrackVector::iterator it = FindTrack(&audio_track_vector_, track_id);
    return it == audio_track_vector_.end() ? NULL : *it;
  }
  virtual talk_base::scoped_refptr<VideoTrackInterface>
      FindVideoTrack(const std::string& track_id) OVERRIDE {
    VideoTrackVector::iterator it = FindTrack(&video_track_vector_, track_id);
    return it == video_track_vector_.end() ? NULL : *it;
  }
  virtual void RegisterObserver(ObserverInterface* observer) OVERRIDE {
    DCHECK(!observer_);
    observer_ = observer;
  }
  virtual void UnregisterObserver(ObserverInterface* observer) OVERRIDE {
    DCHECK(observer_ == observer);
    observer_ = NULL;
  }

 protected:
  virtual ~MockMediaStream() {}

 private:
  std::string label_;
  AudioTrackVector audio_track_vector_;
  VideoTrackVector video_track_vector_;
  webrtc::ObserverInterface* observer_;
};

MockAudioSource::MockAudioSource(
    const webrtc::MediaConstraintsInterface* constraints)
    : observer_(NULL),
      state_(MediaSourceInterface::kInitializing),
      optional_constraints_(constraints->GetOptional()),
      mandatory_constraints_(constraints->GetMandatory()) {
}

MockAudioSource::~MockAudioSource() {}

void MockAudioSource::RegisterObserver(webrtc::ObserverInterface* observer) {
  observer_ = observer;
}

void MockAudioSource::UnregisterObserver(webrtc::ObserverInterface* observer) {
  DCHECK(observer_ == observer);
  observer_ = NULL;
}

void MockAudioSource::SetLive() {
  DCHECK(state_ == MediaSourceInterface::kInitializing ||
         state_ == MediaSourceInterface::kLive);
  state_ = MediaSourceInterface::kLive;
  if (observer_)
    observer_->OnChanged();
}

void MockAudioSource::SetEnded() {
  DCHECK_NE(MediaSourceInterface::kEnded, state_);
  state_ = MediaSourceInterface::kEnded;
  if (observer_)
    observer_->OnChanged();
}

webrtc::MediaSourceInterface::SourceState MockAudioSource::state() const {
  return state_;
}

MockVideoSource::MockVideoSource()
    : state_(MediaSourceInterface::kInitializing) {
}

MockVideoSource::~MockVideoSource() {}

void MockVideoSource::SetVideoCapturer(cricket::VideoCapturer* capturer) {
  capturer_.reset(capturer);
}

cricket::VideoCapturer* MockVideoSource::GetVideoCapturer() {
  return capturer_.get();
}

void MockVideoSource::AddSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

void MockVideoSource::RemoveSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

cricket::VideoRenderer* MockVideoSource::FrameInput() {
  NOTIMPLEMENTED();
  return NULL;
}

void MockVideoSource::RegisterObserver(webrtc::ObserverInterface* observer) {
  observers_.push_back(observer);
}

void MockVideoSource::UnregisterObserver(webrtc::ObserverInterface* observer) {
  for (std::vector<ObserverInterface*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      break;
    }
  }
}

void MockVideoSource::FireOnChanged() {
  std::vector<ObserverInterface*> observers(observers_);
  for (std::vector<ObserverInterface*>::iterator it = observers.begin();
       it != observers.end(); ++it) {
    (*it)->OnChanged();
  }
}

void MockVideoSource::SetLive() {
  DCHECK(state_ == MediaSourceInterface::kInitializing ||
         state_ == MediaSourceInterface::kLive);
  state_ = MediaSourceInterface::kLive;
  FireOnChanged();
}

void MockVideoSource::SetEnded() {
  DCHECK_NE(MediaSourceInterface::kEnded, state_);
  state_ = MediaSourceInterface::kEnded;
  FireOnChanged();
}

webrtc::MediaSourceInterface::SourceState MockVideoSource::state() const {
  return state_;
}

const cricket::VideoOptions* MockVideoSource::options() const {
  NOTIMPLEMENTED();
  return NULL;
}

MockLocalVideoTrack::MockLocalVideoTrack(std::string id,
                                         webrtc::VideoSourceInterface* source)
    : enabled_(false),
      id_(id),
      state_(MediaStreamTrackInterface::kLive),
      source_(source),
      observer_(NULL) {
}

MockLocalVideoTrack::~MockLocalVideoTrack() {}

void MockLocalVideoTrack::AddRenderer(VideoRendererInterface* renderer) {
  NOTIMPLEMENTED();
}

void MockLocalVideoTrack::RemoveRenderer(VideoRendererInterface* renderer) {
  NOTIMPLEMENTED();
}

std::string MockLocalVideoTrack::kind() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string MockLocalVideoTrack::id() const { return id_; }

bool MockLocalVideoTrack::enabled() const { return enabled_; }

MockLocalVideoTrack::TrackState MockLocalVideoTrack::state() const {
  return state_;
}

bool MockLocalVideoTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

bool MockLocalVideoTrack::set_state(TrackState new_state) {
  state_ = new_state;
  if (observer_)
    observer_->OnChanged();
  return true;
}

void MockLocalVideoTrack::RegisterObserver(ObserverInterface* observer) {
  observer_ = observer;
}

void MockLocalVideoTrack::UnregisterObserver(ObserverInterface* observer) {
  DCHECK(observer_ == observer);
  observer_ = NULL;
}

VideoSourceInterface* MockLocalVideoTrack::GetSource() const {
  return source_.get();
}

class MockSessionDescription : public SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& type,
                         const std::string& sdp)
      : type_(type),
        sdp_(sdp) {
  }
  virtual ~MockSessionDescription() {}
  virtual cricket::SessionDescription* description() OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual const cricket::SessionDescription* description() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual std::string session_id() const OVERRIDE {
    NOTIMPLEMENTED();
    return std::string();
  }
  virtual std::string session_version() const OVERRIDE {
    NOTIMPLEMENTED();
    return std::string();
  }
  virtual std::string type() const OVERRIDE {
    return type_;
  }
  virtual bool AddCandidate(const IceCandidateInterface* candidate) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual size_t number_of_mediasections() const OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual bool ToString(std::string* out) const OVERRIDE {
    *out = sdp_;
    return true;
  }

 private:
  std::string type_;
  std::string sdp_;
};

class MockIceCandidate : public IceCandidateInterface {
 public:
  MockIceCandidate(const std::string& sdp_mid,
                   int sdp_mline_index,
                   const std::string& sdp)
      : sdp_mid_(sdp_mid),
        sdp_mline_index_(sdp_mline_index),
        sdp_(sdp) {
  }
  virtual ~MockIceCandidate() {}
  virtual std::string sdp_mid() const OVERRIDE {
    return sdp_mid_;
  }
  virtual int sdp_mline_index() const OVERRIDE {
    return sdp_mline_index_;
  }
  virtual const cricket::Candidate& candidate() const OVERRIDE {
    // This function should never be called. It will intentionally crash. The
    // base class forces us to return a reference.
    NOTREACHED();
    cricket::Candidate* candidate = NULL;
    return *candidate;
  }
  virtual bool ToString(std::string* out) const OVERRIDE {
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string sdp_;
};

MockMediaStreamDependencyFactory::MockMediaStreamDependencyFactory()
    : MediaStreamDependencyFactory(NULL, NULL),
      mock_pc_factory_created_(false) {
}

MockMediaStreamDependencyFactory::~MockMediaStreamDependencyFactory() {}

bool MockMediaStreamDependencyFactory::EnsurePeerConnectionFactory() {
  mock_pc_factory_created_ = true;
  return true;
}

bool MockMediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return mock_pc_factory_created_;
}

scoped_refptr<webrtc::PeerConnectionInterface>
MockMediaStreamDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::IceServers& ice_servers,
    const webrtc::MediaConstraintsInterface* constraints,
    blink::WebFrame* frame,
    webrtc::PeerConnectionObserver* observer) {
  DCHECK(mock_pc_factory_created_);
  return new talk_base::RefCountedObject<MockPeerConnectionImpl>(this);
}

scoped_refptr<webrtc::AudioSourceInterface>
MockMediaStreamDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  last_audio_source_ =
      new talk_base::RefCountedObject<MockAudioSource>(constraints);
  return last_audio_source_;
}

scoped_refptr<webrtc::VideoSourceInterface>
MockMediaStreamDependencyFactory::CreateLocalVideoSource(
    int video_session_id,
    bool is_screencast,
    const webrtc::MediaConstraintsInterface* constraints) {
  last_video_source_ = new talk_base::RefCountedObject<MockVideoSource>();
  return last_video_source_;
}

scoped_refptr<WebAudioCapturerSource>
MockMediaStreamDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source,
    RTCMediaConstraints* constraints) {
  return NULL;
}

scoped_refptr<webrtc::MediaStreamInterface>
MockMediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  DCHECK(mock_pc_factory_created_);
  return new talk_base::RefCountedObject<MockMediaStream>(label);
}

scoped_refptr<webrtc::VideoTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  DCHECK(mock_pc_factory_created_);
  scoped_refptr<webrtc::VideoTrackInterface> track(
      new talk_base::RefCountedObject<MockLocalVideoTrack>(
          id, source));
  return track;
}

scoped_refptr<webrtc::VideoTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    cricket::VideoCapturer* capturer) {
  DCHECK(mock_pc_factory_created_);

  scoped_refptr<MockVideoSource> source =
      new talk_base::RefCountedObject<MockVideoSource>();
  source->SetVideoCapturer(capturer);

  return new talk_base::RefCountedObject<MockLocalVideoTrack>(id, source.get());
}

scoped_refptr<webrtc::AudioTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& id,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    WebAudioCapturerSource* webaudio_source,
    webrtc::AudioSourceInterface* source,
    const webrtc::MediaConstraintsInterface* constraints) {
  DCHECK(mock_pc_factory_created_);
  DCHECK(!capturer.get());
  return WebRtcLocalAudioTrack::Create(
      id, WebRtcAudioCapturer::CreateCapturer(), webaudio_source,
      source, constraints);
}

SessionDescriptionInterface*
MockMediaStreamDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return new MockSessionDescription(type, sdp);
}

webrtc::IceCandidateInterface*
MockMediaStreamDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return new MockIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

scoped_refptr<WebRtcAudioCapturer>
MockMediaStreamDependencyFactory::MaybeCreateAudioCapturer(
    int render_view_id, const StreamDeviceInfo& device_info) {
  return WebRtcAudioCapturer::CreateCapturer();
}

}  // namespace content
