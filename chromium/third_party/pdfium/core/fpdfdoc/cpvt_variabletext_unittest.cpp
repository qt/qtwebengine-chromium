// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfdoc/cpvt_variabletext.h"

#include <memory>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/parser/cpdf_test_document.h"
#include "core/fpdfdoc/cpvt_fontmap.h"
#include "core/fpdfdoc/cpvt_stub_provider.h"
#include "core/fpdfdoc/cpvt_word.h"
#include "core/fxcrt/retain_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

class CPVT_VariableTextTest : public testing::Test {
 public:
  void SetUp() override {
    pdfium::InitializePageModule();
    test_doc_ = std::make_unique<CPDF_TestDocument>();
    font_ = CPDF_Font::GetStockFont(test_doc_.get(), "Helvetica");
    font_map_ = std::make_unique<CPVT_FontMap>(test_doc_.get(), nullptr, font_,
                                               "Helvetica");
    provider_ = std::make_unique<CPVT_StubProvider>(font_map_.get());
  }

  void TearDown() override {
    provider_.reset();
    font_map_.reset();
    font_.Reset();
    test_doc_.reset();
    pdfium::DestroyPageModule();
  }

 protected:
  std::unique_ptr<CPDF_TestDocument> test_doc_;
  RetainPtr<CPDF_Font> font_;
  std::unique_ptr<CPVT_FontMap> font_map_;
  std::unique_ptr<CPVT_StubProvider> provider_;
};

TEST_F(CPVT_VariableTextTest, LTRTextLayout) {
  CPVT_VariableText vt(provider_.get());
  vt.SetPlateRect(CFX_FloatRect(0, 0, 100, 100));
  vt.SetFontSize(10.0f);
  vt.SetMultiLine(false);
  vt.SetAutoReturn(false);
  vt.Initialize();

  // "hello" in English (Left-to-Right layout)
  vt.SetText(L"hello");
  vt.RearrangeAll();

  CPVT_VariableText::Iterator* it = vt.GetIterator();
  it->SetAt(1);  // Set to first word index (Place(0, 0, 0))
  CPVT_Word word;
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ('h', word.word());
  EXPECT_FLOAT_EQ(0.1f, word.CaretX());
  float first_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ('e', word.word());
  EXPECT_FLOAT_EQ(0.2f, word.CaretX());
  float second_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ('l', word.word());
  EXPECT_FLOAT_EQ(0.3f, word.CaretX());
  float third_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ('l', word.word());
  EXPECT_FLOAT_EQ(0.4f, word.CaretX());
  float fourth_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ('o', word.word());
  EXPECT_FLOAT_EQ(0.5f, word.CaretX());
  float fifth_x = word.location().x;

  EXPECT_FALSE(it->NextWord());

  // In LTR, coordinates go left-to-right (0, 10, 20, 30, 40).
  EXPECT_LT(first_x, second_x);
  EXPECT_LT(second_x, third_x);
  EXPECT_LT(third_x, fourth_x);
  EXPECT_LT(fourth_x, fifth_x);
}

TEST_F(CPVT_VariableTextTest, RTLTextLayout) {
  CPVT_VariableText vt(provider_.get());
  vt.SetPlateRect(CFX_FloatRect(0, 0, 100, 100));
  vt.SetFontSize(10.0f);
  vt.SetMultiLine(false);
  vt.SetAutoReturn(false);
  vt.Initialize();

  // Shalom in Hebrew (U+05E9, U+05DC, U+05D5, U+05DD)
  // Logically ordered but should be displayed from right to left.
  vt.SetText(L"\x05E9\x05DC\x05D5\x05DD");
  vt.RearrangeAll();

  CPVT_VariableText::Iterator* it = vt.GetIterator();
  it->SetAt(1);  // Set to first word index (Place(0, 0, 0))
  CPVT_Word word;
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ(0x05E9, word.word());
  EXPECT_FLOAT_EQ(0.3f, word.CaretX());
  float first_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ(0x05DC, word.word());
  EXPECT_FLOAT_EQ(0.2f, word.CaretX());
  float second_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ(0x05D5, word.word());
  EXPECT_FLOAT_EQ(0.1f, word.CaretX());
  float third_x = word.location().x;

  ASSERT_TRUE(it->NextWord());
  ASSERT_TRUE(it->GetWord(word));
  EXPECT_EQ(0x05DD, word.word());
  EXPECT_FLOAT_EQ(0.0f, word.CaretX());
  float fourth_x = word.location().x;

  EXPECT_FALSE(it->NextWord());

  // The first logical character has the largest X coordinate.
  EXPECT_GT(first_x, second_x);
  EXPECT_GT(second_x, third_x);
  EXPECT_GT(third_x, fourth_x);
}

TEST_F(CPVT_VariableTextTest, GetLineCaretX) {
  CPVT_VariableText vt(provider_.get());
  vt.SetPlateRect(CFX_FloatRect(0, 0, 100, 100));
  vt.SetFontSize(10.0f);
  vt.SetMultiLine(false);
  vt.SetAutoReturn(false);
  vt.Initialize();

  CPVT_VariableText::Iterator* it = vt.GetIterator();
  const CPVT_WordPlace kFirstWordPlace(0, 0, -1);

  // Test 1a: Empty text (Left)
  vt.SetAlignment(CPVT_VariableText::Alignment::kLeft);
  vt.SetText(L"");
  vt.RearrangeAll();
  CPVT_Line line;
  it->SetAt(kFirstWordPlace);
  ASSERT_TRUE(it->GetLine(line));
  EXPECT_FLOAT_EQ(0.0f, it->GetLineCaretX(line));
  EXPECT_EQ(kFirstWordPlace, it->GetWordPlace());

  // Test 1b: Empty text (Center)
  vt.SetAlignment(CPVT_VariableText::Alignment::kCenter);
  vt.RearrangeAll();
  it->SetAt(kFirstWordPlace);
  ASSERT_TRUE(it->GetLine(line));
  EXPECT_FLOAT_EQ(50.0f, it->GetLineCaretX(line));
  EXPECT_EQ(kFirstWordPlace, it->GetWordPlace());

  // Test 1c: Empty text (Right)
  vt.SetAlignment(CPVT_VariableText::Alignment::kRight);
  vt.RearrangeAll();
  it->SetAt(kFirstWordPlace);
  ASSERT_TRUE(it->GetLine(line));
  EXPECT_FLOAT_EQ(100.0f, it->GetLineCaretX(line));
  EXPECT_EQ(kFirstWordPlace, it->GetWordPlace());

  // Reset alignment for subsequent tests
  vt.SetAlignment(CPVT_VariableText::Alignment::kLeft);

  // Test 2: LTR text (Helvetica font, starts with left caret)
  vt.SetText(L"hello");
  vt.RearrangeAll();
  it->SetAt(kFirstWordPlace);
  ASSERT_TRUE(it->GetLine(line));
  EXPECT_FLOAT_EQ(0.0f, it->GetLineCaretX(line));
  EXPECT_EQ(kFirstWordPlace, it->GetWordPlace());

  // Test 3: RTL text (starts with right caret)
  vt.SetText(L"\x05E9\x05DC\x05D5\x05DD");
  vt.RearrangeAll();
  it->SetAt(kFirstWordPlace);
  ASSERT_TRUE(it->GetLine(line));
  EXPECT_FLOAT_EQ(0.4f, it->GetLineCaretX(line));
  EXPECT_EQ(kFirstWordPlace, it->GetWordPlace());
}
