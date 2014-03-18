// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_player_manager.h"

#include "base/command_line.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/media/android/browser_demuxer_android.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_source_player.h"
#include "media/base/media_switches.h"

using media::MediaDrmBridge;
using media::MediaPlayerAndroid;
using media::MediaPlayerBridge;
using media::MediaPlayerManager;
using media::MediaSourcePlayer;

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
static const int kMediaPlayerThreshold = 1;

// Maximum sizes for various EME message parameters. These are checks to
// prevent unnecessarily large messages from being passed around, and the sizes
// are somewhat arbitrary as the EME specification doesn't specify any limits.
static const size_t kEmeUuidSize = 16;
static const size_t kEmeTypeMaximum = 50;  // Type is a MIME type.
static const size_t kEmeInitDataMaximum = 10240;  // 10 KB
static const size_t kEmeResponseMaximum = 10240;  // 10 KB

namespace content {

static BrowserMediaPlayerManager::Factory g_factory = NULL;

// static
void BrowserMediaPlayerManager::RegisterFactory(Factory factory) {
  g_factory = factory;
}

// static
BrowserMediaPlayerManager* BrowserMediaPlayerManager::Create(
    RenderViewHost* rvh) {
  if (g_factory)
    return g_factory(rvh);
  return new BrowserMediaPlayerManager(rvh);
}

#if !defined(GOOGLE_TV)
// static
MediaPlayerAndroid* BrowserMediaPlayerManager::CreateMediaPlayer(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id,
    bool hide_url_log,
    MediaPlayerManager* manager,
    BrowserDemuxerAndroid* demuxer) {
  switch (type) {
    case MEDIA_PLAYER_TYPE_URL: {
      MediaPlayerBridge* media_player_bridge = new MediaPlayerBridge(
          player_id, url, first_party_for_cookies, hide_url_log, manager);
      BrowserMediaPlayerManager* browser_media_player_manager =
          static_cast<BrowserMediaPlayerManager*>(manager);
      ContentViewCoreImpl* content_view_core_impl =
          static_cast<ContentViewCoreImpl*>(ContentViewCore::FromWebContents(
              browser_media_player_manager->web_contents_));
      if (!content_view_core_impl) {
        // May reach here due to prerendering. Don't extract the metadata
        // since it is expensive.
        // TODO(qinmin): extract the metadata once the user decided to load
        // the page.
        browser_media_player_manager->OnMediaMetadataChanged(
            player_id, base::TimeDelta(), 0, 0, false);
      } else if (!content_view_core_impl->ShouldBlockMediaRequest(url)) {
        media_player_bridge->Initialize();
      }
      return media_player_bridge;
    }

    case MEDIA_PLAYER_TYPE_MEDIA_SOURCE: {
      return new MediaSourcePlayer(
          player_id, manager, demuxer->CreateDemuxer(demuxer_client_id));
    }
  }

  NOTREACHED();
  return NULL;
}
#endif

BrowserMediaPlayerManager::BrowserMediaPlayerManager(
    RenderViewHost* render_view_host)
    : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)),
      fullscreen_player_id_(-1),
      pending_fullscreen_player_id_(-1),
      fullscreen_player_is_released_(false),
      web_contents_(WebContents::FromRenderViewHost(render_view_host)),
      weak_ptr_factory_(this) {
}

BrowserMediaPlayerManager::~BrowserMediaPlayerManager() {}

bool BrowserMediaPlayerManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserMediaPlayerManager, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_EnterFullscreen, OnEnterFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ExitFullscreen, OnExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_Start, OnStart)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_Seek, OnSeek)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_Pause, OnPause)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_Release, OnReleaseResources)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyMediaPlayer, OnDestroyPlayer)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyAllMediaPlayers,
                        DestroyAllMediaPlayers)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_InitializeCDM,
                        OnInitializeCDM)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_CreateSession, OnCreateSession)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_UpdateSession, OnUpdateSession)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_ReleaseSession, OnReleaseSession)
#if defined(VIDEO_HOLE)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_NotifyExternalSurface,
                        OnNotifyExternalSurface)
#endif  // defined(VIDEO_HOLE)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserMediaPlayerManager::FullscreenPlayerPlay() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    if (fullscreen_player_is_released_) {
      video_view_->OpenVideo();
      fullscreen_player_is_released_ = false;
    }
    player->Start();
    Send(new MediaPlayerMsg_DidMediaPlayerPlay(
        routing_id(), fullscreen_player_id_));
  }
}

void BrowserMediaPlayerManager::FullscreenPlayerPause() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    player->Pause(true);
    Send(new MediaPlayerMsg_DidMediaPlayerPause(
        routing_id(), fullscreen_player_id_));
  }
}

void BrowserMediaPlayerManager::FullscreenPlayerSeek(int msec) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    // TODO(kbalazs): if |fullscreen_player_is_released_| is true
    // at this point, player->GetCurrentTime() will be wrong until
    // FullscreenPlayerPlay (http://crbug.com/322798).
    OnSeekRequest(fullscreen_player_id_,
                  base::TimeDelta::FromMilliseconds(msec));
  }
}

void BrowserMediaPlayerManager::ExitFullscreen(bool release_media_player) {
  Send(new MediaPlayerMsg_DidExitFullscreen(
      routing_id(), fullscreen_player_id_));
  video_view_.reset();
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  fullscreen_player_id_ = -1;
  if (!player)
    return;
  if (release_media_player)
    player->Release();
  else
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void BrowserMediaPlayerManager::OnTimeUpdate(int player_id,
                                             base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaTimeUpdate(
      routing_id(), player_id, current_time));
}

void BrowserMediaPlayerManager::SetVideoSurface(
    gfx::ScopedJavaSurface surface) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (!player)
    return;
  if (!surface.IsEmpty()) {
    Send(new MediaPlayerMsg_DidEnterFullscreen(routing_id(),
                                               player->player_id()));
  }
  player->SetVideoSurface(surface.Pass());
}

void BrowserMediaPlayerManager::OnMediaMetadataChanged(
    int player_id, base::TimeDelta duration, int width, int height,
    bool success) {
  Send(new MediaPlayerMsg_MediaMetadataChanged(
      routing_id(), player_id, duration, width, height, success));
  if (fullscreen_player_id_ == player_id)
    video_view_->UpdateMediaMetadata();
}

void BrowserMediaPlayerManager::OnPlaybackComplete(int player_id) {
  Send(new MediaPlayerMsg_MediaPlaybackCompleted(routing_id(), player_id));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnPlaybackComplete();
}

void BrowserMediaPlayerManager::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  Send(new MediaPlayerMsg_DidMediaPlayerPause(routing_id(), player_id));
  OnReleaseResources(player_id);
}

void BrowserMediaPlayerManager::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new MediaPlayerMsg_MediaBufferingUpdate(
      routing_id(), player_id, percentage));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnBufferingUpdate(percentage);
}

void BrowserMediaPlayerManager::OnSeekRequest(
    int player_id,
    const base::TimeDelta& time_to_seek) {
  Send(new MediaPlayerMsg_SeekRequest(routing_id(), player_id, time_to_seek));
}

void BrowserMediaPlayerManager::OnSeekComplete(
    int player_id,
    const base::TimeDelta& current_time) {
  Send(new MediaPlayerMsg_SeekCompleted(routing_id(), player_id, current_time));
}

void BrowserMediaPlayerManager::OnError(int player_id, int error) {
  Send(new MediaPlayerMsg_MediaError(routing_id(), player_id, error));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnMediaPlayerError(error);
}

void BrowserMediaPlayerManager::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new MediaPlayerMsg_MediaVideoSizeChanged(routing_id(), player_id,
      width, height));
  if (fullscreen_player_id_ == player_id)
    video_view_->OnVideoSizeChanged(width, height);
}

void BrowserMediaPlayerManager::RequestMediaResources(int player_id) {
  int num_active_player = 0;
  ScopedVector<MediaPlayerAndroid>::iterator it;
  for (it = players_.begin(); it != players_.end(); ++it) {
    if (!(*it)->IsPlayerReady())
      continue;

    // The player is already active, ignore it.
    if ((*it)->player_id() == player_id)
      return;
    else
      num_active_player++;
  }

  // Number of active players are less than the threshold, do nothing.
  if (num_active_player < kMediaPlayerThreshold)
    return;

  for (it = players_.begin(); it != players_.end(); ++it) {
    if ((*it)->IsPlayerReady() && !(*it)->IsPlaying() &&
        fullscreen_player_id_ != (*it)->player_id()) {
      (*it)->Release();
      Send(new MediaPlayerMsg_MediaPlayerReleased(
          routing_id(), (*it)->player_id()));
    }
  }
}

void BrowserMediaPlayerManager::ReleaseMediaResources(int player_id) {
  // Nothing needs to be done.
}

media::MediaResourceGetter*
BrowserMediaPlayerManager::GetMediaResourceGetter() {
  if (!media_resource_getter_.get()) {
    RenderProcessHost* host = web_contents()->GetRenderProcessHost();
    BrowserContext* context = host->GetBrowserContext();
    StoragePartition* partition = host->GetStoragePartition();
    fileapi::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : NULL;
    media_resource_getter_.reset(new MediaResourceGetterImpl(
        context, file_system_context, host->GetID(), routing_id()));
  }
  return media_resource_getter_.get();
}

MediaPlayerAndroid* BrowserMediaPlayerManager::GetFullscreenPlayer() {
  return GetPlayer(fullscreen_player_id_);
}

MediaPlayerAndroid* BrowserMediaPlayerManager::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

MediaDrmBridge* BrowserMediaPlayerManager::GetDrmBridge(int media_keys_id) {
  for (ScopedVector<MediaDrmBridge>::iterator it = drm_bridges_.begin();
      it != drm_bridges_.end(); ++it) {
    if ((*it)->media_keys_id() == media_keys_id)
      return *it;
  }
  return NULL;
}

void BrowserMediaPlayerManager::DestroyAllMediaPlayers() {
  players_.clear();
  drm_bridges_.clear();
  if (fullscreen_player_id_ != -1) {
    video_view_.reset();
    fullscreen_player_id_ = -1;
  }
}

void BrowserMediaPlayerManager::OnProtectedSurfaceRequested(int player_id) {
  if (fullscreen_player_id_ == player_id)
    return;

  if (fullscreen_player_id_ != -1) {
    // TODO(qinmin): Determine the correct error code we should report to WMPA.
    OnError(player_id, MediaPlayerAndroid::MEDIA_ERROR_DECODE);
    return;
  }

  // If the player is pending approval, wait for the approval to happen.
  if (media_keys_ids_pending_approval_.end() !=
      media_keys_ids_pending_approval_.find(player_id)) {
    pending_fullscreen_player_id_ = player_id;
    return;
  }

  // Send an IPC to the render process to request the video element to enter
  // fullscreen. OnEnterFullscreen() will be called later on success.
  // This guarantees the fullscreen video will be rendered correctly.
  // During the process, DisableFullscreenEncryptedMediaPlayback() may get
  // called before or after OnEnterFullscreen(). If it is called before
  // OnEnterFullscreen(), the player will not enter fullscreen. And it will
  // retry the process once CreateSession() is allowed to proceed.
  // TODO(qinmin): make this flag default on android.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGestureRequirementForMediaFullscreen)) {
    Send(new MediaPlayerMsg_RequestFullscreen(routing_id(), player_id));
  }
}

// The following 5 functions are EME MediaKeySession events.

void BrowserMediaPlayerManager::OnSessionCreated(
    int media_keys_id,
    uint32 session_id,
    const std::string& web_session_id) {
  Send(new MediaKeysMsg_SessionCreated(
      routing_id(), media_keys_id, session_id, web_session_id));
}

void BrowserMediaPlayerManager::OnSessionMessage(
    int media_keys_id,
    uint32 session_id,
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  Send(new MediaKeysMsg_SessionMessage(
      routing_id(), media_keys_id, session_id, message, destination_url));
}

void BrowserMediaPlayerManager::OnSessionReady(int media_keys_id,
                                               uint32 session_id) {
  Send(new MediaKeysMsg_SessionReady(routing_id(), media_keys_id, session_id));
}

void BrowserMediaPlayerManager::OnSessionClosed(int media_keys_id,
                                                uint32 session_id) {
  Send(new MediaKeysMsg_SessionClosed(routing_id(), media_keys_id, session_id));
}

void BrowserMediaPlayerManager::OnSessionError(
    int media_keys_id,
    uint32 session_id,
    media::MediaKeys::KeyError error_code,
    int system_code) {
  Send(new MediaKeysMsg_SessionError(
      routing_id(), media_keys_id, session_id, error_code, system_code));
}

#if defined(VIDEO_HOLE)
void BrowserMediaPlayerManager::AttachExternalVideoSurface(int player_id,
                                                           jobject surface) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player) {
    player->SetVideoSurface(
        gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
  }
}

void BrowserMediaPlayerManager::DetachExternalVideoSurface(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void BrowserMediaPlayerManager::OnNotifyExternalSurface(
    int player_id, bool is_request, const gfx::RectF& rect) {
  if (!web_contents_)
    return;

  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->NotifyExternalSurface(player_id, is_request, rect);
}
#endif  // defined(VIDEO_HOLE)

void BrowserMediaPlayerManager::DisableFullscreenEncryptedMediaPlayback() {
  if (fullscreen_player_id_ == -1)
    return;

  // If the fullscreen player is not playing back encrypted video, do nothing.
  MediaDrmBridge* drm_bridge = GetDrmBridge(fullscreen_player_id_);
  if (!drm_bridge)
    return;

  // Exit fullscreen.
  pending_fullscreen_player_id_ = fullscreen_player_id_;
  OnExitFullscreen(fullscreen_player_id_);
}

void BrowserMediaPlayerManager::OnEnterFullscreen(int player_id) {
  DCHECK_EQ(fullscreen_player_id_, -1);
  if (media_keys_ids_pending_approval_.find(player_id) !=
      media_keys_ids_pending_approval_.end()) {
    return;
  }

  if (video_view_.get()) {
    fullscreen_player_id_ = player_id;
    video_view_->OpenVideo();
  } else if (!ContentVideoView::HasContentVideoView()) {
    // In Android WebView, two ContentViewCores could both try to enter
    // fullscreen video, we just ignore the second one.
    fullscreen_player_id_ = player_id;
    ContentViewCoreImpl* content_view_core_impl =
        ContentViewCoreImpl::FromWebContents(web_contents());
    video_view_.reset(new ContentVideoView(content_view_core_impl->GetContext(),
        content_view_core_impl->GetContentVideoViewClient(), this));
  }
}

void BrowserMediaPlayerManager::OnExitFullscreen(int player_id) {
  if (fullscreen_player_id_ == player_id) {
    MediaPlayerAndroid* player = GetPlayer(player_id);
    if (player)
      player->SetVideoSurface(gfx::ScopedJavaSurface());
    video_view_->OnExitFullscreen();
  }
}

void BrowserMediaPlayerManager::OnInitialize(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id) {
  DCHECK(type != MEDIA_PLAYER_TYPE_MEDIA_SOURCE || demuxer_client_id > 0)
      << "Media source players must have positive demuxer client IDs: "
      << demuxer_client_id;

  RemovePlayer(player_id);

  RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
      web_contents()->GetRenderProcessHost());
  AddPlayer(CreateMediaPlayer(
      type, player_id, url, first_party_for_cookies, demuxer_client_id,
      host->GetBrowserContext()->IsOffTheRecord(), this,
      host->browser_demuxer_android()));
}

void BrowserMediaPlayerManager::OnStart(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Start();
}

void BrowserMediaPlayerManager::OnSeek(
    int player_id,
    const base::TimeDelta& time) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SeekTo(time);
}

void BrowserMediaPlayerManager::OnPause(
    int player_id,
    bool is_media_related_action) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Pause(is_media_related_action);
}

void BrowserMediaPlayerManager::OnSetVolume(int player_id, double volume) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SetVolume(volume);
}

void BrowserMediaPlayerManager::OnReleaseResources(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Release();
  if (player_id == fullscreen_player_id_)
    fullscreen_player_is_released_ = true;

#if defined(VIDEO_HOLE)
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->NotifyExternalSurface(player_id, false, gfx::RectF());
#endif  // defined(VIDEO_HOLE)
}

void BrowserMediaPlayerManager::OnDestroyPlayer(int player_id) {
  RemovePlayer(player_id);
  if (fullscreen_player_id_ == player_id)
    fullscreen_player_id_ = -1;
}

void BrowserMediaPlayerManager::OnInitializeCDM(
    int media_keys_id,
    const std::vector<uint8>& uuid,
    const GURL& frame_url) {
  if (uuid.size() != kEmeUuidSize) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetDrmBridge() will return null.
    NOTREACHED() << "Invalid UUID for ID: " << media_keys_id;
    return;
  }

  AddDrmBridge(media_keys_id, uuid, frame_url);
  // In EME v0.1b MediaKeys lives in the media element. So the |media_keys_id|
  // is the same as the |player_id|.
  OnSetMediaKeys(media_keys_id, media_keys_id);
}

void BrowserMediaPlayerManager::OnCreateSession(
    int media_keys_id,
    uint32 session_id,
    const std::string& type,
    const std::vector<uint8>& init_data) {
  if (type.length() > kEmeTypeMaximum) {
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }
  if (init_data.size() > kEmeInitDataMaximum) {
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  if (CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDisableInfobarForProtectedMediaIdentifier)) {
    GenerateKeyIfAllowed(media_keys_id, session_id, type, init_data, true);
    return;
  }

  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!drm_bridge) {
    DLOG(WARNING) << "No MediaDrmBridge for ID: " << media_keys_id << " found";
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  if (media_keys_ids_approved_.find(media_keys_id) ==
      media_keys_ids_approved_.end()) {
    media_keys_ids_pending_approval_.insert(media_keys_id);
  }
  web_contents()->GetDelegate()->RequestProtectedMediaIdentifierPermission(
      web_contents(),
      drm_bridge->frame_url(),
      base::Bind(&BrowserMediaPlayerManager::GenerateKeyIfAllowed,
                 weak_ptr_factory_.GetWeakPtr(),
                 media_keys_id,
                 session_id,
                 type,
                 init_data));
}

void BrowserMediaPlayerManager::OnUpdateSession(
    int media_keys_id,
    uint32 session_id,
    const std::vector<uint8>& response) {
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!drm_bridge) {
    DLOG(WARNING) << "No MediaDrmBridge for ID: " << media_keys_id << " found";
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  if (response.size() > kEmeResponseMaximum) {
    DLOG(WARNING) << "Response for ID: " << media_keys_id
                  << " too long: " << response.size();
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  drm_bridge->UpdateSession(session_id, &response[0], response.size());
  // In EME v0.1b MediaKeys lives in the media element. So the |media_keys_id|
  // is the same as the |player_id|.
  // TODO(xhwang): Separate |media_keys_id| and |player_id|.
  MediaPlayerAndroid* player = GetPlayer(media_keys_id);
  if (player)
    player->OnKeyAdded();
}

void BrowserMediaPlayerManager::OnReleaseSession(int media_keys_id,
                                                 uint32 session_id) {
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!drm_bridge) {
    DLOG(WARNING) << "No MediaDrmBridge for ID: " << media_keys_id << " found";
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  drm_bridge->ReleaseSession(session_id);
}

void BrowserMediaPlayerManager::AddPlayer(MediaPlayerAndroid* player) {
  DCHECK(!GetPlayer(player->player_id()));
  players_.push_back(player);
  if (player->IsRemote()) {
    Send(new MediaPlayerMsg_ConnectedToRemoteDevice(routing_id(),
                                                    player->player_id()));
  }
}

void BrowserMediaPlayerManager::RemovePlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    MediaPlayerAndroid* player = *it;
    if (player->player_id() == player_id) {
      if (player->IsRemote()) {
        Send(new MediaPlayerMsg_DisconnectedFromRemoteDevice(
            routing_id(), player->player_id()));
      }
      players_.erase(it);
      break;
    }
  }
}

scoped_ptr<media::MediaPlayerAndroid> BrowserMediaPlayerManager::SwapPlayer(
      int player_id, media::MediaPlayerAndroid* player) {
  media::MediaPlayerAndroid* previous_player = NULL;
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      previous_player = *it;
      players_.weak_erase(it);
      players_.push_back(player);
      if (!previous_player->IsRemote() && player->IsRemote()) {
        Send(new MediaPlayerMsg_ConnectedToRemoteDevice(
            routing_id(), player->player_id()));
      } else if (previous_player->IsRemote() && !player->IsRemote()) {
        Send(new MediaPlayerMsg_DisconnectedFromRemoteDevice(
            routing_id(), player->player_id()));
      }
      break;
    }
  }
  return scoped_ptr<media::MediaPlayerAndroid>(previous_player);
}

void BrowserMediaPlayerManager::AddDrmBridge(int media_keys_id,
                                             const std::vector<uint8>& uuid,
                                             const GURL& frame_url) {
  DCHECK(!GetDrmBridge(media_keys_id));
  // TODO(xhwang/ddorwin): Pass the security level from key system.
  std::string security_level = "L3";
  if (CommandLine::ForCurrentProcess()
          ->HasSwitch(switches::kMediaDrmEnableNonCompositing)) {
    security_level = "L1";
  }

  scoped_ptr<MediaDrmBridge> drm_bridge(MediaDrmBridge::Create(
      media_keys_id, uuid, frame_url, security_level, this));
  if (!drm_bridge) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetDrmBridge() will return null.
    DVLOG(1) << "failed to create drm bridge.";
    return;
  }

  drm_bridges_.push_back(drm_bridge.release());
}

void BrowserMediaPlayerManager::RemoveDrmBridge(int media_keys_id) {
  for (ScopedVector<MediaDrmBridge>::iterator it = drm_bridges_.begin();
      it != drm_bridges_.end(); ++it) {
    if ((*it)->media_keys_id() == media_keys_id) {
      drm_bridges_.erase(it);
      break;
    }
  }
}

void BrowserMediaPlayerManager::OnSetMediaKeys(int player_id,
                                               int media_keys_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!player || !drm_bridge) {
    DVLOG(1) << "OnSetMediaKeys(): Player and MediaKeys must be present.";
    return;
  }
  // TODO(qinmin): add the logic to decide whether we should create the
  // fullscreen surface for EME lv1.
  player->SetDrmBridge(drm_bridge);
}

void BrowserMediaPlayerManager::GenerateKeyIfAllowed(
    int media_keys_id,
    uint32 session_id,
    const std::string& type,
    const std::vector<uint8>& init_data,
    bool allowed) {
  if (!allowed)
    return;

  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!drm_bridge) {
    DLOG(WARNING) << "No MediaDrmBridge for ID: " << media_keys_id << " found";
    OnSessionError(
        media_keys_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }
  media_keys_ids_pending_approval_.erase(media_keys_id);
  media_keys_ids_approved_.insert(media_keys_id);
  drm_bridge->CreateSession(session_id, type, &init_data[0], init_data.size());

  // TODO(qinmin): currently |media_keys_id| and player ID are identical.
  // This might not be true in the future.
  if (pending_fullscreen_player_id_ != media_keys_id)
    return;

  pending_fullscreen_player_id_ = -1;
  MediaPlayerAndroid* player = GetPlayer(media_keys_id);
  if (player->IsPlaying())
    OnProtectedSurfaceRequested(media_keys_id);
}

}  // namespace content
