// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_catalog.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/page/test_with_page_module.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/cpdf_test_document.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxcrt/widestring.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/cpp/fpdf_scopers.h"
#include "testing/gtest/include/gtest/gtest.h"

class PDFCatalogTest : public TestWithPageModule {
 public:
  void SetUp() override {
    TestWithPageModule::SetUp();
    auto test_doc = std::make_unique<CPDF_TestDocument>();
    test_doc_ = test_doc.get();
    doc_.reset(FPDFDocumentFromCPDFDocument(test_doc.release()));
    root_obj_ = pdfium::MakeRetain<CPDF_Dictionary>();
  }

  void TearDown() override {
    test_doc_ = nullptr;
    doc_.reset();
    TestWithPageModule::TearDown();
  }

  FPDF_DOCUMENT doc() { return doc_.get(); }
  CPDF_TestDocument* test_doc() { return test_doc_; }
  RetainPtr<CPDF_Dictionary> root_obj() { return root_obj_; }

 private:
  ScopedFPDFDocument doc_;  // Must outlive `test_doc_`.
  UnownedPtr<CPDF_TestDocument> test_doc_ = nullptr;
  RetainPtr<CPDF_Dictionary> root_obj_;
};

TEST_F(PDFCatalogTest, IsTagged) {
  // Null doc
  EXPECT_FALSE(FPDFCatalog_IsTagged(nullptr));

  // No root
  test_doc()->SetRoot(nullptr);
  EXPECT_FALSE(FPDFCatalog_IsTagged(doc()));

  // Empty root
  test_doc()->SetRoot(root_obj());
  EXPECT_FALSE(FPDFCatalog_IsTagged(doc()));

  // Root with other key
  root_obj()->SetNewFor<CPDF_String>("OTHER_KEY", "other value");
  EXPECT_FALSE(FPDFCatalog_IsTagged(doc()));

  // Root with empty MarkInfo
  auto mark_info_dict = root_obj()->SetNewFor<CPDF_Dictionary>("MarkInfo");
  EXPECT_FALSE(FPDFCatalog_IsTagged(doc()));

  // MarkInfo present but Marked is 0
  mark_info_dict->SetNewFor<CPDF_Number>("Marked", 0);
  EXPECT_FALSE(FPDFCatalog_IsTagged(doc()));

  // MarkInfo present and Marked is 1, PDF is considered tagged.
  mark_info_dict->SetNewFor<CPDF_Number>("Marked", 1);
  EXPECT_TRUE(FPDFCatalog_IsTagged(doc()));
}

TEST_F(PDFCatalogTest, GetLanguage) {
  // Null document.
  EXPECT_EQ(0u, FPDFCatalog_GetLanguage(nullptr, nullptr, 0));

  // Document has no root.
  test_doc()->SetRoot(nullptr);
  EXPECT_EQ(0u, FPDFCatalog_GetLanguage(doc(), nullptr, 0));

  // Document has no Lang entry.
  test_doc()->SetRoot(root_obj());

  // Query size. Expected: NUL in UTF-16LE = 2 bytes.
  EXPECT_EQ(2u, FPDFCatalog_GetLanguage(doc(), nullptr, 0));

  // Validate returned value (empty string).
  std::vector<FPDF_WCHAR> empty_buf(1);
  EXPECT_EQ(2u, FPDFCatalog_GetLanguage(doc(), empty_buf.data(), 2));
  EXPECT_EQ(L"", WideString::FromUTF16LE(pdfium::as_byte_span(empty_buf)));

  // Set Lang entry.
  root_obj()->SetNewFor<CPDF_String>("Lang", "en-US");

  // Query size. Expected: "en-US" + NUL in UTF-16LE = 6 * 2 bytes.
  unsigned long size = FPDFCatalog_GetLanguage(doc(), nullptr, 0);
  EXPECT_EQ(12u, size);

  // Validate returned value ("en-US").
  std::vector<FPDF_WCHAR> buffer(size / sizeof(FPDF_WCHAR));
  EXPECT_EQ(size, FPDFCatalog_GetLanguage(doc(), buffer.data(), size));
  EXPECT_EQ(L"en-US", WideString::FromUTF16LE(pdfium::as_byte_span(buffer)));
}
