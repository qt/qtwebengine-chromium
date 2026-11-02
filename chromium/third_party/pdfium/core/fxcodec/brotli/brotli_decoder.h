// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCODEC_BROTLI_BROTLI_DECODER_H_
#define CORE_FXCODEC_BROTLI_BROTLI_DECODER_H_

#include <stdint.h>

#include "core/fxcodec/data_and_bytes_consumed.h"
#include "core/fxcrt/span.h"

class BrotliDecoder {
 public:
  static DataAndBytesConsumed Decode(pdfium::span<const uint8_t> src_span,
                                     uint32_t estimated_decode_size);
  static void SetBrotliEnabled(bool enabled);
  static bool GetBrotliEnabled();
};

#endif  // CORE_FXCODEC_BROTLI_BROTLI_DECODER_H_
