// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cpdfxfa_docenvironment.h"

#include <utility>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/check.h"
#include "core/fxcrt/retain_ptr.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "fpdfsdk/cpdfsdk_interactiveform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/cxfa_ffwidgethandler.h"
#include "xfa/fxfa/cxfa_readynodeiterator.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_submit.h"

#define IDS_XFA_Validate_Input                                          \
  "At least one required field was empty. Please fill in the required " \
  "fields\r\n(highlighted) before continuing."

// Although there isn't direct casting between these types at present,
// keep the internal and exernal types in sync.
static_assert(FXFA_PAGEVIEWEVENT_POSTADDED ==
                  static_cast<int>(CXFA_FFDoc::PageViewEvent::kPostAdded),
              "kPostAdded mismatch");
static_assert(FXFA_PAGEVIEWEVENT_POSTREMOVED ==
                  static_cast<int>(CXFA_FFDoc::PageViewEvent::kPostRemoved),
              "kPostRemoved mismatch");

CPDFXFA_DocEnvironment::CPDFXFA_DocEnvironment(CPDFXFA_Context* context)
    : context_(context) {
  DCHECK(context_);
}

CPDFXFA_DocEnvironment::~CPDFXFA_DocEnvironment() = default;

void CPDFXFA_DocEnvironment::SetChangeMark(CXFA_FFDoc* hDoc) {
  if (hDoc == context_->GetXFADoc() && context_->GetFormFillEnv()) {
    context_->GetFormFillEnv()->SetChangeMark();
  }
}

void CPDFXFA_DocEnvironment::InvalidateRect(CXFA_FFPageView* pPageView,
                                            const CFX_RectF& rt) {
  if (!context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return;
  }

  if (context_->GetFormType() != FormType::kXFAFull) {
    return;
  }

  RetainPtr<CPDFXFA_Page> pPage = context_->GetXFAPage(pPageView);
  if (!pPage) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  pFormFillEnv->Invalidate(pPage.Get(), rt.ToFloatRect().ToFxRect());
}

void CPDFXFA_DocEnvironment::DisplayCaret(CXFA_FFWidget* hWidget,
                                          bool bVisible,
                                          const CFX_RectF* pRtAnchor) {
  if (!hWidget || !pRtAnchor || !context_->GetXFADoc() ||
      !context_->GetFormFillEnv() || !context_->GetXFADocView()) {
    return;
  }

  if (context_->GetFormType() != FormType::kXFAFull) {
    return;
  }

  CXFA_FFWidgetHandler* pWidgetHandler =
      context_->GetXFADocView()->GetWidgetHandler();
  if (!pWidgetHandler) {
    return;
  }

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView) {
    return;
  }

  RetainPtr<CPDFXFA_Page> pPage = context_->GetXFAPage(pPageView);
  if (!pPage) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  CFX_FloatRect rcCaret = pRtAnchor->ToFloatRect();
  pFormFillEnv->DisplayCaret(pPage.Get(), bVisible, rcCaret.left, rcCaret.top,
                             rcCaret.right, rcCaret.bottom);
}

bool CPDFXFA_DocEnvironment::GetPopupPos(CXFA_FFWidget* hWidget,
                                         float fMinPopup,
                                         float fMaxPopup,
                                         const CFX_RectF& rtAnchor,
                                         CFX_RectF* pPopupRect) {
  if (!hWidget) {
    return false;
  }

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView) {
    return false;
  }

  RetainPtr<CPDFXFA_Page> pPage = context_->GetXFAPage(pXFAPageView);
  if (!pPage) {
    return false;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return false;
  }

  FS_RECTF page_view_rect = pFormFillEnv->GetPageViewRect(pPage.Get());
  int nRotate = hWidget->GetNode()->GetRotate();

  int space_available_below_anchor;
  int space_available_above_anchor;
  switch (nRotate) {
    case 0:
    default: {
      space_available_below_anchor =
          static_cast<int>(page_view_rect.bottom - rtAnchor.bottom());
      space_available_above_anchor =
          static_cast<int>(rtAnchor.top - page_view_rect.top);

      if (rtAnchor.left < page_view_rect.left) {
        pPopupRect->left += page_view_rect.left - rtAnchor.left;
      }
      if (rtAnchor.right() > page_view_rect.right) {
        pPopupRect->left -= rtAnchor.right() - page_view_rect.right;
      }
      break;
    }
    case 90: {
      space_available_below_anchor =
          static_cast<int>(page_view_rect.right - rtAnchor.right());
      space_available_above_anchor =
          static_cast<int>(rtAnchor.left - page_view_rect.left);

      if (rtAnchor.bottom() > page_view_rect.bottom) {
        pPopupRect->left += rtAnchor.bottom() - page_view_rect.bottom;
      }
      if (rtAnchor.top < page_view_rect.top) {
        pPopupRect->left -= page_view_rect.top - rtAnchor.top;
      }
      break;
    }
    case 180: {
      space_available_below_anchor =
          static_cast<int>(rtAnchor.top - page_view_rect.top);
      space_available_above_anchor =
          static_cast<int>(page_view_rect.bottom - rtAnchor.bottom());

      if (rtAnchor.right() > page_view_rect.right) {
        pPopupRect->left += rtAnchor.right() - page_view_rect.right;
      }
      if (rtAnchor.left < page_view_rect.left) {
        pPopupRect->left -= page_view_rect.left - rtAnchor.left;
      }
      break;
    }
    case 270: {
      space_available_below_anchor =
          static_cast<int>(rtAnchor.left - page_view_rect.left);
      space_available_above_anchor =
          static_cast<int>(page_view_rect.right - rtAnchor.right());

      if (rtAnchor.top < page_view_rect.top) {
        pPopupRect->left += page_view_rect.top - rtAnchor.top;
      }
      if (rtAnchor.bottom() > page_view_rect.bottom) {
        pPopupRect->left -= rtAnchor.bottom() - page_view_rect.bottom;
      }
      break;
    }
  }

  // If there is no space on either side, the popup can't be rendered.
  if (space_available_below_anchor <= 0 && space_available_above_anchor <= 0) {
    return false;
  }

  // Determine whether to draw above or below the anchor.
  bool draw_below_anchor;
  if (space_available_below_anchor <= 0) {
    draw_below_anchor = false;
  } else if (space_available_above_anchor <= 0) {
    draw_below_anchor = true;
  } else if (space_available_below_anchor > space_available_above_anchor) {
    draw_below_anchor = true;
  } else {
    draw_below_anchor = false;
  }

  int space_available = (draw_below_anchor ? space_available_below_anchor
                                           : space_available_above_anchor);

  // Set the popup height and y position according to what was decided above.
  float popup_height;
  if (space_available < fMinPopup) {
    popup_height = fMinPopup;
  } else if (space_available > fMaxPopup) {
    popup_height = fMaxPopup;
  } else {
    popup_height = static_cast<float>(space_available);
  }

  switch (nRotate) {
    case 0:
    case 180: {
      if (draw_below_anchor) {
        pPopupRect->top = rtAnchor.height;
      } else {
        pPopupRect->top = -popup_height;
      }
      break;
    }
    case 90:
    case 270: {
      if (draw_below_anchor) {
        pPopupRect->top = rtAnchor.width;
      } else {
        pPopupRect->top = -popup_height;
      }
      break;
    }
    default:
      break;
  }

  pPopupRect->height = popup_height;
  return true;
}

bool CPDFXFA_DocEnvironment::PopupMenu(CXFA_FFWidget* hWidget,
                                       const CFX_PointF& ptPopup) {
  if (!hWidget) {
    return false;
  }

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView) {
    return false;
  }

  RetainPtr<CPDFXFA_Page> pPage = context_->GetXFAPage(pXFAPageView);
  if (!pPage) {
    return false;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return false;
  }

  int menuFlag = 0;
  if (hWidget->CanUndo()) {
    menuFlag |= FXFA_MENU_UNDO;
  }
  if (hWidget->CanRedo()) {
    menuFlag |= FXFA_MENU_REDO;
  }
  if (hWidget->CanPaste()) {
    menuFlag |= FXFA_MENU_PASTE;
  }
  if (hWidget->CanCopy()) {
    menuFlag |= FXFA_MENU_COPY;
  }
  if (hWidget->CanCut()) {
    menuFlag |= FXFA_MENU_CUT;
  }
  if (hWidget->CanSelectAll()) {
    menuFlag |= FXFA_MENU_SELECTALL;
  }

  return pFormFillEnv->PopupMenu(pPage.Get(), menuFlag, ptPopup);
}

void CPDFXFA_DocEnvironment::OnPageViewEvent(CXFA_FFPageView* pPageView,
                                             CXFA_FFDoc::PageViewEvent eEvent) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  if (context_->GetLoadStatus() == CPDFXFA_Context::LoadStatus::kLoading ||
      context_->GetLoadStatus() == CPDFXFA_Context::LoadStatus::kClosing ||
      eEvent != CXFA_FFDoc::PageViewEvent::kStopLayout) {
    return;
  }
  int nNewCount = context_->GetPageCount();
  if (nNewCount == context_->GetOriginalPageCount()) {
    return;
  }

  CXFA_FFDocView* pXFADocView = context_->GetXFADocView();
  if (!pXFADocView) {
    return;
  }

  for (int i = 0; i < context_->GetOriginalPageCount(); ++i) {
    RetainPtr<CPDFXFA_Page> pPage = context_->GetXFAPage(i);
    if (!pPage) {
      continue;
    }

    context_->GetFormFillEnv()->RemovePageView(pPage.Get());
    pPage->SetXFAPageViewIndex(i);
  }

  int flag = (nNewCount < context_->GetOriginalPageCount())
                 ? FXFA_PAGEVIEWEVENT_POSTREMOVED
                 : FXFA_PAGEVIEWEVENT_POSTADDED;
  int count = abs(nNewCount - context_->GetOriginalPageCount());
  context_->SetOriginalPageCount(nNewCount);
  pFormFillEnv->PageEvent(count, flag);
}

void CPDFXFA_DocEnvironment::WidgetPostAdd(CXFA_FFWidget* hWidget) {
  if (context_->GetFormType() != FormType::kXFAFull) {
    return;
  }

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView) {
    return;
  }

  RetainPtr<CPDFXFA_Page> pXFAPage = context_->GetXFAPage(pPageView);
  if (!pXFAPage) {
    return;
  }

  auto* formfill = context_->GetFormFillEnv();
  formfill->GetOrCreatePageView(pXFAPage.Get())->AddAnnotForFFWidget(hWidget);
}

void CPDFXFA_DocEnvironment::WidgetPreRemove(CXFA_FFWidget* hWidget) {
  if (context_->GetFormType() != FormType::kXFAFull) {
    return;
  }

  CXFA_FFPageView* pPageView = hWidget->GetPageView();
  if (!pPageView) {
    return;
  }

  RetainPtr<CPDFXFA_Page> pXFAPage = context_->GetXFAPage(pPageView);
  if (!pXFAPage) {
    return;
  }

  CPDFSDK_PageView* pSdkPageView =
      context_->GetFormFillEnv()->GetOrCreatePageView(pXFAPage.Get());
  pSdkPageView->DeleteAnnotForFFWidget(hWidget);
}

int32_t CPDFXFA_DocEnvironment::CountPages(const CXFA_FFDoc* hDoc) const {
  if (hDoc == context_->GetXFADoc() && context_->GetFormFillEnv()) {
    return context_->GetPageCount();
  }
  return 0;
}

int32_t CPDFXFA_DocEnvironment::GetCurrentPage(const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return -1;
  }

  if (context_->GetFormType() != FormType::kXFAFull) {
    return -1;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  return pFormFillEnv ? pFormFillEnv->GetCurrentPageIndex() : -1;
}

void CPDFXFA_DocEnvironment::SetCurrentPage(CXFA_FFDoc* hDoc,
                                            int32_t iCurPage) {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv() ||
      !context_->ContainsExtensionForm() || iCurPage < 0 ||
      iCurPage >= context_->GetFormFillEnv()->GetPageCount()) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  pFormFillEnv->SetCurrentPage(iCurPage);
}

bool CPDFXFA_DocEnvironment::IsCalculationsEnabled(
    const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return false;
  }
  auto* pForm = context_->GetFormFillEnv()->GetInteractiveForm();
  return pForm->IsXfaCalculateEnabled();
}

void CPDFXFA_DocEnvironment::SetCalculationsEnabled(CXFA_FFDoc* hDoc,
                                                    bool bEnabled) {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return;
  }
  context_->GetFormFillEnv()->GetInteractiveForm()->XfaEnableCalculate(
      bEnabled);
}

WideString CPDFXFA_DocEnvironment::GetTitle(const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc()) {
    return WideString();
  }

  CPDF_Document* pPDFDoc = context_->GetPDFDoc();
  if (!pPDFDoc) {
    return WideString();
  }

  RetainPtr<const CPDF_Dictionary> pInfoDict = pPDFDoc->GetInfo();
  if (!pInfoDict) {
    return WideString();
  }

  ByteString csTitle = pInfoDict->GetByteStringFor("Title");
  return WideString::FromDefANSI(csTitle.AsStringView());
}

void CPDFXFA_DocEnvironment::SetTitle(CXFA_FFDoc* hDoc,
                                      const WideString& wsTitle) {
  if (hDoc != context_->GetXFADoc()) {
    return;
  }

  CPDF_Document* pPDFDoc = context_->GetPDFDoc();
  if (!pPDFDoc) {
    return;
  }

  RetainPtr<CPDF_Dictionary> pInfoDict = pPDFDoc->GetInfo();
  if (pInfoDict) {
    pInfoDict->SetNewFor<CPDF_String>("Title", wsTitle.AsStringView());
  }
}

void CPDFXFA_DocEnvironment::ExportData(CXFA_FFDoc* hDoc,
                                        const WideString& wsFilePath,
                                        bool bXDP) {
  if (hDoc != context_->GetXFADoc()) {
    return;
  }

  if (!context_->ContainsExtensionForm()) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  int fileType = bXDP ? FXFA_SAVEAS_XDP : FXFA_SAVEAS_XML;
  ByteString bs = wsFilePath.ToUTF16LE();
  if (wsFilePath.IsEmpty()) {
    if (!pFormFillEnv->IsJSPlatformPresent()) {
      return;
    }
    WideString filepath = pFormFillEnv->JS_fieldBrowse();
    bs = filepath.ToUTF16LE();
  }

  FPDF_FILEHANDLER* pFileHandler = pFormFillEnv->OpenFile(
      bXDP ? FXFA_SAVEAS_XDP : FXFA_SAVEAS_XML, AsFPDFWideString(&bs), "wb");
  if (!pFileHandler) {
    return;
  }

  RetainPtr<IFX_SeekableStream> fileWrite = MakeSeekableStream(pFileHandler);
  if (fileType == FXFA_SAVEAS_XML) {
    fileWrite->WriteString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
    CXFA_FFDoc* ffdoc = context_->GetXFADocView()->GetDoc();
    ffdoc->SavePackage(
        ToNode(ffdoc->GetXFADoc()->GetXFAObject(XFA_HASHCODE_Data)), fileWrite);
  } else if (fileType == FXFA_SAVEAS_XDP) {
    if (!context_->GetPDFDoc()) {
      return;
    }

    const CPDF_Dictionary* pRoot = context_->GetPDFDoc()->GetRoot();
    if (!pRoot) {
      return;
    }

    RetainPtr<const CPDF_Dictionary> pAcroForm = pRoot->GetDictFor("AcroForm");
    if (!pAcroForm) {
      return;
    }

    RetainPtr<const CPDF_Array> pArray =
        ToArray(pAcroForm->GetObjectFor("XFA"));
    if (!pArray) {
      return;
    }

    for (size_t i = 1; i < pArray->size(); i += 2) {
      RetainPtr<const CPDF_Object> pPDFObj = pArray->GetObjectAt(i);
      RetainPtr<const CPDF_Object> pPrePDFObj = pArray->GetObjectAt(i - 1);
      if (!pPrePDFObj->IsString()) {
        continue;
      }
      if (!pPDFObj->IsReference()) {
        continue;
      }

      RetainPtr<const CPDF_Stream> pStream = ToStream(pPDFObj->GetDirect());
      if (!pStream) {
        continue;
      }
      if (pPrePDFObj->GetString() == "form") {
        CXFA_FFDoc* ffdoc = context_->GetXFADocView()->GetDoc();
        ffdoc->SavePackage(
            ToNode(ffdoc->GetXFADoc()->GetXFAObject(XFA_HASHCODE_Form)),
            fileWrite);
        continue;
      }
      if (pPrePDFObj->GetString() == "datasets") {
        CXFA_FFDoc* ffdoc = context_->GetXFADocView()->GetDoc();
        ffdoc->SavePackage(
            ToNode(ffdoc->GetXFADoc()->GetXFAObject(XFA_HASHCODE_Datasets)),
            fileWrite);
        continue;
      }
      if (i == pArray->size() - 1) {
        WideString wPath = WideString::FromUTF16LE(bs.unsigned_span());
        ByteString bPath = wPath.ToUTF8();
        static const char kFormat[] =
            "\n<pdf href=\"%s\" xmlns=\"http://ns.adobe.com/xdp/pdf/\"/>";
        ByteString content = ByteString::Format(kFormat, bPath.c_str());
        fileWrite->WriteString(content.AsStringView());
      }
      auto pAcc = pdfium::MakeRetain<CPDF_StreamAcc>(std::move(pStream));
      pAcc->LoadAllDataFiltered();
      fileWrite->WriteBlock(pAcc->GetSpan());
    }
  }
  fileWrite->Flush();
}

void CPDFXFA_DocEnvironment::GotoURL(CXFA_FFDoc* hDoc,
                                     const WideString& wsURL) {
  if (hDoc != context_->GetXFADoc()) {
    return;
  }

  if (context_->GetFormType() != FormType::kXFAFull) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  pFormFillEnv->GotoURL(wsURL);
}

bool CPDFXFA_DocEnvironment::IsValidationsEnabled(
    const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return false;
  }

  auto* pForm = context_->GetFormFillEnv()->GetInteractiveForm();
  return pForm->IsXfaValidationsEnabled();
}

void CPDFXFA_DocEnvironment::SetValidationsEnabled(CXFA_FFDoc* hDoc,
                                                   bool bEnabled) {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return;
  }

  context_->GetFormFillEnv()->GetInteractiveForm()->XfaSetValidationsEnabled(
      bEnabled);
}

void CPDFXFA_DocEnvironment::SetFocusWidget(CXFA_FFDoc* hDoc,
                                            CXFA_FFWidget* hWidget) {
  if (hDoc != context_->GetXFADoc()) {
    return;
  }

  if (!hWidget) {
    ObservedPtr<CPDFSDK_Annot> pNull;
    context_->GetFormFillEnv()->SetFocusAnnot(pNull);
    return;
  }

  int pageViewCount = context_->GetFormFillEnv()->GetPageViewCount();
  for (int i = 0; i < pageViewCount; i++) {
    CPDFSDK_PageView* pPageView =
        context_->GetFormFillEnv()->GetPageViewAtIndex(i);
    if (!pPageView) {
      continue;
    }

    ObservedPtr<CPDFSDK_Annot> pAnnot(pPageView->GetAnnotForFFWidget(hWidget));
    if (pAnnot) {
      context_->GetFormFillEnv()->SetFocusAnnot(pAnnot);
      break;
    }
  }
}

void CPDFXFA_DocEnvironment::Print(CXFA_FFDoc* hDoc,
                                   int32_t nStartPage,
                                   int32_t nEndPage,
                                   Mask<XFA_PrintOpt> dwOptions) {
  if (hDoc != context_->GetXFADoc()) {
    return;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return;
  }

  IPDF_JSPLATFORM* js_platform = pFormFillEnv->GetJSPlatform();
  if (!js_platform || !js_platform->Doc_print) {
    return;
  }

  js_platform->Doc_print(js_platform, !!(dwOptions & XFA_PrintOpt::kShowDialog),
                         nStartPage, nEndPage,
                         !!(dwOptions & XFA_PrintOpt::kCanCancel),
                         !!(dwOptions & XFA_PrintOpt::kShrinkPage),
                         !!(dwOptions & XFA_PrintOpt::kAsImage),
                         !!(dwOptions & XFA_PrintOpt::kReverseOrder),
                         !!(dwOptions & XFA_PrintOpt::kPrintAnnot));
}

FX_ARGB CPDFXFA_DocEnvironment::GetHighlightColor(
    const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc() || !context_->GetFormFillEnv()) {
    return 0;
  }

  CPDFSDK_InteractiveForm* pForm =
      context_->GetFormFillEnv()->GetInteractiveForm();
  return AlphaAndColorRefToArgb(pForm->GetHighlightAlpha(),
                                pForm->GetHighlightColor(FormFieldType::kXFA));
}

IJS_Runtime* CPDFXFA_DocEnvironment::GetIJSRuntime(
    const CXFA_FFDoc* hDoc) const {
  if (hDoc != context_->GetXFADoc()) {
    return nullptr;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  return pFormFillEnv ? pFormFillEnv->GetIJSRuntime() : nullptr;
}

CFX_XMLDocument* CPDFXFA_DocEnvironment::GetXMLDoc() const {
  return context_->GetXMLDoc();
}

RetainPtr<IFX_SeekableReadStream> CPDFXFA_DocEnvironment::OpenLinkedFile(
    CXFA_FFDoc* hDoc,
    const WideString& wsLink) {
  CPDFSDK_FormFillEnvironment* pFormFillEnv = context_->GetFormFillEnv();
  if (!pFormFillEnv) {
    return nullptr;
  }

  ByteString bs = wsLink.ToUTF16LE();
  FPDF_FILEHANDLER* pFileHandler =
      pFormFillEnv->OpenFile(0, AsFPDFWideString(&bs), "rb");
  if (!pFileHandler) {
    return nullptr;
  }

  return MakeSeekableStream(pFileHandler);
}
