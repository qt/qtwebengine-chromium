// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jpeg/jpegmodule.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "build/build_config.h"
#include "core/fxcodec/cfx_codec_memory.h"
#include "core/fxcodec/jpeg/jpeg_common.h"
#include "core/fxcodec/scanlinedecoder.h"
#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/raw_span.h"
#include "core/fxge/dib/cfx_dibbase.h"
#include "core/fxge/dib/fx_dib.h"

static pdfium::span<const uint8_t> JpegScanSOI(
    pdfium::span<const uint8_t> src_span) {
  DCHECK(!src_span.empty());

  for (size_t offset = 0; offset + 1 < src_span.size(); ++offset) {
    if (src_span[offset] == 0xff && src_span[offset + 1] == 0xd8) {
      return src_span.subspan(offset);
    }
  }
  return src_span;
}

static bool JpegLoadInfo(pdfium::span<const uint8_t> src_span,
                         JpegModule::ImageInfo* pInfo) {
  src_span = JpegScanSOI(src_span);

  JpegCommon jpeg_common = {};
  jpeg_common.error_mgr.error_exit = jpeg_common_error_fatal;
  jpeg_common.error_mgr.emit_message = jpeg_common_error_do_nothing_int;
  jpeg_common.error_mgr.output_message = jpeg_common_error_do_nothing;
  jpeg_common.error_mgr.format_message = jpeg_common_error_do_nothing_char;
  jpeg_common.error_mgr.reset_error_mgr = jpeg_common_error_do_nothing;
  jpeg_common.error_mgr.trace_level = 0;
  jpeg_common.cinfo.err = &jpeg_common.error_mgr;
  jpeg_common.cinfo.client_data = &jpeg_common;
  if (!jpeg_common_create_decompress(&jpeg_common)) {
    return false;
  }

  jpeg_common.source_mgr.init_source = jpeg_common_src_do_nothing;
  jpeg_common.source_mgr.term_source = jpeg_common_src_do_nothing;
  jpeg_common.source_mgr.skip_input_data = jpeg_common_src_skip_data_or_trap;
  jpeg_common.source_mgr.fill_input_buffer = jpeg_common_src_fill_buffer;
  jpeg_common.source_mgr.resync_to_restart = jpeg_common_src_resync;
  jpeg_common.source_mgr.bytes_in_buffer = src_span.size();
  jpeg_common.source_mgr.next_input_byte = src_span.data();
  jpeg_common.cinfo.src = &jpeg_common.source_mgr;
  if (jpeg_common_read_header(&jpeg_common, TRUE) != JPEG_HEADER_OK) {
    jpeg_common_destroy_decompress(&jpeg_common);
    return false;
  }
  pInfo->width = jpeg_common.cinfo.image_width;
  pInfo->height = jpeg_common.cinfo.image_height;
  pInfo->num_components = jpeg_common.cinfo.num_components;
  pInfo->color_transform = jpeg_common.cinfo.jpeg_color_space == JCS_YCbCr ||
                           jpeg_common.cinfo.jpeg_color_space == JCS_YCCK;
  pInfo->bits_per_components = jpeg_common.cinfo.data_precision;
  jpeg_common_destroy_decompress(&jpeg_common);
  return true;
}

namespace fxcodec {

namespace {

constexpr size_t kKnownBadHeaderWithInvalidHeightByteOffsetStarts[] = {94, 163};

class JpegDecoder final : public ScanlineDecoder {
 public:
  JpegDecoder();
  ~JpegDecoder() override;

  bool Create(pdfium::span<const uint8_t> src_span,
              uint32_t width,
              uint32_t height,
              int nComps,
              bool ColorTransform,
              uint32_t scale_denom);

  // ScanlineDecoder:
  [[nodiscard]] bool Rewind() override;
  pdfium::span<uint8_t> GetNextLine() override;
  uint32_t GetSrcOffset() override;

  bool InitDecode(bool bAcceptKnownBadHeader);

 private:
  void CalcPitch();
  void InitDecompressSrc();

  // Only called when initial jpeg_read_header() fails.
  bool HasKnownBadHeaderWithInvalidHeight(size_t dimension_offset) const;

  // Is a JPEG SOFn marker, which is defined as 0xff, 0xc[0-9a-f].
  bool IsSofSegment(size_t marker_offset) const;

  // Patch up the in-memory JPEG header for known bad JPEGs.
  void PatchUpKnownBadHeaderWithInvalidHeight(size_t dimension_offset);

  // Patch up the JPEG trailer, even if it is correct.
  void PatchUpTrailer();

  pdfium::span<uint8_t> GetWritableSrcData();

  // For a given invalid height byte offset in
  // |kKnownBadHeaderWithInvalidHeightByteOffsetStarts|, the SOFn marker should
  // be this many bytes before that.
  static constexpr size_t kSofMarkerByteOffset = 5;

  JpegCommon common_ = {};
  pdfium::raw_span<const uint8_t> src_span_;
  DataVector<uint8_t> scanline_buf_;
  bool decompress_created_ = false;
  bool started_ = false;
  bool jpeg_transform_ = false;
  uint32_t scale_denom_ = 1;
};

// Returns the output dimension libjpeg produces for `dim` when decoding at
// 1/`scale_denom` scale. Matches jpeg_core_output_dimensions() for the
// power-of-two scalings (scale_denom of 1, 2, 4, 8), which reduce to
// ceil(dim / scale_denom).
uint32_t ScaledJpegSize(uint32_t dim, uint32_t scale_denom) {
  return (dim + scale_denom - 1) / scale_denom;
}

JpegDecoder::JpegDecoder() = default;

JpegDecoder::~JpegDecoder() {
  if (decompress_created_) {
    jpeg_common_destroy_decompress(&common_);
  }

  // Span in superclass can't outlive our buffer.
  last_scanline_ = pdfium::span<uint8_t>();
}

bool JpegDecoder::InitDecode(bool bAcceptKnownBadHeader) {
  common_.cinfo.err = &common_.error_mgr;
  common_.cinfo.client_data = &common_;
  if (!jpeg_common_create_decompress(&common_)) {
    return false;
  }
  decompress_created_ = true;
  common_.cinfo.image_width = orig_width_;
  common_.cinfo.image_height = orig_height_;
  InitDecompressSrc();
  if (jpeg_common_read_header(&common_, TRUE) != JPEG_HEADER_OK) {
    std::optional<size_t> known_bad_header_offset;
    if (bAcceptKnownBadHeader) {
      for (size_t offset : kKnownBadHeaderWithInvalidHeightByteOffsetStarts) {
        if (HasKnownBadHeaderWithInvalidHeight(offset)) {
          known_bad_header_offset = offset;
          break;
        }
      }
    }
    jpeg_common_destroy_decompress(&common_);
    decompress_created_ = false;
    if (!known_bad_header_offset.has_value()) {
      return false;
    }
    PatchUpKnownBadHeaderWithInvalidHeight(known_bad_header_offset.value());
    if (!jpeg_common_create_decompress(&common_)) {
      return false;
    }
    decompress_created_ = true;
    common_.cinfo.image_width = orig_width_;
    common_.cinfo.image_height = orig_height_;
    InitDecompressSrc();
    if (jpeg_common_read_header(&common_, TRUE) != JPEG_HEADER_OK) {
      jpeg_common_destroy_decompress(&common_);
      decompress_created_ = false;
      return false;
    }
  }
  if (common_.cinfo.saw_Adobe_marker) {
    jpeg_transform_ = true;
  }

  if (common_.cinfo.num_components == 3 && !jpeg_transform_) {
    common_.cinfo.out_color_space = common_.cinfo.jpeg_color_space;
  }

  orig_width_ = common_.cinfo.image_width;
  orig_height_ = common_.cinfo.image_height;

  // Reduced-size (scale_denom) decoding collapses each 8x8 DCT block toward its
  // low-frequency content, ultimately its average at 1/8. If a dimension is not
  // a multiple of the MCU size, the encoder had to pad the partial edge blocks;
  // that padding is not required to replicate the edge (some encoders fill it
  // with black), and reduced decoding cannot crop it back out, so it would
  // contaminate the visible edge pixels (e.g. a too-dark right/bottom fringe).
  // Only decode at reduced size when there are no padded edge blocks in any
  // component, which makes the result exact regardless of the encoder. The MCU
  // size in pixels is max_samp_factor * DCTSIZE; derive it from the per-
  // component sampling factors, which are valid immediately after read_header.
  // See https://crbug.com/890745 and
  // https://github.com/libjpeg-turbo/libjpeg-turbo/issues/297.
  int max_h_samp = 1;
  int max_v_samp = 1;
  // SAFETY: libjpeg guarantees `comp_info` points to `num_components` entries
  // once the header has been read.
  auto comp_info = UNSAFE_BUFFERS(
      pdfium::span(common_.cinfo.comp_info,
                   static_cast<size_t>(common_.cinfo.num_components)));
  for (const jpeg_component_info& comp : comp_info) {
    max_h_samp = std::max(max_h_samp, comp.h_samp_factor);
    max_v_samp = std::max(max_v_samp, comp.v_samp_factor);
  }
  if (orig_width_ % (max_h_samp * DCTSIZE) != 0 ||
      orig_height_ % (max_v_samp * DCTSIZE) != 0) {
    // Reset to 1 (no reduction). This is equivalent to using
    // common_.cinfo.scale_denom, which libjpeg leaves at its default of 1 until
    // jpeg_start_decompress(); that has not run yet here.
    scale_denom_ = 1;
  }

  output_width_ = ScaledJpegSize(orig_width_, scale_denom_);
  output_height_ = ScaledJpegSize(orig_height_, scale_denom_);
  return true;
}

bool JpegDecoder::Create(pdfium::span<const uint8_t> src_span,
                         uint32_t width,
                         uint32_t height,
                         int nComps,
                         bool ColorTransform,
                         uint32_t scale_denom) {
  // Only power-of-two scalings up to 1/8 are supported: ScaledJpegSize() (which
  // sizes the scanline buffer via CalcPitch()) only matches libjpeg's actual
  // output dimensions for these. A different value would desynchronize the two
  // and lead to a scanline-buffer overflow.
  DCHECK(scale_denom == 1 || scale_denom == 2 || scale_denom == 4 ||
         scale_denom == 8);
  scale_denom_ = scale_denom;
  src_span_ = JpegScanSOI(src_span);
  if (src_span_.size() < 2) {
    return false;
  }

  PatchUpTrailer();

  common_.error_mgr.error_exit = jpeg_common_error_fatal;
  common_.error_mgr.emit_message = jpeg_common_error_do_nothing_int;
  common_.error_mgr.output_message = jpeg_common_error_do_nothing;
  common_.error_mgr.format_message = jpeg_common_error_do_nothing_char;
  common_.error_mgr.reset_error_mgr = jpeg_common_error_do_nothing;
  common_.source_mgr.init_source = jpeg_common_src_do_nothing;
  common_.source_mgr.term_source = jpeg_common_src_do_nothing;
  common_.source_mgr.skip_input_data = jpeg_common_src_skip_data_or_trap;
  common_.source_mgr.fill_input_buffer = jpeg_common_src_fill_buffer;
  common_.source_mgr.resync_to_restart = jpeg_common_src_resync;
  jpeg_transform_ = ColorTransform;
  output_width_ = orig_width_ = width;
  output_height_ = orig_height_ = height;
  if (!InitDecode(/*bAcceptKnownBadHeader=*/true)) {
    return false;
  }

  if (common_.cinfo.num_components < nComps) {
    return false;
  }

  if (common_.cinfo.image_width < width) {
    return false;
  }

  CalcPitch();
  scanline_buf_ = DataVector<uint8_t>(pitch_);
  comps_ = common_.cinfo.num_components;
  bpc_ = 8;
  started_ = false;
  return true;
}

bool JpegDecoder::Rewind() {
  if (started_) {
    jpeg_common_destroy_decompress(&common_);
    if (!InitDecode(/*bAcceptKnownBadHeader=*/false)) {
      return false;
    }
  }
  // `Created()` validated `scale_denom_` and, for non-MCU-aligned images,
  // reset it to 1 in `InitDecode()` (called from `Create()`, and again just
  // above when re-decoding), so it is safe to use as-is here.
  common_.cinfo.scale_denom = scale_denom_;
  output_width_ = ScaledJpegSize(orig_width_, scale_denom_);
  output_height_ = ScaledJpegSize(orig_height_, scale_denom_);
  if (!jpeg_common_start_decompress(&common_)) {
    jpeg_common_destroy_decompress(&common_);
    return false;
  }
  // `CalcPitch()` sized the scanline buffer for `output_width_` columns,
  // so libjpeg must not produce more than that per scanline.
  CHECK_LE(static_cast<int>(common_.cinfo.output_width), output_width_);
  started_ = true;
  return true;
}

pdfium::span<uint8_t> JpegDecoder::GetNextLine() {
  uint8_t* row_array[] = {scanline_buf_.data()};
  int nlines = jpeg_common_read_scanlines(&common_, row_array, 1u);
  if (nlines <= 0) {
    return pdfium::span<uint8_t>();
  }
  return scanline_buf_;
}

uint32_t JpegDecoder::GetSrcOffset() {
  return static_cast<uint32_t>(src_span_.size() -
                               common_.source_mgr.bytes_in_buffer);
}

void JpegDecoder::CalcPitch() {
  // `output_width_` must be finalized (post scale/alignment decision in
  // `InitDecode()`) before this runs; the sole caller is `Create()`, after
  // `InitDecode()`.
  DCHECK_GT(output_width_, 0);
  pitch_ = static_cast<uint32_t>(output_width_) * common_.cinfo.num_components;
  pitch_ += 3;
  pitch_ /= 4;
  pitch_ *= 4;
}

void JpegDecoder::InitDecompressSrc() {
  common_.cinfo.src = &common_.source_mgr;
  common_.source_mgr.bytes_in_buffer = src_span_.size();
  common_.source_mgr.next_input_byte = src_span_.data();
}

bool JpegDecoder::HasKnownBadHeaderWithInvalidHeight(
    size_t dimension_offset) const {
  // Perform lots of possibly redundant checks to make sure this has no false
  // positives.
  bool bDimensionChecks =
      common_.cinfo.err->msg_code == JERR_IMAGE_TOO_BIG &&
      common_.cinfo.image_width < JPEG_MAX_DIMENSION &&
      common_.cinfo.image_height == 0xffff && orig_width_ > 0 &&
      orig_width_ <= JPEG_MAX_DIMENSION && orig_height_ > 0 &&
      orig_height_ <= JPEG_MAX_DIMENSION;
  if (!bDimensionChecks) {
    return false;
  }

  if (src_span_.size() <= dimension_offset + 3u) {
    return false;
  }

  if (!IsSofSegment(dimension_offset - kSofMarkerByteOffset)) {
    return false;
  }

  const auto pHeaderDimensions = src_span_.subspan(dimension_offset);
  uint8_t nExpectedWidthByte1 = (orig_width_ >> 8) & 0xff;
  uint8_t nExpectedWidthByte2 = orig_width_ & 0xff;
  // Height high byte, height low byte, width high byte, width low byte.
  return pHeaderDimensions[0] == 0xff && pHeaderDimensions[1] == 0xff &&
         pHeaderDimensions[2] == nExpectedWidthByte1 &&
         pHeaderDimensions[3] == nExpectedWidthByte2;
}

bool JpegDecoder::IsSofSegment(size_t marker_offset) const {
  const auto pHeaderMarker = src_span_.subspan(marker_offset);
  return pHeaderMarker[0] == 0xff && pHeaderMarker[1] >= 0xc0 &&
         pHeaderMarker[1] <= 0xcf;
}

void JpegDecoder::PatchUpKnownBadHeaderWithInvalidHeight(
    size_t dimension_offset) {
  DCHECK(src_span_.size() > dimension_offset + 1u);
  auto pData = GetWritableSrcData().subspan(dimension_offset);
  pData[0] = (orig_height_ >> 8) & 0xff;
  pData[1] = orig_height_ & 0xff;
}

void JpegDecoder::PatchUpTrailer() {
  auto pData = GetWritableSrcData();
  pData[src_span_.size() - 2] = 0xff;
  pData[src_span_.size() - 1] = 0xd9;
}

pdfium::span<uint8_t> JpegDecoder::GetWritableSrcData() {
  // SAFETY: const_cast<> doesn't change size.
  return UNSAFE_BUFFERS(
      pdfium::span(const_cast<uint8_t*>(src_span_.data()), src_span_.size()));
}

}  // namespace

// static
std::unique_ptr<ScanlineDecoder> JpegModule::CreateDecoder(
    pdfium::span<const uint8_t> src_span,
    uint32_t width,
    uint32_t height,
    int nComps,
    bool ColorTransform,
    uint32_t scale_denom) {
  DCHECK(!src_span.empty());

  auto pDecoder = std::make_unique<JpegDecoder>();
  if (!pDecoder->Create(src_span, width, height, nComps, ColorTransform,
                        scale_denom)) {
    return nullptr;
  }

  return pDecoder;
}

// static
std::optional<JpegModule::ImageInfo> JpegModule::LoadInfo(
    pdfium::span<const uint8_t> src_span) {
  ImageInfo info;
  if (!JpegLoadInfo(src_span, &info)) {
    return std::nullopt;
  }

  return info;
}

#if BUILDFLAG(IS_WIN)
bool JpegModule::JpegEncode(const RetainPtr<const CFX_DIBBase>& pSource,
                            uint8_t** dest_buf,
                            size_t* dest_size) {
  jpeg_error_mgr jerr;
  jerr.error_exit = jpeg_common_error_do_nothing;
  jerr.emit_message = jpeg_common_error_do_nothing_int;
  jerr.output_message = jpeg_common_error_do_nothing;
  jerr.format_message = jpeg_common_error_do_nothing_char;
  jerr.reset_error_mgr = jpeg_common_error_do_nothing;

  jpeg_compress_struct cinfo = {};  // Aggregate initialization.
  static_assert(std::is_aggregate_v<decltype(cinfo)>);
  cinfo.err = &jerr;
  jpeg_create_compress(&cinfo);
  const int bytes_per_pixel = pSource->GetBPP() / 8;
  uint32_t nComponents = bytes_per_pixel >= 3 ? 3 : 1;
  uint32_t pitch = pSource->GetPitch();
  uint32_t width = pdfium::checked_cast<uint32_t>(pSource->GetWidth());
  uint32_t height = pdfium::checked_cast<uint32_t>(pSource->GetHeight());
  FX_SAFE_UINT32 safe_buf_len = width;
  safe_buf_len *= height;
  safe_buf_len *= nComponents;
  safe_buf_len += 1024;
  if (!safe_buf_len.IsValid()) {
    return false;
  }

  uint32_t dest_buf_length = safe_buf_len.ValueOrDie();
  *dest_buf = FX_TryAlloc(uint8_t, dest_buf_length);
  const int MIN_TRY_BUF_LEN = 1024;
  while (!(*dest_buf) && dest_buf_length > MIN_TRY_BUF_LEN) {
    dest_buf_length >>= 1;
    *dest_buf = FX_TryAlloc(uint8_t, dest_buf_length);
  }
  if (!(*dest_buf)) {
    return false;
  }

  jpeg_destination_mgr dest;
  dest.init_destination = jpeg_common_dest_do_nothing;
  dest.term_destination = jpeg_common_dest_do_nothing;
  dest.empty_output_buffer = jpeg_common_dest_empty;
  dest.next_output_byte = *dest_buf;
  dest.free_in_buffer = dest_buf_length;
  cinfo.dest = &dest;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = nComponents;
  if (nComponents == 1) {
    cinfo.in_color_space = JCS_GRAYSCALE;
  } else if (nComponents == 3) {
    cinfo.in_color_space = JCS_RGB;
  } else {
    cinfo.in_color_space = JCS_CMYK;
  }
  uint8_t* line_buf = nullptr;
  if (nComponents > 1) {
    line_buf = FX_Alloc2D(uint8_t, width, nComponents);
  }

  jpeg_set_defaults(&cinfo);
  jpeg_start_compress(&cinfo, TRUE);
  JSAMPROW row_pointer[1];
  JDIMENSION row;
  while (cinfo.next_scanline < cinfo.image_height) {
    pdfium::span<const uint8_t> src_scan =
        pSource->GetScanline(cinfo.next_scanline);
    if (nComponents > 1) {
      uint8_t* dest_scan = line_buf;
      if (nComponents == 3) {
        UNSAFE_TODO({
          for (uint32_t i = 0; i < width; i++) {
            ReverseCopy3Bytes(dest_scan, src_scan.data());
            dest_scan += 3;
            src_scan = src_scan.subspan(static_cast<size_t>(bytes_per_pixel));
          }
        });
      } else {
        UNSAFE_TODO({
          for (uint32_t i = 0; i < pitch; i++) {
            *dest_scan++ = ~src_scan.front();
            src_scan = src_scan.subspan<1u>();
          }
        });
      }
      row_pointer[0] = line_buf;
    } else {
      row_pointer[0] = const_cast<uint8_t*>(src_scan.data());
    }
    row = cinfo.next_scanline;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
    UNSAFE_TODO({
      if (cinfo.next_scanline == row) {
        static constexpr size_t kJpegBlockSize = 1048576;
        *dest_buf =
            FX_Realloc(uint8_t, *dest_buf, dest_buf_length + kJpegBlockSize);
        dest.next_output_byte =
            *dest_buf + dest_buf_length - dest.free_in_buffer;
        dest_buf_length += kJpegBlockSize;
        dest.free_in_buffer += kJpegBlockSize;
      }
    });
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  FX_Free(line_buf);
  *dest_size = dest_buf_length - static_cast<size_t>(dest.free_in_buffer);

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace fxcodec
