// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FOLDERFONTINFO_H_
#define CORE_FXGE_CFX_FOLDERFONTINFO_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fxcrt/fx_codepage_forward.h"
#include "core/fxcrt/mask.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/systemfontinfo_iface.h"

class CFX_FolderFontInfo : public SystemFontInfoIface {
 public:
  CFX_FolderFontInfo();
  ~CFX_FolderFontInfo() override;

  void AddPath(const ByteString& path);

  // SystemFontInfoIface:
  void EnumFontList(CFX_FontMapper* pMapper) override;
  void* MapFont(int weight,
                bool bItalic,
                FX_Charset charset,
                int pitch_family,
                const ByteString& face) override;
  void* GetFont(const ByteString& face) override;
  size_t GetFontData(void* hFont,
                     uint32_t table,
                     pdfium::span<uint8_t> buffer) override;
  void DeleteFont(void* hFont) override;
  bool GetFaceName(void* hFont, ByteString* name) override;
  bool GetFontCharset(void* hFont, FX_Charset* charset) override;

 protected:
  friend class CFXFolderFontInfoTest;

  class FontFaceInfo {
   public:
    enum class CharsetFlag : uint8_t {
      kNone = 0,
      kAnsi = 1 << 0,
      kSymbol = 1 << 1,
      kShiftJis = 1 << 2,
      kBig5 = 1 << 3,
      kGb = 1 << 4,
      kKorean = 1 << 5,
    };

    static CharsetFlag GetCharset(FX_Charset charset);

    static constexpr int32_t kSimilarityScoreMax = 68;

    FontFaceInfo(ByteString filePath,
                 ByteString faceName,
                 ByteString fontTables,
                 uint32_t fontOffset,
                 uint32_t fileSize);

    bool IsEligibleForFindFont(CharsetFlag flag, FX_Charset charset) const;
    int32_t SimilarityScore(int weight,
                            bool italic,
                            int pitch_family,
                            bool exact_match_bonus) const;

    const ByteString file_path_;
    const ByteString face_name_;
    const ByteString font_tables_;
    const uint32_t font_offset_;
    const uint32_t file_size_;
    uint32_t styles_ = 0;
    Mask<CharsetFlag> charsets_;
  };

  void ScanPath(const ByteString& path);
  void ScanFile(const ByteString& path);
  void ReportFace(const ByteString& path,
                  FILE* pFile,
                  FX_FILESIZE filesize,
                  uint32_t offset);
  void* GetSubstFont(const ByteString& face);
  void* FindFont(int weight,
                 bool bItalic,
                 FX_Charset charset,
                 int pitch_family,
                 const ByteString& family,
                 bool bMatchName);

  std::map<ByteString, std::unique_ptr<FontFaceInfo>> font_list_;
  std::vector<ByteString> path_list_;
  UnownedPtr<CFX_FontMapper> mapper_;
};

#endif  // CORE_FXGE_CFX_FOLDERFONTINFO_H_
