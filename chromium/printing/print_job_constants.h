// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

#include "build/build_config.h"
#include "printing/printing_export.h"

namespace printing {

PRINTING_EXPORT extern const char kIsFirstRequest[];
PRINTING_EXPORT extern const char kPreviewRequestID[];
PRINTING_EXPORT extern const char kPreviewUIID[];
PRINTING_EXPORT extern const char kSettingCloudPrintId[];
PRINTING_EXPORT extern const char kSettingCloudPrintDialog[];
PRINTING_EXPORT extern const char kSettingCollate[];
PRINTING_EXPORT extern const char kSettingColor[];
PRINTING_EXPORT extern const char kSettingSetColorAsDefault[];
PRINTING_EXPORT extern const char kSettingContentHeight[];
PRINTING_EXPORT extern const char kSettingContentWidth[];
PRINTING_EXPORT extern const char kSettingCopies[];
PRINTING_EXPORT extern const char kSettingDeviceName[];
PRINTING_EXPORT extern const char kSettingDuplexMode[];
PRINTING_EXPORT extern const char kSettingFitToPageEnabled[];
PRINTING_EXPORT extern const char kSettingGenerateDraftData[];
PRINTING_EXPORT extern const char kSettingHeaderFooterEnabled[];
PRINTING_EXPORT extern const float kSettingHeaderFooterInterstice;
PRINTING_EXPORT extern const char kSettingHeaderFooterDate[];
PRINTING_EXPORT extern const char kSettingHeaderFooterTitle[];
PRINTING_EXPORT extern const char kSettingHeaderFooterURL[];
PRINTING_EXPORT extern const char kSettingLandscape[];
PRINTING_EXPORT extern const char kSettingMarginBottom[];
PRINTING_EXPORT extern const char kSettingMarginLeft[];
PRINTING_EXPORT extern const char kSettingMarginRight[];
PRINTING_EXPORT extern const char kSettingMarginTop[];
PRINTING_EXPORT extern const char kSettingMarginsCustom[];
PRINTING_EXPORT extern const char kSettingMarginsType[];
PRINTING_EXPORT extern const char kSettingPreviewPageCount[];
PRINTING_EXPORT extern const char kSettingPageRange[];
PRINTING_EXPORT extern const char kSettingPageRangeFrom[];
PRINTING_EXPORT extern const char kSettingPageRangeTo[];
PRINTING_EXPORT extern const char kSettingPageWidth[];
PRINTING_EXPORT extern const char kSettingPageHeight[];
PRINTING_EXPORT extern const char kSettingPreviewModifiable[];
PRINTING_EXPORT extern const char kSettingPrintableAreaX[];
PRINTING_EXPORT extern const char kSettingPrintableAreaY[];
PRINTING_EXPORT extern const char kSettingPrintableAreaWidth[];
PRINTING_EXPORT extern const char kSettingPrintableAreaHeight[];
PRINTING_EXPORT extern const char kSettingPrinterName[];
PRINTING_EXPORT extern const char kSettingPrintToPDF[];
PRINTING_EXPORT extern const char kSettingPrintWithPrivet[];
PRINTING_EXPORT extern const char kSettingTicket[];
PRINTING_EXPORT extern const char kSettingShouldPrintBackgrounds[];
PRINTING_EXPORT extern const char kSettingShouldPrintSelectionOnly[];

PRINTING_EXPORT extern const int FIRST_PAGE_INDEX;
PRINTING_EXPORT extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;
PRINTING_EXPORT extern const char kSettingOpenPDFInPreview[];

#if defined (USE_CUPS)
// Printer color models
PRINTING_EXPORT extern const char kBlack[];
PRINTING_EXPORT extern const char kCMYK[];
PRINTING_EXPORT extern const char kKCMY[];
PRINTING_EXPORT extern const char kCMY_K[];
PRINTING_EXPORT extern const char kCMY[];
PRINTING_EXPORT extern const char kColor[];
PRINTING_EXPORT extern const char kGray[];
PRINTING_EXPORT extern const char kGrayscale[];
PRINTING_EXPORT extern const char kGreyscale[];
PRINTING_EXPORT extern const char kMonochrome[];
PRINTING_EXPORT extern const char kNormal[];
PRINTING_EXPORT extern const char kNormalGray[];
PRINTING_EXPORT extern const char kRGB[];
PRINTING_EXPORT extern const char kRGBA[];
PRINTING_EXPORT extern const char kRGB16[];
#endif

// Print job duplex mode values.
enum DuplexMode {
  UNKNOWN_DUPLEX_MODE = -1,
  SIMPLEX,
  LONG_EDGE,
  SHORT_EDGE,
};

// Specifies the horizontal alignment of the headers and footers.
enum HorizontalHeaderFooterPosition {
  LEFT,
  CENTER,
  RIGHT
};

// Specifies the vertical alignment of the Headers and Footers.
enum VerticalHeaderFooterPosition {
  TOP,
  BOTTOM
};

// Print job color mode values.
enum ColorModel {
  UNKNOWN_COLOR_MODEL,
  GRAY,
  COLOR,
  CMYK,
  CMY,
  KCMY,
  CMY_K,  // CMY_K represents CMY+K.
  BLACK,
  GRAYSCALE,
  RGB,
  RGB16,
  RGBA,
  COLORMODE_COLOR,  // Used in samsung printer ppds.
  COLORMODE_MONOCHROME,  // Used in samsung printer ppds.
  HP_COLOR_COLOR,  // Used in HP color printer ppds.
  HP_COLOR_BLACK,  // Used in HP color printer ppds.
  PRINTOUTMODE_NORMAL,  // Used in foomatic ppds.
  PRINTOUTMODE_NORMAL_GRAY,  // Used in foomatic ppds.
  PROCESSCOLORMODEL_CMYK,  // Used in canon printer ppds.
  PROCESSCOLORMODEL_GREYSCALE,  // Used in canon printer ppds.
  PROCESSCOLORMODEL_RGB,  // Used in canon printer ppds
};

// What kind of margins to use.
enum MarginType {
  DEFAULT_MARGINS,  // Default varies depending on headers being enabled or not
  NO_MARGINS,
  PRINTABLE_AREA_MARGINS,
  CUSTOM_MARGINS,
};

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_H_
