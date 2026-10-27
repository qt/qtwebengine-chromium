// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/android/cfpf_skiafont.h"

#include <utility>

#include "core/fxcrt/check.h"
#include "core/fxcrt/numerics/safe_conversions.h"

CFPF_SkiaFont::CFPF_SkiaFont(RetainPtr<CFX_Face> face, FX_Charset uCharset)
    : face_(std::move(face)), charset_(uCharset) {
  CHECK(face_);
}

CFPF_SkiaFont::~CFPF_SkiaFont() = default;

ByteString CFPF_SkiaFont::GetFamilyName() {
  return face_->GetFamilyName();
}

uint32_t CFPF_SkiaFont::GetFontData(uint32_t dwTable,
                                    pdfium::span<uint8_t> pBuffer) {
  return pdfium::checked_cast<uint32_t>(face_->GetSfntTable(dwTable, pBuffer));
}
