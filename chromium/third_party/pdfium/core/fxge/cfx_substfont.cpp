// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_substfont.h"

CFX_SubstFont::CFX_SubstFont() = default;

CFX_SubstFont::~CFX_SubstFont() = default;

#if defined(PDF_USE_SKIA)
int CFX_SubstFont::GetOriginalWeight() const {
  int weight = weight_;

  // Perform the inverse weight adjustment of UseChromeSerif() to get the
  // original font weight.
  if (family_ == "Chrome Serif") {
    weight = weight * 5 / 4;
  }
  return weight;
}
#endif

void CFX_SubstFont::UseChromeSerif() {
  weight_ = weight_ * 4 / 5;
  family_ = "Chrome Serif";
}

// The following is not a perfect solution and can be further improved.
// For example, if `this` is "Book" and the `base_font_name` is "Bookman",
// this function will return "true" even though the actual font "Bookman"
// is not loaded.
// An exact string match is not possible here, because `subst_font_name`
// will be the same value for different postscript names.
// For example: "Times New Roman" as `subst_font_name` for all of these
// `base_font_name` values: "TimesNewRoman,Bold", "TimesNewRomanPS-Bold",
// "TimesNewRomanBold" and "TimesNewRoman-Bold".
bool CFX_SubstFont::IsActualFontLoaded(const ByteString& base_font_name) const {
  // Skip if we loaded the actual font.
  // example: TimesNewRoman,Bold -> Times New Roman
  ByteString subst_font_name = family_;
  subst_font_name.Remove(' ');
  subst_font_name.MakeLower();

  std::optional<size_t> find =
      base_font_name.Find(subst_font_name.AsStringView());
  return find.has_value() && find.value() == 0;
}
