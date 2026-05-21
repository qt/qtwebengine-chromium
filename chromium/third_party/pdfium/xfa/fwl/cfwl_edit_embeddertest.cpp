// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fwl/cfwl_edit.h"

#include <memory>

#include "core/fxge/cfx_defaultrenderdevice.h"
#include "public/fpdf_ext.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_fwlevent.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_environment.h"
#include "testing/embedder_test_timer_handling_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/xfa_js_embedder_test.h"

namespace pdfium {

namespace {

constexpr char kEmailRecommendedFilledFilename[] = "email_filled";

}  // namespace

class CFWLEditEmbedderTest : public XFAJSEmbedderTest {
 protected:
  void SetUp() override {
    EmbedderTest::SetUp();
    SetDelegate(&delegate_);

    // Arbitrary, picked nice even number, 2020-09-13 12:26:40.
    FSDK_SetTimeFunction([]() -> time_t { return 1600000000; });
    FSDK_SetLocaltimeFunction([](const time_t* t) { return gmtime(t); });
  }

  void TearDown() override {
    FSDK_SetTimeFunction(nullptr);
    FSDK_SetLocaltimeFunction(nullptr);
    EmbedderTest::TearDown();
  }

  void CreateAndInitializeFormPDF(const char* filename) {
    ASSERT_TRUE(OpenDocument(filename));
  }

  EmbedderTestTimerHandlingDelegate delegate() const { return delegate_; }

 private:
  EmbedderTestTimerHandlingDelegate delegate_;
};

TEST_F(CFWLEditEmbedderTest, Trivial) {
  CreateAndInitializeFormPDF("xfa/email_recommended.pdf");
  ScopedPage page = LoadScopedPage(0);
  ASSERT_EQ(0u, delegate().GetAlerts().size());
}

TEST_F(CFWLEditEmbedderTest, LeftClickMouseSelection) {
  CreateAndInitializeFormPDF("xfa/email_recommended.pdf");
  ScopedPage page = LoadScopedPage(0);
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  for (size_t i = 0; i < 10; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }

  // Mouse selection
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 128, 58);
  FORM_OnLButtonDown(form_handle(), page.get(), FWL_EVENTFLAG_ShiftKey, 152,
                     58);

  // 12 == (2 * strlen(defgh)) + 2 (for \0\0)
  ASSERT_EQ(12U, FORM_GetSelectedText(form_handle(), page.get(), nullptr, 0));

  uint16_t buf[6];
  ASSERT_EQ(12U,
            FORM_GetSelectedText(form_handle(), page.get(), &buf, sizeof(buf)));
  EXPECT_EQ("defgh", GetPlatformString(buf));
}

TEST_F(CFWLEditEmbedderTest, DragMouseSelection) {
  // TODO(crbug.com/40096188): Fix this test and enable for Skia variants.
  if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
    return;
  }

  CreateAndInitializeFormPDF("xfa/email_recommended.pdf");
  ScopedPage page = LoadScopedPage(0);
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  for (size_t i = 0; i < 10; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }

  // Mouse selection
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 128, 58);
  FORM_OnMouseMove(form_handle(), page.get(), FWL_EVENTFLAG_ShiftKey, 152, 58);

  // 12 == (2 * strlen(defgh)) + 2 (for \0\0)
  ASSERT_EQ(12U, FORM_GetSelectedText(form_handle(), page.get(), nullptr, 0));

  uint16_t buf[6];
  ASSERT_EQ(12U,
            FORM_GetSelectedText(form_handle(), page.get(), &buf, sizeof(buf)));
  EXPECT_EQ("defgh", GetPlatformString(buf));

  // TODO(hnakashima): This is incorrect. Visually 'abcdefgh' are selected.
  constexpr char kDraggedFilename[] = "drag_mouse_formfill";
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPng(page_bitmap.get(), kDraggedFilename);
  }
}

TEST_F(CFWLEditEmbedderTest, SimpleFill) {
  // TODO(crbug.com/40096188): Fix this test and enable for Skia variants.
  if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
    return;
  }

  CreateAndInitializeFormPDF("xfa/email_recommended.pdf");
  ScopedPage page = LoadScopedPage(0);
  constexpr char kBlankFilename[] = "blank_email";
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPng(page_bitmap.get(), kBlankFilename);
  }

  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  for (size_t i = 0; i < 10; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }

  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPng(page_bitmap.get(), kEmailRecommendedFilledFilename);
  }
}

TEST_F(CFWLEditEmbedderTest, FillWithNewLineWithoutMultiline) {
  // TODO(crbug.com/40096188): Fix this test and enable for Skia variants.
  if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
    return;
  }

  CreateAndInitializeFormPDF("xfa/email_recommended.pdf");
  ScopedPage page = LoadScopedPage(0);
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  for (size_t i = 0; i < 5; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }
  FORM_OnChar(form_handle(), page.get(), '\r', 0);
  for (size_t i = 5; i < 10; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }

  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPng(page_bitmap.get(), kEmailRecommendedFilledFilename);
  }
}

TEST_F(CFWLEditEmbedderTest, FillWithNewLineWithMultiline) {
  CreateAndInitializeFormPDF("xfa/xfa_multiline_textfield.pdf");
  ScopedPage page = LoadScopedPage(0);
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);

  for (size_t i = 0; i < 5; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }
  FORM_OnChar(form_handle(), page.get(), '\r', 0);
  for (size_t i = 5; i < 10; ++i) {
    FORM_OnChar(form_handle(), page.get(), 'a' + i, 0);
  }

  // Should look like:
  // abcde
  // fghij|
  {
    constexpr char kFilledMultilineBasename[] =
        "xfa_multiline_textfield_filled";
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPngWithExpectationSuffix(page_bitmap.get(),
                                            kFilledMultilineBasename);
  }

  for (size_t i = 0; i < 4; ++i) {
    FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Left, 0);
  }

  // Should look like:
  // abcde
  // f|ghij

  // Two backspaces is a workaround because left arrow does not behave well
  // in the first character of a line. It skips back to the previous line.
  for (size_t i = 0; i < 2; ++i) {
    FORM_OnChar(form_handle(), page.get(), '\b', 0);
  }

  // Should look like:
  // abcde|ghij
  {
    constexpr char kMultilineBackspaceBasename[] =
        "xfa_multiline_textfield_backspace";
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPngWithExpectationSuffix(page_bitmap.get(),
                                            kMultilineBackspaceBasename);
  }
}

TEST_F(CFWLEditEmbedderTest, DateTimePickerTest) {
  // TODO(crbug.com/40096188): Fix this test and enable for Skia variants.
  if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
    return;
  }

  CreateAndInitializeFormPDF("xfa/xfa_date_time_edit.pdf");
  ScopedPage page = LoadScopedPage(0);

  // Give focus to date time widget, creating down-arrow button.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 115, 58);
  constexpr char kSelectedFilename[] = "selected_datetime";
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPng(page_bitmap.get(), kSelectedFilename);
  }

  // Click down-arrow button, bringing up calendar widget.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 446, 54);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 446, 54);
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);

    // TODO(tsepez): hermetic fonts.
    // const char kCalendarOpenMD5[] = "02de64e7e83c82c1ef0ae484d671a51d";
    // CompareBitmap(page_bitmap.get(), 612, 792, kCalendarOpenMD5);
  }

  // Click on date on calendar, putting result into field as text.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 100, 162);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 100, 162);
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);

    // TODO(tsepez): hermetic fonts.
    // const char kFilledMD5[] = "1bce66c11f1c87b8d639ce0076ac36d3";
    // CompareBitmap(page_bitmap.get(), 612, 792, kFilledMD5);
  }
}

TEST_F(CFWLEditEmbedderTest, ImageEditTest) {
  CreateAndInitializeFormPDF("xfa/xfa_image_edit.pdf");
  ScopedPage page = LoadScopedPage(0);
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  constexpr char kFilledBasename[] = "xfa_image_edit";
  ScopedFPDFBitmap page_bitmap =
      RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
  CompareBitmapToPngWithFuzzyExpectationSuffix(page_bitmap.get(),
                                               kFilledBasename);
}

TEST_F(CFWLEditEmbedderTest, ComboBoxTest) {
  CreateAndInitializeFormPDF("xfa/xfa_combobox.pdf");
  ScopedPage page = LoadScopedPage(0);

  // Give focus to widget.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 115, 58);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 115, 58);
  {
    constexpr char kFilledBasename[] = "filled_combox";
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    CompareBitmapToPngWithExpectationSuffix(page_bitmap.get(), kFilledBasename);
  }

  // Click on down-arrow button, dropdown list appears.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 438, 53);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 438, 53);
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    // TODO(tsepez): hermetic fonts.
    // const char kFilledMD5[] = "dad642ae8a5afce2591ffbcabbfc58dd";
    // CompareBitmap(page_bitmap.get(), 612, 792, kFilledMD5);
  }

  // Enter drop-down list, selection highlighted.
  FORM_OnMouseMove(form_handle(), page.get(), 0, 253, 107);
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    // TODO(tsepez): hermetic fonts.
    // const char kFilledMD5[] = "dad642ae8a5afce2591ffbcabbfc58dd";
    // CompareBitmap(page_bitmap.get(), 612, 792, kFilledMD5);
  }

  // Click on selection, putting result into field.
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 253, 107);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 253, 107);
  {
    ScopedFPDFBitmap page_bitmap =
        RenderLoadedPageWithFlags(page.get(), FPDF_ANNOT);
    // TODO(tsepez): hermetic fonts.
    // const char kFilledMD5[] = "dad642ae8a5afce2591ffbcabbfc58dd";
    // CompareBitmap(page_bitmap.get(), 612, 792, kFilledMD5);
  }
}

}  // namespace pdfium
