// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/media/media_player_messages_enums_android.h"
#include "content/renderer/media/android/media_info_loader.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/stream_texture_factory_android.h"
#include "content/renderer/media/crypto/proxy_decryptor.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/android/media_player_android.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaPlayer.h"
#include "ui/gfx/rect_f.h"

namespace media {
class Demuxer;
class MediaLog;
}

namespace WebKit {
class WebFrame;
}

namespace webkit {
class WebLayerImpl;
}

namespace content {
class WebMediaPlayerDelegate;
class RendererMediaPlayerManager;
class WebMediaPlayerProxyAndroid;

#if defined(GOOGLE_TV)
class MediaStreamAudioRenderer;
class MediaStreamClient;
#endif

// This class implements WebKit::WebMediaPlayer by keeping the android
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerAndroid
    : public WebKit::WebMediaPlayer,
      public cc::VideoFrameProvider,
      public base::MessageLoop::DestructionObserver,
      public base::SupportsWeakPtr<WebMediaPlayerAndroid> {
 public:
  // Construct a WebMediaPlayerAndroid object. This class communicates
  // with the MediaPlayerAndroid object in the browser process through
  // |proxy|.
  // TODO(qinmin): |frame| argument is used to determine whether the current
  // player can enter fullscreen. This logic should probably be moved into
  // blink, so that enterFullscreen() will not be called if another video is
  // already in fullscreen.
  WebMediaPlayerAndroid(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client,
      base::WeakPtr<WebMediaPlayerDelegate> delegate,
      RendererMediaPlayerManager* manager,
      WebMediaPlayerProxyAndroid* proxy,
      StreamTextureFactory* factory,
      const scoped_refptr<base::MessageLoopProxy>& media_loop,
      media::MediaLog* media_log);
  virtual ~WebMediaPlayerAndroid();

  // WebKit::WebMediaPlayer implementation.
  virtual void enterFullscreen();
  virtual void exitFullscreen();
  virtual bool canEnterFullscreen() const;

  // Resource loading.
  virtual void load(LoadType load_type,
                    const WebKit::WebURL& url,
                    CORSMode cors_mode) OVERRIDE;

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void pause(bool is_media_related_action);
  virtual void seek(double seconds);
  virtual bool supportsFullscreen() const;
  virtual bool supportsSave() const;
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual const WebKit::WebTimeRanges& buffered();
  virtual double maxTimeSeekable() const;

  // Methods for painting.
  virtual void paint(WebKit::WebCanvas* canvas,
                     const WebKit::WebRect& rect,
                     unsigned char alpha);

  virtual bool copyVideoTextureToPlatformTexture(
      WebKit::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int level,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual WebKit::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double currentTime() const;

  virtual bool didLoadingProgress() const;

  // Internal states of loading and network.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const;
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  // Provide statistics.
  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  // cc::VideoFrameProvider implementation. These methods are running on the
  // compositor thread.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

  // Media player callback handlers.
  void OnMediaMetadataChanged(const base::TimeDelta& duration, int width,
                              int height, bool success);
  void OnPlaybackComplete();
  void OnBufferingUpdate(int percentage);
  void OnSeekComplete(const base::TimeDelta& current_time);
  void OnMediaError(int error_type);
  void OnVideoSizeChanged(int width, int height);
  void OnDurationChanged(const base::TimeDelta& duration);

  // Called to update the current time.
  void OnTimeUpdate(const base::TimeDelta& current_time);

  // Functions called when media player status changes.
  void OnMediaPlayerPlay();
  void OnMediaPlayerPause();
  void OnDidEnterFullscreen();
  void OnDidExitFullscreen();

  // Called when the player is released.
  virtual void OnPlayerReleased();

  // This function is called by the RendererMediaPlayerManager to pause the
  // video and release the media player and surface texture when we switch tabs.
  // However, the actual GlTexture is not released to keep the video screenshot.
  virtual void ReleaseMediaResources();

  // Method inherited from DestructionObserver.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Detach the player from its manager.
  void Detach();

#if defined(GOOGLE_TV)
  // Retrieve geometry of the media player (i.e. location and size of the video
  // frame) if changed. Returns true only if the geometry has been changed since
  // the last call.
  bool RetrieveGeometryChange(gfx::RectF* rect);
#endif

  virtual MediaKeyException generateKeyRequest(
      const WebKit::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length) OVERRIDE;
  virtual MediaKeyException addKey(
      const WebKit::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const WebKit::WebString& session_id) OVERRIDE;
  virtual MediaKeyException cancelKeyRequest(
      const WebKit::WebString& key_system,
      const WebKit::WebString& session_id) OVERRIDE;

  void OnKeyAdded(const std::string& session_id);
  void OnKeyError(const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  int system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const std::string& destination_url);

  void OnMediaSourceOpened(WebKit::WebMediaSource* web_media_source);

  void OnNeedKey(const std::string& type,
                 const std::string& session_id,
                 const std::vector<uint8>& init_data);

#if defined(GOOGLE_TV)
  bool InjectMediaStream(MediaStreamClient* media_stream_client,
                         media::Demuxer* demuxer,
                         const base::Closure& destroy_demuxer_cb);
#endif

 protected:
  // Helper method to update the playing state.
  void UpdatePlayingState(bool is_playing_);

  // Helper methods for posting task for setting states and update WebKit.
  void UpdateNetworkState(WebKit::WebMediaPlayer::NetworkState state);
  void UpdateReadyState(WebKit::WebMediaPlayer::ReadyState state);

  // Helper method to reestablish the surface texture peer for android
  // media player.
  void EstablishSurfaceTexturePeer();

  // Requesting whether the surface texture peer needs to be reestablished.
  void SetNeedsEstablishPeer(bool needs_establish_peer);

#if defined(GOOGLE_TV)
  // Request external surface for out-of-band composition.
  void RequestExternalSurface();
#endif

 private:
  void ReallocateVideoFrame();
  void DidLoadMediaInfo(MediaInfoLoader::Status status);

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(
      const WebKit::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);
  MediaKeyException AddKeyInternal(const WebKit::WebString& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const WebKit::WebString& session_id);
  MediaKeyException CancelKeyRequestInternal(
      const WebKit::WebString& key_system,
      const WebKit::WebString& session_id);

  WebKit::WebFrame* const frame_;

  WebKit::WebMediaPlayerClient* const client_;

  // |delegate_| is used to notify the browser process of the player status, so
  // that the browser process can control screen locks.
  // TODO(qinmin): Currently android mediaplayer takes care of the screen
  // lock. So this is only used for media source. Will apply this to regular
  // media tag once http://crbug.com/247892 is fixed.
  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  // Save the list of buffered time ranges.
  WebKit::WebTimeRanges buffered_;

  // Size of the video.
  WebKit::WebSize natural_size_;

  // Size that has been sent to StreamTexture.
  WebKit::WebSize cached_stream_texture_size_;

  // The video frame object used for rendering by the compositor.
  scoped_refptr<media::VideoFrame> current_frame_;

  // Message loop for main renderer thread.
  const scoped_refptr<base::MessageLoopProxy> main_loop_;

  // Message loop for media thread.
  const scoped_refptr<base::MessageLoopProxy> media_loop_;

  // URL of the media file to be fetched.
  GURL url_;

  // Media duration.
  base::TimeDelta duration_;

  // Flag to remember if we have a trusted duration_ value provided by
  // MediaSourceDelegate notifying OnDurationChanged(). In this case, ignore
  // any subsequent duration value passed to OnMediaMetadataChange().
  bool ignore_metadata_duration_change_;

  // The time android media player is trying to seek.
  double pending_seek_;

  // Internal seek state.
  bool seeking_;

  // Whether loading has progressed since the last call to didLoadingProgress.
  mutable bool did_loading_progress_;

  // Manager for managing this object.
  RendererMediaPlayerManager* manager_;

  // Player ID assigned by the |manager_|.
  int player_id_;

  // Current player states.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  // GL texture ID allocated to the video.
  unsigned int texture_id_;

  // GL texture mailbox for texture_id_ to provide in the VideoFrame, and sync
  // point for when the mailbox was produced.
  gpu::Mailbox texture_mailbox_;
  unsigned int texture_mailbox_sync_point_;

  // Stream texture ID allocated to the video.
  unsigned int stream_id_;

  // Whether the mediaplayer is playing.
  bool is_playing_;

  // Wether the mediaplayer has already started playing.
  bool playing_started_;

  // Whether media player needs to re-establish the surface texture peer.
  bool needs_establish_peer_;

  // Whether |stream_texture_proxy_| is initialized.
  bool stream_texture_proxy_initialized_;

  // Whether the video size info is available.
  bool has_size_info_;

  // Whether the video metadata and info are available.
  bool has_media_metadata_;
  bool has_media_info_;

  // Object for allocating stream textures.
  scoped_ptr<StreamTextureFactory> stream_texture_factory_;

  // Object for calling back the compositor thread to repaint the video when a
  // frame available. It should be initialized on the compositor thread.
  ScopedStreamTextureProxy stream_texture_proxy_;

  // Whether media player needs external surface.
  bool needs_external_surface_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  scoped_ptr<webkit::WebLayerImpl> video_weblayer_;

#if defined(GOOGLE_TV)
  // Pixel threshold for external surface usage. Negative value means that the
  // threshold is not defined, so that external surface is never used.
  int external_surface_threshold_;

  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;

  // Media Stream related fields.
  media::Demuxer* demuxer_;
  base::Closure destroy_demuxer_cb_;
  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;
  MediaStreamClient* media_stream_client_;
#endif

  scoped_ptr<MediaSourceDelegate,
             MediaSourceDelegate::Destroyer> media_source_delegate_;

  // Internal pending playback state.
  // Store a playback request that cannot be started immediately.
  bool pending_playback_;

  MediaPlayerHostMsg_Initialize_Type player_type_;

  // Proxy object that delegates method calls on Render Thread.
  // This object is created on the Render Thread and is only called in the
  // destructor.
  WebMediaPlayerProxyAndroid* proxy_;

  // The current playing time. Because the media player is in the browser
  // process, it will regularly update the |current_time_| by calling
  // OnTimeUpdate().
  double current_time_;

  media::MediaLog* media_log_;

  scoped_ptr<MediaInfoLoader> info_loader_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  WebKit::WebString current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // The decryptor that manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> decryptor_;

  base::WeakPtrFactory<WebMediaPlayerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
