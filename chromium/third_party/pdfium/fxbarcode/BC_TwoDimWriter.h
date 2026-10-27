// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_BC_TWODIMWRITER_H_
#define FXBARCODE_BC_TWODIMWRITER_H_

#include <stdint.h>

#include <memory>

#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/span.h"
#include "fxbarcode/BC_Writer.h"

class CBC_CommonBitMatrix;
class CFX_RenderDevice;

class CBC_TwoDimWriter : public CBC_Writer {
 public:
  struct EncodeResult {
    EncodeResult();
    EncodeResult(DataVector<uint8_t> code, int32_t width, int32_t height);
    ~EncodeResult();

    DataVector<uint8_t> code;
    int32_t width = 0;
    int32_t height = 0;
  };

  explicit CBC_TwoDimWriter(bool bFixedSize);
  ~CBC_TwoDimWriter() override;

  bool RenderResult(const EncodeResult& result);
  void RenderDeviceResult(CFX_RenderDevice* device, const CFX_Matrix& matrix);

  int32_t error_correction_level() const { return correction_level_; }

 protected:
  void set_error_correction_level(int32_t level) { correction_level_ = level; }

 private:
  std::unique_ptr<CBC_CommonBitMatrix> output_;
  int32_t multi_x_;
  int32_t multi_y_;
  int32_t left_padding_;
  int32_t top_padding_;
  int32_t input_width_;
  int32_t input_height_;
  int32_t output_width_;
  int32_t output_height_;
  int32_t correction_level_ = 1;
  const bool fixed_size_;
};

#endif  // FXBARCODE_BC_TWODIMWRITER_H_
