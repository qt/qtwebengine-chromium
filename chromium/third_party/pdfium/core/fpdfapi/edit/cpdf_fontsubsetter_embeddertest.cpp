// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/edit/cpdf_fontsubsetter.h"

#include <stdint.h>

#include <array>
#include <numeric>
#include <string>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/numerics/safe_conversions.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/widestring.h"
#include "core/fxge/fx_font.h"
#include "public/fpdf_edit.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/file_util.h"
#include "testing/utils/path_service.h"

using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kArimoBaseFontName[] = "Arimo-Regular";
constexpr char kLohitTamilBaseFontName[] = "Lohit-Tamil";
constexpr char kNotoSansBaseFontName[] = "NotoSansCJKjp-Regular";

constexpr size_t kSubsettedFontPrefixWithPlusLength =
    kSubsettedFontPrefixLength + 1;

// These are cheating slightly to avoid a layering violation, since this file
// cannot include fpdfsdk/cpdfsdk_helpers.h.
CPDF_Document* CPDFDocumentFromFPDFDocument(FPDF_DOCUMENT document) {
  return reinterpret_cast<CPDF_Document*>(document);
}
CPDF_Font* CPDFFontFromFPDFFont(FPDF_FONT font) {
  return reinterpret_cast<CPDF_Font*>(font);
}

// Returns the file path for a test font provided by the third-party test_fonts.
std::string GetTestFontFilePath(const std::string& file_name) {
  return PathService::GetThirdPartyFilePath("test_fonts/test_fonts/" +
                                            file_name);
}

// Returns a list of new object nums used for testing. Since the object nums
// list is used to see if an object num is new, this can exceed the amount of
// objects in the document. This uses a large enough number sufficient for these
// tests. As a result, tests can avoid having to explicitly write the new object
// numbers added when writing text to the document.
std::vector<uint32_t> GetTestNewObjNums() {
  std::vector<uint32_t> test_obj_nums(100);
  std::iota(test_obj_nums.begin(), test_obj_nums.end(), 1);
  return test_obj_nums;
}

// Returns if `actual_name` follows the "XXXXXX+BaseName" pattern, where 'X' is
// an uppercase letter.
bool IsSubsetFontName(const ByteString& actual_name,
                      ByteStringView expected_base_name) {
  if (actual_name.GetLength() !=
      kSubsettedFontPrefixWithPlusLength + expected_base_name.GetLength()) {
    return false;
  }

  // Check the first '+' is in the correct position..
  std::optional<size_t> first_plus = actual_name.Find('+');
  if (!first_plus.has_value() ||
      first_plus.value() != kSubsettedFontPrefixLength) {
    return false;
  }

  // Check there is not an additional '+'.
  std::optional<size_t> last_plus = actual_name.ReverseFind('+');
  if (!last_plus.has_value() || first_plus.value() != last_plus.value()) {
    return false;
  }

  // Check for the tag.
  for (char ch : actual_name.First(kSubsettedFontPrefixLength)) {
    if (!FXSYS_IsUpperASCII(ch)) {
      return false;
    }
  }

  return actual_name.Substr(kSubsettedFontPrefixWithPlusLength) ==
         expected_base_name;
}

// See `StreamSizeIsWithinRange` and its relevant matchers.
bool IsMatchingStream(const CPDF_Object* obj,
                      size_t min_size,
                      size_t max_size) {
  const CPDF_Stream* stream = ToStream(obj);
  if (!stream) {
    return false;
  }

  size_t actual_size = stream->GetRawSize();
  return actual_size >= min_size && actual_size < max_size;
}

// Matcher that verifies the stream does not contain a subtype and that the
// stream size is strictly within the range min inclusive, max exclusive.
MATCHER_P2(StreamSizeIsWithinRange, min_size, max_size, "") {
  const auto& obj = arg.second;
  if (!IsMatchingStream(obj, min_size, max_size)) {
    return false;
  }

  const CPDF_Dictionary* dict = obj->GetDict();
  if (dict->KeyExist("Subtype")) {
    return false;
  }

  RetainPtr<const CPDF_Number> length1 = dict->GetNumberFor("Length1");
  if (!length1 || !length1->IsInteger()) {
    return false;
  }

  int length = length1->GetInteger();
  return length >= 0 &&
         pdfium::checked_cast<size_t>(length) == obj->AsStream()->GetRawSize();
}

// Same as `StreamSizeIsWithinRange`, but checks for a subtype of "OpenType" and
// excludes the Length1 entry instead.
MATCHER_P2(OpenTypeCFFStreamSizeIsWithinRange, min_size, max_size, "") {
  const auto& obj = arg.second;
  if (!IsMatchingStream(obj, min_size, max_size)) {
    return false;
  }

  const CPDF_Dictionary* dict = obj->GetDict();
  if (dict->GetNameFor("Subtype") != "OpenType") {
    return false;
  }

  return !dict->GetNumberFor("Length1");
}

// Matches the Root Font, checking for a valid subset font name.
MATCHER_P(IsRootFont, expected_base_name, "") {
  const auto& obj = arg.second;
  const CPDF_Dictionary* dict = ToDictionary(obj);
  if (!dict) {
    return false;
  }

  if (dict->GetNameFor("Type") != "Font" ||
      dict->GetNameFor("Subtype") != "Type0" ||
      dict->GetNameFor("Encoding") != "Identity-H" ||
      !dict->KeyExist("DescendantFonts")) {
    return false;
  }

  return IsSubsetFontName(dict->GetNameFor("BaseFont"), expected_base_name);
}

// See `IsCIDFont` and its relevant matchers.
bool IsMatchingCIDFont(const CPDF_Object* obj,
                       ByteStringView expected_base_name,
                       ByteStringView expected_subtype) {
  const CPDF_Dictionary* dict = ToDictionary(obj);
  if (!dict) {
    return false;
  }

  if (dict->GetNameFor("Type") != "Font" || !dict->KeyExist("CIDSystemInfo")) {
    return false;
  }

  return IsSubsetFontName(dict->GetNameFor("BaseFont"), expected_base_name) &&
         dict->GetNameFor("Subtype") == expected_subtype;
}

// Matches the CID Font, checking for a valid subset font name and a subtype of
// "CIDFontType2".
MATCHER_P(IsCIDFont, expected_base_name, "") {
  const auto& obj = arg.second;
  return IsMatchingCIDFont(obj, expected_base_name, "CIDFontType2");
}

// Same as `IsCIDFont`, but checks for a subtype of "CIDFontType0" instead.
MATCHER_P(IsOpenTypeCFFCIDFont, expected_base_name, "") {
  const auto& obj = arg.second;
  return IsMatchingCIDFont(obj, expected_base_name, "CIDFontType0");
}

// See `IsFontDescriptor` and its relevant matchers.
bool IsMatchingFontDescriptor(const CPDF_Object* obj,
                              ByteStringView expected_base_name) {
  const CPDF_Dictionary* dict = ToDictionary(obj);
  if (!dict) {
    return false;
  }

  if (dict->GetNameFor("Type") != "FontDescriptor") {
    return false;
  }

  return IsSubsetFontName(dict->GetNameFor("FontName"), expected_base_name);
}

// Matches the FontDescriptor, checking for a valid subset font name and entry
// for "FontFile2".
MATCHER_P(IsFontDescriptor, expected_base_name, "") {
  const auto& obj = arg.second;
  if (!IsMatchingFontDescriptor(obj, expected_base_name)) {
    return false;
  }

  const CPDF_Dictionary* dict = obj->AsDictionary();
  return dict->GetStreamFor("FontFile2") && !dict->KeyExist("FontFile3");
}

// Same as `IsFontDescriptor`, but checks for valid entries in "Flags" and
// "FontFile3" instead.
MATCHER_P(IsOpenTypeCFFFontDescriptor, expected_base_name, "") {
  const auto& obj = arg.second;
  if (!IsMatchingFontDescriptor(obj, expected_base_name)) {
    return false;
  }

  const CPDF_Dictionary* dict = obj->AsDictionary();

  // See ISO 32000-1:2008 section 9.8.2 "Font Descriptor Flags".
  RetainPtr<const CPDF_Number> flags = dict->GetNumberFor("Flags");
  if (!flags || !flags->IsInteger()) {
    return false;
  }

  int flags_int = flags->GetInteger();
  if (!(flags_int & 0x04) || !!(flags_int & 0x20)) {
    return false;
  }

  return !dict->KeyExist("FontFile2") && dict->GetStreamFor("FontFile3");
}

MATCHER(IsWidths, "") {
  const CPDF_Array* array = ToArray(arg.second);
  return array && !array->IsEmpty();
}

MATCHER(IsToUnicode, "") {
  RetainPtr<const CPDF_Stream> stream = ToStream(arg.second);
  if (!stream) {
    return false;
  }

  WideString unicode_text = stream->GetUnicodeText();
  return unicode_text.Contains(L"/CIDInit /ProcSet findresource begin") &&
         unicode_text.Contains(L"begincmap") &&
         unicode_text.Contains(L"endcmap");
}

}  // namespace

// Prints overrides nicely for debugging purposes.
void PrintTo(const RetainPtr<const CPDF_Object>& obj, std::ostream* os) {
  if (!obj) {
    *os << "nullptr";
    return;
  }

  *os << "(Obj type=" << obj->GetType();
  if (obj->IsDictionary()) {
    const CPDF_Dictionary* dict = obj->AsDictionary();
    *os << " {";

    static constexpr std::array<const char*, 4> kKeys = {
        "Type", "Subtype", "BaseFont", "FontName"};
    bool first = true;
    for (const char* key : kKeys) {
      if (dict->KeyExist(key)) {
        if (!first) {
          *os << ", ";
        }
        *os << key << "=" << dict->GetObjectFor(key)->GetString();
        first = false;
      }
    }
    *os << "}";
  } else if (obj->IsStream()) {
    *os << " size=" << obj->AsStream()->GetRawSize();
  } else if (obj->IsArray()) {
    const CPDF_Array* array = obj->AsArray();
    *os << " [size=" << array->size() << "]";
  }

  *os << ")";
}

class CPDFFontSubsetterTest : public EmbedderTest {
 public:
  void InsertNewTextObject(const std::wstring& text,
                           FPDF_PAGE page,
                           FPDF_FONT font) {
    FPDF_PAGEOBJECT text_object =
        FPDFPageObj_CreateTextObj(document(), font, 20.0f);
    EXPECT_TRUE(text_object);

    ScopedFPDFWideString fpdf_text = GetFPDFWideString(text);
    EXPECT_TRUE(FPDFText_SetText(text_object, fpdf_text.get()));

    const FS_MATRIX matrix{1.0f, 0.0f, 0.0f, 1.0f, 50.0f, 200.0f};
    ASSERT_TRUE(FPDFPageObj_TransformF(text_object, &matrix));
    FPDFPage_InsertObject(page, text_object);
    EXPECT_TRUE(FPDFPage_GenerateContent(page));
  }
};

TEST_F(CPDFFontSubsetterTest, NoNewText) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));

  EXPECT_THAT(subsetter.GenerateObjectOverrides({}), IsEmpty());

  EXPECT_THAT(subsetter.GenerateObjectOverrides(GetTestNewObjNums()),
              IsEmpty());

  // Not a text object.
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  FPDFPage_InsertObject(page.get(), rect);
  EXPECT_THAT(subsetter.GenerateObjectOverrides(GetTestNewObjNums()),
              IsEmpty());
}

TEST_F(CPDFFontSubsetterTest, StandardFont) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  ScopedFPDFFont font(FPDFText_LoadStandardFont(document(), "Helvetica"));
  ASSERT_TRUE(font);

  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Hello world", page.get(), font.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  EXPECT_THAT(subsetter.GenerateObjectOverrides(GetTestNewObjNums()),
              IsEmpty());
}

TEST_F(CPDFFontSubsetterTest, OpenType) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  const std::string font_path =
      GetTestFontFilePath("NotoSansCJKjp-Regular.otf");
  ASSERT_FALSE(font_path.empty());

  std::vector<uint8_t> font_data = GetFileContents(font_path.c_str());
  const size_t original_size = font_data.size();
  ASSERT_EQ(16427228u, original_size);

  ScopedFPDFFont font(FPDFText_LoadFont(document(), font_data.data(),
                                        font_data.size(), FPDF_FONT_TRUETYPE,
                                        /*cid=*/true));
  ASSERT_TRUE(font);

  ASSERT_NO_FATAL_FAILURE(InsertNewTextObject(L"这", page.get(), font.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  auto overrides = subsetter.GenerateObjectOverrides(GetTestNewObjNums());
  ASSERT_EQ(6u, overrides.size());

  // Subset size is ~2.5% of the original font file, i.e. ~450 KB.
  EXPECT_THAT(overrides, UnorderedElementsAre(
                             OpenTypeCFFStreamSizeIsWithinRange(
                                 original_size * 0.02, original_size * 0.03),
                             IsRootFont(kNotoSansBaseFontName),
                             IsOpenTypeCFFCIDFont(kNotoSansBaseFontName),
                             IsOpenTypeCFFFontDescriptor(kNotoSansBaseFontName),
                             IsWidths(), IsToUnicode()));
}

TEST_F(CPDFFontSubsetterTest, TrueType) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  const std::string font_path = GetTestFontFilePath("Arimo-Regular.ttf");
  ASSERT_FALSE(font_path.empty());

  std::vector<uint8_t> font_data = GetFileContents(font_path.c_str());
  const size_t original_size = font_data.size();
  ASSERT_EQ(436180u, original_size);

  ScopedFPDFFont font(FPDFText_LoadFont(document(), font_data.data(),
                                        font_data.size(), FPDF_FONT_TRUETYPE,
                                        /*cid=*/true));
  ASSERT_TRUE(font);

  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Hello world", page.get(), font.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  auto overrides = subsetter.GenerateObjectOverrides(GetTestNewObjNums());
  ASSERT_EQ(6u, overrides.size());

  // Subset size is ~3% of the original font file, i.e. ~13 KB.
  EXPECT_THAT(
      overrides,
      UnorderedElementsAre(
          StreamSizeIsWithinRange(original_size * 0.025, original_size * 0.035),
          IsRootFont(kArimoBaseFontName), IsCIDFont(kArimoBaseFontName),
          IsFontDescriptor(kArimoBaseFontName), IsWidths(), IsToUnicode()));
}

TEST_F(CPDFFontSubsetterTest, SingleFontMultipleTexts) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  const std::string font_path = GetTestFontFilePath("Arimo-Regular.ttf");
  ASSERT_FALSE(font_path.empty());

  std::vector<uint8_t> font_data = GetFileContents(font_path.c_str());
  const size_t original_size = font_data.size();
  ASSERT_EQ(436180u, original_size);

  ScopedFPDFFont font(FPDFText_LoadFont(document(), font_data.data(),
                                        font_data.size(), FPDF_FONT_TRUETYPE,
                                        /*cid=*/true));
  ASSERT_TRUE(font);

  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Abcdefg", page.get(), font.get()));
  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Hijklmnop", page.get(), font.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  auto overrides = subsetter.GenerateObjectOverrides(GetTestNewObjNums());
  ASSERT_EQ(6u, overrides.size());

  // Subset size is ~3.5% of the original font file, i.e. ~15 KB.
  EXPECT_THAT(
      overrides,
      UnorderedElementsAre(
          StreamSizeIsWithinRange(original_size * 0.03, original_size * 0.04),
          IsRootFont(kArimoBaseFontName), IsCIDFont(kArimoBaseFontName),
          IsFontDescriptor(kArimoBaseFontName), IsWidths(), IsToUnicode()));
}

TEST_F(CPDFFontSubsetterTest, MultipleFontsMultipleTexts) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  const std::string font_path1 = GetTestFontFilePath("Lohit-Tamil.ttf");
  ASSERT_FALSE(font_path1.empty());
  const std::string font_path2 = GetTestFontFilePath("Arimo-Regular.ttf");
  ASSERT_FALSE(font_path2.empty());

  std::vector<uint8_t> font_data1 = GetFileContents(font_path1.c_str());
  const size_t original_size1 = font_data1.size();
  ASSERT_EQ(48908u, original_size1);
  std::vector<uint8_t> font_data2 = GetFileContents(font_path2.c_str());
  const size_t original_size2 = font_data2.size();
  ASSERT_EQ(436180u, original_size2);

  ScopedFPDFFont font1(FPDFText_LoadFont(document(), font_data1.data(),
                                         font_data1.size(), FPDF_FONT_TRUETYPE,
                                         /*cid=*/true));
  ASSERT_TRUE(font1);
  ScopedFPDFFont font2(FPDFText_LoadFont(document(), font_data2.data(),
                                         font_data2.size(), FPDF_FONT_TRUETYPE,
                                         /*cid=*/true));
  ASSERT_TRUE(font2);

  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"வணக்கம்", page.get(), font1.get()));
  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Goodbye", page.get(), font2.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  auto overrides = subsetter.GenerateObjectOverrides(GetTestNewObjNums());
  ASSERT_EQ(12u, overrides.size());

  // Subset size for `font_data1` is ~6% of the original file, i.e. ~3 KB.
  // Subset size for `font_data2` is ~3% of the original file, i.e. ~13.3 KB.
  EXPECT_THAT(
      overrides,
      UnorderedElementsAre(
          StreamSizeIsWithinRange(original_size1 * 0.055,
                                  original_size1 * 0.065),
          IsRootFont(kLohitTamilBaseFontName),
          IsCIDFont(kLohitTamilBaseFontName),
          IsFontDescriptor(kLohitTamilBaseFontName), IsWidths(), IsToUnicode(),
          StreamSizeIsWithinRange(original_size2 * 0.025,
                                  original_size2 * 0.035),
          IsRootFont(kArimoBaseFontName), IsCIDFont(kArimoBaseFontName),
          IsFontDescriptor(kArimoBaseFontName), IsWidths(), IsToUnicode()));
}

TEST_F(CPDFFontSubsetterTest, ReplaceExistingPrefix) {
  CreateEmptyDocument();
  ScopedFPDFPage page(FPDFPage_New(document(), 0, 400, 400));

  const std::string font_path = GetTestFontFilePath("Arimo-Regular.ttf");
  ASSERT_FALSE(font_path.empty());

  // The file size for `font_path` is ~436 KB.
  std::vector<uint8_t> font_data = GetFileContents(font_path.c_str());
  size_t original_size = font_data.size();
  ASSERT_GT(original_size, 0u);

  ScopedFPDFFont font(FPDFText_LoadFont(document(), font_data.data(),
                                        font_data.size(), FPDF_FONT_TRUETYPE,
                                        /*cid=*/true));

  // Manually insert an existing prefix to the font name.
  CPDF_Font* cfont = CPDFFontFromFPDFFont(font.get());
  RetainPtr<CPDF_Dictionary> font_dict = cfont->GetMutableFontDict();
  ASSERT_TRUE(font_dict);

  font_dict->SetNewFor<CPDF_Name>("BaseFont", "AAAAAA+Arimo-Regular");

  ASSERT_NO_FATAL_FAILURE(
      InsertNewTextObject(L"Hello world", page.get(), font.get()));

  CPDF_FontSubsetter subsetter(CPDFDocumentFromFPDFDocument(document()));
  auto overrides = subsetter.GenerateObjectOverrides(GetTestNewObjNums());
  ASSERT_EQ(6u, overrides.size());

  // Subset size is ~3% of the original font file, i.e. ~13 KB.
  EXPECT_THAT(
      overrides,
      UnorderedElementsAre(
          StreamSizeIsWithinRange(original_size * 0.025, original_size * 0.035),
          IsRootFont(kArimoBaseFontName), IsCIDFont(kArimoBaseFontName),
          IsFontDescriptor(kArimoBaseFontName), IsWidths(), IsToUnicode()));
}
