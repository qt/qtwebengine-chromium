// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_LAYOUT_FGAS_LINEBREAK_H_
#define XFA_FGAS_LAYOUT_FGAS_LINEBREAK_H_

#include <stdint.h>

#include "core/fxcrt/fx_unicode.h"

namespace pdfium {

enum class LineBreakType : uint8_t {
  kUnknown = 0x00,
  kDirectBreak = 0x1A,
  kIndirectBreak = 0x2B,
  kCommonIndirectBreak = 0x3C,
  kCommonProhibitedBreak = 0x4D,
  kProhibitedBreak = 0x5E,
  kHangulSpaceBreak = 0x6F,
};

LineBreakType GetLineBreakTypeFromPair(BreakProperty curr_char,
                                       BreakProperty next_char);

}  // namespace pdfium

#endif  // XFA_FGAS_LAYOUT_FGAS_LINEBREAK_H_
