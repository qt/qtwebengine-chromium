// Copyright 2018 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/utils/bitmap_saver.h"

#include <fstream>
#include <vector>

#include "core/fxcrt/check.h"
#include "testing/utils/png_encode.h"

// static
void BitmapSaver::WriteBitmapToPng(FPDF_BITMAP bitmap,
                                   const std::string& filename) {
  std::vector<uint8_t> png = EncodePng(bitmap);
  DCHECK(!png.empty());
  DCHECK(filename.size() < 256u);

  std::ofstream png_file;
  png_file.open(filename, std::ios_base::out | std::ios_base::binary);
  png_file.write(reinterpret_cast<char*>(&png.front()), png.size());
  DCHECK(png_file.good());
  png_file.close();
}

// static
void BitmapSaver::WriteBitmapToPng(CFX_DIBitmap* bitmap,
                                   const std::string& filename) {
  WriteBitmapToPng(reinterpret_cast<FPDF_BITMAP>(bitmap), filename);
}
