// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONTMGR_H_
#define CORE_FXGE_CFX_FONTMGR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <tuple>

#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fixed_size_data_vector.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/span.h"
#include "core/fxge/cfx_face.h"
#include "core/fxge/freetype/fx_freetype.h"

class CFX_FontMapper;
class CFX_GlyphCache;

class CFX_FontMgr {
 public:
  class FontCacheEntry final : public Retainable, public Observable {
   public:
    CONSTRUCT_VIA_MAKE_RETAIN;

    pdfium::span<const uint8_t> FontData() const { return font_data_; }
    void SetFace(uint32_t face_index, CFX_Face* face);
    CFX_Face* GetFace(uint32_t face_index) const;

   private:
    explicit FontCacheEntry(FixedSizeDataVector<uint8_t> data);
    ~FontCacheEntry() override;

    const FixedSizeDataVector<uint8_t> font_data_;
    std::array<ObservedPtr<CFX_Face>, 16> ttc_faces_;
  };

  // `index` must be less than `CFX_FontMapper::kNumStandardFonts`.
  static pdfium::span<const uint8_t> GetStandardFont(size_t index);
  static pdfium::span<const uint8_t> GetGenericSansFont();
  static pdfium::span<const uint8_t> GetGenericSerifFont();

  CFX_FontMgr();
  ~CFX_FontMgr();

  RetainPtr<FontCacheEntry> GetFontCacheEntry(const ByteString& face_name,
                                              int weight,
                                              bool italic);
  RetainPtr<FontCacheEntry> AddFontCacheEntry(
      const ByteString& face_name,
      int weight,
      bool italic,
      FixedSizeDataVector<uint8_t> data);

  RetainPtr<FontCacheEntry> GetTTCFontCacheEntry(size_t ttc_size,
                                                 uint32_t checksum);
  RetainPtr<FontCacheEntry> AddTTCFontCacheEntry(
      size_t ttc_size,
      uint32_t checksum,
      FixedSizeDataVector<uint8_t> data);

  RetainPtr<CFX_GlyphCache> GetGlyphCache(const CFX_Font* font);

  // Always present.
  CFX_FontMapper* GetBuiltinMapper() const { return builtin_mapper_.get(); }

  FXFT_LibraryRec* GetFTLibrary() const { return ft_library_.get(); }
  bool FTLibrarySupportsHinting() const { return ft_library_supports_hinting_; }

 private:
  using NameWeightItalic = std::tuple<ByteString, int, bool>;
  using SizeChecksum = std::tuple<size_t, uint32_t>;

  // Must come before |builtin_mapper_| and |face_map_|.
  ScopedFXFTLibraryRec const ft_library_;
  std::unique_ptr<CFX_FontMapper> builtin_mapper_;
  std::map<NameWeightItalic, ObservedPtr<FontCacheEntry>> face_map_;
  std::map<SizeChecksum, ObservedPtr<FontCacheEntry>> ttc_face_map_;
  std::map<CFX_Face*, ObservedPtr<CFX_GlyphCache>> glyph_cache_map_;
  const bool ft_library_supports_hinting_;
};

#endif  // CORE_FXGE_CFX_FONTMGR_H_
