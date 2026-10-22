// Copyright 2024 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_catalog.h"

#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/fpdf_edit.h"
#include "testing/embedder_test.h"
#include "testing/fx_string_testhelpers.h"

using FPDFCatalogTest = EmbedderTest;

TEST_F(FPDFCatalogTest, SetLanguageInvalidDocument) {
  // Document cannot be nullptr.
  ScopedFPDFWideString en_us_str = GetFPDFWideString(L"en-US");
  EXPECT_FALSE(FPDFCatalog_SetLanguage(nullptr, en_us_str.get()));

  ScopedFPDFDocument doc(FPDF_CreateNewDocument());
  CPDF_Document* cpdf_doc = CPDFDocumentFromFPDFDocument(doc.get());

  // Language cannot be null.
  ASSERT_TRUE(cpdf_doc->GetRoot());
  EXPECT_FALSE(FPDFCatalog_SetLanguage(doc.get(), nullptr));

  // Catalog cannot be nullptr.
  cpdf_doc->SetRootForTesting(nullptr);
  EXPECT_FALSE(FPDFCatalog_SetLanguage(doc.get(), en_us_str.get()));
}

TEST_F(FPDFCatalogTest, SetLanguageNewDocument) {
  ScopedFPDFDocument doc(FPDF_CreateNewDocument());

  const CPDF_Dictionary* catalog =
      CPDFDocumentFromFPDFDocument(doc.get())->GetRoot();
  ASSERT_TRUE(catalog);

  // The new document shouldn't have any entry for /Lang.
  EXPECT_FALSE(catalog->GetStringFor("Lang"));

  // Add a new entry.
  ScopedFPDFWideString en_us_str = GetFPDFWideString(L"en-US");
  EXPECT_TRUE(FPDFCatalog_SetLanguage(doc.get(), en_us_str.get()));

  RetainPtr<const CPDF_String> result_language = catalog->GetStringFor("Lang");
  ASSERT_TRUE(result_language);
  EXPECT_EQ("en-US", result_language->GetString());
}

TEST_F(FPDFCatalogTest, SetLanguageExistingDocument) {
  ASSERT_TRUE(OpenDocument("tagged_table.pdf"));

  const CPDF_Dictionary* catalog =
      CPDFDocumentFromFPDFDocument(document())->GetRoot();
  ASSERT_TRUE(catalog);

  // The PDF already has an existing entry for /Lang.
  RetainPtr<const CPDF_String> result_language = catalog->GetStringFor("Lang");
  ASSERT_TRUE(result_language);
  EXPECT_EQ("en-US", result_language->GetString());

  // Replace the existing entry.
  ScopedFPDFWideString hu_str = GetFPDFWideString(L"hu");
  EXPECT_TRUE(FPDFCatalog_SetLanguage(document(), hu_str.get()));

  result_language = catalog->GetStringFor("Lang");
  ASSERT_TRUE(result_language);
  EXPECT_EQ("hu", result_language->GetString());
}

TEST_F(FPDFCatalogTest, GetLanguageInvalidDocument) {
  // Document cannot be nullptr.
  EXPECT_EQ(0u, FPDFCatalog_GetLanguage(nullptr, nullptr, 0));

  // New document has no Lang entry.
  ScopedFPDFDocument doc(FPDF_CreateNewDocument());
  EXPECT_EQ(2u, FPDFCatalog_GetLanguage(doc.get(), nullptr, 0));
}

TEST_F(FPDFCatalogTest, GetLanguageRoundTrip) {
  ScopedFPDFDocument doc(FPDF_CreateNewDocument());

  // Set language.
  ScopedFPDFWideString en_us_str = GetFPDFWideString(L"en-US");
  EXPECT_TRUE(FPDFCatalog_SetLanguage(doc.get(), en_us_str.get()));

  // Query size. Expected: "en-US" + NUL in UTF-16LE = 6 * 2 bytes.
  unsigned long size = FPDFCatalog_GetLanguage(doc.get(), nullptr, 0);
  EXPECT_EQ(12u, size);

  // Get actual value.
  std::vector<FPDF_WCHAR> buffer = GetFPDFWideStringBuffer(size);
  EXPECT_EQ(size, FPDFCatalog_GetLanguage(doc.get(), buffer.data(), size));
  EXPECT_EQ(L"en-US", GetPlatformWString(buffer.data()));
}

TEST_F(FPDFCatalogTest, GetLanguageExistingDocument) {
  ASSERT_TRUE(OpenDocument("tagged_table.pdf"));

  // Query size. Expected: "en-US" + NUL in UTF-16LE = 6 * 2 bytes.
  unsigned long size = FPDFCatalog_GetLanguage(document(), nullptr, 0);
  EXPECT_EQ(12u, size);

  // Get actual value.
  std::vector<FPDF_WCHAR> buffer = GetFPDFWideStringBuffer(size);
  EXPECT_EQ(size, FPDFCatalog_GetLanguage(document(), buffer.data(), size));
  EXPECT_EQ(L"en-US", GetPlatformWString(buffer.data()));
}
