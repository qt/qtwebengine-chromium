// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCODEC_BMP_BMP_DECODER_DELEGATE_H_
#define CORE_FXCODEC_BMP_BMP_DECODER_DELEGATE_H_

#include <stdint.h>

#include "core/fxcrt/span.h"

namespace fxcodec {

class BmpDecoderDelegate {
 public:
  virtual bool BmpInputImagePositionBuf(uint32_t rcd_pos) = 0;
  virtual void BmpReadScanline(uint32_t row_num,
                               pdfium::span<const uint8_t> row_buf) = 0;
};

}  // namespace fxcodec

using BmpDecoderDelegate = fxcodec::BmpDecoderDelegate;

#endif  // CORE_FXCODEC_BMP_BMP_DECODER_DELEGATE_H_
