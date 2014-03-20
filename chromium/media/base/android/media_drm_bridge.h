// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_

#include <jni.h>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

class GURL;

namespace media {

class MediaPlayerManager;

// This class provides DRM services for android EME implementation.
// TODO(qinmin): implement all the functions in this class.
class MEDIA_EXPORT MediaDrmBridge : public MediaKeys {
 public:
  enum SecurityLevel {
    SECURITY_LEVEL_NONE = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_3 = 3,
  };

  typedef base::Callback<void(bool)> ResetCredentialsCB;

  virtual ~MediaDrmBridge();

  // Returns a MediaDrmBridge instance if |scheme_uuid| is supported, or a NULL
  // pointer otherwise.
  static scoped_ptr<MediaDrmBridge> Create(
      int media_keys_id,
      const std::vector<uint8>& scheme_uuid,
      const GURL& frame_url,
      const std::string& security_level,
      MediaPlayerManager* manager);

  // Checks whether MediaDRM is available.
  static bool IsAvailable();

  static bool IsSecurityLevelSupported(const std::vector<uint8>& scheme_uuid,
                                       const std::string& security_level);

  static bool IsCryptoSchemeSupported(const std::vector<uint8>& scheme_uuid,
                                      const std::string& container_mime_type);

  static bool IsSecureDecoderRequired(const std::string& security_level_str);

  static bool RegisterMediaDrmBridge(JNIEnv* env);

  // MediaKeys implementations.
  virtual bool CreateSession(uint32 session_id,
                             const std::string& type,
                             const uint8* init_data,
                             int init_data_length) OVERRIDE;
  virtual void UpdateSession(uint32 session_id,
                             const uint8* response,
                             int response_length) OVERRIDE;
  virtual void ReleaseSession(uint32 session_id) OVERRIDE;

  // Returns a MediaCrypto object if it's already created. Returns a null object
  // otherwise.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Sets callback which will be called when MediaCrypto is ready.
  // If |closure| is null, previously set callback will be cleared.
  void SetMediaCryptoReadyCB(const base::Closure& closure);

  // Called after a MediaCrypto object is created.
  void OnMediaCryptoReady(JNIEnv* env, jobject j_media_drm);

  // Callbacks for firing session events.
  void OnSessionCreated(JNIEnv* env,
                        jobject j_media_drm,
                        jint j_session_id,
                        jstring j_web_session_id);
  void OnSessionMessage(JNIEnv* env,
                        jobject j_media_drm,
                        jint j_session_id,
                        jbyteArray j_message,
                        jstring j_destination_url);
  void OnSessionReady(JNIEnv* env, jobject j_media_drm, jint j_session_id);
  void OnSessionClosed(JNIEnv* env, jobject j_media_drm, jint j_session_id);
  void OnSessionError(JNIEnv* env, jobject j_media_drm, jint j_session_id);

  // Reset the device credentials.
  void ResetDeviceCredentials(const ResetCredentialsCB& callback);

  // Called by the java object when credential reset is completed.
  void OnResetDeviceCredentialsCompleted(JNIEnv* env, jobject, bool success);

  // Helper function to determine whether a protected surface is needed for the
  // video playback.
  bool IsProtectedSurfaceRequired();

  int media_keys_id() const { return media_keys_id_; }

  GURL frame_url() const { return frame_url_; }

 private:
  static bool IsSecureDecoderRequired(SecurityLevel security_level);

  MediaDrmBridge(int media_keys_id,
                 const std::vector<uint8>& scheme_uuid,
                 const GURL& frame_url,
                 const std::string& security_level,
                 MediaPlayerManager* manager);

  // Get the security level of the media.
  SecurityLevel GetSecurityLevel();

  // ID of the MediaKeys object.
  int media_keys_id_;

  // UUID of the key system.
  std::vector<uint8> scheme_uuid_;

  // media stream's frame URL.
  const GURL frame_url_;

  // Java MediaDrm instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_;

  // Non-owned pointer.
  MediaPlayerManager* manager_;

  base::Closure media_crypto_ready_cb_;

  ResetCredentialsCB reset_credentials_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
