// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"

class GURL;

namespace blink {
#if defined(ENABLE_PEPPER_CDMS)
class WebFrame;
class WebMediaPlayerClient;
#endif  // defined(ENABLE_PEPPER_CDMS)
}

namespace content {

#if defined(OS_ANDROID)
class RendererMediaPlayerManager;
#endif  // defined(OS_ANDROID)

// ProxyDecryptor is for EME v0.1b only. It should not be used for the WD API.
// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
//
// Now that the Pepper API calls use session ID to match responses with
// requests, this class maintains a mapping between session ID and web session
// ID. Callers of this class expect web session IDs in the responses.
// Session IDs are internal unique references to the session. Web session IDs
// are the CDM generated ID for the session, and are what are visible to users.
//
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
// TODO(xhwang): The ProxyDecryptor is not a Decryptor. Find a better name!
class ProxyDecryptor {
 public:
  // These are similar to the callbacks in media_keys.h, but pass back the
  // web session ID rather than the internal session ID.
  typedef base::Callback<void(const std::string& session_id)> KeyAddedCB;
  typedef base::Callback<void(const std::string& session_id,
                              media::MediaKeys::KeyError error_code,
                              int system_code)> KeyErrorCB;
  typedef base::Callback<void(const std::string& session_id,
                              const std::vector<uint8>& message,
                              const std::string& default_url)> KeyMessageCB;

  ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
      blink::WebMediaPlayerClient* web_media_player_client,
      blink::WebFrame* web_frame,
#elif defined(OS_ANDROID)
      RendererMediaPlayerManager* manager,
      int media_keys_id,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const KeyAddedCB& key_added_cb,
      const KeyErrorCB& key_error_cb,
      const KeyMessageCB& key_message_cb);
  virtual ~ProxyDecryptor();

  // Only call this once.
  bool InitializeCDM(const std::string& key_system, const GURL& frame_url);

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // May only be called after InitializeCDM() succeeds.
  bool GenerateKeyRequest(const std::string& type,
                          const uint8* init_data,
                          int init_data_length);
  void AddKey(const uint8* key, int key_length,
              const uint8* init_data, int init_data_length,
              const std::string& session_id);
  void CancelKeyRequest(const std::string& session_id);

 private:
  // Session_id <-> web_session_id map.
  typedef std::map<uint32, std::string> SessionIdMap;

  // Helper function to create MediaKeys to handle the given |key_system|.
  scoped_ptr<media::MediaKeys> CreateMediaKeys(const std::string& key_system,
                                               const GURL& frame_url);

  // Callbacks for firing session events.
  void OnSessionCreated(uint32 session_id, const std::string& web_session_id);
  void OnSessionMessage(uint32 session_id,
                        const std::vector<uint8>& message,
                        const std::string& default_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      int system_code);

  // Helper function to determine session_id for the provided |web_session_id|.
  uint32 LookupSessionId(const std::string& web_session_id);

  // Helper function to determine web_session_id for the provided |session_id|.
  // The returned web_session_id is only valid on the main thread, and should be
  // stored by copy.
  const std::string& LookupWebSessionId(uint32 session_id);

  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

#if defined(ENABLE_PEPPER_CDMS)
  // Callback for cleaning up a Pepper-based CDM.
  void DestroyHelperPlugin();

  // Needed to create the PpapiDecryptor.
  blink::WebMediaPlayerClient* web_media_player_client_;
  blink::WebFrame* web_frame_;
#elif defined(OS_ANDROID)
  RendererMediaPlayerManager* manager_;
  int media_keys_id_;
#endif  // defined(ENABLE_PEPPER_CDMS)

  // The real MediaKeys that manages key operations for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<media::MediaKeys> media_keys_;

  // Callbacks for firing key events.
  KeyAddedCB key_added_cb_;
  KeyErrorCB key_error_cb_;
  KeyMessageCB key_message_cb_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  // Session IDs are used to uniquely track sessions so that CDM callbacks
  // can get mapped to the correct session ID. Session ID should be unique
  // per renderer process for debugging purposes.
  static uint32 next_session_id_;

  SessionIdMap sessions_;

  bool is_clear_key_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
