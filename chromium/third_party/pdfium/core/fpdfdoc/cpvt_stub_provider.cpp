// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfdoc/cpvt_stub_provider.h"

CPVT_StubProvider::CPVT_StubProvider(IPVT_FontMap* font_map)
    : CPVT_VariableText::Provider(font_map) {}

CPVT_StubProvider::~CPVT_StubProvider() = default;

int CPVT_StubProvider::GetCharWidth(int32_t nFontIndex, uint16_t word) {
  return 10;
}

int32_t CPVT_StubProvider::GetTypeAscent(int32_t nFontIndex) {
  return 10;
}

int32_t CPVT_StubProvider::GetTypeDescent(int32_t nFontIndex) {
  return -2;
}

int32_t CPVT_StubProvider::GetWordFontIndex(uint16_t word,
                                            FX_Charset charset,
                                            int32_t nFontIndex) {
  return 0;
}

int32_t CPVT_StubProvider::GetDefaultFontIndex() {
  return 0;
}
