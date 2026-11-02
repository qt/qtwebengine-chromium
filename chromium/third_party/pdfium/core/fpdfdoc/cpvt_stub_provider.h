// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FPDFDOC_CPVT_STUB_PROVIDER_H_
#define CORE_FPDFDOC_CPVT_STUB_PROVIDER_H_

#include "core/fpdfdoc/cpvt_variabletext.h"

class IPVT_FontMap;

// A stub that provides fixed font metrics (like a character width of 10)
// for testing text layout.
class CPVT_StubProvider final : public CPVT_VariableText::Provider {
 public:
  explicit CPVT_StubProvider(IPVT_FontMap* font_map);
  ~CPVT_StubProvider() override;

  // CPVT_VariableText::Provider:
  int GetCharWidth(int32_t nFontIndex, uint16_t word) override;
  int32_t GetTypeAscent(int32_t nFontIndex) override;
  int32_t GetTypeDescent(int32_t nFontIndex) override;
  int32_t GetWordFontIndex(uint16_t word,
                           FX_Charset charset,
                           int32_t nFontIndex) override;
  int32_t GetDefaultFontIndex() override;
};

#endif  // CORE_FPDFDOC_CPVT_STUB_PROVIDER_H_
