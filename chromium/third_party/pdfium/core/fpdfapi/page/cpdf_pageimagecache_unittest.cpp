// Copyright 2023 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/page/cpdf_pageimagecache.h"

#include <memory>
#include <string>
#include <utility>

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/page/test_with_page_module.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fxcrt/cfx_fileaccess_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/path_service.h"

namespace pdfium {

using CPDFPageImageCacheTest = TestWithPageModule;

TEST_F(CPDFPageImageCacheTest, RenderBug1924) {
  // If you render a page with a JPEG2000 image as a thumbnail (small picture)
  // first, the image that gets cached has a low resolution. If you afterwards
  // render it full-size, you should get a larger image - the image cache will
  // be regenerate.
  {
    std::string file_path = PathService::GetTestFilePath("jpx_lzw.pdf");
    ASSERT_FALSE(file_path.empty());
    auto document =
        std::make_unique<CPDF_Document>(std::make_unique<CPDF_DocRenderData>(),
                                        std::make_unique<CPDF_DocPageData>());
    ASSERT_EQ(document->LoadDoc(
                  CFX_FileAccessStream::CreateFromFilename(file_path.c_str()),
                  nullptr),
              CPDF_Parser::SUCCESS);

    RetainPtr<CPDF_Dictionary> page_dict =
        document->GetMutablePageDictionary(0);
    ASSERT_TRUE(page_dict);
    auto page =
        pdfium::MakeRetain<CPDF_Page>(document.get(), std::move(page_dict));
    page->AddPageImageCache();
    page->ParseContent();

    CPDF_PageImageCache* page_image_cache = page->GetPageImageCache();
    ASSERT_TRUE(page_image_cache);

    CPDF_PageObject* page_obj = page->GetPageObjectByIndex(0);
    ASSERT_TRUE(page_obj);
    CPDF_ImageObject* image = page_obj->AsImage();
    ASSERT_TRUE(image);

    // Render with small scale.
    bool should_continue = page_image_cache->StartGetCachedBitmap(
        image->GetImage(), nullptr, page->GetMutablePageResources(), true,
        CPDF_ColorSpace::Family::kICCBased, false, {50, 50});
    while (should_continue) {
      should_continue = page_image_cache->Continue(nullptr);
    }

    RetainPtr<CFX_DIBBase> bitmap_small = page_image_cache->DetachCurBitmap();

    // And render with large scale.
    should_continue = page_image_cache->StartGetCachedBitmap(
        image->GetImage(), nullptr, page->GetMutablePageResources(), true,
        CPDF_ColorSpace::Family::kICCBased, false, {100, 100});
    while (should_continue) {
      should_continue = page_image_cache->Continue(nullptr);
    }

    RetainPtr<CFX_DIBBase> bitmap_large = page_image_cache->DetachCurBitmap();

    ASSERT_GT(bitmap_large->GetWidth(), bitmap_small->GetWidth());
    ASSERT_GT(bitmap_large->GetHeight(), bitmap_small->GetHeight());

    ASSERT_TRUE(page->AsPDFPage());
    page->AsPDFPage()->ClearView();
  }
}

TEST_F(CPDFPageImageCacheTest, RenderReducedDctThenFullSize) {
  // A DCTDecode (JPEG) image drawn small is decoded at reduced resolution via
  // libjpeg's scale_denom (like the JPEG2000 path above). If it is then drawn
  // larger, the cache must re-decode at a higher resolution rather than
  // upscaling the cached low-resolution bitmap. The image is 400x400 and a
  // multiple of the MCU size, so reduced decoding is allowed.
  std::string file_path = PathService::GetTestFilePath("jpeg_reduced_size.pdf");
  ASSERT_FALSE(file_path.empty());
  auto document =
      std::make_unique<CPDF_Document>(std::make_unique<CPDF_DocRenderData>(),
                                      std::make_unique<CPDF_DocPageData>());
  ASSERT_EQ(
      document->LoadDoc(
          CFX_FileAccessStream::CreateFromFilename(file_path.c_str()), nullptr),
      CPDF_Parser::SUCCESS);

  RetainPtr<CPDF_Dictionary> page_dict = document->GetMutablePageDictionary(0);
  ASSERT_TRUE(page_dict);
  auto page =
      pdfium::MakeRetain<CPDF_Page>(document.get(), std::move(page_dict));
  page->AddPageImageCache();
  page->ParseContent();

  CPDF_PageImageCache* page_image_cache = page->GetPageImageCache();
  ASSERT_TRUE(page_image_cache);

  CPDF_PageObject* page_obj = page->GetPageObjectByIndex(0);
  ASSERT_TRUE(page_obj);
  CPDF_ImageObject* image = page_obj->AsImage();
  ASSERT_TRUE(image);

  // Render with small scale: 400/50 -> 1/8, so a 50x50 bitmap.
  bool should_continue = page_image_cache->StartGetCachedBitmap(
      image->GetImage(), nullptr, page->GetMutablePageResources(), true,
      CPDF_ColorSpace::Family::kUnknown, false, {50, 50});
  while (should_continue) {
    should_continue = page_image_cache->Continue(nullptr);
  }
  RetainPtr<CFX_DIBBase> bitmap_small = page_image_cache->DetachCurBitmap();
  ASSERT_TRUE(bitmap_small);
  EXPECT_EQ(bitmap_small->GetWidth(), 50);
  EXPECT_EQ(bitmap_small->GetHeight(), 50);

  // Render with large scale: 400/100 -> 1/4, so a 100x100 bitmap. The cached
  // 50x50 bitmap is too small, so this forces a higher-resolution re-decode.
  should_continue = page_image_cache->StartGetCachedBitmap(
      image->GetImage(), nullptr, page->GetMutablePageResources(), true,
      CPDF_ColorSpace::Family::kUnknown, false, {100, 100});
  while (should_continue) {
    should_continue = page_image_cache->Continue(nullptr);
  }
  RetainPtr<CFX_DIBBase> bitmap_large = page_image_cache->DetachCurBitmap();
  ASSERT_TRUE(bitmap_large);

  EXPECT_EQ(bitmap_large->GetWidth(), 100);
  EXPECT_EQ(bitmap_large->GetHeight(), 100);

  ASSERT_TRUE(page->AsPDFPage());
  page->AsPDFPage()->ClearView();
}

TEST_F(CPDFPageImageCacheTest, RenderReducedDctWithSMask) {
  // The base image (DCTDecode) reduces when drawn small, but its soft mask is
  // always loaded at full resolution (StartLoadMaskDIB passes a zero max size).
  // The base and mask therefore have different dimensions; this exercises that
  // path and confirms both are produced without a dimension-mismatch failure.
  std::string file_path =
      PathService::GetTestFilePath("jpeg_reduced_size_with_smask.pdf");
  ASSERT_FALSE(file_path.empty());
  auto document =
      std::make_unique<CPDF_Document>(std::make_unique<CPDF_DocRenderData>(),
                                      std::make_unique<CPDF_DocPageData>());
  ASSERT_EQ(
      document->LoadDoc(
          CFX_FileAccessStream::CreateFromFilename(file_path.c_str()), nullptr),
      CPDF_Parser::SUCCESS);

  RetainPtr<CPDF_Dictionary> page_dict = document->GetMutablePageDictionary(0);
  ASSERT_TRUE(page_dict);
  auto page =
      pdfium::MakeRetain<CPDF_Page>(document.get(), std::move(page_dict));
  page->AddPageImageCache();
  page->ParseContent();

  CPDF_PageImageCache* page_image_cache = page->GetPageImageCache();
  ASSERT_TRUE(page_image_cache);

  CPDF_PageObject* page_obj = page->GetPageObjectByIndex(0);
  ASSERT_TRUE(page_obj);
  CPDF_ImageObject* image = page_obj->AsImage();
  ASSERT_TRUE(image);

  // Draw small so the 400x400 base is decoded at reduced resolution.
  bool should_continue = page_image_cache->StartGetCachedBitmap(
      image->GetImage(), nullptr, page->GetMutablePageResources(), true,
      CPDF_ColorSpace::Family::kUnknown, /*bLoadMask=*/true, {50, 50});
  while (should_continue) {
    should_continue = page_image_cache->Continue(nullptr);
  }

  RetainPtr<CFX_DIBBase> bitmap = page_image_cache->DetachCurBitmap();
  RetainPtr<CFX_DIBBase> mask = page_image_cache->DetachCurMask();
  ASSERT_TRUE(bitmap);
  ASSERT_TRUE(mask);

  // The base is reduced; the mask is loaded at full resolution. The compositor
  // scales each to the destination independently, so the differing sizes are
  // expected and handled.
  EXPECT_EQ(bitmap->GetWidth(), 50);
  EXPECT_EQ(bitmap->GetHeight(), 50);
  EXPECT_EQ(mask->GetWidth(), 400);
  EXPECT_EQ(mask->GetHeight(), 400);

  ASSERT_TRUE(page->AsPDFPage());
  page->AsPDFPage()->ClearView();
}

TEST_F(CPDFPageImageCacheTest, NonMcuAlignedDctIsNotReduced) {
  // Reduced-size decoding is only safe when the image dimensions are a multiple
  // of the MCU size (otherwise padded edge blocks would contaminate the
  // result). This 408x408 image is 4:2:0, so its MCU is 16x16 and 408 is not a
  // multiple of it (408 % 16 == 8), even though 408 % 8 == 0. Drawing it small
  // must therefore still decode it at full resolution rather than reducing it.
  std::string file_path =
      PathService::GetTestFilePath("jpeg_unaligned_no_reduce.pdf");
  ASSERT_FALSE(file_path.empty());
  auto document =
      std::make_unique<CPDF_Document>(std::make_unique<CPDF_DocRenderData>(),
                                      std::make_unique<CPDF_DocPageData>());
  ASSERT_EQ(
      document->LoadDoc(
          CFX_FileAccessStream::CreateFromFilename(file_path.c_str()), nullptr),
      CPDF_Parser::SUCCESS);

  RetainPtr<CPDF_Dictionary> page_dict = document->GetMutablePageDictionary(0);
  ASSERT_TRUE(page_dict);
  auto page =
      pdfium::MakeRetain<CPDF_Page>(document.get(), std::move(page_dict));
  page->AddPageImageCache();
  page->ParseContent();

  CPDF_PageImageCache* page_image_cache = page->GetPageImageCache();
  ASSERT_TRUE(page_image_cache);

  CPDF_PageObject* page_obj = page->GetPageObjectByIndex(0);
  ASSERT_TRUE(page_obj);
  CPDF_ImageObject* image = page_obj->AsImage();
  ASSERT_TRUE(image);

  // Draw small: an MCU-aligned image would be reduced here, but this one must
  // not be, so the decoded bitmap stays at the full 408x408.
  bool should_continue = page_image_cache->StartGetCachedBitmap(
      image->GetImage(), nullptr, page->GetMutablePageResources(), true,
      CPDF_ColorSpace::Family::kUnknown, false, {50, 50});
  while (should_continue) {
    should_continue = page_image_cache->Continue(nullptr);
  }

  RetainPtr<CFX_DIBBase> bitmap = page_image_cache->DetachCurBitmap();
  ASSERT_TRUE(bitmap);
  EXPECT_EQ(bitmap->GetWidth(), 408);
  EXPECT_EQ(bitmap->GetHeight(), 408);

  ASSERT_TRUE(page->AsPDFPage());
  page->AsPDFPage()->ClearView();
}

}  // namespace pdfium
