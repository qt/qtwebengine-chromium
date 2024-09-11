/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// based on //chrome/browser/pdf/pdf_extension_util.cc
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qtwebengine/browser/pdf/pdf_extension_util.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/zoom/page_zoom_constants.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "qtwebengine/browser/pdf/pdf_extension_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"

namespace pdf_extension_util {

namespace {

// Adds strings that are used both by the stand-alone PDF Viewer and the Print
// Preview PDF Viewer.
static void AddCommonStrings(base::Value::Dict* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"errorDialogTitle", IDS_PDF_ERROR_DIALOG_TITLE},
      {"pageLoadFailed", IDS_PDF_PAGE_LOAD_FAILED},
      {"pageLoading", IDS_PDF_PAGE_LOADING},
      {"pageReload", IDS_PDF_PAGE_RELOAD_BUTTON},
      {"tooltipFitToPage", IDS_PDF_TOOLTIP_FIT_PAGE},
      {"tooltipFitToWidth", IDS_PDF_TOOLTIP_FIT_WIDTH},
      {"tooltipZoomIn", IDS_PDF_TOOLTIP_ZOOM_IN},
      {"tooltipZoomOut", IDS_PDF_TOOLTIP_ZOOM_OUT},
      {"twoUpViewEnable", IDS_PDF_TWO_UP_VIEW_ENABLE},
  };
  for (const auto& resource : kPdfResources)
    dict->Set(resource.name, l10n_util::GetStringUTF16(resource.id));

  dict->Set("presetZoomFactors", zoom::GetPresetZoomFactorsAsJSON());
  dict->Set("chromeRefresh2023Attribute",
            features::IsChromeWebuiRefresh2023() ? "chrome-refresh-2023" : "");
  dict->Set("pdfOopifEnabled",
            base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif) ? "pdfOopifEnabled" : "");
}

// Adds strings that are used only by the stand-alone PDF Viewer.
static void AddPdfViewerStrings(base::Value::Dict* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
    {"annotationsShowToggle", IDS_PDF_ANNOTATIONS_SHOW_TOGGLE},
    {"bookmarks", IDS_PDF_BOOKMARKS},
    {"bookmarkExpandIconAriaLabel", IDS_PDF_BOOKMARK_EXPAND_ICON_ARIA_LABEL},
    {"downloadEdited", IDS_PDF_DOWNLOAD_EDITED},
    {"downloadOriginal", IDS_PDF_DOWNLOAD_ORIGINAL},
    {"labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER},
    {"menu", IDS_MENU},
    {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
    {"oversizeAttachmentWarning", IDS_PDF_OVERSIZE_ATTACHMENT_WARNING},
    {"passwordDialogTitle", IDS_PDF_PASSWORD_DIALOG_TITLE},
    {"passwordInvalid", IDS_PDF_PASSWORD_INVALID},
    {"passwordPrompt", IDS_PDF_NEED_PASSWORD},
    {"passwordSubmit", IDS_PDF_PASSWORD_SUBMIT},
    {"present", IDS_PDF_PRESENT},
    {"propertiesApplication", IDS_PDF_PROPERTIES_APPLICATION},
    {"propertiesAuthor", IDS_PDF_PROPERTIES_AUTHOR},
    {"propertiesCreated", IDS_PDF_PROPERTIES_CREATED},
    {"propertiesDialogClose", IDS_CLOSE},
    {"propertiesDialogTitle", IDS_PDF_PROPERTIES_DIALOG_TITLE},
    {"propertiesFastWebView", IDS_PDF_PROPERTIES_FAST_WEB_VIEW},
    {"propertiesFastWebViewNo", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_NO},
    {"propertiesFastWebViewYes", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_YES},
    {"propertiesFileName", IDS_PDF_PROPERTIES_FILE_NAME},
    {"propertiesFileSize", IDS_PDF_PROPERTIES_FILE_SIZE},
    {"propertiesKeywords", IDS_PDF_PROPERTIES_KEYWORDS},
    {"propertiesModified", IDS_PDF_PROPERTIES_MODIFIED},
    {"propertiesPageCount", IDS_PDF_PROPERTIES_PAGE_COUNT},
    {"propertiesPageSize", IDS_PDF_PROPERTIES_PAGE_SIZE},
    {"propertiesPdfProducer", IDS_PDF_PROPERTIES_PDF_PRODUCER},
    {"propertiesPdfVersion", IDS_PDF_PROPERTIES_PDF_VERSION},
    {"propertiesSubject", IDS_PDF_PROPERTIES_SUBJECT},
    {"propertiesTitle", IDS_PDF_PROPERTIES_TITLE},
    {"rotationStateLabel0", IDS_PDF_ROTATION_STATE_LABEL_0},
    {"rotationStateLabel90", IDS_PDF_ROTATION_STATE_LABEL_90},
    {"rotationStateLabel180", IDS_PDF_ROTATION_STATE_LABEL_180},
    {"rotationStateLabel270", IDS_PDF_ROTATION_STATE_LABEL_270},
    {"thumbnailPageAriaLabel", IDS_PDF_THUMBNAIL_PAGE_ARIA_LABEL},
    {"tooltipAttachments", IDS_PDF_TOOLTIP_ATTACHMENTS},
    {"tooltipDocumentOutline", IDS_PDF_TOOLTIP_DOCUMENT_OUTLINE},
    {"tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD},
    {"tooltipDownloadAttachment", IDS_PDF_TOOLTIP_DOWNLOAD_ATTACHMENT},
    {"tooltipPrint", IDS_PDF_TOOLTIP_PRINT},
    {"tooltipRotateCCW", IDS_PDF_TOOLTIP_ROTATE_CCW},
    {"tooltipThumbnails", IDS_PDF_TOOLTIP_THUMBNAILS},
    {"zoomTextInputAriaLabel", IDS_PDF_ZOOM_TEXT_INPUT_ARIA_LABEL},

    {"tooltipAnnotate", IDS_PDF_ANNOTATION_ANNOTATE},
    {"annotationDocumentTooLarge", IDS_PDF_ANNOTATION_DOCUMENT_TOO_LARGE},
    {"annotationDocumentProtected", IDS_PDF_ANNOTATION_DOCUMENT_PROTECTED},
    {"annotationDocumentRotated", IDS_PDF_ANNOTATION_DOCUMENT_ROTATED},
    {"annotationEditInDefaultView", IDS_PDF_ANNOTATION_EDIT_IN_DEFAULT_VIEW},
    {"annotationResetRotate", IDS_PDF_ANNOTATION_RESET_ROTATE},
    {"annotationResetTwoPageView", IDS_PDF_ANNOTATION_RESET_TWO_PAGE_VIEW},
    {"annotationResetRotateAndTwoPageView",
     IDS_PDF_ANNOTATION_RESET_ROTATE_AND_TWO_PAGE_VIEW},
    {"cancelButton", IDS_CANCEL},
    {"annotationPen", IDS_PDF_ANNOTATION_PEN},
    {"annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER},
    {"annotationEraser", IDS_PDF_ANNOTATION_ERASER},
    {"annotationUndo", IDS_PDF_ANNOTATION_UNDO},
    {"annotationRedo", IDS_PDF_ANNOTATION_REDO},
    {"annotationExpand", IDS_PDF_ANNOTATION_EXPAND},
    {"annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK},
    {"annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED},
    {"annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW},
    {"annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN},
    {"annotationColorCyan", IDS_PDF_ANNOTATION_COLOR_CYAN},
    {"annotationColorPurple", IDS_PDF_ANNOTATION_COLOR_PURPLE},
    {"annotationColorBrown", IDS_PDF_ANNOTATION_COLOR_BROWN},
    {"annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE},
    {"annotationColorCrimson", IDS_PDF_ANNOTATION_COLOR_CRIMSON},
    {"annotationColorAmber", IDS_PDF_ANNOTATION_COLOR_AMBER},
    {"annotationColorAvocadoGreen", IDS_PDF_ANNOTATION_COLOR_AVOCADO_GREEN},
    {"annotationColorCobaltBlue", IDS_PDF_ANNOTATION_COLOR_COBALT_BLUE},
    {"annotationColorDeepPurple", IDS_PDF_ANNOTATION_COLOR_DEEP_PURPLE},
    {"annotationColorDarkBrown", IDS_PDF_ANNOTATION_COLOR_DARK_BROWN},
    {"annotationColorDarkGrey", IDS_PDF_ANNOTATION_COLOR_DARK_GREY},
    {"annotationColorHotPink", IDS_PDF_ANNOTATION_COLOR_HOT_PINK},
    {"annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE},
    {"annotationColorLime", IDS_PDF_ANNOTATION_COLOR_LIME},
    {"annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE},
    {"annotationColorViolet", IDS_PDF_ANNOTATION_COLOR_VIOLET},
    {"annotationColorTeal", IDS_PDF_ANNOTATION_COLOR_TEAL},
    {"annotationColorLightGrey", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY},
    {"annotationColorLightPink", IDS_PDF_ANNOTATION_COLOR_LIGHT_PINK},
    {"annotationColorLightOrange", IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE},
    {"annotationColorLightGreen", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN},
    {"annotationColorLightBlue", IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE},
    {"annotationColorLavender", IDS_PDF_ANNOTATION_COLOR_LAVENDER},
    {"annotationColorLightTeal", IDS_PDF_ANNOTATION_COLOR_LIGHT_TEAL},
    {"annotationSize1", IDS_PDF_ANNOTATION_SIZE1},
    {"annotationSize2", IDS_PDF_ANNOTATION_SIZE2},
    {"annotationSize3", IDS_PDF_ANNOTATION_SIZE3},
    {"annotationSize4", IDS_PDF_ANNOTATION_SIZE4},
    {"annotationSize8", IDS_PDF_ANNOTATION_SIZE8},
    {"annotationSize12", IDS_PDF_ANNOTATION_SIZE12},
    {"annotationSize16", IDS_PDF_ANNOTATION_SIZE16},
    {"annotationSize20", IDS_PDF_ANNOTATION_SIZE20},
  };
  for (const auto& resource : kPdfResources)
    dict->Set(resource.name, l10n_util::GetStringUTF16(resource.id));

  webui::SetLoadTimeDataDefaults(content::GetContentClient()->browser()->GetApplicationLocale(),
                                 dict);
}

} // namespace

void AddStrings(PdfViewerContext context, base::Value::Dict* dict) {
  AddCommonStrings(dict);
  if (context == PdfViewerContext::kPdfViewer ||
      context == PdfViewerContext::kAll) {
    AddPdfViewerStrings(dict);
  }
  if (context == PdfViewerContext::kPrintPreview ||
      context == PdfViewerContext::kAll) {
    // Nothing to do yet, since there are no PrintPreview-only strings.
  }
}

void AddAdditionalData(bool enable_annotations, base::Value::Dict* dict) {
  bool printing_enabled = true;
  bool annotations_enabled = false;
  dict->Set("printingEnabled", printing_enabled);
  dict->Set("pdfAnnotationsEnabled", annotations_enabled);
}

} // namespace pdf_extension_util
