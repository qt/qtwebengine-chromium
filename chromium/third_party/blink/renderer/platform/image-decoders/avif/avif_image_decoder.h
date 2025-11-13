// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_

#include <memory>
#include <vector>

#include "media/media_buildflags.h"

#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(ENABLE_CRABBYAVIF)
#include "third_party/crabbyavif/src/include/avif/avif.h"
#else
#include "third_party/libavif/src/include/avif/avif.h"

namespace crabbyavif {
enum avifResult {
  AVIF_RESULT_OK = ::AVIF_RESULT_OK,
  AVIF_RESULT_UNKNOWN_ERROR = ::AVIF_RESULT_UNKNOWN_ERROR,
  AVIF_RESULT_INVALID_FTYP = ::AVIF_RESULT_INVALID_FTYP,
  AVIF_RESULT_NO_CONTENT = ::AVIF_RESULT_NO_CONTENT,
  AVIF_RESULT_NO_YUV_FORMAT_SELECTED = ::AVIF_RESULT_NO_YUV_FORMAT_SELECTED,
  AVIF_RESULT_REFORMAT_FAILED = ::AVIF_RESULT_REFORMAT_FAILED,
  AVIF_RESULT_UNSUPPORTED_DEPTH = ::AVIF_RESULT_UNSUPPORTED_DEPTH,
  AVIF_RESULT_ENCODE_COLOR_FAILED = ::AVIF_RESULT_ENCODE_COLOR_FAILED,
  AVIF_RESULT_ENCODE_ALPHA_FAILED = ::AVIF_RESULT_ENCODE_ALPHA_FAILED,
  AVIF_RESULT_BMFF_PARSE_FAILED = ::AVIF_RESULT_BMFF_PARSE_FAILED,
  AVIF_RESULT_MISSING_IMAGE_ITEM = ::AVIF_RESULT_MISSING_IMAGE_ITEM,
  AVIF_RESULT_DECODE_COLOR_FAILED = ::AVIF_RESULT_DECODE_COLOR_FAILED,
  AVIF_RESULT_DECODE_ALPHA_FAILED = ::AVIF_RESULT_DECODE_ALPHA_FAILED,
  AVIF_RESULT_COLOR_ALPHA_SIZE_MISMATCH = ::AVIF_RESULT_COLOR_ALPHA_SIZE_MISMATCH,
  AVIF_RESULT_ISPE_SIZE_MISMATCH = ::AVIF_RESULT_ISPE_SIZE_MISMATCH,
  AVIF_RESULT_NO_CODEC_AVAILABLE = ::AVIF_RESULT_NO_CODEC_AVAILABLE,
  AVIF_RESULT_NO_IMAGES_REMAINING = ::AVIF_RESULT_NO_IMAGES_REMAINING,
  AVIF_RESULT_INVALID_EXIF_PAYLOAD = ::AVIF_RESULT_INVALID_EXIF_PAYLOAD,
  AVIF_RESULT_INVALID_IMAGE_GRID = ::AVIF_RESULT_INVALID_IMAGE_GRID,
  AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION = ::AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION,
  AVIF_RESULT_TRUNCATED_DATA = ::AVIF_RESULT_TRUNCATED_DATA,
  AVIF_RESULT_IO_NOT_SET = ::AVIF_RESULT_IO_NOT_SET,
  AVIF_RESULT_IO_ERROR = ::AVIF_RESULT_IO_ERROR,
  AVIF_RESULT_WAITING_ON_IO = ::AVIF_RESULT_WAITING_ON_IO,
  AVIF_RESULT_INVALID_ARGUMENT = ::AVIF_RESULT_INVALID_ARGUMENT,
  AVIF_RESULT_NOT_IMPLEMENTED = ::AVIF_RESULT_NOT_IMPLEMENTED,
  AVIF_RESULT_OUT_OF_MEMORY = ::AVIF_RESULT_OUT_OF_MEMORY,
  AVIF_RESULT_CANNOT_CHANGE_SETTING = ::AVIF_RESULT_CANNOT_CHANGE_SETTING,
  AVIF_RESULT_INCOMPATIBLE_IMAGE = ::AVIF_RESULT_INCOMPATIBLE_IMAGE,
  AVIF_RESULT_ENCODE_GAIN_MAP_FAILED = ::AVIF_RESULT_ENCODE_GAIN_MAP_FAILED,
  AVIF_RESULT_DECODE_GAIN_MAP_FAILED = ::AVIF_RESULT_DECODE_GAIN_MAP_FAILED,
  AVIF_RESULT_INVALID_TONE_MAPPED_IMAGE = ::AVIF_RESULT_INVALID_TONE_MAPPED_IMAGE,
};

enum avifPixelFormat {
  AVIF_PIXEL_FORMAT_NONE = ::AVIF_PIXEL_FORMAT_NONE,
  AVIF_PIXEL_FORMAT_YUV444 = ::AVIF_PIXEL_FORMAT_YUV444,
  AVIF_PIXEL_FORMAT_YUV422 = ::AVIF_PIXEL_FORMAT_YUV422,
  AVIF_PIXEL_FORMAT_YUV420 = ::AVIF_PIXEL_FORMAT_YUV420,
  AVIF_PIXEL_FORMAT_YUV400 = ::AVIF_PIXEL_FORMAT_YUV400,
};

enum avifRange {
  AVIF_RANGE_LIMITED = ::AVIF_RANGE_LIMITED,
  AVIF_RANGE_FULL = ::AVIF_RANGE_FULL,
};

enum avifChannelIndex {
  AVIF_CHAN_Y = ::AVIF_CHAN_Y,
  AVIF_CHAN_U = ::AVIF_CHAN_U,
  AVIF_CHAN_V = ::AVIF_CHAN_V,
  AVIF_CHAN_A = ::AVIF_CHAN_A,
};

constexpr static const int CRABBY_AVIF_TRUE = 1;
constexpr static const int CRABBY_AVIF_FALSE = 0;
constexpr static const uint32_t AVIF_STRICT_PIXI_REQUIRED = (1 << 0);
constexpr static const uint32_t AVIF_STRICT_CLAP_VALID = (1 << 1);
constexpr static const uint32_t AVIF_IMAGE_CONTENT_GAIN_MAP = (1 << 2);
constexpr static const int32_t CRABBY_AVIF_REPETITION_COUNT_INFINITE = -1;
constexpr static const int32_t CRABBY_AVIF_REPETITION_COUNT_UNKNOWN = -2;
constexpr static const uint32_t AVIF_TRANSFORM_CLAP = (1 << 1);
constexpr static const uint32_t AVIF_TRANSFORM_IROT = (1 << 2);
constexpr static const uint32_t AVIF_TRANSFORM_IMIR = (1 << 3);
}  // namespace crabbyavif
#endif  // BUILDFLAG(ENABLE_CRABBYAVIF)

namespace blink {

class FastSharedBufferReader;

class PLATFORM_EXPORT AVIFImageDecoder final : public ImageDecoder {
 public:
  AVIFImageDecoder(AlphaOption,
                   HighBitDepthDecodingOption,
                   ColorBehavior,
                   cc::AuxImage,
                   wtf_size_t max_decoded_bytes,
                   AnimationOption);
  AVIFImageDecoder(const AVIFImageDecoder&) = delete;
  AVIFImageDecoder& operator=(const AVIFImageDecoder&) = delete;
  ~AVIFImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;
  bool ImageIsHighBitDepth() override;
  void OnSetData(scoped_refptr<SegmentReader> data) override;
  bool GetGainmapInfoAndData(
      SkGainmapInfo& out_gainmap_info,
      scoped_refptr<SegmentReader>& out_gainmap_data) const override;
  cc::YUVSubsampling GetYUVSubsampling() const override;
  gfx::Size DecodedYUVSize(cc::YUVIndex) const override;
  wtf_size_t DecodedYUVWidthBytes(cc::YUVIndex) const override;
  SkYUVColorSpace GetYUVColorSpace() const override;
  uint8_t GetYUVBitDepth() const override;
  void DecodeToYUV() override;
  int RepetitionCount() const override;
  bool FrameIsReceivedAtIndex(wtf_size_t) const override;
  std::optional<base::TimeDelta> FrameTimestampAtIndex(
      wtf_size_t) const override;
  base::TimeDelta FrameDurationAtIndex(wtf_size_t) const override;
  bool ImageHasBothStillAndAnimatedSubImages() const override;

  // Returns true if the data in fast_reader begins with a valid FileTypeBox
  // (ftyp) that supports the brand 'avif' or 'avis'.
  static bool MatchesAVIFSignature(const FastSharedBufferReader& fast_reader);

  gfx::ColorSpace GetColorSpaceForTesting() const;

 private:
  struct AvifIOData {
    AvifIOData();
    AvifIOData(scoped_refptr<const SegmentReader> reader,
               bool all_data_received);
    ~AvifIOData();

    scoped_refptr<const SegmentReader> reader;
    std::vector<uint8_t> buffer ALLOW_DISCOURAGED_TYPE("Required by libavif");
    bool all_data_received = false;
  };

  void ParseMetadata();

  // ImageDecoder:
  void DecodeSize() override;
  wtf_size_t DecodeFrameCount() override;
  void InitializeNewFrame(wtf_size_t) override;
  void Decode(wtf_size_t) override;
  bool CanReusePreviousFrameBuffer(wtf_size_t) const override;

#if BUILDFLAG(ENABLE_CRABBYAVIF)
  // Implements crabbyavif::avifIOReadFunc, the |read| function in the
  // crabbyavif::avifIO struct.
  static crabbyavif::avifResult ReadFromSegmentReader(
      crabbyavif::avifIO* io,
      uint32_t read_flags,
      uint64_t offset,
      size_t size,
      crabbyavif::avifROData* out);
#else
  // Implements avifIOReadFunc, the |read| function in the avifIO struct.
  static avifResult ReadFromSegmentReader(avifIO* io,
                                          uint32_t read_flags,
                                          uint64_t offset,
                                          size_t size,
                                          avifROData* out);
#endif

  // Creates |decoder_| if not yet created and decodes the size and frame count.
  bool UpdateDemuxer();

  // Decodes the frame at index |index| and checks if the frame's size, bit
  // depth, and YUV format matches those reported by the container. The decoded
  // frame is available in decoded_image_.
  crabbyavif::avifResult DecodeImage(wtf_size_t index);

  // Crops |decoded_image_|.
  void CropDecodedImage();

  // Renders the rows [from_row, *to_row) of |image| to |buffer|. Returns
  // whether |image| was rendered successfully. On return, the in/out argument
  // |*to_row| may be decremented in case of subsampled chroma needing more
  // data.
#if BUILDFLAG(ENABLE_CRABBYAVIF)
  bool RenderImage(const crabbyavif::avifImage* image,
#else
  bool RenderImage(const avifImage* image,
#endif
                   int from_row,
                   int* to_row,
                   ImageFrame* buffer);

  // Applies color profile correction to the rows [from_row, to_row) of
  // |buffer|, if desired.
  void ColorCorrectImage(int from_row, int to_row, ImageFrame* buffer);

  // Returns decoder_->image or decoder_->image->gainMap->image depending on
  // aux_image_. May be nullptr if requesting the gain map image
  // (cc::AuxImage::kGainmap) but no gain map is present.
#if BUILDFLAG(ENABLE_CRABBYAVIF)
  crabbyavif::avifImage* GetDecoderImage() const;
#else
  avifImage* GetDecoderImage() const;
#endif

  bool have_parsed_current_data_ = false;
  // The image width and height (before cropping, if any) from the container.
  //
  // Note: container_width_, container_height_, decoder_->image->width, and
  // decoder_->image->height are the width and height of the full image. Size()
  // returns the size of the cropped image (the clean aperture).
  uint32_t container_width_ = 0;
  uint32_t container_height_ = 0;
  // The bit depth from the container.
  uint8_t bit_depth_ = 0;
  bool decode_to_half_float_ = false;
  uint8_t chroma_shift_x_ = 0;
  uint8_t chroma_shift_y_ = 0;
  bool progressive_ = false;
  // Number of displayed rows for a non-progressive still image.
  int incrementally_displayed_height_ = 0;
  // The YUV format from the container.
  crabbyavif::avifPixelFormat avif_yuv_format_ =
      crabbyavif::AVIF_PIXEL_FORMAT_NONE;
  wtf_size_t decoded_frame_count_ = 0;
  SkYUVColorSpace yuv_color_space_ = SkYUVColorSpace::kIdentity_SkYUVColorSpace;
  // Used to call UpdateBppHistogram<"Avif">() at most once to record the
  // bits-per-pixel value of the image when the image is successfully decoded.
  CrossThreadOnceFunction<void(gfx::Size, size_t)>
      update_bpp_histogram_callback_;
  // Whether the 'clap' (clean aperture) property should be ignored, e.g.
  // because the 'clap' property is invalid or unsupported.
  bool ignore_clap_ = false;
  // The origin (top left corner) of the clean aperture. Used only when the
  // image has a valid 'clap' (clean aperture) property.
  gfx::Point clap_origin_;
  // A copy of decoder_->image with the width, height, and plane buffers
  // adjusted to those of the clean aperture. Used only when the image has a
  // 'clap' (clean aperture) property.
#if BUILDFLAG(ENABLE_CRABBYAVIF)
  std::unique_ptr<crabbyavif::avifImage,
                  decltype(&crabbyavif::crabby_avifImageDestroy)>
      cropped_image_{nullptr, crabbyavif::crabby_avifImageDestroy};
#else
  std::unique_ptr<avifImage, decltype(&avifImageDestroy)> cropped_image_{
      nullptr, avifImageDestroy};
#endif
  // Set by a successful DecodeImage() call to either decoder_->image or
  // cropped_image_.get() depending on whether the image has a 'clap' (clean
  // aperture) property.
#if BUILDFLAG(ENABLE_CRABBYAVIF)
  raw_ptr<const crabbyavif::avifImage, DanglingUntriaged> decoded_image_ =
      nullptr;
#else
  raw_ptr<const avifImage, DanglingUntriaged> decoded_image_ = nullptr;
#endif
  // The declaration order of the next three fields is important. decoder_
  // points to avif_io_, and avif_io_ points to avif_io_data_. The destructor
  // must destroy them in that order.
  AvifIOData avif_io_data_;
#if BUILDFLAG(ENABLE_CRABBYAVIF)
  crabbyavif::avifIO avif_io_ = {};
  std::unique_ptr<crabbyavif::avifDecoder,
                  decltype(&crabbyavif::crabby_avifDecoderDestroy)>
      decoder_{nullptr, crabbyavif::crabby_avifDecoderDestroy};
#else
  avifIO avif_io_ = {};
  std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)> decoder_{
      nullptr, avifDecoderDestroy};
#endif

  const AnimationOption animation_option_;

  // Used temporarily for incremental decoding and for some YUV to RGB color
  // conversions.
  Vector<uint8_t> previous_last_decoded_row_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_
