// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_H_
#define CDM_CONTENT_DECRYPTION_MODULE_H_

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

// Define CDM_EXPORT so that functionality implemented by the CDM module
// can be exported to consumers.
#if defined(WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __declspec(dllexport)
#else
#define CDM_EXPORT __declspec(dllimport)
#endif  // defined(CDM_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __attribute__((visibility("default")))
#else
#define CDM_EXPORT
#endif

#endif  // defined(WIN32)

// The version number must be rolled when the exported functions are updated!
// If the CDM and the adapter use different versions of these functions, the
// adapter will fail to load or crash!
#define CDM_MODULE_VERSION 4

// Build the versioned entrypoint name.
// The extra macros are necessary to expand version to an actual value.
#define INITIALIZE_CDM_MODULE \
  BUILD_ENTRYPOINT(InitializeCdmModule, CDM_MODULE_VERSION)
#define BUILD_ENTRYPOINT(name, version) \
  BUILD_ENTRYPOINT_NO_EXPANSION(name, version)
#define BUILD_ENTRYPOINT_NO_EXPANSION(name, version) name##_##version

extern "C" {
CDM_EXPORT void INITIALIZE_CDM_MODULE();

CDM_EXPORT void DeinitializeCdmModule();

// Returns a pointer to the requested CDM Host interface upon success.
// Returns NULL if the requested CDM Host interface is not supported.
// The caller should cast the returned pointer to the type matching
// |host_interface_version|.
typedef void* (*GetCdmHostFunc)(int host_interface_version, void* user_data);

// Returns a pointer to the requested CDM upon success.
// Returns NULL if an error occurs or the requested |cdm_interface_version| or
// |key_system| is not supported or another error occurs.
// The caller should cast the returned pointer to the type matching
// |cdm_interface_version|.
// Caller retains ownership of arguments and must call Destroy() on the returned
// object.
CDM_EXPORT void* CreateCdmInstance(
    int cdm_interface_version,
    const char* key_system, uint32_t key_system_size,
    GetCdmHostFunc get_cdm_host_func, void* user_data);

CDM_EXPORT const char* GetCdmVersion();
}

namespace cdm {

class AudioFrames_1;
class AudioFrames_2;
typedef AudioFrames_2 AudioFrames;

class Host_1;
class Host_2;
class Host_3;

class DecryptedBlock;
class VideoFrame;

enum Status {
  kSuccess = 0,
  kNeedMoreData,  // Decoder needs more data to produce a decoded frame/sample.
  kNoKey,  // The required decryption key is not available.
  kSessionError,  // Session management error.
  kDecryptError,  // Decryption failed.
  kDecodeError,  // Error decoding audio or video.
  kDeferredInitialization  // Decoder is not ready for initialization.
};

// This must be consistent with MediaKeyError defined in the spec:
// https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#error-codes
// The error codes are in the process of changing. For now, support the minimum
// required set with backwards compatible values.
enum MediaKeyError {
  kUnknownError = 1,
  kClientError = 2,
  kOutputError = 4
};

// An input buffer can be split into several continuous subsamples.
// A SubsampleEntry specifies the number of clear and cipher bytes in each
// subsample. For example, the following buffer has three subsamples:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
//
// For decryption, all of the cipher bytes in a buffer should be concatenated
// (in the subsample order) into a single logical stream. The clear bytes should
// not be considered as part of decryption.
//
// Stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
// Decrypted stream:    | decrypted1|  decrypted2 |   decrypted3  |
//
// After decryption, the decrypted bytes should be copied over the position
// of the corresponding cipher bytes in the original buffer to form the output
// buffer. Following the above example, the decrypted buffer should be:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
//
// TODO(xhwang): Add checks to make sure these structs have fixed layout.
struct SubsampleEntry {
  SubsampleEntry(uint32_t clear_bytes, uint32_t cipher_bytes)
      : clear_bytes(clear_bytes), cipher_bytes(cipher_bytes) {}

  uint32_t clear_bytes;
  uint32_t cipher_bytes;
};

// Represents an input buffer to be decrypted (and possibly decoded). It does
// own any pointers in this struct.
struct InputBuffer {
  InputBuffer()
      : data(NULL),
        data_size(0),
        data_offset(0),
        key_id(NULL),
        key_id_size(0),
        iv(NULL),
        iv_size(0),
        subsamples(NULL),
        num_subsamples(0),
        timestamp(0) {}

  const uint8_t* data;  // Pointer to the beginning of the input data.
  uint32_t data_size;  // Size (in bytes) of |data|.

  uint32_t data_offset;  // Number of bytes to be discarded before decryption.

  const uint8_t* key_id;  // Key ID to identify the decryption key.
  uint32_t key_id_size;  // Size (in bytes) of |key_id|.

  const uint8_t* iv;  // Initialization vector.
  uint32_t iv_size;  // Size (in bytes) of |iv|.

  const struct SubsampleEntry* subsamples;
  uint32_t num_subsamples;  // Number of subsamples in |subsamples|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

struct AudioDecoderConfig {
  enum AudioCodec {
    kUnknownAudioCodec = 0,
    kCodecVorbis,
    kCodecAac
  };

  AudioDecoderConfig()
      : codec(kUnknownAudioCodec),
        channel_count(0),
        bits_per_channel(0),
        samples_per_second(0),
        extra_data(NULL),
        extra_data_size(0) {}

  AudioCodec codec;
  int32_t channel_count;
  int32_t bits_per_channel;
  int32_t samples_per_second;

  // Optional byte data required to initialize audio decoders, such as the
  // vorbis setup header.
  uint8_t* extra_data;
  uint32_t extra_data_size;
};

// Supported sample formats for AudioFrames.
enum AudioFormat {
  kUnknownAudioFormat = 0,  // Unknown format value. Used for error reporting.
  kAudioFormatU8,  // Interleaved unsigned 8-bit w/ bias of 128.
  kAudioFormatS16,  // Interleaved signed 16-bit.
  kAudioFormatS32,  // Interleaved signed 32-bit.
  kAudioFormatF32,  // Interleaved float 32-bit.
  kAudioFormatPlanarS16,  // Signed 16-bit planar.
  kAudioFormatPlanarF32,  // Float 32-bit planar.
};

// Surface formats based on FOURCC labels, see: http://www.fourcc.org/yuv.php
enum VideoFormat {
  kUnknownVideoFormat = 0,  // Unknown format value. Used for error reporting.
  kYv12,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  kI420  // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
};

struct Size {
  Size() : width(0), height(0) {}
  Size(int32_t width, int32_t height) : width(width), height(height) {}

  int32_t width;
  int32_t height;
};

struct VideoDecoderConfig {
  enum VideoCodec {
    kUnknownVideoCodec = 0,
    kCodecVp8,
    kCodecH264
  };

  enum VideoCodecProfile {
    kUnknownVideoCodecProfile = 0,
    kVp8ProfileMain,
    kH264ProfileBaseline,
    kH264ProfileMain,
    kH264ProfileExtended,
    kH264ProfileHigh,
    kH264ProfileHigh10,
    kH264ProfileHigh422,
    kH264ProfileHigh444Predictive
  };

  VideoDecoderConfig()
      : codec(kUnknownVideoCodec),
        profile(kUnknownVideoCodecProfile),
        format(kUnknownVideoFormat),
        extra_data(NULL),
        extra_data_size(0) {}

  VideoCodec codec;
  VideoCodecProfile profile;
  VideoFormat format;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  Size coded_size;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8_t* extra_data;
  uint32_t extra_data_size;
};

enum StreamType {
  kStreamTypeAudio = 0,
  kStreamTypeVideo = 1
};

// Structure provided to ContentDecryptionModule::OnPlatformChallengeResponse()
// after a platform challenge was initiated via Host::SendPlatformChallenge().
// All values will be NULL / zero in the event of a challenge failure.
struct PlatformChallengeResponse {
  // |challenge| provided during Host::SendPlatformChallenge() combined with
  // nonce data and signed with the platform's private key.
  const uint8_t* signed_data;
  uint32_t signed_data_length;

  // RSASSA-PKCS1-v1_5-SHA256 signature of the |signed_data| block.
  const uint8_t* signed_data_signature;
  uint32_t signed_data_signature_length;

  // X.509 device specific certificate for the |service_id| requested.
  const uint8_t* platform_key_certificate;
  uint32_t platform_key_certificate_length;
};

// Supported output protection methods for use with EnableOutputProtection() and
// returned by OnQueryOutputProtectionStatus().
enum OutputProtectionMethods {
  kProtectionNone = 0,
  kProtectionHDCP = 1 << 0
};

// Connected output link types returned by OnQueryOutputProtectionStatus().
enum OutputLinkTypes {
  kLinkTypeNone = 0,
  kLinkTypeUnknown = 1 << 0,
  kLinkTypeInternal = 1 << 1,
  kLinkTypeVGA = 1 << 2,
  kLinkTypeHDMI = 1 << 3,
  kLinkTypeDVI = 1 << 4,
  kLinkTypeDisplayPort = 1 << 5,
  kLinkTypeNetwork = 1 << 6
};

//
// WARNING: Deprecated.  Will be removed in the near future.  CDMs should
// implement ContentDecryptionModule_2 instead.

// ContentDecryptionModule interface that all CDMs need to implement.
// The interface is versioned for backward compatibility.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class ContentDecryptionModule_1 {
 public:
  static const int kVersion = 1;
  typedef Host_1 Host;

  // Generates a |key_request| given |type| and |init_data|.
  //
  // Returns kSuccess if the key request was successfully generated, in which
  // case the CDM must send the key message by calling Host::SendKeyMessage().
  // Returns kSessionError if any error happened, in which case the CDM must
  // send a key error by calling Host::SendKeyError().
  virtual Status GenerateKeyRequest(
      const char* type, uint32_t type_size,
      const uint8_t* init_data, uint32_t init_data_size) = 0;

  // Adds the |key| to the CDM to be associated with |key_id|.
  //
  // Returns kSuccess if the key was successfully added, kSessionError
  // otherwise.
  virtual Status AddKey(const char* session_id, uint32_t session_id_size,
                        const uint8_t* key, uint32_t key_size,
                        const uint8_t* key_id, uint32_t key_id_size) = 0;

  // Cancels any pending key request made to the CDM for |session_id|.
  //
  // Returns kSuccess if all pending key requests for |session_id| were
  // successfully canceled or there was no key request to be canceled,
  // kSessionError otherwise.
  virtual Status CancelKeyRequest(
      const char* session_id, uint32_t session_id_size) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kSessionError if |audio_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kSessionError if |video_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |video_frame| (|format| == kEmptyVideoFrame) is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer& encrypted_buffer,
                                         AudioFrames_1* audio_frames) = 0;

  // Destroys the object in the same context as it was created.
  virtual void Destroy() = 0;

 protected:
  ContentDecryptionModule_1() {}
  virtual ~ContentDecryptionModule_1() {}
};

// ContentDecryptionModule interface that all CDMs need to implement.
// The interface is versioned for backward compatibility.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class ContentDecryptionModule_2 {
 public:
  static const int kVersion = 2;
  typedef Host_2 Host;

  // Generates a |key_request| given |type| and |init_data|.
  //
  // Returns kSuccess if the key request was successfully generated, in which
  // case the CDM must send the key message by calling Host::SendKeyMessage().
  // Returns kSessionError if any error happened, in which case the CDM must
  // send a key error by calling Host::SendKeyError().
  virtual Status GenerateKeyRequest(
      const char* type, uint32_t type_size,
      const uint8_t* init_data, uint32_t init_data_size) = 0;

  // Adds the |key| to the CDM to be associated with |key_id|.
  //
  // Returns kSuccess if the key was successfully added, kSessionError
  // otherwise.
  virtual Status AddKey(const char* session_id, uint32_t session_id_size,
                        const uint8_t* key, uint32_t key_size,
                        const uint8_t* key_id, uint32_t key_id_size) = 0;

  // Cancels any pending key request made to the CDM for |session_id|.
  //
  // Returns kSuccess if all pending key requests for |session_id| were
  // successfully canceled or there was no key request to be canceled,
  // kSessionError otherwise.
  virtual Status CancelKeyRequest(
      const char* session_id, uint32_t session_id_size) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kSessionError if |audio_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kSessionError if |video_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |video_frame| (|format| == kEmptyVideoFrame) is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer& encrypted_buffer,
                                         AudioFrames* audio_frames) = 0;

  // Called by the host after a platform challenge was initiated via
  // Host::SendPlatformChallenge().
  virtual void OnPlatformChallengeResponse(
      const PlatformChallengeResponse& response) = 0;

  // Called by the host after a call to Host::QueryOutputProtectionStatus(). The
  // |link_mask| is a bit mask of OutputLinkTypes and |output_protection_mask|
  // is a bit mask of OutputProtectionMethods.
  virtual void OnQueryOutputProtectionStatus(
      uint32_t link_mask, uint32_t output_protection_mask) = 0;

  // Destroys the object in the same context as it was created.
  virtual void Destroy() = 0;

 protected:
  ContentDecryptionModule_2() {}
  virtual ~ContentDecryptionModule_2() {}
};

// ContentDecryptionModule interface that all CDMs need to implement.
// The interface is versioned for backward compatibility.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class ContentDecryptionModule_3 {
 public:
  static const int kVersion = 3;
  typedef Host_3 Host;

  // CreateSession(), UpdateSession(), and ReleaseSession() get passed a
  // |session_id| for a MediaKeySession object. It must be used in the reply via
  // Host methods (e.g. Host::OnSessionMessage()).
  // Note: |session_id| is different from MediaKeySession's sessionId attribute,
  // which is referred to as |web_session_id| in this file.

  // Creates a session given |type| and |init_data|.
  virtual void CreateSession(
      uint32_t session_id,
      const char* type, uint32_t type_size,
      const uint8_t* init_data, uint32_t init_data_size) = 0;

  // Updates the session with |response|.
  virtual void UpdateSession(
      uint32_t session_id,
      const uint8_t* response, uint32_t response_size) = 0;

  // Releases the resources for the session.
  virtual void ReleaseSession(uint32_t session_id) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kSessionError if |audio_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kSessionError if |video_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |video_frame| (|format| == kEmptyVideoFrame) is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer& encrypted_buffer,
                                         AudioFrames* audio_frames) = 0;

  // Called by the host after a platform challenge was initiated via
  // Host::SendPlatformChallenge().
  virtual void OnPlatformChallengeResponse(
      const PlatformChallengeResponse& response) = 0;

  // Called by the host after a call to Host::QueryOutputProtectionStatus(). The
  // |link_mask| is a bit mask of OutputLinkTypes and |output_protection_mask|
  // is a bit mask of OutputProtectionMethods.
  virtual void OnQueryOutputProtectionStatus(
      uint32_t link_mask, uint32_t output_protection_mask) = 0;

  // Destroys the object in the same context as it was created.
  virtual void Destroy() = 0;

 protected:
  ContentDecryptionModule_3() {}
  virtual ~ContentDecryptionModule_3() {}
};

typedef ContentDecryptionModule_3 ContentDecryptionModule;

// Represents a buffer created by Allocator implementations.
class Buffer {
 public:
  // Destroys the buffer in the same context as it was created.
  virtual void Destroy() = 0;

  virtual uint32_t Capacity() const = 0;
  virtual uint8_t* Data() = 0;
  virtual void SetSize(uint32_t size) = 0;
  virtual uint32_t Size() const = 0;

 protected:
  Buffer() {}
  virtual ~Buffer() {}

 private:
  Buffer(const Buffer&);
  void operator=(const Buffer&);
};

// Host interface that the CDM can call into to access browser side services.
// Host interfaces are versioned for backward compatibility. CDM should use
// HostFactory object to request a Host interface of a particular version.
class Host_1 {
 public:
  static const int kVersion = 1;

  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(uint32_t capacity) = 0;

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current epoch wall time in seconds.
  virtual double GetCurrentWallTimeInSeconds() = 0;

  // Sends a keymessage event to the application.
  // Length parameters should not include null termination.
  virtual void SendKeyMessage(
      const char* session_id, uint32_t session_id_length,
      const char* message, uint32_t message_length,
      const char* default_url, uint32_t default_url_length) = 0;

  // Sends a keyerror event to the application.
  // |session_id_length| should not include null termination.
  virtual void SendKeyError(const char* session_id,
                            uint32_t session_id_length,
                            MediaKeyError error_code,
                            uint32_t system_code) = 0;

  // Get private data from the host. This function is limited to internal use.
  typedef const void* (*GetPrivateInterface)(const char* interface_name);
  virtual void GetPrivateData(int32_t* instance,
                              GetPrivateInterface* get_interface) = 0;

 protected:
  Host_1() {}
  virtual ~Host_1() {}
};

class Host_2 {
 public:
  static const int kVersion = 2;

  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(uint32_t capacity) = 0;

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current epoch wall time in seconds.
  virtual double GetCurrentWallTimeInSeconds() = 0;

  // Sends a keymessage event to the application.
  // Length parameters should not include null termination.
  virtual void SendKeyMessage(
      const char* session_id, uint32_t session_id_length,
      const char* message, uint32_t message_length,
      const char* default_url, uint32_t default_url_length) = 0;

  // Sends a keyerror event to the application.
  // |session_id_length| should not include null termination.
  virtual void SendKeyError(const char* session_id,
                            uint32_t session_id_length,
                            MediaKeyError error_code,
                            uint32_t system_code) = 0;

  // Get private data from the host. This function is limited to internal use.
  virtual void GetPrivateData(int32_t* instance,
                              Host_1::GetPrivateInterface* get_interface) = 0;

  // Sends a platform challenge for the given |service_id|. |challenge| is at
  // most 256 bits of data to be signed. Once the challenge has been completed,
  // the host will call ContentDecryptionModule::OnPlatformChallengeResponse()
  // with the signed challenge response and platform certificate. Length
  // parameters should not include null termination.
  virtual void SendPlatformChallenge(
      const char* service_id, uint32_t service_id_length,
      const char* challenge, uint32_t challenge_length) = 0;

  // Attempts to enable output protection (e.g. HDCP) on the display link. The
  // |desired_protection_mask| is a bit mask of OutputProtectionMethods. No
  // status callback is issued, the CDM must call QueryOutputProtectionStatus()
  // periodically to ensure the desired protections are applied.
  virtual void EnableOutputProtection(uint32_t desired_protection_mask) = 0;

  // Requests the current output protection status. Once the host has the status
  // it will call ContentDecryptionModule::OnQueryOutputProtectionStatus().
  virtual void QueryOutputProtectionStatus() = 0;

  // Must be called by the CDM if it returned kDeferredInitialization during
  // InitializeAudioDecoder() or InitializeVideoDecoder().
  virtual void OnDeferredInitializationDone(StreamType stream_type,
                                            Status decoder_status) = 0;

 protected:
  Host_2() {}
  virtual ~Host_2() {}
};

class Host_3 {
 public:
  static const int kVersion = 3;

  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(uint32_t capacity) = 0;

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current epoch wall time in seconds.
  virtual double GetCurrentWallTimeInSeconds() = 0;

  // Called by the CDM when a session is created and the value for the
  // MediaKeySession's sessionId attribute is available (|web_session_id|).
  // This must be called before OnSessionMessage() or OnSessionReady() is called
  // for |session_id|. |web_session_id_length| should not include null
  // termination.
  virtual void OnSessionCreated(
      uint32_t session_id,
      const char* web_session_id, uint32_t web_session_id_length) = 0;

  // Called by the CDM when it has a message for session |session_id|.
  // Length parameters should not include null termination.
  virtual void OnSessionMessage(
      uint32_t session_id,
      const char* message, uint32_t message_length,
      const char* destination_url, uint32_t destination_url_length) = 0;

  // Called by the CDM when session |session_id| is ready.
  virtual void OnSessionReady(uint32_t session_id) = 0;

  // Called by the CDM when session |session_id| is closed.
  virtual void OnSessionClosed(uint32_t session_id) = 0;

  // Called by the CDM when an error occurs in session |session_id|.
  virtual void OnSessionError(uint32_t session_id,
                              MediaKeyError error_code,
                              uint32_t system_code) = 0;

  // The following are optional methods that may not be implemented on all
  // platforms.

  // Sends a platform challenge for the given |service_id|. |challenge| is at
  // most 256 bits of data to be signed. Once the challenge has been completed,
  // the host will call ContentDecryptionModule::OnPlatformChallengeResponse()
  // with the signed challenge response and platform certificate. Length
  // parameters should not include null termination.
  virtual void SendPlatformChallenge(
      const char* service_id, uint32_t service_id_length,
      const char* challenge, uint32_t challenge_length) = 0;

  // Attempts to enable output protection (e.g. HDCP) on the display link. The
  // |desired_protection_mask| is a bit mask of OutputProtectionMethods. No
  // status callback is issued, the CDM must call QueryOutputProtectionStatus()
  // periodically to ensure the desired protections are applied.
  virtual void EnableOutputProtection(uint32_t desired_protection_mask) = 0;

  // Requests the current output protection status. Once the host has the status
  // it will call ContentDecryptionModule::OnQueryOutputProtectionStatus().
  virtual void QueryOutputProtectionStatus() = 0;

  // Must be called by the CDM if it returned kDeferredInitialization during
  // InitializeAudioDecoder() or InitializeVideoDecoder().
  virtual void OnDeferredInitializationDone(StreamType stream_type,
                                            Status decoder_status) = 0;

 protected:
  Host_3() {}
  virtual ~Host_3() {}
};

// Represents a decrypted block that has not been decoded.
class DecryptedBlock {
 public:
  virtual void SetDecryptedBuffer(Buffer* buffer) = 0;
  virtual Buffer* DecryptedBuffer() = 0;

  // TODO(tomfinegan): Figure out if timestamp is really needed. If it is not,
  // we can just pass Buffer pointers around.
  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  DecryptedBlock() {}
  virtual ~DecryptedBlock() {}
};

class VideoFrame {
 public:
  enum VideoPlane {
    kYPlane = 0,
    kUPlane = 1,
    kVPlane = 2,
    kMaxPlanes = 3,
  };

  virtual void SetFormat(VideoFormat format) = 0;
  virtual VideoFormat Format() const = 0;

  virtual void SetSize(cdm::Size size) = 0;
  virtual cdm::Size Size() const = 0;

  virtual void SetFrameBuffer(Buffer* frame_buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

  virtual void SetPlaneOffset(VideoPlane plane, uint32_t offset) = 0;
  virtual uint32_t PlaneOffset(VideoPlane plane) = 0;

  virtual void SetStride(VideoPlane plane, uint32_t stride) = 0;
  virtual uint32_t Stride(VideoPlane plane) = 0;

  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  VideoFrame() {}
  virtual ~VideoFrame() {}
};

//
// WARNING: Deprecated.  Will be removed in the near future.  CDMs should be
// implementing ContentDecryptionModule_2 instead which uses AudioFrames_2.
//
// Represents decrypted and decoded audio frames. AudioFrames can contain
// multiple audio output buffers, which are serialized into this format:
//
// |<------------------- serialized audio buffer ------------------->|
// | int64_t timestamp | int64_t length | length bytes of audio data |
//
// For example, with three audio output buffers, the AudioFrames will look
// like this:
//
// |<----------------- AudioFrames ------------------>|
// | audio buffer 0 | audio buffer 1 | audio buffer 2 |
class AudioFrames_1 {
 public:
  virtual void SetFrameBuffer(Buffer* buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

 protected:
  AudioFrames_1() {}
  virtual ~AudioFrames_1() {}
};

// Same as AudioFrames except the format of the data may be specified to avoid
// unnecessary conversion steps. Planar data should be stored end to end; e.g.,
// |ch1 sample1||ch1 sample2|....|ch1 sample_last||ch2 sample1|...
class AudioFrames_2 {
 public:
  virtual void SetFrameBuffer(Buffer* buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

  // Layout of the audio data.  Defaults to kAudioFormatS16.
  virtual void SetFormat(AudioFormat format) = 0;
  virtual AudioFormat Format() const = 0;

 protected:
  AudioFrames_2() {}
  virtual ~AudioFrames_2() {}
};

}  // namespace cdm

#endif  // CDM_CONTENT_DECRYPTION_MODULE_H_
