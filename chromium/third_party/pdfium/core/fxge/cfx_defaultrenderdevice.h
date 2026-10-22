// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_DEFAULTRENDERDEVICE_H_
#define CORE_FXGE_CFX_DEFAULTRENDERDEVICE_H_

#include <memory>

#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/dib/fx_dib.h"

class SkCanvas;

class CFX_DefaultRenderDevice final : public CFX_RenderDevice {
 public:
  CFX_DefaultRenderDevice();
  ~CFX_DefaultRenderDevice() override;

  bool Attach(RetainPtr<CFX_DIBitmap> pBitmap);
  bool AttachWithRgbByteOrder(RetainPtr<CFX_DIBitmap> pBitmap,
                              bool bRgbByteOrder);
  bool AttachWithBackdropAndGroupKnockout(
      RetainPtr<CFX_DIBitmap> pBitmap,
      RetainPtr<CFX_DIBitmap> pBackdropBitmap,
      bool bGroupKnockout);
#if defined(PDF_USE_SKIA)
  [[nodiscard]] bool AttachCanvas(SkCanvas& canvas);
#endif

  [[nodiscard]] bool Create(int width, int height, FXDIB_Format format);
  [[nodiscard]] bool CreateWithBackdrop(int width,
                                        int height,
                                        FXDIB_Format format,
                                        RetainPtr<CFX_DIBitmap> backdrop);

  void Clear(uint32_t color);

 private:
  bool AttachImpl(RetainPtr<CFX_DIBitmap> pBitmap,
                  bool bRgbByteOrder,
                  RetainPtr<CFX_DIBitmap> pBackdropBitmap,
                  bool bGroupKnockout);

#if defined(PDF_USE_AGG)
  bool AttachAggImpl(RetainPtr<CFX_DIBitmap> pBitmap,
                     bool bRgbByteOrder,
                     RetainPtr<CFX_DIBitmap> pBackdropBitmap,
                     bool bGroupKnockout);

  bool CreateAgg(int width,
                 int height,
                 FXDIB_Format format,
                 RetainPtr<CFX_DIBitmap> pBackdropBitmap);
#endif

#if defined(PDF_USE_SKIA)
  bool AttachSkiaImpl(RetainPtr<CFX_DIBitmap> pBitmap,
                      bool bRgbByteOrder,
                      RetainPtr<CFX_DIBitmap> pBackdropBitmap,
                      bool bGroupKnockout);

  bool CreateSkia(int width,
                  int height,
                  FXDIB_Format format,
                  RetainPtr<CFX_DIBitmap> pBackdropBitmap);
#endif
};

#endif  // CORE_FXGE_CFX_DEFAULTRENDERDEVICE_H_
