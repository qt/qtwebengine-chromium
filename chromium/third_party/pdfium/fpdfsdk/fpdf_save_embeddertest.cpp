// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/span.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_ppo.h"
#include "public/fpdf_save.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/file_util.h"
#include "testing/utils/path_service.h"

using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;

class FPDFSaveEmbedderTest : public EmbedderTest {};

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDoc) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocWithVersion) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocWithBadVersion) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, -1));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));

  ClearString();
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));

  ClearString();
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 18));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocIncremental) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_INCREMENTAL, 14));
  // Version gets taken as-is from input document.
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\n%\xa0\xf2\xa4\xf4"));
  // Additional output produced vs. non incremental.
  // Check that the size is larger than the old, broken incremental save size.
  EXPECT_GT(GetString().size(), 985u);
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocNoIncremental) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_NO_INCREMENTAL, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocRemoveSecurityDeprecated) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this,
                                   FPDF_REMOVE_SECURITY_DEPRECATED, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocRemoveSecurity) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_REMOVE_SECURITY, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocBadFlags) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 999999, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveCopiedDoc) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  ScopedPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_ImportPages(output_doc.get(), document(), "1", 0));
  EXPECT_TRUE(FPDF_SaveAsCopy(output_doc.get(), this, 0));
}

TEST_F(FPDFSaveEmbedderTest, Bug42271133) {
  ASSERT_TRUE(OpenDocument("bug_42271133.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Arbitrarily remove the first page object.
  {
    ScopedFPDFPageObject text_object(FPDFPage_GetObject(page.get(), 0));
    ASSERT_TRUE(text_object);
    ASSERT_TRUE(FPDFPage_RemoveObject(page.get(), text_object.get()));
  }

  // Regenerate dirty stream and save the document.
  ASSERT_TRUE(FPDFPage_GenerateContent(page.get()));
  ASSERT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // Reload saved document.
  ScopedSavedDoc saved_document = OpenScopedSavedDocument();
  ASSERT_TRUE(saved_document);
  ScopedSavedPage saved_page = LoadScopedSavedPage(0);
  ASSERT_TRUE(saved_page);

  // Assert path fill color is not changed to black.
  auto path_obj = FPDFPage_GetObject(saved_page.get(), 0);
  ASSERT_TRUE(path_obj);
  unsigned int r;
  unsigned int g;
  unsigned int b;
  unsigned int a;
  ASSERT_TRUE(FPDFPageObj_GetFillColor(path_obj, &r, &g, &b, &a));
  EXPECT_EQ(180u, r);
  EXPECT_EQ(180u, g);
  EXPECT_EQ(180u, b);
}

TEST_F(FPDFSaveEmbedderTest, SaveLinearizedDoc) {
  const int kPageCount = 3;
  std::array<std::string, kPageCount> original_md5;

  ASSERT_TRUE(OpenDocument("linearized.pdf"));
  for (int i = 0; i < kPageCount; ++i) {
    ScopedPage page = LoadScopedPage(i);
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    EXPECT_EQ(612, FPDFBitmap_GetWidth(bitmap.get()));
    EXPECT_EQ(792, FPDFBitmap_GetHeight(bitmap.get()));
    original_md5[i] = HashBitmap(bitmap.get());
  }

  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.6\r\n"));
  EXPECT_THAT(GetString(), HasSubstr("/Root "));
  EXPECT_THAT(GetString(), HasSubstr("/Info "));
  EXPECT_THAT(GetString(), HasSubstr("/Size 37"));
  EXPECT_THAT(GetString(), HasSubstr("35 0 obj"));
  EXPECT_THAT(GetString(), HasSubstr("36 0 obj"));
  EXPECT_THAT(GetString(), Not(HasSubstr("37 0 obj")));
  EXPECT_THAT(GetString(), Not(HasSubstr("38 0 obj")));
  EXPECT_EQ(7986u, GetString().size());

  // Make sure new document renders the same as the old one.
  ScopedSavedDoc saved_document = OpenScopedSavedDocument();
  ASSERT_TRUE(saved_document);
  for (int i = 0; i < kPageCount; ++i) {
    ScopedSavedPage page = LoadScopedSavedPage(i);
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap = RenderSavedPage(page.get());
    EXPECT_EQ(original_md5[i], HashBitmap(bitmap.get()));
  }
}

TEST_F(FPDFSaveEmbedderTest, Bug1409) {
  ASSERT_TRUE(OpenDocument("jpx_lzw.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  while (FPDFPage_CountObjects(page.get()) > 0) {
    ScopedFPDFPageObject object(FPDFPage_GetObject(page.get(), 0));
    ASSERT_TRUE(object);
    ASSERT_TRUE(FPDFPage_RemoveObject(page.get(), object.get()));
  }
  ASSERT_TRUE(FPDFPage_GenerateContent(page.get()));

  ASSERT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // The new document should render as empty.
  ScopedSavedDoc saved_document = OpenScopedSavedDocument();
  ASSERT_TRUE(saved_document);
  ScopedSavedPage saved_page = LoadScopedSavedPage(0);
  ASSERT_TRUE(saved_page);
  ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page.get());
  CompareBitmap(bitmap.get(), pdfium::kBlankPage612By792Png);

  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_THAT(GetString(), HasSubstr("/Root "));
  EXPECT_THAT(GetString(), Not(HasSubstr("/Image")));
  EXPECT_LT(GetString().size(), 600u);
}

#ifdef PDF_ENABLE_XFA
TEST_F(FPDFSaveEmbedderTest, SaveXFADoc) {
  ASSERT_TRUE(OpenDocument("simple_xfa.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  ScopedSavedDoc saved_document = OpenScopedSavedDocument();
  ASSERT_TRUE(saved_document);
  // TODO(tsepez): check for XFA forms in document
}
#endif  // PDF_ENABLE_XFA

TEST_F(FPDFSaveEmbedderTest, Bug342) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("0000000000 65535 f\r\n"));
  EXPECT_THAT(GetString(), Not(HasSubstr("0000000000 65536 f\r\n")));
}

TEST_F(FPDFSaveEmbedderTest, Bug905142) {
  ASSERT_TRUE(OpenDocument("bug_905142.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("/Length 0"));
}

// Should not trigger a DCHECK() failure in CFX_FileBufferArchive.
// Fails because the PDF is malformed.
TEST_F(FPDFSaveEmbedderTest, Bug1328389) {
  ASSERT_TRUE(OpenDocument("bug_1328389.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("/Foo/"));
}

TEST_F(FPDFSaveEmbedderTest, IncrementalSaveWithModifications) {
  ASSERT_TRUE(OpenDocument("rectangles.pdf"));

  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Get the original bitmap for comparison
  ScopedFPDFBitmap original_bitmap = RenderLoadedPage(page.get());
  std::string original_md5 = HashBitmap(original_bitmap.get());

  // Count text objects on a page
  auto count_text_objects = [](FPDF_PAGE page) {
    int object_count = FPDFPage_CountObjects(page);
    int text_count = 0;
    for (int i = 0; i < object_count; ++i) {
      FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
      if (FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_TEXT) {
        ++text_count;
      }
    }
    return text_count;
  };

  // Verify the original PDF does not have any text objects
  EXPECT_EQ(0, count_text_objects(page.get()));

  // Add a new text object to modify the page.
  ScopedFPDFPageObject text_object(
      FPDFPageObj_NewTextObj(document(), "Arial", 12.0f));
  ScopedFPDFWideString text = GetFPDFWideString(L"Test Incremental Save");
  FPDFText_SetText(text_object.get(), text.get());
  FPDFPageObj_Transform(text_object.get(), 1, 0, 0, 1, 100, 100);
  FPDFPage_InsertObject(page.get(), text_object.release());
  ASSERT_TRUE(FPDFPage_GenerateContent(page.get()));

  ASSERT_TRUE(FPDF_SaveAsCopy(document(), this, FPDF_INCREMENTAL));

  // Verify the saved document
  // Count occurrences of key markers
  auto count_occurrences = [](const std::string& str,
                              const std::string& substr) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
      ++count;
      pos += substr.size();
    }
    return count;
  };

  // Should contain incremental save markers (original + incremental)
  std::string saved_content = GetString();
  EXPECT_EQ(2u, count_occurrences(saved_content, "trailer"));
  // In incremental PDF saving, /Prev points to the previous xref table's
  // offset. Since we're doing only one incremental save operation, there's only
  // one /Prev entry pointing to the original PDF's xref table.
  EXPECT_EQ(1u, count_occurrences(saved_content, "/Prev"));
  EXPECT_EQ(2u, count_occurrences(saved_content, "startxref"));
  EXPECT_EQ(2u, count_occurrences(saved_content, "%%EOF"));

  // Load the saved document and verify the modification is visible
  ScopedSavedDoc saved_doc = OpenScopedSavedDocument();
  ASSERT_TRUE(saved_doc);
  ScopedSavedPage saved_page = LoadScopedSavedPage(0);
  ASSERT_TRUE(saved_page);

  // The rendered output should be different from the original
  ScopedFPDFBitmap saved_bitmap = RenderSavedPage(saved_page.get());
  std::string saved_md5 = HashBitmap(saved_bitmap.get());
  EXPECT_NE(original_md5, saved_md5);

  // Verify the text object exists after the save
  EXPECT_EQ(1, count_text_objects(saved_page.get()));
}

class FPDFSaveWithFontSubsetEmbedderTest : public FPDFSaveEmbedderTest {
 public:
  static constexpr char kSaveNewTextFilename[] = "save_new_text";
  static constexpr std::array<FPDF_WCHAR, 38> kExpectedSingleTextAdded = {
      // "Hello, world!\r\n"
      'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\r',
      '\n',
      // "Goodbye, world!\r\n"
      'G', 'o', 'o', 'd', 'b', 'y', 'e', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!',
      '\r', '\n',
      // "这是第一句。"
      0x8FD9, 0x662F, 0x7B2C, 0x2F00, 0x53E5, 0x3002};
  static constexpr std::array<FPDF_WCHAR, 14> kExpectedSecondTextAdded = {
      // "Hello again."
      '\r', '\n', 'H', 'e', 'l', 'l', 'o', ' ', 'a', 'g', 'a', 'i', 'n', '.'};

  ScopedFPDFFont LoadTestFont(const std::string& font_path) {
    std::vector<uint8_t> font_data = GetFileContents(font_path.c_str());
    if (font_data.empty()) {
      return nullptr;
    }

    return ScopedFPDFFont(FPDFText_LoadFont(
        document(), font_data.data(), font_data.size(), FPDF_FONT_TRUETYPE,
        /*cid=*/true));
  }

  void InsertNewTextObject(const FPDF_PAGE& page,
                           FPDF_FONT font,
                           FPDF_WIDESTRING text,
                           const CFX_PointF& point) {
    FPDF_PAGEOBJECT text_obj =
        FPDFPageObj_CreateTextObj(document(), font, 24.0f);

    ASSERT_TRUE(FPDFText_SetText(text_obj, text));

    const FS_MATRIX matrix{1.0f, 0.0f, 0.0f, 1.0f, point.x, point.y};
    ASSERT_TRUE(FPDFPageObj_TransformF(text_obj, &matrix));
    FPDFPage_InsertObject(page, text_obj);
    ASSERT_TRUE(FPDFPage_GenerateContent(page));
  }

  // `expected` must not include the NUL terminator.
  void TestExtractedFont(pdfium::span<const FPDF_WCHAR> expected) {
    ScopedSavedDoc saved_doc = OpenScopedSavedDocument();
    ASSERT_TRUE(saved_doc);
    ScopedSavedPage saved_page = LoadScopedSavedPage(0);
    ASSERT_TRUE(saved_page);
    ScopedFPDFTextPage text_page(FPDFText_LoadPage(saved_page.get()));
    ASSERT_TRUE(text_page);

    size_t expected_size = expected.size();
    int actual_count = FPDFText_CountChars(text_page.get());
    ASSERT_EQ(static_cast<int>(expected_size), actual_count);

    // Includes the NUL terminator.
    std::vector<FPDF_WCHAR> actual_buffer(actual_count + 1);
    ASSERT_EQ(actual_count + 1,
              FPDFText_GetText(text_page.get(), 0, actual_count,
                               actual_buffer.data()));

    EXPECT_THAT(pdfium::span(actual_buffer).first(expected_size),
                ElementsAreArray(expected));
    EXPECT_EQ(0, actual_buffer.back());
  }
};

TEST_F(FPDFSaveWithFontSubsetEmbedderTest, SaveWithSubsetWithoutNewText) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, FPDF_SUBSET_NEW_FONTS));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveWithFontSubsetEmbedderTest, SaveWithoutSubsetWithNewText) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFFont font = LoadTestFont(PathService::GetThirdPartyFilePath(
      "NotoSansCJK/NotoSansSC-Regular.subset.otf"));
  ASSERT_TRUE(font);

  ScopedFPDFWideString text = GetFPDFWideString(L"这是第一句。");
  ASSERT_NO_FATAL_FAILURE(InsertNewTextObject(
      page.get(), font.get(), text.get(), CFX_PointF(10.0f, 10.0f)));

  ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
  CompareBitmapWithExpectationSuffix(bitmap.get(), kSaveNewTextFilename);

  // Verify the file size increase is larger when not subsetting the new text's
  // font.
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_EQ(5004u, GetString().size());

  // Verify the text is visible.
  VerifySavedDocumentWithExpectationSuffix(kSaveNewTextFilename);

  TestExtractedFont(kExpectedSingleTextAdded);
}

TEST_F(FPDFSaveWithFontSubsetEmbedderTest, SaveWithSubsetWithNewText) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFFont font = LoadTestFont(PathService::GetThirdPartyFilePath(
      "NotoSansCJK/NotoSansSC-Regular.subset.otf"));
  ASSERT_TRUE(font);

  ScopedFPDFWideString text = GetFPDFWideString(L"这是第一句。");
  ASSERT_NO_FATAL_FAILURE(InsertNewTextObject(
      page.get(), font.get(), text.get(), CFX_PointF(10.0f, 10.0f)));

  ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
  CompareBitmapWithExpectationSuffix(bitmap.get(), kSaveNewTextFilename);

  // Verify the file size increase is smaller when subsetting the new text's
  // font, since text only contains a subset of the characters in the test font.
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, FPDF_SUBSET_NEW_FONTS));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_EQ(4453u, GetString().size());

  // Verify the text is visible.
  VerifySavedDocumentWithExpectationSuffix(kSaveNewTextFilename);

  TestExtractedFont(kExpectedSingleTextAdded);
}

TEST_F(FPDFSaveWithFontSubsetEmbedderTest,
       SaveWithSubsetMultipleFontsMultipleTexts) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFFont font1 = LoadTestFont(PathService::GetThirdPartyFilePath(
      "NotoSansCJK/NotoSansSC-Regular.subset.otf"));
  ASSERT_TRUE(font1);

  ScopedFPDFWideString text1 = GetFPDFWideString(L"这是第一句。");
  ASSERT_NO_FATAL_FAILURE(InsertNewTextObject(
      page.get(), font1.get(), text1.get(), CFX_PointF(10.0f, 10.0f)));

  ScopedFPDFFont font2 = LoadTestFont(PathService::GetThirdPartyFilePath(
      "test_fonts/test_fonts/Arimo-Regular.ttf"));
  ASSERT_TRUE(font2);

  ScopedFPDFWideString text2 = GetFPDFWideString(L"Hello again.");
  ASSERT_NO_FATAL_FAILURE(InsertNewTextObject(
      page.get(), font2.get(), text2.get(), CFX_PointF(10.0f, 150.0f)));

  constexpr char kSaveMultipleFontsFilename[] = "save_multiple_fonts";
  ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
  CompareBitmapWithExpectationSuffix(bitmap.get(), kSaveMultipleFontsFilename);

  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, FPDF_SUBSET_NEW_FONTS));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  // Subsetting reduces the file from ~259KB to ~12KB. Allow a tolerance of
  // +/- 500 bytes to accommodate any potential internal changes.
  EXPECT_NEAR(GetString().size(), 12000u, 500u);

  // Verify the text is visible.
  VerifySavedDocumentWithExpectationSuffix(kSaveMultipleFontsFilename);

  std::vector<FPDF_WCHAR> expected_multiple_text_added;
  expected_multiple_text_added.reserve(kExpectedSingleTextAdded.size() +
                                       kExpectedSecondTextAdded.size());
  std::ranges::copy(kExpectedSingleTextAdded,
                    std::back_inserter(expected_multiple_text_added));
  std::ranges::copy(kExpectedSecondTextAdded,
                    std::back_inserter(expected_multiple_text_added));

  TestExtractedFont(expected_multiple_text_added);
}
