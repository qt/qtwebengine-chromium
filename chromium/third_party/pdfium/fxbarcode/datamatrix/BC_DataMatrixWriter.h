// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_DATAMATRIX_BC_DATAMATRIXWRITER_H_
#define FXBARCODE_DATAMATRIX_BC_DATAMATRIXWRITER_H_

#include <stdint.h>

#include "core/fxcrt/widestring.h"
#include "fxbarcode/BC_TwoDimWriter.h"

class CBC_DataMatrixWriter final : public CBC_TwoDimWriter {
 public:
  CBC_DataMatrixWriter();
  ~CBC_DataMatrixWriter() override;

  EncodeResult Encode(const WideString& contents);

  // CBC_TwoDimWriter:
  bool SetErrorCorrectionLevel(int32_t level) override;
};

#endif  // FXBARCODE_DATAMATRIX_BC_DATAMATRIXWRITER_H_
