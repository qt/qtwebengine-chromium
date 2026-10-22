// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/page/cpdf_page.h"

#include "constants/page_object.h"
#include "core/fpdfapi/page/test_with_page_module.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/cpdf_test_document.h"
#include "testing/gtest/include/gtest/gtest.h"

using CPDF_PageTest = TestWithPageModule;

TEST_F(CPDF_PageTest, IsValidPageDict) {
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(nullptr));

  auto page_dict = pdfium::MakeRetain<CPDF_Dictionary>();
  page_dict->SetNewFor<CPDF_Name>(pdfium::page_object::kType, "Page");
  EXPECT_TRUE(CPDF_Page::IsValidPageDict(page_dict));
}

TEST_F(CPDF_PageTest, IsValidPageDictWrongTypes) {
  auto page_dict = pdfium::MakeRetain<CPDF_Dictionary>();
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));

  page_dict->SetNewFor<CPDF_Name>(pdfium::page_object::kType, "Font");
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));

  page_dict->SetNewFor<CPDF_String>(pdfium::page_object::kType, "Page");
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));

  page_dict->SetNewFor<CPDF_Null>(pdfium::page_object::kType);
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));
}

TEST_F(CPDF_PageTest, IsValidPageDictReference) {
  CPDF_TestDocument doc;
  auto page_dict = doc.NewIndirect<CPDF_Dictionary>();
  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Name>(nullptr, "Page")));
  EXPECT_TRUE(CPDF_Page::IsValidPageDict(page_dict));

  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Name>(nullptr, "Pages")));
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));

  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Null>()));
  EXPECT_FALSE(CPDF_Page::IsValidPageDict(page_dict));
}

TEST_F(CPDF_PageTest, IsValidPageDictLoose) {
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(nullptr));

  auto page_dict = pdfium::MakeRetain<CPDF_Dictionary>();
  EXPECT_TRUE(CPDF_Page::IsValidPageDictLoose(page_dict));

  page_dict->SetNewFor<CPDF_Name>(pdfium::page_object::kType, "Page");
  EXPECT_TRUE(CPDF_Page::IsValidPageDictLoose(page_dict));
}

TEST_F(CPDF_PageTest, IsValidPageDictLooseWrongTypes) {
  auto page_dict = pdfium::MakeRetain<CPDF_Dictionary>();
  page_dict->SetNewFor<CPDF_Name>(pdfium::page_object::kType, "Font");
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(page_dict));

  page_dict->SetNewFor<CPDF_String>(pdfium::page_object::kType, "Page");
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(page_dict));

  page_dict->SetNewFor<CPDF_Null>(pdfium::page_object::kType);
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(page_dict));
}

TEST_F(CPDF_PageTest, IsValidPageDictLooseReference) {
  CPDF_TestDocument doc;
  auto page_dict = doc.NewIndirect<CPDF_Dictionary>();
  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Name>(nullptr, "Page")));
  EXPECT_TRUE(CPDF_Page::IsValidPageDictLoose(page_dict));

  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Name>(nullptr, "Pages")));
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(page_dict));

  page_dict->SetNewFor<CPDF_Reference>(
      pdfium::page_object::kType, &doc,
      doc.AddIndirectObject(pdfium::MakeRetain<CPDF_Null>()));
  EXPECT_FALSE(CPDF_Page::IsValidPageDictLoose(page_dict));
}
