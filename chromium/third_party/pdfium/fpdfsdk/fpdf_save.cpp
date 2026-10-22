// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_save.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fpdfapi/edit/cpdf_creator.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/mask.h"
#include "core/fxcrt/stl_util.h"
#include "fpdfsdk/cpdfsdk_filewriteadapter.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/fpdf_edit.h"

#ifdef PDF_ENABLE_XFA
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/cfx_memorystream.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "public/fpdf_formfill.h"
#endif

static_assert(FPDF_INCREMENTAL == CPDF_Creator::CreateFlags::kIncremental);
static_assert(FPDF_NO_INCREMENTAL == CPDF_Creator::CreateFlags::kNoOriginal);
static_assert(FPDF_REMOVE_SECURITY_DEPRECATED ==
              CPDF_Creator::CreateFlags::kRemoveSecurityDeprecated);
static_assert(FPDF_REMOVE_SECURITY ==
              CPDF_Creator::CreateFlags::kRemoveSecurity);
static_assert(FPDF_SUBSET_NEW_FONTS ==
              CPDF_Creator::CreateFlags::kSubsetNewFonts);

namespace {

#ifdef PDF_ENABLE_XFA
bool SaveXFADocumentData(
    CPDFXFA_Context* context,
    std::vector<RetainPtr<IFX_SeekableStream>>* file_list) {
  if (!context) {
    return false;
  }

  if (!context->ContainsExtensionForm()) {
    return true;
  }

  CPDF_Document* doc = context->GetPDFDoc();
  if (!doc) {
    return false;
  }

  RetainPtr<CPDF_Dictionary> root = doc->GetMutableRoot();
  if (!root) {
    return false;
  }

  RetainPtr<CPDF_Dictionary> acro_form = root->GetMutableDictFor("AcroForm");
  if (!acro_form) {
    return false;
  }

  RetainPtr<CPDF_Object> xfa = acro_form->GetMutableObjectFor("XFA");
  if (!xfa) {
    return true;
  }

  CPDF_Array* xfa_array = xfa->AsMutableArray();
  if (!xfa_array) {
    return false;
  }

  int size = fxcrt::CollectionSize<int>(*xfa_array);
  int form_index = -1;
  int datasets_index = -1;
  for (int i = 0; i < size - 1; i++) {
    RetainPtr<const CPDF_Object> xfa_obj = xfa_array->GetObjectAt(i);
    if (!xfa_obj->IsString()) {
      continue;
    }
    if (xfa_obj->GetString() == "form") {
      form_index = i + 1;
    } else if (xfa_obj->GetString() == "datasets") {
      datasets_index = i + 1;
    }
  }

  RetainPtr<CPDF_Stream> form_stream;
  if (form_index != -1) {
    // Get form CPDF_Stream
    RetainPtr<CPDF_Object> form_obj = xfa_array->GetMutableObjectAt(form_index);
    if (form_obj->IsReference()) {
      RetainPtr<CPDF_Object> form_direct_obj = form_obj->GetMutableDirect();
      if (form_direct_obj && form_direct_obj->IsStream()) {
        form_stream.Reset(form_direct_obj->AsMutableStream());
      }
    } else if (form_obj->IsStream()) {
      form_stream.Reset(form_obj->AsMutableStream());
    }
  }

  RetainPtr<CPDF_Stream> datasets_stream;
  if (datasets_index != -1) {
    // Get datasets CPDF_Stream
    RetainPtr<CPDF_Object> datasets_obj =
        xfa_array->GetMutableObjectAt(datasets_index);
    if (datasets_obj->IsReference()) {
      CPDF_Reference* datasets_ref_obj = datasets_obj->AsMutableReference();
      RetainPtr<CPDF_Object> datasets_direct_obj =
          datasets_ref_obj->GetMutableDirect();
      if (datasets_direct_obj && datasets_direct_obj->IsStream()) {
        datasets_stream.Reset(datasets_direct_obj->AsMutableStream());
      }
    } else if (datasets_obj->IsStream()) {
      datasets_stream.Reset(datasets_obj->AsMutableStream());
    }
  }
  // L"datasets"
  {
    RetainPtr<IFX_SeekableStream> file_write =
        pdfium::MakeRetain<CFX_MemoryStream>();
    if (context->SaveDatasetsPackage(file_write) && file_write->GetSize() > 0) {
      if (datasets_index != -1) {
        if (datasets_stream) {
          datasets_stream->InitStreamFromFile(file_write);
        }
      } else {
        auto data_stream = doc->NewIndirect<CPDF_Stream>(
            file_write, doc->New<CPDF_Dictionary>());
        int last_index = fxcrt::CollectionSize<int>(*xfa_array) - 2;
        xfa_array->InsertNewAt<CPDF_String>(last_index, "datasets");
        xfa_array->InsertNewAt<CPDF_Reference>(last_index + 1, doc,
                                               data_stream->GetObjNum());
      }
      file_list->push_back(std::move(file_write));
    }
  }
  // L"form"
  {
    RetainPtr<IFX_SeekableStream> file_write =
        pdfium::MakeRetain<CFX_MemoryStream>();
    if (context->SaveFormPackage(file_write) && file_write->GetSize() > 0) {
      if (form_index != -1) {
        if (form_stream) {
          form_stream->InitStreamFromFile(file_write);
        }
      } else {
        auto data_stream = doc->NewIndirect<CPDF_Stream>(
            file_write, doc->New<CPDF_Dictionary>());
        int last_index = fxcrt::CollectionSize<int>(*xfa_array) - 2;
        xfa_array->InsertNewAt<CPDF_String>(last_index, "form");
        xfa_array->InsertNewAt<CPDF_Reference>(last_index + 1, doc,
                                               data_stream->GetObjNum());
      }
      file_list->push_back(std::move(file_write));
    }
  }
  return true;
}
#endif  // PDF_ENABLE_XFA

bool DoDocSave(FPDF_DOCUMENT document,
               FPDF_FILEWRITE* file_write,
               FPDF_DWORD flags,
               std::optional<int> version) {
  CPDF_Document* doc = CPDFDocumentFromFPDFDocument(document);
  if (!doc) {
    return false;
  }

#ifdef PDF_ENABLE_XFA
  auto* context = static_cast<CPDFXFA_Context*>(doc->GetExtension());
  if (context) {
    std::vector<RetainPtr<IFX_SeekableStream>> file_list;
    context->SendPreSaveToXFADoc(&file_list);
    SaveXFADocumentData(context, &file_list);
  }
#endif  // PDF_ENABLE_XFA

  CPDF_Creator file_maker(
      doc, pdfium::MakeRetain<CPDFSDK_FileWriteAdapter>(file_write));
  bool create_result = file_maker.Create(
      Mask<CPDF_Creator::CreateFlags>::FromUnderlyingUnchecked(
          static_cast<uint32_t>(flags)),
      version.value_or(0));

#ifdef PDF_ENABLE_XFA
  if (context) {
    context->SendPostSaveToXFADoc();
  }
#endif  // PDF_ENABLE_XFA

  return create_result;
}

}  // namespace

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDF_SaveAsCopy(FPDF_DOCUMENT document,
                                                    FPDF_FILEWRITE* file_write,
                                                    FPDF_DWORD flags) {
  return DoDocSave(document, file_write, flags, {});
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDF_SaveWithVersion(FPDF_DOCUMENT document,
                     FPDF_FILEWRITE* file_write,
                     FPDF_DWORD flags,
                     int fileVersion) {
  return DoDocSave(document, file_write, flags, fileVersion);
}
