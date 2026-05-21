// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_fontmgr.h"

#include <array>
#include <iterator>
#include <memory>
#include <utility>

#include "core/fxcrt/check_op.h"
#include "core/fxcrt/fixed_size_data_vector.h"
#include "core/fxge/cfx_font.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_glyphcache.h"
#include "core/fxge/cfx_substfont.h"
#include "core/fxge/fontdata/chromefontdata/chromefontdata.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/systemfontinfo_iface.h"

namespace {

constexpr std::array<pdfium::span<const uint8_t>,
                     CFX_FontMapper::kNumStandardFonts>
    kFoxitFonts = {{
        kFoxitFixedFontData,
        kFoxitFixedBoldFontData,
        kFoxitFixedBoldItalicFontData,
        kFoxitFixedItalicFontData,
        kFoxitSansFontData,
        kFoxitSansBoldFontData,
        kFoxitSansBoldItalicFontData,
        kFoxitSansItalicFontData,
        kFoxitSerifFontData,
        kFoxitSerifBoldFontData,
        kFoxitSerifBoldItalicFontData,
        kFoxitSerifItalicFontData,
        kFoxitSymbolFontData,
        kFoxitDingbatsFontData,
    }};

constexpr pdfium::span<const uint8_t> kGenericSansFont = kFoxitSansMMFontData;
constexpr pdfium::span<const uint8_t> kGenericSerifFont = kFoxitSerifMMFontData;

}  // namespace

CFX_FontMgr::FontCacheEntry::FontCacheEntry(FixedSizeDataVector<uint8_t> data)
    : font_data_(std::move(data)) {}

CFX_FontMgr::FontCacheEntry::~FontCacheEntry() = default;

void CFX_FontMgr::FontCacheEntry::SetFace(uint32_t face_index, CFX_Face* face) {
  CHECK_LT(face_index, std::size(ttc_faces_));
  ttc_faces_[face_index].Reset(face);
}

CFX_Face* CFX_FontMgr::FontCacheEntry::GetFace(uint32_t face_index) const {
  CHECK_LT(face_index, std::size(ttc_faces_));
  return ttc_faces_[face_index].Get();
}

CFX_FontMgr::CFX_FontMgr()
    : ft_library_(InitializeFreeType()),
      builtin_mapper_(std::make_unique<CFX_FontMapper>(this)),
      ft_library_supports_hinting_(
          FreeTypeSetLcdFilterMode(ft_library_.get()) ||
          FreeTypeVersionSupportsHinting(ft_library_.get())) {}

CFX_FontMgr::~CFX_FontMgr() = default;

RetainPtr<CFX_FontMgr::FontCacheEntry> CFX_FontMgr::GetFontCacheEntry(
    const ByteString& face_name,
    int weight,
    bool italic) {
  auto it = face_map_.find({face_name, weight, italic});
  return it != face_map_.end() ? pdfium::WrapRetain(it->second.Get()) : nullptr;
}

RetainPtr<CFX_FontMgr::FontCacheEntry> CFX_FontMgr::AddFontCacheEntry(
    const ByteString& face_name,
    int weight,
    bool italic,
    FixedSizeDataVector<uint8_t> data) {
  auto cache_entry = pdfium::MakeRetain<FontCacheEntry>(std::move(data));
  face_map_[{face_name, weight, italic}].Reset(cache_entry.Get());
  return cache_entry;
}

RetainPtr<CFX_FontMgr::FontCacheEntry> CFX_FontMgr::GetTTCFontCacheEntry(
    size_t ttc_size,
    uint32_t checksum) {
  auto it = ttc_face_map_.find({ttc_size, checksum});
  return it != ttc_face_map_.end() ? pdfium::WrapRetain(it->second.Get())
                                   : nullptr;
}

RetainPtr<CFX_FontMgr::FontCacheEntry> CFX_FontMgr::AddTTCFontCacheEntry(
    size_t ttc_size,
    uint32_t checksum,
    FixedSizeDataVector<uint8_t> data) {
  auto new_entry = pdfium::MakeRetain<FontCacheEntry>(std::move(data));
  ttc_face_map_[{ttc_size, checksum}].Reset(new_entry.Get());
  return new_entry;
}

RetainPtr<CFX_GlyphCache> CFX_FontMgr::GetGlyphCache(const CFX_Font* font) {
  RetainPtr<CFX_Face> face = font->GetFace();
  auto it = glyph_cache_map_.find(face.Get());
  if (it != glyph_cache_map_.end() && it->second) {
    return pdfium::WrapRetain(it->second.Get());
  }
  auto new_cache = pdfium::MakeRetain<CFX_GlyphCache>(face);
  glyph_cache_map_[face.Get()].Reset(new_cache.Get());
  return new_cache;
}

// static
pdfium::span<const uint8_t> CFX_FontMgr::GetStandardFont(size_t index) {
  return kFoxitFonts[index];
}

// static
pdfium::span<const uint8_t> CFX_FontMgr::GetGenericSansFont() {
  return kGenericSansFont;
}

// static
pdfium::span<const uint8_t> CFX_FontMgr::GetGenericSerifFont() {
  return kGenericSerifFont;
}
