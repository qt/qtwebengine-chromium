// Copyright 2019 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXGE_CFX_FACE_H_
#define CORE_FXGE_CFX_FACE_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/cfx_read_only_vector_stream.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/span.h"
#include "core/fxge/freetype/fx_freetype.h"
#include "core/fxge/fx_font.h"

namespace fxge {
enum class FontEncoding : uint32_t;
}

class CFX_Font;
class CFX_FontMgr;
class CFX_GlyphBitmap;
class CFX_Path;
class CFX_SubstFont;

class CFX_Face final : public Retainable, public Observable {
 public:
  using CharMap = void*;

  // Note that this corresponds to the cmap header in fonts, and not the cmap
  // data in PDFs.
  struct CharMapId {
    friend constexpr bool operator==(const CharMapId&,
                                     const CharMapId&) = default;

    int platform_id;
    int encoding_id;
  };

  // Aliases for some commonly used cmaps.
  static constexpr CharMapId kMacRomanCmapId{1, 0};
  static constexpr CharMapId kWindowsSymbolCmapId{3, 0};
  static constexpr CharMapId kWindowsUnicodeCmapId{3, 1};

  static RetainPtr<CFX_Face> New(CFX_FontMgr* font_mgr,
                                 RetainPtr<Retainable> desc,
                                 pdfium::span<const uint8_t> data,
                                 uint32_t face_index);

#if defined(PDF_ENABLE_XFA)
  static RetainPtr<CFX_Face> NewFromVectorStream(
      CFX_FontMgr* font_mgr,
      const RetainPtr<CFX_ReadOnlyVectorStream>& font_stream,
      uint32_t face_index);
#endif

#if BUILDFLAG(IS_ANDROID)
  static RetainPtr<CFX_Face> OpenFromFilePath(CFX_FontMgr* font_mgr,
                                              ByteStringView path,
                                              int32_t face_index);
#endif

  bool HasGlyphNames() const;
  bool IsTtOt() const;
  bool IsFixedWidth() const;
  bool IsItalic() const;
  bool IsBold() const;

  ByteString GetFamilyName() const;
  ByteString GetStyleName() const;

  FX_RECT GetBBox() const;
  uint16_t GetUnitsPerEm() const;
  int16_t GetAscender() const;
  int16_t GetDescender() const;

  pdfium::span<uint8_t> GetData() const;

  // Returns the size of the data, or 0 on failure. Only write into `buffer` if
  // it is large enough to hold the data.
  size_t GetSfntTable(uint32_t table, pdfium::span<uint8_t> buffer);

  int GetGlyphCount() const;
  // TODO(crbug.com/42271048): Can this method be private?
  FX_RECT GetGlyphBBox() const;
  std::optional<FX_RECT> GetFontGlyphBBox(uint32_t glyph_index);
  std::unique_ptr<CFX_GlyphBitmap> RenderGlyph(const CFX_Font* font,
                                               uint32_t glyph_index,
                                               bool bFontStyle,
                                               const CFX_Matrix& matrix,
                                               int dest_width,
                                               FontAntiAliasingMode anti_alias);
  std::unique_ptr<CFX_Path> LoadGlyphPath(uint32_t glyph_index,
                                          int dest_width,
                                          bool is_vertical,
                                          const CFX_SubstFont* subst_font);
  int GetGlyphTTWidth() const;
  int GetGlyphWidth(uint32_t glyph_index,
                    int dest_width,
                    int weight,
                    const CFX_SubstFont* subst_font);
  ByteString GetGlyphName(uint32_t glyph_index);

  int GetCharIndex(uint32_t code);
  int GetNameIndex(const char* name);

  FX_RECT GetCharBBox(uint32_t code, int glyph_index);

  std::vector<CharCodeAndIndex> GetCharCodesAndIndices(char32_t max_char);

  CharMap GetCurrentCharMap() const;
  std::optional<fxge::FontEncoding> GetCurrentCharMapEncoding() const;
  CharMapId GetCharMapIdByIndex(size_t index) const;
  int GetCharMapPlatformIdByIndex(size_t index) const;
  int GetCharMapEncodingIdByIndex(size_t index) const;
  fxge::FontEncoding GetCharMapEncodingByIndex(size_t index) const;
  size_t GetCharMapCount() const;
  int LoadGlyph(uint32_t glyph_index, bool scale);
  ByteString GetPostscriptName();
  CFX_Size GetPixelSize() const;
  void SetCharMap(CharMap map);
  void SetCharMapByIndex(size_t index);
  bool SelectCharMap(fxge::FontEncoding encoding);

#if defined(PDF_ENABLE_XFA) || BUILDFLAG(IS_ANDROID)
  // Returns enum FontStyle values.
  uint32_t GetFontStyle();

  std::optional<std::array<uint32_t, 2>> GetOs2CodePageRange();
#endif

#if defined(PDF_ENABLE_XFA)
  bool IsScalable() const;
  int GetNumFaces() const;
  std::optional<std::array<uint32_t, 4>> GetOs2UnicodeRange();
#endif

#if BUILDFLAG(IS_WIN)
  bool CanEmbed();
#endif

  bool HasFaceRec() const { return !!GetRec(); }

 private:
  CFX_Face(FXFT_FaceRec* pRec, RetainPtr<Retainable> pDesc);
  ~CFX_Face() override;

  FXFT_FaceRec* GetRec() { return rec_.get(); }
  const FXFT_FaceRec* GetRec() const { return rec_.get(); }

  bool IsTricky() const;
  bool SetPixelSize(uint32_t width, uint32_t height);
  void AdjustVariationParams(int glyph_index, int dest_width, int weight);

  pdfium::span<const FT_CharMap> GetCharMaps() const;

#if BUILDFLAG(IS_ANDROID) || defined(PDF_ENABLE_XFA)
  std::optional<std::array<uint8_t, 2>> GetOs2Panose();
#endif

  // `owned_font_stream_` must outlive `rec_`.
  RetainPtr<CFX_ReadOnlyVectorStream> owned_font_stream_;
  ScopedFXFTFaceRec const rec_;
  RetainPtr<Retainable> const desc_;
};

#endif  // CORE_FXGE_CFX_FACE_H_
