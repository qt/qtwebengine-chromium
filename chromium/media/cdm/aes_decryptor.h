// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CRYPTO_AES_DECRYPTOR_H_
#define MEDIA_CRYPTO_AES_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"

namespace crypto {
class SymmetricKey;
}

namespace media {

// Decrypts an AES encrypted buffer into an unencrypted buffer. The AES
// encryption must be CTR with a key size of 128bits.
class MEDIA_EXPORT AesDecryptor : public MediaKeys, public Decryptor {
 public:
  AesDecryptor(const KeyAddedCB& key_added_cb,
               const KeyErrorCB& key_error_cb,
               const KeyMessageCB& key_message_cb);
  virtual ~AesDecryptor();

  // MediaKeys implementation.
  virtual bool GenerateKeyRequest(const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) OVERRIDE;
  virtual void AddKey(const uint8* key, int key_length,
                      const uint8* init_data, int init_data_length,
                      const std::string& session_id) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;
  virtual Decryptor* GetDecryptor() OVERRIDE;

  // Decryptor implementation.
  virtual void RegisterNewKeyCB(StreamType stream_type,
                                const NewKeyCB& key_added_cb) OVERRIDE;
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;
  virtual void CancelDecrypt(StreamType stream_type) OVERRIDE;
  virtual void InitializeAudioDecoder(const AudioDecoderConfig& config,
                                      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void InitializeVideoDecoder(const VideoDecoderConfig& config,
                                      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void DecryptAndDecodeAudio(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) OVERRIDE;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) OVERRIDE;
  virtual void ResetDecoder(StreamType stream_type) OVERRIDE;
  virtual void DeinitializeDecoder(StreamType stream_type) OVERRIDE;

 private:
  // TODO(fgalligan): Remove this and change KeyMap to use crypto::SymmetricKey
  // as there are no decryptors that are performing an integrity check.
  // Helper class that manages the decryption key.
  class DecryptionKey {
   public:
    explicit DecryptionKey(const std::string& secret);
    ~DecryptionKey();

    // Creates the encryption key.
    bool Init();

    crypto::SymmetricKey* decryption_key() { return decryption_key_.get(); }

   private:
    // The base secret that is used to create the decryption key.
    const std::string secret_;

    // The key used to decrypt the data.
    scoped_ptr<crypto::SymmetricKey> decryption_key_;

    DISALLOW_COPY_AND_ASSIGN(DecryptionKey);
  };

  // Creates a DecryptionKey using |key_string| and associates it with |key_id|.
  // Returns true if successful.
  bool AddDecryptionKey(const std::string& key_id,
                        const std::string& key_string);

  // Gets a DecryptionKey associated with |key_id|. The AesDecryptor still owns
  // the key. Returns NULL if no key is associated with |key_id|.
  DecryptionKey* GetKey(const std::string& key_id) const;

  // Callbacks for firing key events.
  KeyAddedCB key_added_cb_;
  KeyErrorCB key_error_cb_;
  KeyMessageCB key_message_cb_;

  // KeyMap owns the DecryptionKey* and must delete them when they are
  // not needed any more.
  typedef base::hash_map<std::string, DecryptionKey*> KeyMap;

  // Since only Decrypt() is called off the renderer thread, we only need to
  // protect |key_map_|, the only member variable that is shared between
  // Decrypt() and other methods.
  KeyMap key_map_;  // Protected by the |key_map_lock_|.
  mutable base::Lock key_map_lock_;  // Protects the |key_map_|.

  // Make session ID unique per renderer by making it static.
  // TODO(xhwang): Make session ID more strictly defined if needed:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16739#c0
  static uint32 next_session_id_;

  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_AES_DECRYPTOR_H_
