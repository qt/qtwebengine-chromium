// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_
#define MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"

typedef struct vpx_codec_ctx vpx_dec_ctx_t;

namespace media {
namespace cast {

class Vp8Decoder {
 public:
  explicit Vp8Decoder(int number_of_cores);

  ~Vp8Decoder();

  // Initialize the decoder.
  void InitDecode(int number_of_cores);

  // Decode encoded image (as a part of a video stream).
  bool Decode(const EncodedVideoFrame& input_image,
              I420VideoFrame* decoded_frame);

 private:
  scoped_ptr<vpx_dec_ctx_t> decoder_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_
