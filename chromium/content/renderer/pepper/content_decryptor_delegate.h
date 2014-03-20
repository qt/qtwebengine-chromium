// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/sample_format.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ui/gfx/size.h"

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
}

namespace content {

class PPB_Buffer_Impl;

class ContentDecryptorDelegate {
 public:
  // ContentDecryptorDelegate does not take ownership of
  // |plugin_decryption_interface|. Therefore |plugin_decryption_interface|
  // must outlive this object.
  ContentDecryptorDelegate(
      PP_Instance pp_instance,
      const PPP_ContentDecryptor_Private* plugin_decryption_interface);
  ~ContentDecryptorDelegate();

  void Initialize(const std::string& key_system);

  void SetSessionEventCallbacks(
      const media::SessionCreatedCB& session_created_cb,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionReadyCB& session_ready_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionErrorCB& session_error_cb);

  // Provides access to PPP_ContentDecryptor_Private.
  bool CreateSession(uint32 session_id,
                     const std::string& type,
                     const uint8* init_data,
                     int init_data_length);
  bool UpdateSession(uint32 session_id,
                     const uint8* response,
                     int response_length);
  bool ReleaseSession(uint32 session_id);
  bool Decrypt(media::Decryptor::StreamType stream_type,
               const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
               const media::Decryptor::DecryptCB& decrypt_cb);
  bool CancelDecrypt(media::Decryptor::StreamType stream_type);
  bool InitializeAudioDecoder(
      const media::AudioDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  bool InitializeVideoDecoder(
      const media::VideoDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  // TODO(tomfinegan): Add callback args for DeinitializeDecoder() and
  // ResetDecoder()
  bool DeinitializeDecoder(media::Decryptor::StreamType stream_type);
  bool ResetDecoder(media::Decryptor::StreamType stream_type);
  // Note: These methods can be used with unencrypted data.
  bool DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::AudioDecodeCB& audio_decode_cb);
  bool DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::VideoDecodeCB& video_decode_cb);

  // PPB_ContentDecryptor_Private dispatching methods.
  void OnSessionCreated(uint32 session_id, PP_Var web_session_id_var);
  void OnSessionMessage(uint32 session_id,
                        PP_Var message,
                        PP_Var destination_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      int32_t media_error,
                      int32_t system_code);
  void DeliverBlock(PP_Resource decrypted_block,
                    const PP_DecryptedBlockInfo* block_info);
  void DecoderInitializeDone(PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             PP_Bool success);
  void DecoderDeinitializeDone(PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);
  void DeliverFrame(PP_Resource decrypted_frame,
                    const PP_DecryptedFrameInfo* frame_info);
  void DeliverSamples(PP_Resource audio_frames,
                      const PP_DecryptedSampleInfo* sample_info);

 private:
  // Cancels the pending decrypt-and-decode callback for |stream_type|.
  void CancelDecode(media::Decryptor::StreamType stream_type);

  // Fills |resource| with a PPB_Buffer_Impl and copies the data from
  // |encrypted_buffer| into the buffer resource. This method reuses
  // |audio_input_resource_| and |video_input_resource_| to reduce the latency
  // in requesting new PPB_Buffer_Impl resources. The caller must make sure that
  // |audio_input_resource_| or |video_input_resource_| is available before
  // calling this method.
  //
  // An end of stream |encrypted_buffer| is represented as a null |resource|.
  //
  // Returns true upon success and false if any error happened.
  bool MakeMediaBufferResource(
      media::Decryptor::StreamType stream_type,
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      scoped_refptr<PPB_Buffer_Impl>* resource);

  void FreeBuffer(uint32_t buffer_id);

  void SetBufferToFreeInTrackingInfo(PP_DecryptTrackingInfo* tracking_info);

  // Deserializes audio data stored in |audio_frames| into individual audio
  // buffers in |frames|. Returns true upon success.
  bool DeserializeAudioFrames(PP_Resource audio_frames,
                              size_t data_size,
                              media::SampleFormat sample_format,
                              media::Decryptor::AudioBuffers* frames);

  const PP_Instance pp_instance_;
  const PPP_ContentDecryptor_Private* const plugin_decryption_interface_;

  // TODO(ddorwin): Remove after updating the Pepper API to not use key system.
  std::string key_system_;

  // Callbacks for firing session events.
  media::SessionCreatedCB session_created_cb_;
  media::SessionMessageCB session_message_cb_;
  media::SessionReadyCB session_ready_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionErrorCB session_error_cb_;

  gfx::Size natural_size_;

  // Request ID for tracking pending content decryption callbacks.
  // Note that zero indicates an invalid request ID.
  // TODO(xhwang): Add completion callbacks for Reset/Stop and remove the use
  // of request IDs.
  uint32_t next_decryption_request_id_;

  uint32_t pending_audio_decrypt_request_id_;
  media::Decryptor::DecryptCB pending_audio_decrypt_cb_;

  uint32_t pending_video_decrypt_request_id_;
  media::Decryptor::DecryptCB pending_video_decrypt_cb_;

  uint32_t pending_audio_decoder_init_request_id_;
  media::Decryptor::DecoderInitCB pending_audio_decoder_init_cb_;

  uint32_t pending_video_decoder_init_request_id_;
  media::Decryptor::DecoderInitCB pending_video_decoder_init_cb_;

  uint32_t pending_audio_decode_request_id_;
  media::Decryptor::AudioDecodeCB pending_audio_decode_cb_;

  uint32_t pending_video_decode_request_id_;
  media::Decryptor::VideoDecodeCB pending_video_decode_cb_;

  // Cached audio and video input buffers. See MakeMediaBufferResource.
  scoped_refptr<PPB_Buffer_Impl> audio_input_resource_;
  scoped_refptr<PPB_Buffer_Impl> video_input_resource_;

  std::queue<uint32_t> free_buffers_;

  // Keep track of audio parameters.
  int audio_samples_per_second_;
  int audio_channel_count_;

  base::WeakPtr<ContentDecryptorDelegate> weak_this_;
  base::WeakPtrFactory<ContentDecryptorDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentDecryptorDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_CONTENT_DECRYPTOR_DELEGATE_H_
