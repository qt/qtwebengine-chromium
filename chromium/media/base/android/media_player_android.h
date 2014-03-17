// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "url/gurl.h"

namespace media {

class MediaDrmBridge;
class MediaPlayerManager;

// This class serves as the base class for different media player
// implementations on Android. Subclasses need to provide their own
// MediaPlayerAndroid::Create() implementation.
class MEDIA_EXPORT MediaPlayerAndroid {
 public:
  virtual ~MediaPlayerAndroid();

  // Error types for MediaErrorCB.
  enum MediaErrorType {
    MEDIA_ERROR_FORMAT,
    MEDIA_ERROR_DECODE,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
  };

  // Passing an external java surface object to the player.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) = 0;

  // Start playing the media.
  virtual void Start() = 0;

  // Pause the media.
  virtual void Pause(bool is_media_related_action) = 0;

  // Seek to a particular position, based on renderer signaling actual seek
  // with MediaPlayerHostMsg_Seek. If eventual success, OnSeekComplete() will be
  // called.
  virtual void SeekTo(const base::TimeDelta& timestamp) = 0;

  // Release the player resources.
  virtual void Release() = 0;

  // Set the player volume.
  virtual void SetVolume(double volume) = 0;

  // Get the media information from the player.
  virtual bool IsRemote() const;
  virtual int GetVideoWidth() = 0;
  virtual int GetVideoHeight() = 0;
  virtual base::TimeDelta GetDuration() = 0;
  virtual base::TimeDelta GetCurrentTime() = 0;
  virtual bool IsPlaying() = 0;
  virtual bool IsPlayerReady() = 0;
  virtual bool CanPause() = 0;
  virtual bool CanSeekForward() = 0;
  virtual bool CanSeekBackward() = 0;
  virtual GURL GetUrl();
  virtual GURL GetFirstPartyForCookies();

  // Pass a drm bridge to a player.
  virtual void SetDrmBridge(MediaDrmBridge* drm_bridge);

  // Notifies the player that a decryption key has been added. The player
  // may want to start/resume playback if it is waiting for a key.
  virtual void OnKeyAdded();

  int player_id() { return player_id_; }

 protected:
  MediaPlayerAndroid(int player_id,
                     MediaPlayerManager* manager);

  MediaPlayerManager* manager() { return manager_; }

 private:
  // Player ID assigned to this player.
  int player_id_;

  // Resource manager for all the media players.
  MediaPlayerManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerAndroid);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_ANDROID_H_
