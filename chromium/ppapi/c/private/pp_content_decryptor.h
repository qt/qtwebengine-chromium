/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/pp_content_decryptor.idl modified Mon Oct 21 18:38:44 2013. */

#ifndef PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_
#define PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * The <code>PP_DecryptTrackingInfo</code> struct contains necessary information
 * that can be used to associate the decrypted block with a decrypt request
 * and/or an input block.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_DecryptTrackingInfo {
  /**
   * Client-specified identifier for the associated decrypt request. By using
   * this value, the client can associate the decrypted block with a decryption
   * request.
   */
  uint32_t request_id;
  /**
   * A unique buffer ID to identify a PPB_Buffer_Dev. Unlike a PP_Resource,
   * this ID is identical at both the renderer side and the plugin side.
   * In <code>PPB_ContentDecryptor_Private</code> calls, this is the ID of the
   * buffer associated with the decrypted block/frame/samples.
   * In <code>PPP_ContentDecryptor_Private</code> calls, this is the ID of a
   * buffer that is no longer need at the renderer side, which can be released
   * or recycled by the plugin. This ID can be 0 if there is no buffer to be
   * released or recycled.
   */
  uint32_t buffer_id;
  /**
   * Timestamp in microseconds of the associated block. By using this value,
   * the client can associate the decrypted (and decoded) data with an input
   * block. This is needed because buffers may be delivered out of order and
   * not in response to the <code>request_id</code> they were provided with.
   */
  int64_t timestamp;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptTrackingInfo, 16);

/**
 * The <code>PP_DecryptSubsampleDescription</code> struct contains information
 * to support subsample decryption.
 *
 * An input block can be split into several continuous subsamples.
 * A <code>PP_DecryptSubsampleEntry</code> specifies the number of clear and
 * cipher bytes in each subsample. For example, the following block has three
 * subsamples:
 *
 * |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
 * |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
 *
 * For decryption, all of the cipher bytes in a block should be treated as a
 * contiguous (in the subsample order) logical stream. The clear bytes should
 * not be considered as part of decryption.
 *
 * Logical stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
 * Decrypted stream:            | decrypted1|  decrypted2 |   decrypted3  |
 *
 * After decryption, the decrypted bytes should be copied over the position
 * of the corresponding cipher bytes in the original block to form the output
 * block. Following the above example, the decrypted block should be:
 *
 * |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
 * |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
 */
struct PP_DecryptSubsampleDescription {
  /**
   * Size in bytes of clear data in a subsample entry.
   */
  uint32_t clear_bytes;
  /**
   * Size in bytes of encrypted data in a subsample entry.
   */
  uint32_t cipher_bytes;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptSubsampleDescription, 8);

/**
 * The <code>PP_EncryptedBlockInfo</code> struct contains all the information
 * needed to decrypt an encrypted block.
 */
struct PP_EncryptedBlockInfo {
  /**
   * Information needed by the client to track the block to be decrypted.
   */
  struct PP_DecryptTrackingInfo tracking_info;
  /**
   * Size in bytes of data to be decrypted (data_offset included).
   */
  uint32_t data_size;
  /**
   * Size in bytes of data to be discarded before applying the decryption.
   */
  uint32_t data_offset;
  /**
   * Key ID of the block to be decrypted.
   *
   * TODO(xhwang): For WebM the key ID can be as large as 2048 bytes in theory.
   * But it's not used in current implementations. If we really need to support
   * it, we should move key ID out as a separate parameter, e.g.
   * as a <code>PP_Var</code>, or make the whole
   * <code>PP_EncryptedBlockInfo</code> as a <code>PP_Resource</code>.
   */
  uint8_t key_id[64];
  uint32_t key_id_size;
  /**
   * Initialization vector of the block to be decrypted.
   */
  uint8_t iv[16];
  uint32_t iv_size;
  /**
   * Subsample information of the block to be decrypted.
   */
  struct PP_DecryptSubsampleDescription subsamples[16];
  uint32_t num_subsamples;
  /**
   * 4-byte padding to make the size of <code>PP_EncryptedBlockInfo</code>
   * a multiple of 8 bytes. The value of this field should not be used.
   */
  uint32_t padding;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_EncryptedBlockInfo, 248);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_DecryptedFrameFormat</code> contains video frame formats.
 */
typedef enum {
  PP_DECRYPTEDFRAMEFORMAT_UNKNOWN = 0,
  PP_DECRYPTEDFRAMEFORMAT_YV12 = 1,
  PP_DECRYPTEDFRAMEFORMAT_I420 = 2
} PP_DecryptedFrameFormat;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptedFrameFormat, 4);

/**
 * <code>PP_DecryptedSampleFormat</code> contains audio sample formats.
 */
typedef enum {
  PP_DECRYPTEDSAMPLEFORMAT_UNKNOWN = 0,
  PP_DECRYPTEDSAMPLEFORMAT_U8 = 1,
  PP_DECRYPTEDSAMPLEFORMAT_S16 = 2,
  PP_DECRYPTEDSAMPLEFORMAT_S32 = 3,
  PP_DECRYPTEDSAMPLEFORMAT_F32 = 4,
  PP_DECRYPTEDSAMPLEFORMAT_PLANAR_S16 = 5,
  PP_DECRYPTEDSAMPLEFORMAT_PLANAR_F32 = 6
} PP_DecryptedSampleFormat;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptedSampleFormat, 4);

/**
 * The <code>PP_DecryptResult</code> enum contains decryption and decoding
 * result constants.
 */
typedef enum {
  /** The decryption (and/or decoding) operation finished successfully. */
  PP_DECRYPTRESULT_SUCCESS = 0,
  /** The decryptor did not have the necessary decryption key. */
  PP_DECRYPTRESULT_DECRYPT_NOKEY = 1,
  /** The input was accepted by the decoder but no frame(s) can be produced. */
  PP_DECRYPTRESULT_NEEDMOREDATA = 2,
  /** An unexpected error happened during decryption. */
  PP_DECRYPTRESULT_DECRYPT_ERROR = 3,
  /** An unexpected error happened during decoding. */
  PP_DECRYPTRESULT_DECODE_ERROR = 4
} PP_DecryptResult;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptResult, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_DecryptedBlockInfo</code> struct contains the decryption result and
 * tracking info associated with the decrypted block.
 */
struct PP_DecryptedBlockInfo {
  /**
   * Result of the decryption (and/or decoding) operation.
   */
  PP_DecryptResult result;
  /**
   * Size in bytes of decrypted data, which may be less than the size of the
   * corresponding buffer.
   */
  uint32_t data_size;
  /**
   * Information needed by the client to track the block to be decrypted.
   */
  struct PP_DecryptTrackingInfo tracking_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptedBlockInfo, 24);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_DecryptedFramePlanes</code> provides YUV plane index values for
 * accessing plane offsets stored in <code>PP_DecryptedFrameInfo</code>.
 */
typedef enum {
  PP_DECRYPTEDFRAMEPLANES_Y = 0,
  PP_DECRYPTEDFRAMEPLANES_U = 1,
  PP_DECRYPTEDFRAMEPLANES_V = 2
} PP_DecryptedFramePlanes;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptedFramePlanes, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_DecryptedFrameInfo</code> contains the result of the
 * decrypt and decode operation on the associated frame, information required
 * to access the frame data in buffer, and tracking info.
 */
struct PP_DecryptedFrameInfo {
  /**
   * Result of the decrypt and decode operation.
   */
  PP_DecryptResult result;
  /**
   * Format of the decrypted frame.
   */
  PP_DecryptedFrameFormat format;
  /**
   * Offsets into the buffer resource for accessing video planes.
   */
  int32_t plane_offsets[3];
  /**
   * Stride of each plane.
   */
  int32_t strides[3];
  /**
   * Width of the video frame, in pixels.
   */
  int32_t width;
  /**
   * Height of the video frame, in pixels.
   */
  int32_t height;
  /**
   * Information needed by the client to track the decrypted frame.
   */
  struct PP_DecryptTrackingInfo tracking_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptedFrameInfo, 56);

/**
 * <code>PP_DecryptedSampleInfo</code> contains the result of the
 * decrypt and decode operation on the associated samples, information required
 * to access the sample data in buffer, and tracking info.
 */
struct PP_DecryptedSampleInfo {
  /**
   * Result of the decrypt and decode operation.
   */
  PP_DecryptResult result;
  /**
   * Format of the decrypted samples.
   */
  PP_DecryptedSampleFormat format;
  /**
   * Size in bytes of decrypted samples.
   */
  uint32_t data_size;
  /**
   * 4-byte padding to make the size of <code>PP_DecryptedSampleInfo</code>
   * a multiple of 8 bytes. The value of this field should not be used.
   */
  uint32_t padding;
  /**
   * Information needed by the client to track the decrypted samples.
   */
  struct PP_DecryptTrackingInfo tracking_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptedSampleInfo, 32);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_AudioCodec</code> contains audio codec type constants.
 */
typedef enum {
  PP_AUDIOCODEC_UNKNOWN = 0,
  PP_AUDIOCODEC_VORBIS = 1,
  PP_AUDIOCODEC_AAC = 2
} PP_AudioCodec;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_AudioCodec, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_AudioDecoderConfig</code> contains audio decoder configuration
 * information required to initialize audio decoders, and a request ID
 * that allows clients to associate a decoder initialization request with a
 * status response. Note: When <code>codec</code> requires extra data for
 * initialization, the data is sent as a <code>PP_Resource</code> carried
 * alongside <code>PP_AudioDecoderConfig</code>.
 */
struct PP_AudioDecoderConfig {
  /**
   * The audio codec to initialize.
   */
  PP_AudioCodec codec;
  /**
   * Number of audio channels.
   */
  int32_t channel_count;
  /**
   * Size of each audio channel.
   */
  int32_t bits_per_channel;
  /**
   * Audio sampling rate.
   */
  int32_t samples_per_second;
  /**
   * Client-specified identifier for the associated audio decoder initialization
   * request. By using this value, the client can associate a decoder
   * initialization status response with an initialization request.
   */
  uint32_t request_id;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_AudioDecoderConfig, 20);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_VideoCodec</code> contains video codec type constants.
 */
typedef enum {
  PP_VIDEOCODEC_UNKNOWN = 0,
  PP_VIDEOCODEC_VP8 = 1,
  PP_VIDEOCODEC_H264 = 2
} PP_VideoCodec;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VideoCodec, 4);

/**
 * <code>PP_VideoCodecProfile</code> contains video codec profile type
 * constants required for video decoder configuration.
 *.
 */
typedef enum {
  PP_VIDEOCODECPROFILE_UNKNOWN = 0,
  PP_VIDEOCODECPROFILE_VP8_MAIN = 1,
  PP_VIDEOCODECPROFILE_H264_BASELINE = 2,
  PP_VIDEOCODECPROFILE_H264_MAIN = 3,
  PP_VIDEOCODECPROFILE_H264_EXTENDED = 4,
  PP_VIDEOCODECPROFILE_H264_HIGH = 5,
  PP_VIDEOCODECPROFILE_H264_HIGH_10 = 6,
  PP_VIDEOCODECPROFILE_H264_HIGH_422 = 7,
  PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE = 8
} PP_VideoCodecProfile;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VideoCodecProfile, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_VideoDecoderConfig</code> contains video decoder configuration
 * information required to initialize video decoders, and a request ID
 * that allows clients to associate a decoder initialization request with a
 * status response. Note: When <code>codec</code> requires extra data for
 * initialization, the data is sent as a <code>PP_Resource</code> carried
 * alongside <code>PP_VideoDecoderConfig</code>.
 */
struct PP_VideoDecoderConfig {
  /**
   * The video codec to initialize.
   */
  PP_VideoCodec codec;
  /**
   * Profile to use when initializing the video codec.
   */
  PP_VideoCodecProfile profile;
  /**
   * Output video format.
   */
  PP_DecryptedFrameFormat format;
  /**
   * Width of decoded video frames, in pixels.
   */
  int32_t width;
  /**
   * Height of decoded video frames, in pixels.
   */
  int32_t height;
  /**
   * Client-specified identifier for the associated video decoder initialization
   * request. By using this value, the client can associate a decoder
   * initialization status response with an initialization request.
   */
  uint32_t request_id;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_VideoDecoderConfig, 24);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_DecryptorStreamType</code> contains stream type constants.
 */
typedef enum {
  PP_DECRYPTORSTREAMTYPE_AUDIO = 0,
  PP_DECRYPTORSTREAMTYPE_VIDEO = 1
} PP_DecryptorStreamType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptorStreamType, 4);
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_ */

