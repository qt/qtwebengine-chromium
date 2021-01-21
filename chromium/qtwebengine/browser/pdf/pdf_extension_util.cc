// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

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
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENABLE_PDF_INK2)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENABLE_PDF_INK2)

namespace pdf_extension_util {

namespace {

// Adds strings that are used both by the stand-alone PDF Viewer and the Print
// Preview PDF Viewer.
static void AddCommonStrings(base::DictValue* dict) {
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
  dict->Set("pdfOopifEnabled",
            base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif) ? "pdfOopifEnabled" : "");
}

// Adds strings that are used only by the stand-alone PDF Viewer.
static void AddPdfViewerStrings(base::DictValue* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"annotationsShowToggle", IDS_PDF_ANNOTATIONS_SHOW_TOGGLE},
      {"bookmarks", IDS_PDF_BOOKMARKS},
      {"bookmarkExpandIconAriaLabel", IDS_PDF_BOOKMARK_EXPAND_ICON_ARIA_LABEL},
      {"downloadEdited", IDS_PDF_DOWNLOAD_EDITED},
      {"downloadOriginal", IDS_PDF_DOWNLOAD_ORIGINAL},
      {"labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER},
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
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      {"searchifyInProgress", IDS_PDF_SEARCHIFY_IN_PROGRESS},
#endif
      {"sidebarLabel", IDS_PDF_SIDEBAR_LABEL},
      {"thumbnailPageAriaLabel", IDS_PDF_THUMBNAIL_PAGE_ARIA_LABEL},
      {"tooltipAttachments", IDS_PDF_TOOLTIP_ATTACHMENTS},
      {"tooltipDocumentOutline", IDS_PDF_TOOLTIP_DOCUMENT_OUTLINE},
      {"tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD},
      {"tooltipDownloadAttachment", IDS_PDF_TOOLTIP_DOWNLOAD_ATTACHMENT},
      {"tooltipPrint", IDS_PDF_TOOLTIP_PRINT},
      {"tooltipRotateCCW", IDS_PDF_TOOLTIP_ROTATE_CCW},
      {"tooltipThumbnails", IDS_PDF_TOOLTIP_THUMBNAILS},
      {"zoomTextInputAriaLabel", IDS_PDF_ZOOM_TEXT_INPUT_ARIA_LABEL},
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
      {"saveToDriveDialogCancelUploadButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_CANCEL_UPLOAD_BUTTON_LABEL},
      {"saveToDriveDialogConnectionErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_CONNECTION_ERROR_MESSAGE},
      {"saveToDriveDialogErrorTitle", IDS_SAVE_TO_DRIVE_DIALOG_ERROR_TITLE},
      {"saveToDriveDialogManageStorageButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_MANAGE_STORAGE_BUTTON_LABEL},
      {"saveToDriveDialogOpenInDriveButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_OPEN_IN_DRIVE_BUTTON_LABEL},
      {"saveToDriveDialogRetryButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_RETRY_BUTTON_LABEL},
      {"saveToDriveDialogSessionTimeoutErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_SESSION_TIMEOUT_ERROR_MESSAGE},
      {"saveToDriveDialogStorageFullErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_STORAGE_FULL_ERROR_MESSAGE},
      {"saveToDriveDialogSuccessMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_SUCCESS_MESSAGE},
      {"saveToDriveDialogSuccessTitle", IDS_SAVE_TO_DRIVE_DIALOG_SUCCESS_TITLE},
      {"saveToDriveDialogUnknownErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_UNKNOWN_ERROR_MESSAGE},
      {"saveToDriveDialogUploadingTitle",
       IDS_SAVE_TO_DRIVE_DIALOG_UPLOADING_TITLE},
      {"tooltipSaveToDrive", IDS_PDF_TOOLTIP_SAVE_TO_DRIVE},
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#if BUILDFLAG(ENABLE_PDF_INK2)
      {"cancelButton", IDS_CANCEL},
      {"annotationPen", IDS_PDF_ANNOTATION_PEN},
      {"annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER},
      {"annotationEraser", IDS_PDF_ANNOTATION_ERASER},
      {"annotationUndo", IDS_PDF_ANNOTATION_UNDO},
      {"annotationRedo", IDS_PDF_ANNOTATION_REDO},
      {"annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK},
      {"annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED},
      {"annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW},
      {"annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN},
      {"annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE},
      {"annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE},
      {"annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE},
      {"annotationColorLightGrey", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY},
      {"annotationColorLightOrange", IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE},
      {"annotationColorLightGreen", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN},
      {"annotationColorLightBlue", IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE},
      {"ink2Draw", IDS_PDF_INK2_DRAW},
      {"ink2Tool", IDS_PDF_INK2_ANNOTATION_TOOL},
      {"ink2Size", IDS_PDF_INK2_ANNOTATION_SIZE},
      {"ink2Color", IDS_PDF_INK2_ANNOTATION_COLOR},
      {"ink2BrushSizeExtraThin", IDS_PDF_INK2_ANNOTATION_SIZE_EXTRA_THIN},
      {"ink2BrushSizeThin", IDS_PDF_INK2_ANNOTATION_SIZE_THIN},
      {"ink2BrushSizeMedium", IDS_PDF_INK2_ANNOTATION_SIZE_MEDIUM},
      {"ink2BrushSizeThick", IDS_PDF_INK2_ANNOTATION_SIZE_THICK},
      {"ink2BrushSizeExtraThick", IDS_PDF_INK2_ANNOTATION_SIZE_EXTRA_THICK},
      {"ink2BrushColorLightRed", IDS_PDF_INK2_ANNOTATION_COLOR_LIGHT_RED},
      {"ink2BrushColorLightYellow", IDS_PDF_INK2_ANNOTATION_COLOR_LIGHT_YELLOW},
      {"ink2BrushColorDarkGrey1", IDS_PDF_INK2_ANNOTATION_COLOR_DARK_GREY_1},
      {"ink2BrushColorDarkGrey2", IDS_PDF_INK2_ANNOTATION_COLOR_DARK_GREY_2},
      {"ink2BrushColorRed1", IDS_PDF_INK2_ANNOTATION_COLOR_RED_1},
      {"ink2BrushColorYellow1", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_1},
      {"ink2BrushColorGreen1", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_1},
      {"ink2BrushColorBlue1", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_1},
      {"ink2BrushColorTan1", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_1},
      {"ink2BrushColorRed2", IDS_PDF_INK2_ANNOTATION_COLOR_RED_2},
      {"ink2BrushColorYellow2", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_2},
      {"ink2BrushColorGreen2", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_2},
      {"ink2BrushColorBlue2", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_2},
      {"ink2BrushColorTan2", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_2},
      {"ink2BrushColorRed3", IDS_PDF_INK2_ANNOTATION_COLOR_RED_3},
      {"ink2BrushColorYellow3", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_3},
      {"ink2BrushColorGreen3", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_3},
      {"ink2BrushColorBlue3", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_3},
      {"ink2BrushColorTan3", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_3},
      {"ink2TextAnnotation", IDS_PDF_INK2_TEXT_ANNOTATION},
      {"ink2TextFont", IDS_PDF_INK2_TEXT_FONT},
      {"ink2TextFontSansSerif", IDS_PDF_INK2_TEXT_FONT_SANS_SERIF},
      {"ink2TextFontSerif", IDS_PDF_INK2_TEXT_FONT_SERIF},
      {"ink2TextFontMonospace", IDS_PDF_INK2_TEXT_FONT_MONOSPACE},
      {"ink2TextFontSize", IDS_PDF_INK2_TEXT_FONT_SIZE},
      {"ink2TextStyles", IDS_PDF_INK2_TEXT_STYLES},
      {"ink2TextStyleBold", IDS_PDF_INK2_TEXT_STYLE_BOLD},
      {"ink2TextStyleItalic", IDS_PDF_INK2_TEXT_STYLE_ITALIC},
      {"ink2TextAlignment", IDS_PDF_INK2_TEXT_ALIGNMENT},
      {"ink2TextAlignLeft", IDS_PDF_INK2_TEXT_ALIGN_LEFT},
      {"ink2TextAlignCenter", IDS_PDF_INK2_TEXT_ALIGN_CENTER},
      {"ink2TextAlignRight", IDS_PDF_INK2_TEXT_ALIGN_RIGHT},
      {"ink2TextColor", IDS_PDF_INK2_TEXT_COLOR},
      {"ink2TextColorCyan1", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_1},
      {"ink2TextColorCyan2", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_2},
      {"ink2TextColorCyan3", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_3},
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
  };

  for (const auto& resource : kPdfResources)
    dict->Set(resource.name, l10n_util::GetStringUTF16(resource.id));

  webui::SetLoadTimeDataDefaults(content::GetContentClient()->browser()->GetApplicationLocale(),
                                 dict);
}

} // namespace

void AddStrings(PdfViewerContext context, base::DictValue* dict) {
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

void AddAdditionalData(bool enable_annotations, base::DictValue* dict) {
  bool printing_enabled = true;
  bool annotations_enabled = false;
#if BUILDFLAG(ENABLE_PDF_INK2)
  bool use_ink2 = base::FeatureList::IsEnabled(chrome_pdf::features::kPdfInk2);
  if (use_ink2) {
    annotations_enabled = enable_annotations;
  }
  dict->Set("pdfInk2Enabled", use_ink2);
  bool text_annotations_enabled =
      use_ink2 && chrome_pdf::features::kPdfInk2TextAnnotations.Get();
  dict->Set("pdfTextAnnotationsEnabled", text_annotations_enabled);
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
  dict->Set("printingEnabled", printing_enabled);
  dict->Set("pdfAnnotationsEnabled", annotations_enabled);
  dict->Set("PdfGetSaveDataInBlocks",
            base::FeatureList::IsEnabled(
                chrome_pdf::features::kPdfGetSaveDataInBlocks));
  dict->Set("pdfUseShowSaveFilePicker",
            base::FeatureList::IsEnabled(
                chrome_pdf::features::kPdfUseShowSaveFilePicker));
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  dict->Set("pdfSaveToDrive",
            base::FeatureList::IsEnabled(chrome_pdf::features::kPdfSaveToDrive));
#endif
  dict->Set("pdfSearchifySaveEnabled", false);
            // chrome_pdf::features::IsPdfSearchifySaveEnabled());
}

} // namespace pdf_extension_util
