// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/fdrm/fx_crypt.h"
#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/containers/contains.h"
#include "core/fxcrt/fx_memcpy_wrappers.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/notreached.h"
#include "core/fxcrt/numerics/checked_math.h"
#include "core/fxcrt/numerics/safe_conversions.h"
#include "core/fxcrt/span_util.h"
#include "core/fxcrt/zip.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "core/fxge/dib/fx_dib.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_dataavail.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"
#include "testing/embedder_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/image_diff/image_diff_png.h"
#include "testing/test_loader.h"
#include "testing/utils/bitmap_saver.h"
#include "testing/utils/file_util.h"
#include "testing/utils/hash.h"
#include "testing/utils/path_service.h"
#include "testing/utils/pixel_diff_util.h"
#include "testing/utils/png_encode.h"
#include "third_party/simdutf/simdutf.h"

namespace {

int GetBitmapBytesPerPixel(FPDF_BITMAP bitmap) {
  return EmbedderTest::BytesPerPixelForFormat(FPDFBitmap_GetFormat(bitmap));
}

#if BUILDFLAG(IS_WIN)
int CALLBACK GetRecordProc(HDC hdc,
                           HANDLETABLE* handle_table,
                           const ENHMETARECORD* record,
                           int objects_count,
                           LPARAM param) {
  auto& records = *reinterpret_cast<std::vector<const ENHMETARECORD*>*>(param);
  records.push_back(record);
  return 1;
}
#endif  // BUILDFLAG(IS_WIN)

// These "jump" into the delegate to do actual testing.
void UnsupportedHandlerTrampoline(UNSUPPORT_INFO* info, int type) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  delegate->UnsupportedHandler(type);
}

int AlertTrampoline(IPDF_JSPLATFORM* platform,
                    FPDF_WIDESTRING message,
                    FPDF_WIDESTRING title,
                    int type,
                    int icon) {
  auto* delegate = static_cast<EmbedderTest*>(platform)->GetDelegate();
  return delegate->Alert(message, title, type, icon);
}

int SetTimerTrampoline(FPDF_FORMFILLINFO* info, int msecs, TimerCallback fn) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->SetTimer(msecs, fn);
}

void KillTimerTrampoline(FPDF_FORMFILLINFO* info, int id) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->KillTimer(id);
}

FPDF_PAGE GetPageTrampoline(FPDF_FORMFILLINFO* info,
                            FPDF_DOCUMENT document,
                            int page_index) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->GetPage(info, document, page_index);
}

void DoURIActionTrampoline(FPDF_FORMFILLINFO* info, FPDF_BYTESTRING uri) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->DoURIAction(uri);
}

void DoGoToActionTrampoline(FPDF_FORMFILLINFO* info,
                            int page_index,
                            int zoom_mode,
                            float* pos_array,
                            int array_size) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->DoGoToAction(info, page_index, zoom_mode, pos_array,
                                array_size);
}

void OnFocusChangeTrampoline(FPDF_FORMFILLINFO* info,
                             FPDF_ANNOTATION annot,
                             int page_index) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->OnFocusChange(info, annot, page_index);
}

void DoURIActionWithKeyboardModifierTrampoline(FPDF_FORMFILLINFO* info,
                                               FPDF_BYTESTRING uri,
                                               int modifiers) {
  auto* delegate = static_cast<EmbedderTest*>(info)->GetDelegate();
  return delegate->DoURIActionWithKeyboardModifier(info, uri, modifiers);
}

// These do nothing (but must return a reasonable default value).
void InvalidateStub(FPDF_FORMFILLINFO* pThis,
                    FPDF_PAGE page,
                    double left,
                    double top,
                    double right,
                    double bottom) {}

void OutputSelectedRectStub(FPDF_FORMFILLINFO* pThis,
                            FPDF_PAGE page,
                            double left,
                            double top,
                            double right,
                            double bottom) {}

void SetCursorStub(FPDF_FORMFILLINFO* pThis, int nCursorType) {}

FPDF_SYSTEMTIME GetLocalTimeStub(FPDF_FORMFILLINFO* pThis) {
  return {122, 11, 6, 28, 12, 59, 59, 500};
}

void OnChangeStub(FPDF_FORMFILLINFO* pThis) {}

FPDF_PAGE GetCurrentPageStub(FPDF_FORMFILLINFO* pThis, FPDF_DOCUMENT document) {
  return GetPageTrampoline(pThis, document, 0);
}

int GetRotationStub(FPDF_FORMFILLINFO* pThis, FPDF_PAGE page) {
  return 0;
}

void ExecuteNamedActionStub(FPDF_FORMFILLINFO* pThis, FPDF_BYTESTRING name) {}

void SetTextFieldFocusStub(FPDF_FORMFILLINFO* pThis,
                           FPDF_WIDESTRING value,
                           FPDF_DWORD valueLen,
                           FPDF_BOOL is_focus) {}

#ifdef PDF_ENABLE_XFA
void DisplayCaretStub(FPDF_FORMFILLINFO* pThis,
                      FPDF_PAGE page,
                      FPDF_BOOL bVisible,
                      double left,
                      double top,
                      double right,
                      double bottom) {}

int GetCurrentPageIndexStub(FPDF_FORMFILLINFO* pThis, FPDF_DOCUMENT document) {
  return 0;
}

void SetCurrentPageStub(FPDF_FORMFILLINFO* pThis,
                        FPDF_DOCUMENT document,
                        int iCurPage) {}

void GotoURLStub(FPDF_FORMFILLINFO* pThis,
                 FPDF_DOCUMENT document,
                 FPDF_WIDESTRING wsURL) {}

void GetPageViewRectStub(FPDF_FORMFILLINFO* pThis,
                         FPDF_PAGE page,
                         double* left,
                         double* top,
                         double* right,
                         double* bottom) {
  *left = 0.0;
  *top = 0.0;
  *right = 512.0;
  *bottom = 512.0;
}

void PageEventStub(FPDF_FORMFILLINFO* pThis,
                   int page_count,
                   FPDF_DWORD event_type) {}

FPDF_BOOL PopupMenuStub(FPDF_FORMFILLINFO* pThis,
                        FPDF_PAGE page,
                        FPDF_WIDGET hWidget,
                        int menuFlag,
                        float x,
                        float y) {
  return true;
}

FPDF_FILEHANDLER* OpenFileStub(FPDF_FORMFILLINFO* pThis,
                               int fileFlag,
                               FPDF_WIDESTRING wsURL,
                               const char* mode) {
  return nullptr;
}

void EmailToStub(FPDF_FORMFILLINFO* pThis,
                 FPDF_FILEHANDLER* fileHandler,
                 FPDF_WIDESTRING pTo,
                 FPDF_WIDESTRING pSubject,
                 FPDF_WIDESTRING pCC,
                 FPDF_WIDESTRING pBcc,
                 FPDF_WIDESTRING pMsg) {}

void UploadToStub(FPDF_FORMFILLINFO* pThis,
                  FPDF_FILEHANDLER* fileHandler,
                  int fileFlag,
                  FPDF_WIDESTRING uploadTo) {}

int GetPlatformStub(FPDF_FORMFILLINFO* pThis, void* platform, int length) {
  return 0;
}

int GetLanguageStub(FPDF_FORMFILLINFO* pThis, void* language, int length) {
  return 0;
}

FPDF_FILEHANDLER* DownloadFromURLStub(FPDF_FORMFILLINFO* pThis,
                                      FPDF_WIDESTRING URL) {
  static const char kString[] = "<body>secrets</body>";
  static FPDF_FILEHANDLER kFakeFileHandler = {
      nullptr,
      [](void*) -> void {},
      [](void*) -> FPDF_DWORD { return sizeof(kString) - 1; },
      [](void*, FPDF_DWORD off, void* buffer, FPDF_DWORD size) -> FPDF_RESULT {
        FXSYS_memcpy(buffer, kString,
                     std::min<size_t>(size, sizeof(kString) - 1));
        return 0;
      },
      [](void*, FPDF_DWORD, const void*, FPDF_DWORD) -> FPDF_RESULT {
        return -1;
      },
      [](void*) -> FPDF_RESULT { return 0; },
      [](void*, FPDF_DWORD) -> FPDF_RESULT { return 0; }};
  return &kFakeFileHandler;
}

FPDF_BOOL PostRequestURLStub(FPDF_FORMFILLINFO* pThis,
                             FPDF_WIDESTRING wsURL,
                             FPDF_WIDESTRING wsData,
                             FPDF_WIDESTRING wsContentType,
                             FPDF_WIDESTRING wsEncode,
                             FPDF_WIDESTRING wsHeader,
                             FPDF_BSTR* response) {
  const char kString[] = "p\0o\0s\0t\0e\0d\0";
  FPDF_BStr_Set(response, kString, sizeof(kString) - 1);
  return true;
}

FPDF_BOOL PutRequestURLStub(FPDF_FORMFILLINFO* pThis,
                            FPDF_WIDESTRING wsURL,
                            FPDF_WIDESTRING wsData,
                            FPDF_WIDESTRING wsEncode) {
  return true;
}
#endif  // PDF_ENABLE_XFA

std::string_view GetPlatformNameSuffix() {
#if BUILDFLAG(IS_WIN)
  return "_win";
#elif BUILDFLAG(IS_APPLE)
  return "_mac";
#else
  return "_linux";
#endif
}

std::string_view GetCpuArchSuffix() {
#if BUILDFLAG(IS_APPLE) && !defined(ARCH_CPU_ARM64)
  return "_x86";
#else
  return "";
#endif  // BUILDFLAG(IS_APPLE) && !defined(ARCH_CPU_ARM64)
}

int GetPlatformMaxPixelDelta() {
#if BUILDFLAG(IS_APPLE) && !defined(ARCH_CPU_ARM64)
  return 1;
#else
  return 0;
#endif  // BUILDFLAG(IS_APPLE) && !defined(ARCH_CPU_ARM64)
}

std::string GetEmbedderTestExpectationPath(
    std::string_view expectation_png_name) {
  std::string path = PathService::GetTestFilePath("embedder_tests");
  if (path.empty()) {
    return std::string();
  }

  path.push_back(PATH_SEPARATOR);
  path.append(expectation_png_name);
  path.append(".png");
  return path;
}

std::vector<std::string> GetEmbedderTestExpectationsWithSuffixPath(
    std::string_view expectation_png_name) {
  const std::string basename(expectation_png_name);
  const std::string renderer =
      CFX_DefaultRenderDevice::UseSkiaRenderer() ? "_skia" : "_agg";
  const std::string platform_suffix(GetPlatformNameSuffix());
  const std::string cpu_arch_suffix(GetCpuArchSuffix());
  const bool has_cpu_arch_suffix = !cpu_arch_suffix.empty();
  std::vector<std::string> expectation_names;
  expectation_names.reserve(has_cpu_arch_suffix ? 6 : 4);

  if (has_cpu_arch_suffix) {
    expectation_names.push_back(basename + renderer + platform_suffix +
                                cpu_arch_suffix);
  }
  expectation_names.push_back(basename + renderer + platform_suffix);
  expectation_names.push_back(basename + renderer);

  if (has_cpu_arch_suffix) {
    expectation_names.push_back(basename + platform_suffix + cpu_arch_suffix);
  }
  expectation_names.push_back(basename + platform_suffix);
  expectation_names.push_back(basename);

  std::vector<std::string> results;
  for (const auto& name : expectation_names) {
    results.push_back(GetEmbedderTestExpectationPath(name));
    if (results.back().empty()) {
      return {};
    }
  }
  return results;
}

struct DecodedPng {
  int width = -1;
  int height = -1;
  std::vector<uint8_t> pixel_data;  // BGRA
};

DecodedPng DecodePngData(pdfium::span<const uint8_t> png_data) {
  DecodedPng results;

  int width = -1;
  int height = -1;
  std::vector<uint8_t> decoded_png = image_diff_png::DecodePNG(
      png_data, /*reverse_byte_order=*/true, &width, &height);
  if (width > 0 && height > 0 && !decoded_png.empty()) {
    results.width = width;
    results.height = height;
    results.pixel_data = std::move(decoded_png);
  }
  return results;
}

int CompareBGRxBitmapToPng(pdfium::span<const uint8_t> bitmap_span,
                           size_t bitmap_stride,
                           const DecodedPng& decoded_png,
                           int max_pixel_per_channel_delta) {
  const size_t unsigned_width = static_cast<size_t>(decoded_png.width);
  auto decoded_png_span32 = fxcrt::reinterpret_span<const uint32_t>(
      pdfium::span(decoded_png.pixel_data));
  int pixels_different = 0;
  for (int h = 0; h < decoded_png.height; ++h) {
    auto decoded_png_row = decoded_png_span32.first(unsigned_width);
    decoded_png_span32 = decoded_png_span32.subspan(unsigned_width);
    auto bitmap_row = fxcrt::reinterpret_span<const uint32_t>(
        bitmap_span.first(bitmap_stride));
    bitmap_span = bitmap_span.subspan(bitmap_stride);
    for (int w = 0; w < decoded_png.width; ++w) {
      uint32_t png_pixel = decoded_png_row[w];
      uint32_t bitmap_pixel = bitmap_row[w];
      if (png_pixel == bitmap_pixel) {
        continue;
      }

      if (max_pixel_per_channel_delta == 0 ||
          MaxPixelPerChannelDelta(png_pixel, bitmap_pixel) >
              max_pixel_per_channel_delta) {
        ++pixels_different;
      }
    }
  }
  return pixels_different;
}

int CompareGrayBitmapToPng(pdfium::span<const uint8_t> bitmap_span,
                           size_t bitmap_stride,
                           const DecodedPng& decoded_png,
                           int max_pixel_per_channel_delta) {
  const int width = decoded_png.width;
  const size_t dest_row_width = static_cast<size_t>(width);
  const int height = decoded_png.height;
  const size_t bgrx_stride = width * sizeof(FX_BGRA_STRUCT<uint8_t>);
  std::vector<uint8_t> bgrx_buffer(bgrx_stride * height);
  auto bgrx_span = fxcrt::reinterpret_span<FX_BGRA_STRUCT<uint8_t>>(
      pdfium::span<uint8_t>(bgrx_buffer));
  for (int h = 0; h < height; ++h) {
    auto src_row = bitmap_span.subspan(h * bitmap_stride, bitmap_stride);
    auto dest_row = bgrx_span.take_first(dest_row_width);
    for (auto [src_pixel, dest_pixel] : fxcrt::Zip(src_row, dest_row)) {
      dest_pixel.blue = src_pixel;
      dest_pixel.green = src_pixel;
      dest_pixel.red = src_pixel;
      dest_pixel.alpha = 255;
    }
  }
  return CompareBGRxBitmapToPng(bgrx_buffer, bgrx_stride, decoded_png,
                                max_pixel_per_channel_delta);
}

#ifdef PDF_USE_SKIA
int CompareBGRxPremultBitmapToPng(pdfium::span<const uint8_t> bitmap_span,
                                  size_t bitmap_stride,
                                  const DecodedPng& decoded_png,
                                  int max_pixel_per_channel_delta) {
  std::vector<uint8_t> bitmap_data(bitmap_span.begin(), bitmap_span.end());
  pdfium::span<uint8_t> converted_bitmap_span{bitmap_data};

  for (int h = 0; h < decoded_png.height; ++h) {
    auto bitmap_row = fxcrt::reinterpret_span<FX_BGRA_STRUCT<uint8_t>>(
        converted_bitmap_span.first(bitmap_stride));
    converted_bitmap_span = converted_bitmap_span.subspan(bitmap_stride);
    for (int w = 0; w < decoded_png.width; ++w) {
      bitmap_row[w] = UnPreMultiplyColor(bitmap_row[w]);
    }
  }
  return CompareBGRxBitmapToPng(bitmap_span, bitmap_stride, decoded_png,
                                max_pixel_per_channel_delta);
}
#endif  // PDF_USE_SKIA

int CompareBGRBitmapToPng(pdfium::span<const uint8_t> bitmap_span,
                          size_t bitmap_stride,
                          const DecodedPng& decoded_png,
                          int max_pixel_per_channel_delta) {
  const int width = decoded_png.width;
  const size_t dest_row_width = static_cast<size_t>(width);
  const int height = decoded_png.height;
  const size_t bgrx_stride = width * sizeof(FX_BGRA_STRUCT<uint8_t>);
  std::vector<uint8_t> bgrx_buffer(bgrx_stride * height);
  auto bgrx_span = fxcrt::reinterpret_span<FX_BGRA_STRUCT<uint8_t>>(
      pdfium::span<uint8_t>(bgrx_buffer));
  for (int h = 0; h < height; ++h) {
    auto src_row = fxcrt::reinterpret_span<const FX_BGR_STRUCT<uint8_t>>(
        bitmap_span.subspan(h * bitmap_stride, bitmap_stride));
    auto dest_row = bgrx_span.take_first(dest_row_width);
    for (int w = 0; w < width; ++w) {
      dest_row[w].blue = src_row[w].blue;
      dest_row[w].green = src_row[w].green;
      dest_row[w].red = src_row[w].red;
      dest_row[w].alpha = 255;
    }
  }
  return CompareBGRxBitmapToPng(bgrx_buffer, bgrx_stride, decoded_png,
                                max_pixel_per_channel_delta);
}

std::string EncodeBase64(pdfium::span<const uint8_t> png) {
  std::string base64_png(simdutf::base64_length_from_binary(png.size()), '\0');
  size_t base64_len = simdutf::binary_to_base64(png, base64_png);
  CHECK_EQ(base64_len, base64_png.size());
  return "data:image/png;base64," + base64_png;
}

std::string EncodeBase64Png(FPDF_BITMAP bitmap) {
  return EncodeBase64(EncodePng(bitmap));
}

void CompareBitmapToPngData(FPDF_BITMAP bitmap,
                            pdfium::span<const uint8_t> png_data,
                            int max_pixel_per_channel_delta) {
  DecodedPng decoded_png = DecodePngData(png_data);
  ASSERT_GT(decoded_png.width, 0);
  ASSERT_GT(decoded_png.height, 0);
  ASSERT_FALSE(decoded_png.pixel_data.empty());

  const int stride = FPDFBitmap_GetStride(bitmap);
  const int width = FPDFBitmap_GetWidth(bitmap);
  const int height = FPDFBitmap_GetHeight(bitmap);
  ASSERT_GT(stride, 0);
  ASSERT_EQ(width, decoded_png.width);
  ASSERT_EQ(height, decoded_png.height);

  FX_SAFE_SIZE_T size = stride;
  size *= height;
  auto bitmap_span =
      pdfium::span(static_cast<const uint8_t*>(FPDFBitmap_GetBuffer(bitmap)),
                   size.ValueOrDie());

  int pixels_different;
  switch (FPDFBitmap_GetFormat(bitmap)) {
    case FPDFBitmap_Gray:
      pixels_different = CompareGrayBitmapToPng(
          bitmap_span, stride, decoded_png, max_pixel_per_channel_delta);
      break;
    case FPDFBitmap_BGR:
      pixels_different = CompareBGRBitmapToPng(bitmap_span, stride, decoded_png,
                                               max_pixel_per_channel_delta);
      break;
    case FPDFBitmap_BGRx:
    case FPDFBitmap_BGRA: {
      pixels_different = CompareBGRxBitmapToPng(
          bitmap_span, stride, decoded_png, max_pixel_per_channel_delta);
      break;
    }
#ifdef PDF_USE_SKIA
    case FPDFBitmap_BGRA_Premul:
      pixels_different = CompareBGRxPremultBitmapToPng(
          bitmap_span, stride, decoded_png, max_pixel_per_channel_delta);
      break;
#endif  // PDF_USE_SKIA
    default:
      // Support other formats as-needed.
      NOTREACHED();
  }
  EXPECT_EQ(pixels_different, 0)
      << ", Actual pixels (open in browser):\n"
      << EncodeBase64Png(bitmap) << "\nExpected pixels (open in browser):\n"
      << EncodeBase64(png_data);
}

}  // namespace

EmbedderTest::EmbedderTest()
    : default_delegate_(std::make_unique<EmbedderTest::Delegate>()),
      delegate_(default_delegate_.get()) {
  FPDF_FILEWRITE::version = 1;
  FPDF_FILEWRITE::WriteBlock = WriteBlockCallback;
}

EmbedderTest::~EmbedderTest() = default;

void EmbedderTest::SetUp() {
  UNSUPPORT_INFO* info = static_cast<UNSUPPORT_INFO*>(this);
  *info = {
      .version = 1,
      .FSDK_UnSupport_Handler = UnsupportedHandlerTrampoline,
  };
  FSDK_SetUnSpObjProcessHandler(info);
}

void EmbedderTest::TearDown() {
  // Use an EXPECT_EQ() here and continue to let TearDown() finish as cleanly as
  // possible. This can fail when an DCHECK test fails in a test case.
  EXPECT_EQ(0U, page_map_.size());
  EXPECT_EQ(0U, saved_page_map_.size());
  if (document()) {
    CloseDocument();
  }
}

void EmbedderTest::CreateEmptyDocument() {
  CreateEmptyDocumentWithoutFormFillEnvironment();
  form_handle_.reset(SetupFormFillEnvironment(
      document(), JavaScriptOption::kEnableJavaScript));
}

void EmbedderTest::CreateEmptyDocumentWithoutFormFillEnvironment() {
  document_.reset(FPDF_CreateNewDocument());
  CHECK(document_);
}

bool EmbedderTest::OpenDocument(const std::string& filename) {
  return OpenDocumentWithOptions(filename, nullptr,
                                 LinearizeOption::kDefaultLinearize,
                                 JavaScriptOption::kEnableJavaScript);
}

bool EmbedderTest::OpenDocumentLinearized(const std::string& filename) {
  return OpenDocumentWithOptions(filename, nullptr,
                                 LinearizeOption::kMustLinearize,
                                 JavaScriptOption::kEnableJavaScript);
}

bool EmbedderTest::OpenDocumentWithPassword(const std::string& filename,
                                            const char* password) {
  return OpenDocumentWithOptions(filename, password,
                                 LinearizeOption::kDefaultLinearize,
                                 JavaScriptOption::kEnableJavaScript);
}

bool EmbedderTest::OpenDocumentWithoutJavaScript(const std::string& filename) {
  return OpenDocumentWithOptions(filename, nullptr,
                                 LinearizeOption::kDefaultLinearize,
                                 JavaScriptOption::kDisableJavaScript);
}

bool EmbedderTest::OpenDocumentWithOptions(const std::string& filename,
                                           const char* password,
                                           LinearizeOption linearize_option,
                                           JavaScriptOption javascript_option) {
  std::string file_path = PathService::GetTestFilePath(filename);
  if (file_path.empty()) {
    return false;
  }

  file_contents_ = GetFileContents(file_path.c_str());
  if (file_contents_.empty()) {
    return false;
  }

  EXPECT_TRUE(!loader_);
  loader_ = std::make_unique<TestLoader>(file_contents_);

  file_access_ = {
      .m_FileLen = pdfium::checked_cast<unsigned long>(file_contents_.size()),
      .m_GetBlock = TestLoader::GetBlock,
      .m_Param = loader_.get(),
  };

  fake_file_access_ = std::make_unique<FakeFileAccess>(&file_access_);
  return OpenDocumentHelper(password, linearize_option, javascript_option,
                            fake_file_access_.get(), &document_, &avail_,
                            &form_handle_);
}

bool EmbedderTest::OpenDocumentHelper(const char* password,
                                      LinearizeOption linearize_option,
                                      JavaScriptOption javascript_option,
                                      FakeFileAccess* network_simulator,
                                      ScopedFPDFDocument* document,
                                      ScopedFPDFAvail* avail,
                                      ScopedFPDFFormHandle* form_handle) {
  network_simulator->AddSegment(0, 1024);
  network_simulator->SetRequestedDataAvailable();
  avail->reset(FPDFAvail_Create(network_simulator->GetFileAvail(),
                                network_simulator->GetFileAccess()));
  FPDF_AVAIL avail_ptr = avail->get();
  FPDF_DOCUMENT document_ptr = nullptr;
  if (FPDFAvail_IsLinearized(avail_ptr) == PDF_LINEARIZED) {
    int32_t nRet = PDF_DATA_NOTAVAIL;
    while (nRet == PDF_DATA_NOTAVAIL) {
      network_simulator->SetRequestedDataAvailable();
      nRet = FPDFAvail_IsDocAvail(avail_ptr,
                                  network_simulator->GetDownloadHints());
    }
    if (nRet == PDF_DATA_ERROR) {
      return false;
    }

    document->reset(FPDFAvail_GetDocument(avail_ptr, password));
    document_ptr = document->get();
    if (!document_ptr) {
      return false;
    }

    nRet = PDF_DATA_NOTAVAIL;
    while (nRet == PDF_DATA_NOTAVAIL) {
      network_simulator->SetRequestedDataAvailable();
      nRet = FPDFAvail_IsFormAvail(avail_ptr,
                                   network_simulator->GetDownloadHints());
    }
    if (nRet == PDF_FORM_ERROR) {
      return false;
    }

    int page_count = FPDF_GetPageCount(document_ptr);
    for (int i = 0; i < page_count; ++i) {
      nRet = PDF_DATA_NOTAVAIL;
      while (nRet == PDF_DATA_NOTAVAIL) {
        network_simulator->SetRequestedDataAvailable();
        nRet = FPDFAvail_IsPageAvail(avail_ptr, i,
                                     network_simulator->GetDownloadHints());
      }
      if (nRet == PDF_DATA_ERROR) {
        return false;
      }
    }
  } else {
    if (linearize_option == LinearizeOption::kMustLinearize) {
      return false;
    }
    network_simulator->SetWholeFileAvailable();
    document->reset(
        FPDF_LoadCustomDocument(network_simulator->GetFileAccess(), password));
    document_ptr = document->get();
    if (!document_ptr) {
      return false;
    }
  }
  form_handle->reset(SetupFormFillEnvironment(document_ptr, javascript_option));

  int doc_type = FPDF_GetFormType(document_ptr);
  if (doc_type == FORMTYPE_XFA_FULL || doc_type == FORMTYPE_XFA_FOREGROUND) {
    FPDF_LoadXFA(document_ptr);
  }

  return true;
}

void EmbedderTest::CloseDocument() {
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_WC);
  form_handle_.reset();
  document_.reset();
  avail_.reset();
  fake_file_access_.reset();
  file_access_ = {};
  loader_.reset();
  file_contents_ = {};
}

FPDF_FORMHANDLE EmbedderTest::SetupFormFillEnvironment(
    FPDF_DOCUMENT doc,
    JavaScriptOption javascript_option) {
  IPDF_JSPLATFORM* platform = static_cast<IPDF_JSPLATFORM*>(this);
  *platform = {};
  platform->version = 3;
  platform->app_alert = AlertTrampoline;

  FPDF_FORMFILLINFO* formfillinfo = static_cast<FPDF_FORMFILLINFO*>(this);
  *formfillinfo = {};
  formfillinfo->version = form_fill_info_version_;
  formfillinfo->FFI_Invalidate = InvalidateStub;
  formfillinfo->FFI_OutputSelectedRect = OutputSelectedRectStub;
  formfillinfo->FFI_SetCursor = SetCursorStub;
  formfillinfo->FFI_SetTimer = SetTimerTrampoline;
  formfillinfo->FFI_KillTimer = KillTimerTrampoline;
  formfillinfo->FFI_GetLocalTime = GetLocalTimeStub;
  formfillinfo->FFI_OnChange = OnChangeStub;
  formfillinfo->FFI_GetPage = GetPageTrampoline;
  formfillinfo->FFI_GetCurrentPage = GetCurrentPageStub;
  formfillinfo->FFI_GetRotation = GetRotationStub;
  formfillinfo->FFI_ExecuteNamedAction = ExecuteNamedActionStub;
  formfillinfo->FFI_SetTextFieldFocus = SetTextFieldFocusStub;
  formfillinfo->FFI_DoURIAction = DoURIActionTrampoline;
  formfillinfo->FFI_DoGoToAction = DoGoToActionTrampoline;
#ifdef PDF_ENABLE_XFA
  formfillinfo->FFI_DisplayCaret = DisplayCaretStub;
  formfillinfo->FFI_GetCurrentPageIndex = GetCurrentPageIndexStub;
  formfillinfo->FFI_SetCurrentPage = SetCurrentPageStub;
  formfillinfo->FFI_GotoURL = GotoURLStub;
  formfillinfo->FFI_GetPageViewRect = GetPageViewRectStub;
  formfillinfo->FFI_PageEvent = PageEventStub;
  formfillinfo->FFI_PopupMenu = PopupMenuStub;
  formfillinfo->FFI_OpenFile = OpenFileStub;
  formfillinfo->FFI_EmailTo = EmailToStub;
  formfillinfo->FFI_UploadTo = UploadToStub;
  formfillinfo->FFI_GetPlatform = GetPlatformStub;
  formfillinfo->FFI_GetLanguage = GetLanguageStub;
  formfillinfo->FFI_DownloadFromURL = DownloadFromURLStub;
  formfillinfo->FFI_PostRequestURL = PostRequestURLStub;
  formfillinfo->FFI_PutRequestURL = PutRequestURLStub;
#endif  // PDF_ENABLE_XFA
  formfillinfo->FFI_OnFocusChange = OnFocusChangeTrampoline;
  formfillinfo->FFI_DoURIActionWithKeyboardModifier =
      DoURIActionWithKeyboardModifierTrampoline;

  if (javascript_option == JavaScriptOption::kEnableJavaScript) {
    formfillinfo->m_pJsPlatform = platform;
  }

  FPDF_FORMHANDLE form_handle =
      FPDFDOC_InitFormFillEnvironment(doc, formfillinfo);
  SetInitialFormFieldHighlight(form_handle);
  return form_handle;
}

void EmbedderTest::DoOpenActions() {
  CHECK(form_handle());
  FORM_DoDocumentJSAction(form_handle());
  FORM_DoDocumentOpenAction(form_handle());
}

int EmbedderTest::GetFirstPageNum() {
  int first_page = FPDFAvail_GetFirstPageNum(document());
  (void)FPDFAvail_IsPageAvail(avail(), first_page,
                              fake_file_access_->GetDownloadHints());
  return first_page;
}

int EmbedderTest::GetPageCount() {
  int page_count = FPDF_GetPageCount(document());
  for (int i = 0; i < page_count; ++i) {
    (void)FPDFAvail_IsPageAvail(avail(), i,
                                fake_file_access_->GetDownloadHints());
  }
  return page_count;
}

EmbedderTest::ScopedPage EmbedderTest::LoadScopedPage(int page_index) {
  return ScopedPage(this, page_index);
}

FPDF_PAGE EmbedderTest::LoadPage(int page_index) {
  return LoadPageCommon(page_index, /*do_events=*/true);
}

FPDF_PAGE EmbedderTest::LoadPageNoEvents(int page_index) {
  return LoadPageCommon(page_index, /*do_events=*/false);
}

FPDF_PAGE EmbedderTest::LoadPageCommon(int page_index, bool do_events) {
  CHECK(form_handle());
  CHECK_GE(page_index, 0);
  CHECK(!pdfium::Contains(page_map_, page_index));

  FPDF_PAGE page = FPDF_LoadPage(document(), page_index);
  if (!page) {
    return nullptr;
  }

  if (do_events) {
    FORM_OnAfterLoadPage(page, form_handle());
    FORM_DoPageAAction(page, form_handle(), FPDFPAGE_AACTION_OPEN);
  }
  page_map_[page_index] = page;
  return page;
}

void EmbedderTest::UnloadPage(FPDF_PAGE page) {
  UnloadPageCommon(page, true);
}

void EmbedderTest::UnloadPageNoEvents(FPDF_PAGE page) {
  UnloadPageCommon(page, false);
}

void EmbedderTest::UnloadPageCommon(FPDF_PAGE page, bool do_events) {
  CHECK(form_handle());
  int page_index = GetPageNumberForLoadedPage(page);
  CHECK_GE(page_index, 0);

  if (do_events) {
    FORM_DoPageAAction(page, form_handle(), FPDFPAGE_AACTION_CLOSE);
    FORM_OnBeforeClosePage(page, form_handle());
  }
  FPDF_ClosePage(page);
  page_map_.erase(page_index);
}

void EmbedderTest::SetInitialFormFieldHighlight(FPDF_FORMHANDLE form) {
  FPDF_SetFormFieldHighlightColor(form, FPDF_FORMFIELD_UNKNOWN, 0xFFE4DD);
  FPDF_SetFormFieldHighlightAlpha(form, 100);
}

ScopedFPDFBitmap EmbedderTest::RenderLoadedPage(FPDF_PAGE page) {
  return RenderLoadedPageWithFlags(page, 0);
}

ScopedFPDFBitmap EmbedderTest::RenderLoadedPageWithFlags(FPDF_PAGE page,
                                                         int flags) {
  int page_index = GetPageNumberForLoadedPage(page);
  CHECK_GE(page_index, 0);
  return RenderPageWithFlags(page, form_handle(), flags);
}

ScopedFPDFBitmap EmbedderTest::RenderSavedPage(FPDF_PAGE page) {
  return RenderSavedPageWithFlags(page, 0);
}

ScopedFPDFBitmap EmbedderTest::RenderSavedPageWithFlags(FPDF_PAGE page,
                                                        int flags) {
  int page_index = GetPageNumberForSavedPage(page);
  CHECK_GE(page_index, 0);
  return RenderPageWithFlags(page, saved_form_handle(), flags);
}

// static
ScopedFPDFBitmap EmbedderTest::RenderPageWithFlags(FPDF_PAGE page,
                                                   FPDF_FORMHANDLE handle,
                                                   int flags) {
  int width = static_cast<int>(FPDF_GetPageWidthF(page));
  int height = static_cast<int>(FPDF_GetPageHeightF(page));
  int alpha = FPDFPage_HasTransparency(page) ? 1 : 0;
  ScopedFPDFBitmap bitmap(FPDFBitmap_Create(width, height, alpha));
  FPDF_DWORD fill_color = alpha ? 0x00000000 : 0xFFFFFFFF;
  if (!FPDFBitmap_FillRect(bitmap.get(), 0, 0, width, height, fill_color)) {
    return nullptr;
  }
  FPDF_RenderPageBitmap(bitmap.get(), page, 0, 0, width, height, 0, flags);
  FPDF_FFLDraw(handle, bitmap.get(), page, 0, 0, width, height, 0, flags);
  return bitmap;
}

// static
ScopedFPDFBitmap EmbedderTest::RenderPage(FPDF_PAGE page) {
  return RenderPageWithFlags(page, nullptr, 0);
}

#if BUILDFLAG(IS_WIN)
// static
std::vector<uint8_t> EmbedderTest::RenderPageWithFlagsToEmf(FPDF_PAGE page,
                                                            int flags) {
  HDC dc = CreateEnhMetaFileA(nullptr, nullptr, nullptr, nullptr);

  int width = static_cast<int>(FPDF_GetPageWidthF(page));
  int height = static_cast<int>(FPDF_GetPageHeightF(page));
  HRGN rgn = CreateRectRgn(0, 0, width, height);
  SelectClipRgn(dc, rgn);
  DeleteObject(rgn);

  SelectObject(dc, GetStockObject(NULL_PEN));
  SelectObject(dc, GetStockObject(WHITE_BRUSH));
  // If a PS_NULL pen is used, the dimensions of the rectangle are 1 pixel less.
  Rectangle(dc, 0, 0, width + 1, height + 1);

  CHECK(FPDF_RenderPage(dc, page, 0, 0, width, height, 0, flags));

  HENHMETAFILE emf = CloseEnhMetaFile(dc);
  UINT size_in_bytes = GetEnhMetaFileBits(emf, 0, nullptr);
  std::vector<uint8_t> buffer(size_in_bytes);
  GetEnhMetaFileBits(emf, size_in_bytes, buffer.data());
  DeleteEnhMetaFile(emf);
  return buffer;
}

// static
std::string EmbedderTest::GetPostScriptFromEmf(
    pdfium::span<const uint8_t> emf_data) {
  // This comes from Emf::InitFromData() in Chromium.
  HENHMETAFILE emf = SetEnhMetaFileBits(
      pdfium::checked_cast<UINT>(emf_data.size()), emf_data.data());
  if (!emf) {
    return std::string();
  }

  // This comes from Emf::Enumerator::Enumerator() in Chromium.
  std::vector<const ENHMETARECORD*> records;
  if (!EnumEnhMetaFile(nullptr, emf, &GetRecordProc, &records, nullptr)) {
    DeleteEnhMetaFile(emf);
    return std::string();
  }

  // This comes from PostScriptMetaFile::SafePlayback() in Chromium.
  std::string ps_data;
  for (const auto* record : records) {
    if (record->iType != EMR_GDICOMMENT) {
      continue;
    }

    // PostScript data is encapsulated inside EMF comment records.
    // The first two bytes of the comment indicate the string length. The rest
    // is the actual string data.
    const auto* comment = reinterpret_cast<const EMRGDICOMMENT*>(record);
    const char* data = reinterpret_cast<const char*>(comment->Data);
    uint16_t size = *reinterpret_cast<const uint16_t*>(data);
    data += 2;
    ps_data.append(data, size);
  }
  DeleteEnhMetaFile(emf);
  return ps_data;
}
#endif  // BUILDFLAG(IS_WIN)

EmbedderTest::ScopedSavedDoc EmbedderTest::OpenScopedSavedDocument() {
  return ScopedSavedDoc(this);
}

FPDF_DOCUMENT EmbedderTest::OpenSavedDocument() {
  return OpenSavedDocumentWithPassword(nullptr);
}

// static
int EmbedderTest::BytesPerPixelForFormat(int format) {
  FXDIB_Format fx_format = FXDIBFormatFromFPDFFormat(format);
  CHECK_NE(fx_format, FXDIB_Format::kInvalid);
  return GetCompsFromFormat(fx_format);
}

FPDF_DOCUMENT EmbedderTest::OpenSavedDocumentWithPassword(
    const char* password) {
  // Copy data to prevent clearing it before saved document close.
  saved_document_file_data_ = data_string_;
  saved_file_access_ = {
      .m_FileLen = pdfium::checked_cast<unsigned long>(data_string_.size()),
      .m_GetBlock = GetBlockFromString,
      .m_Param = &saved_document_file_data_,
  };
  saved_fake_file_access_ =
      std::make_unique<FakeFileAccess>(&saved_file_access_);

  EXPECT_TRUE(OpenDocumentHelper(
      password, LinearizeOption::kDefaultLinearize,
      JavaScriptOption::kEnableJavaScript, saved_fake_file_access_.get(),
      &saved_document_, &saved_avail_, &saved_form_handle_));
  return saved_document();
}

void EmbedderTest::CloseSavedDocument() {
  CHECK(saved_document());

  saved_form_handle_.reset();
  saved_document_.reset();
  saved_avail_.reset();
}

EmbedderTest::ScopedSavedPage EmbedderTest::LoadScopedSavedPage(
    int page_index) {
  return ScopedSavedPage(this, page_index);
}

FPDF_PAGE EmbedderTest::LoadSavedPage(int page_index) {
  CHECK(saved_form_handle());
  CHECK_GE(page_index, 0);
  CHECK(!pdfium::Contains(saved_page_map_, page_index));

  FPDF_PAGE page = FPDF_LoadPage(saved_document(), page_index);
  if (!page) {
    return nullptr;
  }

  FORM_OnAfterLoadPage(page, saved_form_handle());
  FORM_DoPageAAction(page, saved_form_handle(), FPDFPAGE_AACTION_OPEN);
  saved_page_map_[page_index] = page;
  return page;
}

void EmbedderTest::CloseSavedPage(FPDF_PAGE page) {
  CHECK(saved_form_handle());

  int page_index = GetPageNumberForSavedPage(page);
  CHECK_GE(page_index, 0);

  FORM_DoPageAAction(page, saved_form_handle(), FPDFPAGE_AACTION_CLOSE);
  FORM_OnBeforeClosePage(page, saved_form_handle());
  FPDF_ClosePage(page);

  saved_page_map_.erase(page_index);
}

void EmbedderTest::VerifySavedRenderingToPng(
    FPDF_PAGE page,
    std::string_view expectation_png_name) {
  ScopedFPDFBitmap bitmap = VerifySavedRenderingCommon(page);
  CompareBitmapToPng(bitmap.get(), expectation_png_name);
}

void EmbedderTest::VerifySavedRenderingToPngWithExpectationSuffix(
    FPDF_PAGE page,
    std::string_view expectation_png_name) {
  ScopedFPDFBitmap bitmap = VerifySavedRenderingCommon(page);
  CompareBitmapToPngWithExpectationSuffix(bitmap.get(), expectation_png_name);
}

void EmbedderTest::VerifySavedRendering(FPDF_PAGE page,
                                        int width,
                                        int height,
                                        const char* md5) {
  ScopedFPDFBitmap bitmap = VerifySavedRenderingCommon(page);
  CompareBitmap(bitmap.get(), width, height, md5);
}

ScopedFPDFBitmap EmbedderTest::VerifySavedRenderingCommon(FPDF_PAGE page) {
  CHECK(page);
  CHECK(saved_document());
  return RenderSavedPageWithFlags(page, FPDF_ANNOT);
}

void EmbedderTest::VerifySavedDocumentToPng(
    std::string_view expectation_png_name) {
  ScopedFPDFBitmap bitmap = VerifySavedDocumentCommon();
  CompareBitmapToPng(bitmap.get(), expectation_png_name);
}

void EmbedderTest::VerifySavedDocumentToPngWithExpectationSuffix(
    std::string_view expectation_png_name) {
  ScopedFPDFBitmap bitmap = VerifySavedDocumentCommon();
  CompareBitmapToPngWithExpectationSuffix(bitmap.get(), expectation_png_name);
}

void EmbedderTest::VerifySavedDocument(int width, int height, const char* md5) {
  ScopedFPDFBitmap bitmap = VerifySavedDocumentCommon();
  CompareBitmap(bitmap.get(), width, height, md5);
}

ScopedFPDFBitmap EmbedderTest::VerifySavedDocumentCommon() {
  ScopedSavedDoc saved_doc = OpenScopedSavedDocument();
  if (!saved_doc) {
    return nullptr;
  }

  ScopedSavedPage page = LoadScopedSavedPage(0);
  return VerifySavedRenderingCommon(page.get());
}

void EmbedderTest::SetWholeFileAvailable() {
  CHECK(fake_file_access_);
  fake_file_access_->SetWholeFileAvailable();
}

void EmbedderTest::SetDocumentFromAvail() {
  document_.reset(FPDFAvail_GetDocument(avail(), nullptr));
}

void EmbedderTest::CreateAvail(FX_FILEAVAIL* file_avail,
                               FPDF_FILEACCESS* file) {
  avail_.reset(FPDFAvail_Create(file_avail, file));
}

FPDF_PAGE EmbedderTest::Delegate::GetPage(FPDF_FORMFILLINFO* info,
                                          FPDF_DOCUMENT document,
                                          int page_index) {
  EmbedderTest* test = static_cast<EmbedderTest*>(info);
  auto it = test->page_map_.find(page_index);
  return it != test->page_map_.end() ? it->second : nullptr;
}

// static
std::string EmbedderTest::HashBitmap(FPDF_BITMAP bitmap) {
  int stride = FPDFBitmap_GetStride(bitmap);
  int usable_bytes_per_row =
      GetBitmapBytesPerPixel(bitmap) * FPDFBitmap_GetWidth(bitmap);
  int height = FPDFBitmap_GetHeight(bitmap);
  auto span = pdfium::span(static_cast<uint8_t*>(FPDFBitmap_GetBuffer(bitmap)),
                           static_cast<size_t>(stride) * height);

  CRYPT_md5_context context = CRYPT_MD5Start();
  for (int i = 0; i < height; ++i) {
    CRYPT_MD5Update(&context,
                    span.subspan(static_cast<size_t>(i * stride),
                                 static_cast<size_t>(usable_bytes_per_row)));
  }
  uint8_t digest[16];
  CRYPT_MD5Finish(&context, digest);
  return CryptToBase16(digest);
}

// static
void EmbedderTest::WriteBitmapToPng(FPDF_BITMAP bitmap,
                                    const std::string& filename) {
  BitmapSaver::WriteBitmapToPng(bitmap, filename);
}

// static
void EmbedderTest::CompareBitmapToPng(FPDF_BITMAP bitmap,
                                      std::string_view expectation_png_name) {
  std::string png_path = GetEmbedderTestExpectationPath(expectation_png_name);
  std::vector<uint8_t> png_data = GetFileContents(png_path.c_str());
  ASSERT_FALSE(png_data.empty())
      << "No expectation file matching " << expectation_png_name
      << ", Actual pixels (open in browser):\n"
      << EncodeBase64Png(bitmap);
  SCOPED_TRACE(testing::Message() << "CompareBitmapToPng() with " << png_path);
  CompareBitmapToPngData(bitmap, png_data, /*max_pixel_per_channel_delta=*/0);
  if (EmbedderTestEnvironment::GetInstance()->write_pngs()) {
    WriteBitmapToPng(bitmap, png_path);
  }
}

// static
void EmbedderTest::CompareBitmapToPngWithExpectationSuffix(
    FPDF_BITMAP bitmap,
    std::string_view expectation_png_name,
    int max_pixel_per_channel_delta) {
  std::vector<std::string> candidate_png_path =
      GetEmbedderTestExpectationsWithSuffixPath(expectation_png_name);
  for (const std::string& png_path : candidate_png_path) {
    if (!CanReadFile(png_path.c_str())) {
      continue;
    }

    SCOPED_TRACE(testing::Message()
                 << "CompareBitmapToPngWithExpectationSuffix() with "
                 << png_path);
    CompareBitmapToPngData(bitmap, GetFileContents(png_path.c_str()),
                           max_pixel_per_channel_delta);
    if (EmbedderTestEnvironment::GetInstance()->write_pngs()) {
      WriteBitmapToPng(bitmap, png_path);
    }
    return;
  }

  ADD_FAILURE() << "No expectation file matching " << expectation_png_name
                << ", Actual pixels (open in browser):\n"
                << EncodeBase64Png(bitmap);
}

// static
void EmbedderTest::CompareBitmapToPngWithFuzzyExpectationSuffix(
    FPDF_BITMAP bitmap,
    std::string_view expectation_png_name) {
  CompareBitmapToPngWithExpectationSuffix(bitmap, expectation_png_name,
                                          GetPlatformMaxPixelDelta());
}

// static
void EmbedderTest::CompareBitmap(FPDF_BITMAP bitmap,
                                 int expected_width,
                                 int expected_height,
                                 const char* expected_md5sum) {
  ASSERT_EQ(expected_width, FPDFBitmap_GetWidth(bitmap));
  ASSERT_EQ(expected_height, FPDFBitmap_GetHeight(bitmap));

  // The expected stride is calculated using the same formula as in
  // CFX_DIBitmap::CalculatePitchAndSize(), which sets the bitmap stride.
  const int expected_stride =
      (expected_width * GetBitmapBytesPerPixel(bitmap) * 8 + 31) / 32 * 4;
  ASSERT_EQ(expected_stride, FPDFBitmap_GetStride(bitmap));

  if (!expected_md5sum) {
    return;
  }

  std::string actual_md5sum = HashBitmap(bitmap);
  EXPECT_EQ(expected_md5sum, actual_md5sum);
  if (EmbedderTestEnvironment::GetInstance()->write_pngs()) {
    WriteBitmapToPng(bitmap, actual_md5sum + ".png");
  }
}

// static
int EmbedderTest::WriteBlockCallback(FPDF_FILEWRITE* pFileWrite,
                                     const void* data,
                                     unsigned long size) {
  EmbedderTest* pThis = static_cast<EmbedderTest*>(pFileWrite);

  pThis->data_string_.append(static_cast<const char*>(data), size);

  if (pThis->filestream_.is_open()) {
    pThis->filestream_.write(static_cast<const char*>(data), size);
  }

  return 1;
}

// static
int EmbedderTest::GetBlockFromString(void* param,
                                     unsigned long pos,
                                     unsigned char* buf,
                                     unsigned long size) {
  std::string* new_file = static_cast<std::string*>(param);
  CHECK(new_file);

  pdfium::CheckedNumeric<size_t> end = pos;
  end += size;
  CHECK_LE(end.ValueOrDie(), new_file->size());

  FXSYS_memcpy(buf, new_file->data() + pos, size);
  return 1;
}

// static
int EmbedderTest::GetPageNumberForPage(const PageNumberToHandleMap& page_map,
                                       FPDF_PAGE page) {
  for (const auto& it : page_map) {
    if (it.second == page) {
      int page_index = it.first;
      CHECK_GE(page_index, 0);
      return page_index;
    }
  }
  return -1;
}

int EmbedderTest::GetPageNumberForLoadedPage(FPDF_PAGE page) const {
  return GetPageNumberForPage(page_map_, page);
}

int EmbedderTest::GetPageNumberForSavedPage(FPDF_PAGE page) const {
  return GetPageNumberForPage(saved_page_map_, page);
}

#ifndef NDEBUG
void EmbedderTest::OpenPDFFileForWrite(const std::string& filename) {
  filestream_.open(filename, std::ios_base::binary);
}

void EmbedderTest::ClosePDFFileForWrite() {
  filestream_.close();
}
#endif

EmbedderTest::ScopedSavedDoc::ScopedSavedDoc()
    : test_(nullptr), doc_(nullptr) {}

EmbedderTest::ScopedSavedDoc::ScopedSavedDoc(EmbedderTest* test)
    : test_(test), doc_(test->OpenSavedDocument()) {}

EmbedderTest::ScopedSavedDoc::ScopedSavedDoc(ScopedSavedDoc&& that) noexcept
    : test_(std::move(that.test_)), doc_(std::exchange(that.doc_, nullptr)) {}

EmbedderTest::ScopedSavedDoc& EmbedderTest::ScopedSavedDoc::operator=(
    ScopedSavedDoc&& that) noexcept {
  test_ = std::move(that.test_);
  doc_ = std::exchange(that.doc_, nullptr);
  return *this;
}

EmbedderTest::ScopedSavedDoc::~ScopedSavedDoc() {
  if (doc_) {
    test_->CloseSavedDocument();
  }
}

EmbedderTest::ScopedPageBase::ScopedPageBase(EmbedderTest* test, FPDF_PAGE page)
    : test_(test), page_(page) {}

EmbedderTest::ScopedPageBase::~ScopedPageBase() = default;

EmbedderTest::ScopedPage::ScopedPage() : ScopedPageBase(nullptr, nullptr) {}

EmbedderTest::ScopedPage::ScopedPage(EmbedderTest* test, int page_index)
    : ScopedPageBase(test, test->LoadPage(page_index)) {}

EmbedderTest::ScopedPage::ScopedPage(EmbedderTest::ScopedPage&& that) noexcept
    : ScopedPageBase(std::move(that.test_),
                     std::exchange(that.page_, nullptr)) {}

EmbedderTest::ScopedPage& EmbedderTest::ScopedPage::operator=(
    EmbedderTest::ScopedPage&& that) noexcept {
  test_ = std::move(that.test_);
  page_ = std::exchange(that.page_, nullptr);
  return *this;
}

EmbedderTest::ScopedPage::~ScopedPage() {
  if (page_) {
    test_->UnloadPage(page_);
  }
}

EmbedderTest::ScopedSavedPage::ScopedSavedPage()
    : ScopedPageBase(nullptr, nullptr) {}

EmbedderTest::ScopedSavedPage::ScopedSavedPage(EmbedderTest* test,
                                               int page_index)
    : ScopedPageBase(test, test->LoadSavedPage(page_index)) {}

EmbedderTest::ScopedSavedPage::ScopedSavedPage(
    EmbedderTest::ScopedSavedPage&& that) noexcept
    : ScopedPageBase(std::move(that.test_),
                     std::exchange(that.page_, nullptr)) {}

EmbedderTest::ScopedSavedPage& EmbedderTest::ScopedSavedPage::operator=(
    EmbedderTest::ScopedSavedPage&& that) noexcept {
  test_ = std::move(that.test_);
  page_ = std::exchange(that.page_, nullptr);
  return *this;
}

EmbedderTest::ScopedSavedPage::~ScopedSavedPage() {
  if (page_) {
    test_->CloseSavedPage(page_);
  }
}
