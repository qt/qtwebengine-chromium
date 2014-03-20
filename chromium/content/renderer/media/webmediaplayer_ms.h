// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/layers/video_frame_provider.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
class WebMediaPlayerClient;
}

namespace media {
class MediaLog;
}

namespace webkit {
class WebLayerImpl;
}

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamClient;
class VideoFrameProvider;
class WebMediaPlayerDelegate;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// VideoFrameProvider
//   provides video frames for rendering.
//
// TODO(wjia): add AudioPlayer.
// AudioPlayer
//   plays audio streams.
//
// blink::WebMediaPlayerClient
//   WebKit client of this media player object.
class WebMediaPlayerMS
    : public blink::WebMediaPlayer,
      public cc::VideoFrameProvider,
      public base::SupportsWeakPtr<WebMediaPlayerMS> {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides VideoFrameProvider.
  WebMediaPlayerMS(blink::WebFrame* frame,
                   blink::WebMediaPlayerClient* client,
                   base::WeakPtr<WebMediaPlayerDelegate> delegate,
                   MediaStreamClient* media_stream_client,
                   media::MediaLog* media_log);
  virtual ~WebMediaPlayerMS();

  virtual void load(LoadType load_type,
                    const blink::WebURL& url,
                    CORSMode cors_mode) OVERRIDE;

  // Playback controls.
  virtual void play() OVERRIDE;
  virtual void pause() OVERRIDE;
  virtual bool supportsFullscreen() const OVERRIDE;
  virtual bool supportsSave() const OVERRIDE;
  virtual void seek(double seconds);
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual void setPreload(blink::WebMediaPlayer::Preload preload) OVERRIDE;
  virtual const blink::WebTimeRanges& buffered() OVERRIDE;
  virtual double maxTimeSeekable() const;

  // Methods for painting.
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha) OVERRIDE;

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const OVERRIDE;
  virtual bool hasAudio() const OVERRIDE;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const OVERRIDE;

  // Getters of playback state.
  virtual bool paused() const OVERRIDE;
  virtual bool seeking() const OVERRIDE;
  virtual double duration() const;
  virtual double currentTime() const;

  // Internal states of loading and network.
  virtual blink::WebMediaPlayer::NetworkState networkState() const OVERRIDE;
  virtual blink::WebMediaPlayer::ReadyState readyState() const OVERRIDE;

  virtual bool didLoadingProgress() const OVERRIDE;

  virtual bool hasSingleSecurityOrigin() const OVERRIDE;
  virtual bool didPassCORSAccessCheck() const OVERRIDE;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  virtual unsigned decodedFrameCount() const OVERRIDE;
  virtual unsigned droppedFrameCount() const OVERRIDE;
  virtual unsigned audioDecodedByteCount() const OVERRIDE;
  virtual unsigned videoDecodedByteCount() const OVERRIDE;

  // VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

 private:
  // The callback for VideoFrameProvider to signal a new frame is available.
  void OnFrameAvailable(const scoped_refptr<media::VideoFrame>& frame);
  // Need repaint due to state change.
  void RepaintInternal();

  // The callback for source to report error.
  void OnSourceError();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  blink::WebMediaPlayerClient* GetClient();

  blink::WebFrame* frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  blink::WebTimeRanges buffered_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  blink::WebMediaPlayerClient* client_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  MediaStreamClient* media_stream_client_;

  // Specify content:: to disambiguate from cc::.
  scoped_refptr<content::VideoFrameProvider> video_frame_provider_;
  bool paused_;

  // |current_frame_| is updated only on main thread. The object it holds
  // can be freed on the compositor thread if it is the last to hold a
  // reference but media::VideoFrame is a thread-safe ref-pointer.
  scoped_refptr<media::VideoFrame> current_frame_;
  // |current_frame_used_| is updated on both main and compositing thread.
  // It's used to track whether |current_frame_| was painted for detecting
  // when to increase |dropped_frame_count_|.
  bool current_frame_used_;
  base::Lock current_frame_lock_;
  bool pending_repaint_;

  scoped_ptr<webkit::WebLayerImpl> video_weblayer_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  bool received_first_frame_;
  bool sequence_started_;
  base::TimeDelta start_time_;
  unsigned total_frame_count_;
  unsigned dropped_frame_count_;
  media::SkCanvasVideoRenderer video_renderer_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;

  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
