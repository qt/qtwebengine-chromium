// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_PNG_ENCODE_H_
#define TESTING_UTILS_PNG_ENCODE_H_

#include <stdint.h>

#include <vector>

#include "core/fxcrt/span.h"
#include "public/fpdfview.h"

std::vector<uint8_t> EncodePng(pdfium::span<const uint8_t> input,
                               int width,
                               int height,
                               int stride,
                               int format);
std::vector<uint8_t> EncodePng(FPDF_BITMAP bitmap);

#endif  // TESTING_UTILS_PNG_ENCODE_H_
