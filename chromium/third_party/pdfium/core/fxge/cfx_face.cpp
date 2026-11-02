// Copyright 2019 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_face.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/fixed_size_data_vector.h"
#include "core/fxcrt/notreached.h"
#include "core/fxcrt/numerics/clamped_math.h"
#include "core/fxcrt/numerics/safe_conversions.h"
#include "core/fxcrt/numerics/safe_math.h"
#include "core/fxcrt/to_underlying.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_cttgsubtable.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/cfx_glyphbitmap.h"
#include "core/fxge/cfx_glyphcache.h"
#include "core/fxge/cfx_path.h"
#include "core/fxge/cfx_substfont.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/fx_dib.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/fx_fontencoding.h"

#if defined(PDF_ENABLE_XFA)
#include "core/fxge/cfx_cttnametable.h"
#endif  // defined(PDF_ENABLE_XFA)

#if defined(PDF_USE_SKIA)
#include "third_party/skia/include/core/SkTypeface.h"  // nogncheck
#endif

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#include "third_party/skia/include/core/SkFont.h"         // nogncheck
#include "third_party/skia/include/core/SkFontMetrics.h"  // nogncheck
#include "third_party/skia/include/core/SkFontTypes.h"    // nogncheck
#include "third_party/skia/include/core/SkPath.h"         // nogncheck
#include "third_party/skia/include/core/SkRect.h"         // nogncheck
#endif

#if defined(PDF_ENABLE_FONTATIONS)
#include "core/fxge/skrifa/src/main.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"
#endif

namespace {

struct OUTLINE_PARAMS {
  UnownedPtr<CFX_Path> path_;
  FT_Pos cur_x_;
  FT_Pos cur_y_;
};

constexpr float kCoordUnit = 64 * 64.0f;
constexpr int kThousandthMinInt = std::numeric_limits<int>::min() / 1000;
constexpr int kThousandthMaxInt = std::numeric_limits<int>::max() / 1000;

constexpr int kMaxGlyphDimension = 2048;

// Boundary value to avoid integer overflow when adding 1/64th of the value.
constexpr int kMaxRectTop = 2114445437;


int FTPosToCBoxInt(FT_Pos pos) {
  // Boundary values to avoid integer overflow when multiplied by 1000.
  static constexpr FT_Pos kMinCBox = -2147483;
  static constexpr FT_Pos kMaxCBox = 2147483;
  return static_cast<int>(std::clamp(pos, kMinCBox, kMaxCBox));
}

void Outline_CheckEmptyContour(OUTLINE_PARAMS* param) {
  size_t size;
  {
    pdfium::span<const CFX_Path::Point> points = param->path_->GetPoints();
    size = points.size();

    if (size >= 2 &&
        points[size - 2].IsTypeAndOpen(CFX_Path::Point::Type::kMove) &&
        points[size - 2].point_ == points[size - 1].point_) {
      size -= 2;
    }
    if (size >= 4 &&
        points[size - 4].IsTypeAndOpen(CFX_Path::Point::Type::kMove) &&
        points[size - 3].IsTypeAndOpen(CFX_Path::Point::Type::kBezier) &&
        points[size - 3].point_ == points[size - 4].point_ &&
        points[size - 2].point_ == points[size - 4].point_ &&
        points[size - 1].point_ == points[size - 4].point_) {
      size -= 4;
    }
  }
  // Only safe after |points| has been destroyed.
  param->path_->GetPoints().resize(size);
}

int Outline_MoveTo(const FT_Vector* to, void* user) {
  OUTLINE_PARAMS* param = static_cast<OUTLINE_PARAMS*>(user);

  Outline_CheckEmptyContour(param);

  param->path_->ClosePath();
  param->path_->AppendPoint(CFX_PointF(to->x / kCoordUnit, to->y / kCoordUnit),
                            CFX_Path::Point::Type::kMove);

  param->cur_x_ = to->x;
  param->cur_y_ = to->y;
  return 0;
}

int Outline_LineTo(const FT_Vector* to, void* user) {
  OUTLINE_PARAMS* param = static_cast<OUTLINE_PARAMS*>(user);

  param->path_->AppendPoint(CFX_PointF(to->x / kCoordUnit, to->y / kCoordUnit),
                            CFX_Path::Point::Type::kLine);

  param->cur_x_ = to->x;
  param->cur_y_ = to->y;
  return 0;
}

int Outline_ConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
  OUTLINE_PARAMS* param = static_cast<OUTLINE_PARAMS*>(user);

  param->path_->AppendPoint(
      CFX_PointF(
          (param->cur_x_ + (control->x - param->cur_x_) * 2 / 3) / kCoordUnit,
          (param->cur_y_ + (control->y - param->cur_y_) * 2 / 3) / kCoordUnit),
      CFX_Path::Point::Type::kBezier);

  param->path_->AppendPoint(
      CFX_PointF((control->x + (to->x - control->x) / 3) / kCoordUnit,
                 (control->y + (to->y - control->y) / 3) / kCoordUnit),
      CFX_Path::Point::Type::kBezier);

  param->path_->AppendPoint(CFX_PointF(to->x / kCoordUnit, to->y / kCoordUnit),
                            CFX_Path::Point::Type::kBezier);

  param->cur_x_ = to->x;
  param->cur_y_ = to->y;
  return 0;
}

int Outline_CubicTo(const FT_Vector* control1,
                    const FT_Vector* control2,
                    const FT_Vector* to,
                    void* user) {
  OUTLINE_PARAMS* param = static_cast<OUTLINE_PARAMS*>(user);

  param->path_->AppendPoint(
      CFX_PointF(control1->x / kCoordUnit, control1->y / kCoordUnit),
      CFX_Path::Point::Type::kBezier);

  param->path_->AppendPoint(
      CFX_PointF(control2->x / kCoordUnit, control2->y / kCoordUnit),
      CFX_Path::Point::Type::kBezier);

  param->path_->AppendPoint(CFX_PointF(to->x / kCoordUnit, to->y / kCoordUnit),
                            CFX_Path::Point::Type::kBezier);

  param->cur_x_ = to->x;
  param->cur_y_ = to->y;
  return 0;
}

#if defined(PDF_ENABLE_FONTATIONS)
std::unique_ptr<CFX_Path> ConvertOutline(const skrifa::Outline& outline) {
  auto skrifa_path = std::make_unique<CFX_Path>();
  auto point_idx = 0;
  CFX_PointF current_point(0, 0);
  for (auto verb : outline.verbs) {
    switch (verb) {
      case skrifa::PathVerb::MoveTo: {
        auto p = outline.points[point_idx++];
        current_point = CFX_PointF(p.x, p.y);
        skrifa_path->AppendPoint(current_point, CFX_Path::Point::Type::kMove);
        break;
      }
      case skrifa::PathVerb::LineTo: {
        auto p = outline.points[point_idx++];
        current_point = CFX_PointF(p.x, p.y);
        skrifa_path->AppendPoint(current_point, CFX_Path::Point::Type::kLine);
        break;
      }
      case skrifa::PathVerb::QuadTo: {
        auto c0 = outline.points[point_idx++];
        auto p = outline.points[point_idx++];
        // Convert quadratic to cubic bezier to match FreeType
        // decomposition.
        skrifa_path->AppendPoint(
            CFX_PointF(current_point.x + (c0.x - current_point.x) * 2 / 3,
                       current_point.y + (c0.y - current_point.y) * 2 / 3),
            CFX_Path::Point::Type::kBezier);
        skrifa_path->AppendPoint(
            CFX_PointF(c0.x + (p.x - c0.x) / 3, c0.y + (p.y - c0.y) / 3),
            CFX_Path::Point::Type::kBezier);
        current_point = CFX_PointF(p.x, p.y);
        skrifa_path->AppendPoint(current_point, CFX_Path::Point::Type::kBezier);
        break;
      }
      case skrifa::PathVerb::CurveTo: {
        auto c0 = outline.points[point_idx++];
        auto c1 = outline.points[point_idx++];
        auto p = outline.points[point_idx++];
        skrifa_path->AppendPoint(CFX_PointF(c0.x, c0.y),
                                 CFX_Path::Point::Type::kBezier);
        skrifa_path->AppendPoint(CFX_PointF(c1.x, c1.y),
                                 CFX_Path::Point::Type::kBezier);
        current_point = CFX_PointF(p.x, p.y);
        skrifa_path->AppendPoint(current_point, CFX_Path::Point::Type::kBezier);
        break;
      }
      case skrifa::PathVerb::Close:
        skrifa_path->ClosePath();
        break;
    }
  }
  return skrifa_path;
}
#endif  // defined(PDF_ENABLE_FONTATIONS)

FT_Encoding ToFTEncoding(fxge::FontEncoding encoding) {
  switch (encoding) {
    case fxge::FontEncoding::kAdobeCustom:
      return FT_ENCODING_ADOBE_CUSTOM;
    case fxge::FontEncoding::kAdobeExpert:
      return FT_ENCODING_ADOBE_EXPERT;
    case fxge::FontEncoding::kAdobeStandard:
      return FT_ENCODING_ADOBE_STANDARD;
    case fxge::FontEncoding::kAppleRoman:
      return FT_ENCODING_APPLE_ROMAN;
    case fxge::FontEncoding::kBig5:
      return FT_ENCODING_BIG5;
    case fxge::FontEncoding::kGB2312:
      return FT_ENCODING_PRC;
    case fxge::FontEncoding::kJohab:
      return FT_ENCODING_JOHAB;
    case fxge::FontEncoding::kLatin1:
      return FT_ENCODING_ADOBE_LATIN_1;
    case fxge::FontEncoding::kNone:
      return FT_ENCODING_NONE;
    case fxge::FontEncoding::kOldLatin2:
      return FT_ENCODING_OLD_LATIN_2;
    case fxge::FontEncoding::kSjis:
      return FT_ENCODING_SJIS;
    case fxge::FontEncoding::kSymbol:
      return FT_ENCODING_MS_SYMBOL;
    case fxge::FontEncoding::kUnicode:
      return FT_ENCODING_UNICODE;
    case fxge::FontEncoding::kWansung:
      return FT_ENCODING_WANSUNG;
  }
}

fxge::FontEncoding ToFontEncoding(uint32_t ft_encoding) {
  switch (ft_encoding) {
    case FT_ENCODING_ADOBE_CUSTOM:
      return fxge::FontEncoding::kAdobeCustom;
    case FT_ENCODING_ADOBE_EXPERT:
      return fxge::FontEncoding::kAdobeExpert;
    case FT_ENCODING_ADOBE_STANDARD:
      return fxge::FontEncoding::kAdobeStandard;
    case FT_ENCODING_APPLE_ROMAN:
      return fxge::FontEncoding::kAppleRoman;
    case FT_ENCODING_BIG5:
      return fxge::FontEncoding::kBig5;
    case FT_ENCODING_PRC:
      return fxge::FontEncoding::kGB2312;
    case FT_ENCODING_JOHAB:
      return fxge::FontEncoding::kJohab;
    case FT_ENCODING_ADOBE_LATIN_1:
      return fxge::FontEncoding::kLatin1;
    case FT_ENCODING_NONE:
      return fxge::FontEncoding::kNone;
    case FT_ENCODING_OLD_LATIN_2:
      return fxge::FontEncoding::kOldLatin2;
    case FT_ENCODING_SJIS:
      return fxge::FontEncoding::kSjis;
    case FT_ENCODING_MS_SYMBOL:
      return fxge::FontEncoding::kSymbol;
    case FT_ENCODING_UNICODE:
      return fxge::FontEncoding::kUnicode;
    case FT_ENCODING_WANSUNG:
      return fxge::FontEncoding::kWansung;
  }
  NOTREACHED();
}

FX_RECT FXRectFromFTPos(FT_Pos left, FT_Pos top, FT_Pos right, FT_Pos bottom) {
  return FX_RECT(pdfium::checked_cast<int32_t>(left),
                 pdfium::checked_cast<int32_t>(top),
                 pdfium::checked_cast<int32_t>(right),
                 pdfium::checked_cast<int32_t>(bottom));
}

FX_RECT ScaledFXRectFromFTPos(FT_Pos left,
                              FT_Pos top,
                              FT_Pos right,
                              FT_Pos bottom,
                              int x_scale,
                              int y_scale) {
  if (x_scale == 0 || y_scale == 0) {
    return FXRectFromFTPos(left, top, right, bottom);
  }

  return FXRectFromFTPos(left * 1000 / x_scale, top * 1000 / y_scale,
                         right * 1000 / x_scale, bottom * 1000 / y_scale);
}

FT_Render_Mode FtRenderModeFromFontAntiAliasingMode(
    FontAntiAliasingMode anti_alias) {
  switch (anti_alias) {
    case FontAntiAliasingMode::kNormal:
      return FT_RENDER_MODE_NORMAL;
    case FontAntiAliasingMode::kMono:
      return FT_RENDER_MODE_MONO;
    case FontAntiAliasingMode::kLcd:
      return FT_RENDER_MODE_LCD;
  }
  NOTREACHED();
}

// Sets the given transform on the FaceRec, and resets it to the identity when
// it goes out of scope.
class ScopedFaceTransform {
 public:
  FX_STACK_ALLOCATED();

  ScopedFaceTransform(FT_FaceRec* rec, FT_Matrix* matrix) : rec_(rec) {
    FT_Set_Transform(rec_, matrix, nullptr);
  }

  ~ScopedFaceTransform() {
    FT_Matrix matrix = {0x10000L, 0L, 0L, 0x10000L};
    FT_Set_Transform(rec_, &matrix, nullptr);
  }

 private:
  UnownedPtr<FT_FaceRec> const rec_;
};

}  // namespace

#if defined(PDF_ENABLE_FONTATIONS)
struct SkrifaFontHolder {
  explicit SkrifaFontHolder(rust::Box<skrifa::SkrifaFont> f)
      : font(std::move(f)) {}
  rust::Box<skrifa::SkrifaFont> font;
};
#endif  // defined(PDF_ENABLE_FONTATIONS)

// static
RetainPtr<CFX_Face> CFX_Face::New(RetainPtr<Retainable> cache_entry,
                                  RetainPtr<CFX_ReadOnlySpanStream> font_stream,
                                  uint32_t face_index) {
  CFX_FontMgr* font_mgr = CFX_GEModule::Get()->GetFontMgr();
  pdfium::span<const uint8_t> data = font_stream->span();
  FT_FaceRec* face_rec = nullptr;
  if (FT_New_Memory_Face(font_mgr->GetFTLibrary(), data.data(),
                         pdfium::checked_cast<FT_Long>(data.size()),
                         pdfium::checked_cast<FT_Long>(face_index),
                         &face_rec) != 0) {
    return nullptr;
  }
  if (FT_Set_Pixel_Sizes(face_rec, 64, 64) != 0) {
    return nullptr;
  }

  std::unique_ptr<SkrifaFontHolder> skrifa_font;
#if defined(PDF_ENABLE_FONTATIONS)
  auto raw_font = skrifa::new_font(rust::Slice(data), face_index);
  if (raw_font->is_ok()) {
    skrifa_font = std::make_unique<SkrifaFontHolder>(std::move(raw_font));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)

  // Private ctor.
  RetainPtr<CFX_Face> result = pdfium::WrapRetain(
      new CFX_Face(std::move(cache_entry), std::move(font_stream), face_rec,
                   std::move(skrifa_font)));

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  result->GetOrCreateSkTypeface();
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return result;
}

bool CFX_Face::HasGlyphNames() const {
  const bool ft_result = !!(GetRec()->face_flags & FT_FACE_FLAG_GLYPH_NAMES);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, skrifa_font_->font->has_glyph_names());
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

bool CFX_Face::IsTtOt() const {
  const bool ft_result = !!(GetRec()->face_flags & FT_FACE_FLAG_SFNT);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result, skia_typeface_->countTables() > 0);
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

ByteString CFX_Face::GetFontFormat() {
  const ByteString ft_result(FT_Get_Font_Format(GetRec()));
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    const char* skrifa_format;
    switch (skrifa_font_->font->font_type()) {
      case skrifa::FaceFormat::TrueType:
        skrifa_format = "TrueType";
        break;
      case skrifa::FaceFormat::Type1:
        skrifa_format = "Type 1";
        break;
      case skrifa::FaceFormat::Cff:
        skrifa_format = "CFF";
        break;
      default:
        skrifa_format = "";
        break;
    }
    CHECK_EQ(ft_result, ByteString(skrifa_format));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

bool CFX_Face::IsTricky() const {
  const bool ft_result = !!(GetRec()->face_flags & FT_FACE_FLAG_TRICKY);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, skrifa_font_->font->is_tricky());
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

bool CFX_Face::IsFixedWidth() const {
  const bool ft_result = !!(GetRec()->face_flags & FT_FACE_FLAG_FIXED_WIDTH);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result, skia_typeface_->isFixedPitch());
  }
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, skrifa_font_->font->is_fixed_pitch());
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

#if defined(PDF_ENABLE_XFA)
bool CFX_Face::IsScalable() const {
  const bool ft_result = !!(GetRec()->face_flags & FT_FACE_FLAG_SCALABLE);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, skrifa_font_->font->is_scalable());
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}
#endif  // defined(PDF_ENABLE_XFA)

bool CFX_Face::IsItalic() const {
  const bool ft_result = !!(GetRec()->style_flags & FT_STYLE_FLAG_ITALIC);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result, skia_typeface_->isItalic());
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

bool CFX_Face::IsBold() const {
  const bool ft_result = !!(GetRec()->style_flags & FT_STYLE_FLAG_BOLD);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result, skia_typeface_->isBold());
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

ByteString CFX_Face::GetFamilyName() const {
  ByteString ft_result(GetRec()->family_name);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    rust::Str skrifa_result = skrifa_font_->font->family_name();
    CHECK_EQ(ft_result.IsEmpty(), skrifa_result.empty());
    if (!ft_result.IsEmpty() && !skrifa_result.empty()) {
      ByteString skrifa_bs = ByteString(ByteStringView(skrifa_result));
      ByteString ft_clean = ft_result;
      ByteString skrifa_clean = skrifa_bs;
      MaybeRemoveSubsettedFontPrefix(ft_clean);
      MaybeRemoveSubsettedFontPrefix(skrifa_clean);
      CHECK_EQ(ft_clean, skrifa_clean);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkString name;
    skia_typeface_->getFamilyName(&name);
    CHECK_EQ(ft_result, ByteString(name.c_str()));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

ByteString CFX_Face::GetStyleName() const {
  ByteString ft_result(GetRec()->style_name);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    rust::String skrifa_result = skrifa_font_->font->style_name();
    CHECK_EQ(ft_result.IsEmpty(), skrifa_result.empty());
    if (!ft_result.IsEmpty() && !skrifa_result.empty()) {
      CHECK_EQ(ft_result, ByteString(skrifa_result.c_str()));
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

FX_RECT CFX_Face::GetBBox() const {
  const FX_RECT ft_result =
      FX_RECT(pdfium::checked_cast<int32_t>(GetRec()->bbox.xMin),
              pdfium::checked_cast<int32_t>(GetRec()->bbox.yMin),
              pdfium::checked_cast<int32_t>(GetRec()->bbox.xMax),
              pdfium::checked_cast<int32_t>(GetRec()->bbox.yMax));
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, GetUnitsPerEm());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    CHECK_EQ(ft_result.left, static_cast<int32_t>(metrics.fXMin));
    CHECK_EQ(ft_result.bottom, -static_cast<int32_t>(metrics.fTop));
    CHECK_EQ(ft_result.right, static_cast<int32_t>(metrics.fXMax));
    CHECK_EQ(ft_result.top, -static_cast<int32_t>(metrics.fBottom));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

uint16_t CFX_Face::GetUnitsPerEm() const {
  const uint16_t ft_result =
      pdfium::checked_cast<uint16_t>(GetRec()->units_per_EM);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, pdfium::checked_cast<uint16_t>(
                            skrifa_font_->font->units_per_em()));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result,
             pdfium::checked_cast<uint16_t>(skia_typeface_->getUnitsPerEm()));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

int CFX_Face::EmAdjust(int value) const {
  return GetUnitsPerEm() == 0 ? value : value * 1000 / GetUnitsPerEm();
}

int16_t CFX_Face::GetAscender() const {
  const int16_t ft_result = pdfium::checked_cast<int16_t>(GetRec()->ascender);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result,
             static_cast<int16_t>(std::round(skrifa_font_->font->ascent())));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, GetUnitsPerEm());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    // Freetype ascender is often exactly -metrics.fAscent.
    CHECK_EQ(ft_result, static_cast<int16_t>(-metrics.fAscent));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

int16_t CFX_Face::GetDescender() const {
  const int16_t ft_result = pdfium::checked_cast<int16_t>(GetRec()->descender);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result,
             static_cast<int16_t>(std::round(skrifa_font_->font->descent())));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, GetUnitsPerEm());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    // Freetype descender is often exactly -metrics.fDescent.
    CHECK_EQ(ft_result, static_cast<int16_t>(-metrics.fDescent));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

pdfium::span<const uint8_t> CFX_Face::GetData() const {
  return font_stream_->span();
}

size_t CFX_Face::GetSfntTable(uint32_t table, pdfium::span<uint8_t> buffer) {
  size_t ft_result = 0;
  unsigned long length = pdfium::checked_cast<unsigned long>(buffer.size());
  if (length) {
    int error = FT_Load_Sfnt_Table(GetRec(), table, 0, buffer.data(), &length);
    if (!error && length == buffer.size()) {
      ft_result = buffer.size();
    }
  } else {
    int error = FT_Load_Sfnt_Table(GetRec(), table, 0, nullptr, &length);
    if (!error && length) {
      ft_result = pdfium::checked_cast<size_t>(length);
    }
  }

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    if (buffer.empty()) {
      CHECK_EQ(ft_result, skia_typeface_->getTableSize(table));
    } else {
      std::vector<uint8_t> skia_buffer(buffer.size());
      size_t skia_result = skia_typeface_->getTableData(
          table, 0, skia_buffer.size(), skia_buffer.data());
      CHECK_EQ(ft_result, skia_result);
      if (ft_result > 0) {
        CHECK(std::equal(buffer.begin(), buffer.end(), skia_buffer.begin()));
      }
    }
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

std::unique_ptr<CFX_CTTGSUBTable> CFX_Face::ParseGSUBTable() {
  static constexpr uint32_t kGsubTag =
      CFX_FontMapper::MakeTag('G', 'S', 'U', 'B');
  size_t length = GetSfntTable(kGsubTag, {});
  if (!length) {
    return nullptr;
  }
  auto sub_data = FixedSizeDataVector<uint8_t>::Uninit(length);
  if (!GetSfntTable(kGsubTag, sub_data.span())) {
    return nullptr;
  }
  // CFX_CTTGSUBTable parses the data and stores all the values in its structs.
  // It does not store pointers into `sub_data`.
  return std::make_unique<CFX_CTTGSUBTable>(sub_data.span());
}

#if defined(PDF_ENABLE_XFA)
std::unique_ptr<CFX_CTTNameTable> CFX_Face::ParseNameTable() {
  static constexpr uint32_t kNameTag =
      CFX_FontMapper::MakeTag('n', 'a', 'm', 'e');
  size_t length = GetSfntTable(kNameTag, {});
  if (!length) {
    return nullptr;
  }
  auto name_data = FixedSizeDataVector<uint8_t>::Uninit(length);
  if (!GetSfntTable(kNameTag, name_data.span())) {
    return nullptr;
  }
  return std::make_unique<CFX_CTTNameTable>(name_data.span());
}

std::optional<std::array<uint32_t, 4>> CFX_Face::GetOs2UnicodeRange() {
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(GetRec(), FT_SFNT_OS2));
  std::optional<std::array<uint32_t, 4>> ft_result;
  if (os2) {
    ft_result =
        std::array<uint32_t, 4>{static_cast<uint32_t>(os2->ulUnicodeRange1),
                                static_cast<uint32_t>(os2->ulUnicodeRange2),
                                static_cast<uint32_t>(os2->ulUnicodeRange3),
                                static_cast<uint32_t>(os2->ulUnicodeRange4)};
  }

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  std::optional<std::array<uint32_t, 4>> skrifa_result;
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    skrifa::UnicodeRange range;
    if (skrifa_font_->font->get_os2_unicode_range(range)) {
      skrifa_result = std::array<uint32_t, 4>{range.range1, range.range2,
                                              range.range3, range.range4};
    }
  }
  CHECK_EQ(ft_result.has_value(), skrifa_result.has_value());
  if (ft_result.has_value() && skrifa_result.has_value()) {
    for (size_t i = 0; i < 4; ++i) {
      CHECK_EQ((*ft_result)[i], (*skrifa_result)[i]);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}
#endif  // defined(PDF_ENABLE_XFA)

#if defined(PDF_ENABLE_XFA) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
std::optional<std::array<uint32_t, 2>> CFX_Face::GetOs2CodePageRange() {
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(GetRec(), FT_SFNT_OS2));
  std::optional<std::array<uint32_t, 2>> ft_result;
  if (os2) {
    ft_result =
        std::array<uint32_t, 2>{static_cast<uint32_t>(os2->ulCodePageRange1),
                                static_cast<uint32_t>(os2->ulCodePageRange2)};
  }

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  std::optional<std::array<uint32_t, 2>> skrifa_result;
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    skrifa::CodePageRange range;
    if (skrifa_font_->font->get_os2_code_page_range(range)) {
      skrifa_result = std::array<uint32_t, 2>{range.range1, range.range2};
    }
  }

  CHECK_EQ(ft_result.has_value(), skrifa_result.has_value());
  if (ft_result.has_value() && skrifa_result.has_value()) {
    CHECK_EQ((*ft_result)[0], (*skrifa_result)[0]);
    CHECK_EQ((*ft_result)[1], (*skrifa_result)[1]);
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}

std::optional<std::array<uint8_t, 2>> CFX_Face::GetOs2Panose() {
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(GetRec(), FT_SFNT_OS2));
  std::optional<std::array<uint8_t, 2>> ft_result;
  if (os2) {
    ft_result = std::array<uint8_t, 2>{os2->panose[0], os2->panose[1]};
  }

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  std::optional<std::array<uint8_t, 2>> skrifa_result;
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    skrifa::Os2Panose panose;
    if (skrifa_font_->font->get_os2_panose(panose)) {
      skrifa_result = std::array<uint8_t, 2>{panose.b0, panose.b1};
    }
  }

  CHECK_EQ(ft_result.has_value(), skrifa_result.has_value());
  if (ft_result.has_value() && skrifa_result.has_value()) {
    CHECK_EQ((*ft_result)[0], (*skrifa_result)[0]);
    CHECK_EQ((*ft_result)[1], (*skrifa_result)[1]);
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}
#endif  // defined(PDF_ENABLE_XFA) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_LINUX)

int CFX_Face::GetGlyphCount() const {
  const int ft_result = pdfium::checked_cast<int>(GetRec()->num_glyphs);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    CHECK_EQ(ft_result, static_cast<int>(skrifa_font_->font->num_glyphs()));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    CHECK_EQ(ft_result, skia_typeface_->countGlyphs());
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

std::unique_ptr<CFX_GlyphBitmap> CFX_Face::RenderGlyph(
    uint32_t glyph_index,
    bool is_cid_font,
    bool is_vertical,
    const CFX_Matrix& matrix,
    int dest_width,
    FontAntiAliasingMode anti_alias,
    const CFX_SubstFont* subst_font) {
  // TODO(https://crbug.com/42271123): Implement glyph rendering in
  // Skia/Fontations.
  FT_Matrix ft_matrix;
  ft_matrix.xx = matrix.a / 64 * 65536;
  ft_matrix.xy = matrix.c / 64 * 65536;
  ft_matrix.yx = matrix.b / 64 * 65536;
  ft_matrix.yy = matrix.d / 64 * 65536;
  if (subst_font) {
    int skew = subst_font->GetEffectiveSkew(is_cid_font);
    if (skew) {
      if (is_vertical) {
        ft_matrix.yx += ft_matrix.yy * skew / 100;
      } else {
        ft_matrix.xy -= ft_matrix.xx * skew / 100;
      }
    }
    if (subst_font->IsBuiltInGenericFont()) {
      AdjustVariationParams(glyph_index, dest_width, subst_font->GetWeight());
    }
  }

  ScopedFaceTransform scoped_transform(GetRec(), &ft_matrix);
  int load_flags = FT_LOAD_NO_BITMAP | FT_LOAD_PEDANTIC;
  if (!IsTtOt()) {
    load_flags |= FT_LOAD_NO_HINTING;
  }
  FT_FaceRec* rec = GetRec();
  int error = FT_Load_Glyph(rec, glyph_index, load_flags);
  if (error) {
    // if an error is returned, try to reload glyphs without hinting.
    if (load_flags & FT_LOAD_NO_HINTING) {
      return nullptr;
    }

    load_flags |= FT_LOAD_NO_HINTING;
    load_flags &= ~FT_LOAD_PEDANTIC;
    error = FT_Load_Glyph(rec, glyph_index, load_flags);
    if (error) {
      return nullptr;
    }
  }

  auto* glyph = rec->glyph;
  if (subst_font) {
    int level = subst_font->GetEmboldenLevelForRender(
        is_cid_font, static_cast<int32_t>(ft_matrix.xx),
        static_cast<int32_t>(ft_matrix.xy));
    if (level < 0) {
      return nullptr;
    }
    if (level > 0) {
      FT_Outline_Embolden(&glyph->outline, level);
    }
  }
  CFX_FontMgr* font_mgr = CFX_GEModule::Get()->GetFontMgr();
  FT_Library_SetLcdFilter(font_mgr->GetFTLibrary(), FT_LCD_FILTER_DEFAULT);
  error =
      FT_Render_Glyph(glyph, FtRenderModeFromFontAntiAliasingMode(anti_alias));
  if (error) {
    return nullptr;
  }

  const FT_Bitmap& ft_bitmap = glyph->bitmap;
  if (ft_bitmap.width > kMaxGlyphDimension ||
      ft_bitmap.rows > kMaxGlyphDimension) {
    return nullptr;
  }
  int dib_width = ft_bitmap.width;
  const FXDIB_Format format = anti_alias == FontAntiAliasingMode::kMono
                                  ? FXDIB_Format::k1bppMask
                                  : FXDIB_Format::k8bppMask;
  RetainPtr<CFX_DIBitmap> new_bitmap = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!new_bitmap->Create(dib_width, ft_bitmap.rows, format)) {
    return nullptr;
  }
  auto pGlyphBitmap = std::make_unique<CFX_GlyphBitmap>(
      glyph->bitmap_left, glyph->bitmap_top, new_bitmap);

  const uint32_t src_pitch = abs(ft_bitmap.pitch);
  pdfium::span<const uint8_t> src_span =
      UNSAFE_TODO(pdfium::span<const uint8_t>(ft_bitmap.buffer,
                                              src_pitch * ft_bitmap.rows));

  if (anti_alias != FontAntiAliasingMode::kMono &&
      ft_bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
    new_bitmap->Populate8bbpMaskFrom1bppSpan(src_span, src_pitch);
  } else {
    new_bitmap->PopulateFromSpan(src_span, src_pitch);
  }
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  // TODO(https://crbug.com/42271123): Compute equivalent result via Skia or
  // Skrifa.
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return pGlyphBitmap;
}

std::unique_ptr<CFX_Path> CFX_Face::LoadGlyphPath(
    uint32_t glyph_index,
    int dest_width,
    bool is_vertical,
    const CFX_SubstFont* subst_font) {
#if defined(PDF_ENABLE_FONTATIONS)
  if (CFX_GEModule::Get()->GetFontMgr()->GetFontBackend() ==
      CFX_FontMgr::FontBackend::kFontations) {
    if (skrifa_font_ && skrifa_font_->font->is_ok()) {
      skrifa::Outline outline;
      if (skrifa_font_->font->unscaled_outline(glyph_index, outline)) {
        return ConvertOutline(outline);
      }
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  FT_FaceRec* rec = GetRec();
  FT_Set_Pixel_Sizes(rec, 0, 64);
  FT_Matrix ft_matrix = {65536, 0, 0, 65536};
  if (subst_font) {
    int skew = subst_font->GetSkew();
    if (skew) {
      if (is_vertical) {
        ft_matrix.yx += ft_matrix.yy * skew / 100;
      } else {
        ft_matrix.xy -= ft_matrix.xx * skew / 100;
      }
    }
    if (subst_font->IsBuiltInGenericFont()) {
      AdjustVariationParams(glyph_index, dest_width, subst_font->GetWeight());
    }
  }
  ScopedFaceTransform scoped_transform(GetRec(), &ft_matrix);
  int load_flags = FT_LOAD_NO_BITMAP;
  if (!IsTtOt() || !IsTricky()) {
    load_flags |= FT_LOAD_NO_HINTING;
  }
  if (FT_Load_Glyph(rec, glyph_index, load_flags)) {
    return nullptr;
  }
  if (subst_font) {
    int level = subst_font->GetEmboldenLevelForLoad();
    if (level > 0) {
      FT_Outline_Embolden(&rec->glyph->outline, level);
    }
  }

  FT_Outline_Funcs funcs;
  funcs.move_to = Outline_MoveTo;
  funcs.line_to = Outline_LineTo;
  funcs.conic_to = Outline_ConicTo;
  funcs.cubic_to = Outline_CubicTo;
  funcs.shift = 0;
  funcs.delta = 0;

  auto pPath = std::make_unique<CFX_Path>();
  OUTLINE_PARAMS params = {
      .path_ = pPath.get(),
      .cur_x_ = 0,
      .cur_y_ = 0,
  };

  FT_Outline_Decompose(&rec->glyph->outline, &funcs, &params);
  if (pPath->GetPoints().empty()) {
    return nullptr;
  }

  Outline_CheckEmptyContour(&params);
  pPath->ClosePath();

  return pPath;
}

int CFX_Face::GetGlyphTTWidth(uint32_t glyph_index) const {
  const auto* fontglyph = GetRec()->glyph;
  DCHECK_EQ(glyph_index, fontglyph->glyph_index);

  const int ft_result =
      NormalizeFontMetric(fontglyph->metrics.horiAdvance, GetUnitsPerEm());
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    skrifa::Outline outline;
    if (skrifa_font_->font->unscaled_outline(glyph_index, outline)) {
      const int skrifa_result = NormalizeFontMetric(
          static_cast<int64_t>(outline.advance_width + 0.5), GetUnitsPerEm());
      CHECK_EQ(ft_result, skrifa_result);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, GetUnitsPerEm());
    font.setHinting(SkFontHinting::kNone);
    uint16_t skia_glyph_index = static_cast<uint16_t>(glyph_index);
    SkScalar width;
    font.getWidths(pdfium::span_from_ref(skia_glyph_index),
                   pdfium::span_from_ref(width));
    const int sk_result =
        NormalizeFontMetric(static_cast<int64_t>(width + 0.5), GetUnitsPerEm());
    CHECK_EQ(ft_result, sk_result);
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

int CFX_Face::GetGlyphWidth(uint32_t glyph_index,
                            int dest_width,
                            int weight,
                            const CFX_SubstFont* subst_font) {
  if (subst_font && subst_font->IsBuiltInGenericFont()) {
    AdjustVariationParams(glyph_index, dest_width, weight);
  }

  FT_FaceRec* rec = GetRec();
  int err = FT_Load_Glyph(
      rec, glyph_index, FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
  if (err) {
    return 0;
  }

  FT_Pos horizontal_advance = rec->glyph->metrics.horiAdvance;
  if (horizontal_advance < kThousandthMinInt ||
      horizontal_advance > kThousandthMaxInt) {
    return 0;
  }

  const int ft_result = EmAdjust(static_cast<int>(horizontal_advance));
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, GetUnitsPerEm());
    font.setHinting(SkFontHinting::kNone);
    uint16_t skia_glyph_index = static_cast<uint16_t>(glyph_index);
    SkScalar width;
    font.getWidths(pdfium::span_from_ref(skia_glyph_index),
                   pdfium::span_from_ref(width));
    const int sk_result = EmAdjust(static_cast<int>(width + 0.5));
    CHECK_EQ(ft_result, sk_result);
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

ByteString CFX_Face::GetGlyphName(uint32_t glyph_index) {
  char name[256] = {};
  FT_Get_Glyph_Name(GetRec(), glyph_index, name, sizeof(name));
  name[255] = 0;
  ByteString ft_result(name);

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    rust::String skrifa_result = skrifa_font_->font->glyph_name(glyph_index);
    CHECK_EQ(ft_result.IsEmpty(), skrifa_result.empty());
    if (!ft_result.IsEmpty() && !skrifa_result.empty()) {
      CHECK_EQ(ft_result, ByteString(skrifa_result.c_str()));
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}

int CFX_Face::GetCharIndex(uint32_t code) {
#if defined(PDF_ENABLE_FONTATIONS)
  if (CFX_GEModule::Get()->GetFontMgr()->GetFontBackend() ==
      CFX_FontMgr::FontBackend::kFontations) {
    if (skrifa_font_ && skrifa_font_->font->is_ok()) {
      if (skrifa_font_->font->font_type() == skrifa::FaceFormat::Type1) {
        if (code <= 0xFF) {
          return skrifa_font_->font->code_to_gid(static_cast<uint8_t>(code));
        }
      }
      return skrifa_font_->font->unicode_to_gid(code);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  const int ft_result = FT_Get_Char_Index(GetRec(), code);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    int skrifa_result = -1;
    FT_CharMap charmap = GetRec()->charmap;
    if (charmap) {
      if (charmap->encoding == FT_ENCODING_UNICODE) {
        skrifa_result =
            static_cast<int>(skrifa_font_->font->unicode_to_gid(code));
      } else if (skrifa_font_->font->font_type() == skrifa::FaceFormat::Type1 &&
                 code <= 0xFF) {
        if (charmap->encoding == FT_ENCODING_ADOBE_CUSTOM ||
            charmap->encoding == FT_ENCODING_ADOBE_STANDARD ||
            charmap->encoding == FT_ENCODING_ADOBE_EXPERT ||
            charmap->encoding == FT_ENCODING_APPLE_ROMAN) {
          skrifa_result = static_cast<int>(
              skrifa_font_->font->code_to_gid(static_cast<uint8_t>(code)));
        }
      }
    }
    if (skrifa_result != -1) {
      CHECK_EQ(ft_result, skrifa_result);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    FT_CharMap charmap = GetRec()->charmap;
    if (charmap && charmap->encoding == FT_ENCODING_UNICODE) {
      CHECK_EQ(static_cast<uint16_t>(ft_result),
               skia_typeface_->unicharToGlyph(code));
    }
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

int CFX_Face::GetNameIndex(const char* name) {
  int ft_result = FT_Get_Name_Index(GetRec(), name);

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    uint32_t skrifa_result = skrifa_font_->font->name_index(name);
    CHECK_EQ(ft_result, static_cast<int>(skrifa_result));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}

int CFX_Face::LoadGlyph(uint32_t glyph_index, bool scale) {
  FT_Int32 args = FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
  if (!scale) {
    args |= FT_LOAD_NO_SCALE;
  }
  const int ft_result = FT_Load_Glyph(GetRec(), glyph_index, args);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    bool skia_valid =
        glyph_index < static_cast<uint32_t>(skia_typeface_->countGlyphs());
    if (ft_result == 0) {
      CHECK(skia_valid);
    }
  }
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    bool skrifa_result = skrifa_font_->font->has_outline(glyph_index);
    CHECK_EQ(ft_result == 0, skrifa_result);
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

ByteString CFX_Face::GetPostscriptName() {
  const char* ft_result = FT_Get_Postscript_Name(GetRec());
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    rust::Str skrifa_result = skrifa_font_->font->postscript_name();
    CHECK_EQ(!ft_result, skrifa_result.empty());
    if (ft_result && !skrifa_result.empty()) {
      CHECK_EQ(ByteString(ft_result),
               UNSAFE_BUFFERS(
                   ByteString(skrifa_result.data(), skrifa_result.size())));
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkString name;
    if (skia_typeface_->getPostScriptName(&name)) {
      CHECK_EQ(ByteString(ft_result), ByteString(name.c_str()));
    } else {
      CHECK(!ft_result);
    }
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ByteString(ft_result);
}

CFX_Size CFX_Face::GetPixelSize() const {
  int pixel_size_x = GetRec()->size->metrics.x_ppem;
  int pixel_size_y = GetRec()->size->metrics.y_ppem;
  return {pixel_size_x, pixel_size_y};
}

std::optional<FX_RECT> CFX_Face::GetFontGlyphBBox(uint32_t glyph_index) {
  if (IsTricky()) {
    int error = FT_Set_Char_Size(GetRec(), 0, 1000 * 64, 72, 72);
    if (error) {
      return std::nullopt;
    }

    error = LoadGlyph(glyph_index, /*scale=*/true);
    if (error) {
      return std::nullopt;
    }

    FT_Glyph glyph;
    error = FT_Get_Glyph(GetRec()->glyph, &glyph);
    if (error) {
      return std::nullopt;
    }

    FT_BBox cbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &cbox);
    CFX_Size pixelSize = GetPixelSize();
    FX_RECT result =
        ScaledFXRectFromFTPos(cbox.xMin, cbox.yMax, cbox.xMax, cbox.yMin,
                              pixelSize.width, pixelSize.height);
    result.top = std::min(result.top, static_cast<int>(GetAscender()));
    result.bottom = std::max(result.bottom, static_cast<int>(GetDescender()));
    FT_Done_Glyph(glyph);
    return result;
  }
  if (LoadGlyph(glyph_index, /*scale=*/false) != 0) {
    return std::nullopt;
  }
  int em = GetUnitsPerEm();
  const FX_RECT ft_result = ScaledFXRectFromFTPos(
      GetRec()->glyph->metrics.horiBearingX,
      GetRec()->glyph->metrics.horiBearingY - GetRec()->glyph->metrics.height,
      GetRec()->glyph->metrics.horiBearingX + GetRec()->glyph->metrics.width,
      GetRec()->glyph->metrics.horiBearingY, em, em);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, em);
    font.setHinting(SkFontHinting::kNone);
    uint16_t skia_glyph_index = static_cast<uint16_t>(glyph_index);
    SkRect bounds = font.getBounds(skia_glyph_index, nullptr);

    CHECK_EQ(ft_result.left,
             NormalizeFontMetric(static_cast<int32_t>(bounds.fLeft), em));
    CHECK_EQ(ft_result.top,
             NormalizeFontMetric(static_cast<int32_t>(-bounds.fTop), em));
    CHECK_EQ(ft_result.right,
             NormalizeFontMetric(static_cast<int32_t>(bounds.fRight), em));
    CHECK_EQ(ft_result.bottom,
             NormalizeFontMetric(static_cast<int32_t>(-bounds.fBottom), em));
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

FX_RECT CFX_Face::GetCharBBox(uint32_t code, int glyph_index) {
  FX_RECT rect;
  FT_FaceRec* rec = GetRec();
  if (IsTricky()) {
    int err =
        FT_Load_Glyph(rec, glyph_index, FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    if (!err) {
      FT_Glyph glyph;
      err = FT_Get_Glyph(rec->glyph, &glyph);
      if (!err) {
        FT_BBox cbox;
        FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &cbox);
        const int xMin = FTPosToCBoxInt(cbox.xMin);
        const int xMax = FTPosToCBoxInt(cbox.xMax);
        const int yMin = FTPosToCBoxInt(cbox.yMin);
        const int yMax = FTPosToCBoxInt(cbox.yMax);
        const int pixel_size_x = rec->size->metrics.x_ppem;
        const int pixel_size_y = rec->size->metrics.y_ppem;
        if (pixel_size_x == 0 || pixel_size_y == 0) {
          rect = FX_RECT(xMin, yMax, xMax, yMin);
        } else {
          rect =
              FX_RECT(xMin * 1000 / pixel_size_x, yMax * 1000 / pixel_size_y,
                      xMax * 1000 / pixel_size_x, yMin * 1000 / pixel_size_y);
        }
        rect.top = std::min(rect.top, static_cast<int>(GetAscender()));
        rect.bottom = std::max(rect.bottom, static_cast<int>(GetDescender()));
        FT_Done_Glyph(glyph);
      }
    }
  } else {
    int err = LoadGlyph(glyph_index, /*scale=*/false);
    if (err == 0) {
      rect = GetGlyphBBox(glyph_index);
      if (rect.top <= kMaxRectTop) {
        rect.top += rect.top / 64;
      } else {
        rect.top = std::numeric_limits<int>::max();
      }
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
      if (skrifa_font_ && skrifa_font_->font->is_ok()) {
        skrifa::BoundingBox bbox =
            skrifa_font_->font->glyph_bounds(glyph_index);
        const uint16_t upem = GetUnitsPerEm();
        FX_RECT skrifa_result(NormalizeFontMetric(bbox.x_min, upem),
                              NormalizeFontMetric(bbox.y_max, upem),
                              NormalizeFontMetric(bbox.x_max, upem),
                              NormalizeFontMetric(bbox.y_min, upem));
        if (skrifa_result.top <= kMaxRectTop) {
          skrifa_result.top += skrifa_result.top / 64;
        } else {
          skrifa_result.top = std::numeric_limits<int>::max();
        }
        CHECK(rect.Near(skrifa_result, 2));
      }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
    }
  }
  return rect;
}

FX_RECT CFX_Face::GetGlyphBBox(uint32_t glyph_index) const {
  const auto* glyph = GetRec()->glyph;
  DCHECK_EQ(glyph_index, glyph->glyph_index);

  pdfium::ClampedNumeric<FT_Pos> left = glyph->metrics.horiBearingX;
  pdfium::ClampedNumeric<FT_Pos> top = glyph->metrics.horiBearingY;
  const uint16_t upem = GetUnitsPerEm();
  FX_RECT ft_result(NormalizeFontMetric(left, upem),
                    NormalizeFontMetric(top, upem),
                    NormalizeFontMetric(left + glyph->metrics.width, upem),
                    NormalizeFontMetric(top - glyph->metrics.height, upem));

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    skrifa::BoundingBox bbox = skrifa_font_->font->glyph_bounds(glyph_index);
    FX_RECT skrifa_result(NormalizeFontMetric(bbox.x_min, upem),
                          NormalizeFontMetric(bbox.y_max, upem),
                          NormalizeFontMetric(bbox.x_max, upem),
                          NormalizeFontMetric(bbox.y_min, upem));
    CHECK(ft_result.Near(skrifa_result, 2));
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
  if (skia_typeface_) {
    SkFont font(skia_typeface_, upem);
    font.setHinting(SkFontHinting::kNone);
    uint16_t skia_glyph_index = static_cast<uint16_t>(glyph_index);

    std::optional<SkPath> path = font.getPath(skia_glyph_index);
    int skia_left = 0;
    int skia_top = 0;
    int skia_right = 0;
    int skia_bottom = 0;
    SkRect path_bounds = SkRect::MakeEmpty();
    if (path.has_value()) {
      path_bounds = path->getBounds();
      skia_left = NormalizeFontMetric(
          static_cast<int32_t>(std::round(path_bounds.fLeft)), upem);
      skia_top = NormalizeFontMetric(
          static_cast<int32_t>(std::round(-path_bounds.fTop)), upem);
      skia_right = NormalizeFontMetric(
          static_cast<int32_t>(std::round(path_bounds.fRight)), upem);
      skia_bottom = NormalizeFontMetric(
          static_cast<int32_t>(std::round(-path_bounds.fBottom)), upem);
    }

    CHECK_EQ(ft_result.left, skia_left);
    CHECK_EQ(ft_result.top, skia_top);
    CHECK_EQ(ft_result.right, skia_right);
    CHECK_EQ(ft_result.bottom, skia_bottom);
  }
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}

std::vector<CharCodeAndIndex> CFX_Face::GetCharCodesAndIndices(
    char32_t max_char) {
  CharCodeAndIndex char_code_and_index;
  char_code_and_index.char_code = static_cast<uint32_t>(
      FT_Get_First_Char(GetRec(), &char_code_and_index.glyph_index));
  if (char_code_and_index.char_code > max_char) {
    return {};
  }
  std::vector<CharCodeAndIndex> results = {char_code_and_index};
  while (true) {
    char_code_and_index.char_code = static_cast<uint32_t>(FT_Get_Next_Char(
        GetRec(), results.back().char_code, &char_code_and_index.glyph_index));
    if (char_code_and_index.char_code > max_char ||
        char_code_and_index.glyph_index == 0) {
      break;
    }
    results.push_back(char_code_and_index);
  }

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    auto skrifa_result =
        skrifa_font_->font->get_char_codes_and_indices(max_char);

    CHECK_EQ(results.size(), skrifa_result.size());
    for (size_t i = 0; i < results.size(); ++i) {
      CHECK_EQ(results[i].char_code, skrifa_result[i].char_code);
      CHECK_EQ(results[i].glyph_index, skrifa_result[i].glyph_index);
    }
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return results;
}

CFX_Face::CharMap CFX_Face::GetCurrentCharMap() const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return GetRec()->charmap;
}

std::optional<fxge::FontEncoding> CFX_Face::GetCurrentCharMapEncoding() const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  if (!GetRec()->charmap) {
    return std::nullopt;
  }
  return ToFontEncoding(GetRec()->charmap->encoding);
}

CFX_Face::CharMapId CFX_Face::GetCharMapIdByIndex(size_t index) const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return {.platform_id = GetCharMapPlatformIdByIndex(index),
          .encoding_id = GetCharMapEncodingIdByIndex(index)};
}

int CFX_Face::GetCharMapPlatformIdByIndex(size_t index) const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return GetCharMaps()[index]->platform_id;
}

int CFX_Face::GetCharMapEncodingIdByIndex(size_t index) const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return GetCharMaps()[index]->encoding_id;
}

fxge::FontEncoding CFX_Face::GetCharMapEncodingByIndex(size_t index) const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return ToFontEncoding(GetCharMaps()[index]->encoding);
}

size_t CFX_Face::GetCharMapCount() const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  return GetRec()->charmaps
             ? pdfium::checked_cast<size_t>(GetRec()->num_charmaps)
             : 0;
}

pdfium::span<const FT_CharMap> CFX_Face::GetCharMaps() const {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  size_t count = GetCharMapCount();
  if (count == 0) {
    return {};
  }
  // SAFETY: required from library to provide correct count.
  return UNSAFE_BUFFERS({GetRec()->charmaps, count});
}

void CFX_Face::SetCharMap(CharMap map) {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  FT_Set_Charmap(GetRec(), static_cast<FT_CharMap>(map));
}

void CFX_Face::SetCharMapByIndex(size_t index) {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  CHECK_LT(index, GetCharMapCount());
  // SAFETY: required from library as enforced by check above.
  SetCharMap(UNSAFE_BUFFERS(GetRec()->charmaps[index]));
}

bool CFX_Face::SelectCharMap(fxge::FontEncoding encoding) {
  // TODO(https://crbug.com/42271123): Implement charmap management in
  // Skia/Fontations.
  FT_Error error = FT_Select_Charmap(GetRec(), ToFTEncoding(encoding));
  return !error;
}

#if defined(PDF_ENABLE_XFA)
int CFX_Face::GetNumFaces() const {
  const int ft_result = pdfium::checked_cast<int>(GetRec()->num_faces);
#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  pdfium::span<const uint8_t> data = GetData();
  uint32_t skrifa_result = skrifa::get_num_faces(rust::Slice(data));
  CHECK_EQ(ft_result, static_cast<int>(skrifa_result));
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
  return ft_result;
}
#endif  // defined(PDF_ENABLE_XFA)

#if BUILDFLAG(IS_WIN)
bool CFX_Face::CanEmbed() {
  FT_UShort fstype = FT_Get_FSType_Flags(GetRec());
  bool ft_result = (fstype & (FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING |
                              FT_FSTYPE_BITMAP_EMBEDDING_ONLY)) == 0;

#if defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)
#if defined(PDF_ENABLE_FONTATIONS)
  if (skrifa_font_ && skrifa_font_->font->is_ok()) {
    bool skrifa_result = false;
    uint16_t fs_type = 0;
    if (skrifa_font_->font->get_os2_fs_type(fs_type)) {
      skrifa_result =
          (fs_type &
           (fxcrt::to_underlying(skrifa::FsType::RestrictedLicenseEmbedding) |
            fxcrt::to_underlying(skrifa::FsType::BitmapEmbeddingOnly))) == 0;
    }
    CHECK_EQ(ft_result, skrifa_result);
  }
#endif  // defined(PDF_ENABLE_FONTATIONS)
#endif  // defined(PDF_ENABLE_SKIA_TYPEFACE_CHECKS)

  return ft_result;
}
#endif  // BUILDFLAG(IS_WIN)

CFX_Face::CFX_Face(
    RetainPtr<Retainable> cache_entry,
    RetainPtr<CFX_ReadOnlySpanStream> font_stream,
    FT_FaceRec* rec,
    [[maybe_unused]] std::unique_ptr<SkrifaFontHolder> skrifa_font)
    : cache_entry_(std::move(cache_entry)),
      font_stream_(std::move(font_stream)),
      rec_(rec)
#if defined(PDF_ENABLE_FONTATIONS)
      ,
      skrifa_font_(std::move(skrifa_font))
#endif  // defined(PDF_ENABLE_FONTATIONS)
{
  DCHECK(rec_);
}

#if defined(PDF_USE_SKIA)
SkTypeface* CFX_Face::GetOrCreateSkTypeface() {
  if (!skia_typeface_) {
    skia_typeface_ =
        CFX_GEModule::Get()->GetFontMgr()->MakeSkTypeface(GetData());
  }
  return skia_typeface_.get();
}
#endif  // defined(PDF_USE_SKIA)

CFX_Face::~CFX_Face() = default;

void CFX_Face::AdjustVariationParams(int glyph_index,
                                     int dest_width,
                                     int weight) {
  // TODO(https://crbug.com/42271123): Implement variation parameters adjustment
  // in Skia/Fontations.
  DCHECK_GE(dest_width, 0);

  FT_FaceRec* rec = GetRec();
  ScopedFXFTMMVar variation_desc(rec);
  if (!variation_desc) {
    return;
  }

  FT_Pos coords[2];
  if (weight == 0) {
    coords[0] = variation_desc.GetAxisDefault(0) / 65536;
  } else {
    coords[0] = weight;
  }

  if (dest_width == 0) {
    coords[1] = variation_desc.GetAxisDefault(1) / 65536;
  } else {
    FT_Long min_param = variation_desc.GetAxisMin(1) / 65536;
    FT_Long max_param = variation_desc.GetAxisMax(1) / 65536;
    coords[1] = min_param;
    FT_Set_MM_Design_Coordinates(rec, 2, coords);
    FT_Load_Glyph(rec, glyph_index,
                  FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    FT_Pos min_width = rec->glyph->metrics.horiAdvance * 1000 / GetUnitsPerEm();
    coords[1] = max_param;
    FT_Set_MM_Design_Coordinates(rec, 2, coords);
    FT_Load_Glyph(rec, glyph_index,
                  FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    FT_Pos max_width = rec->glyph->metrics.horiAdvance * 1000 / GetUnitsPerEm();
    if (max_width == min_width) {
      return;
    }
    FT_Pos param = min_param + (max_param - min_param) *
                                   (dest_width - min_width) /
                                   (max_width - min_width);
    coords[1] = param;
  }
  FT_Set_MM_Design_Coordinates(rec, 2, coords);
}

#if defined(PDF_ENABLE_XFA) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
uint32_t CFX_Face::GetFontStyle() {
  uint32_t style = 0;
  if (IsBold()) {
    style |= pdfium::kFontStyleForceBold;
  }
  if (IsItalic()) {
    style |= pdfium::kFontStyleItalic;
  }
  if (IsFixedWidth()) {
    style |= pdfium::kFontStyleFixedPitch;
  }

  std::optional<std::array<uint32_t, 2>> code_page_range =
      GetOs2CodePageRange();
  if (code_page_range.has_value() && (code_page_range.value()[0] & (1 << 31))) {
    style |= pdfium::kFontStyleSymbolic;
  }

  std::optional<std::array<uint8_t, 2>> panose = GetOs2Panose();
  if (panose.has_value() && panose.value()[0] == 2) {
    uint8_t serif = panose.value()[1];
    if ((serif > 1 && serif < 10) || serif > 13) {
      style |= pdfium::kFontStyleSerif;
    }
  }
  return style;
}
#endif  // defined(PDF_ENABLE_XFA) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_LINUX)
