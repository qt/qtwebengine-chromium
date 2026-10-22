// Copyright 2022 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_defaultrenderdevice.h"

#include <utility>

#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/dib/cfx_dibitmap.h"

#if defined(PDF_USE_AGG)
#include "core/fxge/agg/cfx_agg_devicedriver.h"
#endif

#if defined(PDF_USE_SKIA)
#include "core/fxge/skia/fx_skia_device.h"
#endif

CFX_DefaultRenderDevice::CFX_DefaultRenderDevice() = default;

CFX_DefaultRenderDevice::~CFX_DefaultRenderDevice() = default;

bool CFX_DefaultRenderDevice::Attach(RetainPtr<CFX_DIBitmap> pBitmap) {
  return AttachWithRgbByteOrder(std::move(pBitmap), false);
}

bool CFX_DefaultRenderDevice::AttachWithRgbByteOrder(
    RetainPtr<CFX_DIBitmap> pBitmap,
    bool bRgbByteOrder) {
  return AttachImpl(std::move(pBitmap), bRgbByteOrder, nullptr, false);
}

bool CFX_DefaultRenderDevice::AttachWithBackdropAndGroupKnockout(
    RetainPtr<CFX_DIBitmap> pBitmap,
    RetainPtr<CFX_DIBitmap> pBackdropBitmap,
    bool bGroupKnockout) {
  return AttachImpl(std::move(pBitmap), false, std::move(pBackdropBitmap),
                    bGroupKnockout);
}

bool CFX_DefaultRenderDevice::CFX_DefaultRenderDevice::AttachImpl(
    RetainPtr<CFX_DIBitmap> pBitmap,
    bool bRgbByteOrder,
    RetainPtr<CFX_DIBitmap> pBackdropBitmap,
    bool bGroupKnockout) {
#if defined(PDF_USE_SKIA)
  if (CFX_GEModule::Get()->UseSkiaRenderer()) {
    return AttachSkiaImpl(std::move(pBitmap), bRgbByteOrder,
                          std::move(pBackdropBitmap), bGroupKnockout);
  }
#endif
#if defined(PDF_USE_AGG)
  return AttachAggImpl(std::move(pBitmap), bRgbByteOrder,
                       std::move(pBackdropBitmap), bGroupKnockout);
#else
  return false;
#endif
}

bool CFX_DefaultRenderDevice::Create(int width,
                                     int height,
                                     FXDIB_Format format) {
  return CreateWithBackdrop(width, height, format, nullptr);
}

bool CFX_DefaultRenderDevice::CreateWithBackdrop(
    int width,
    int height,
    FXDIB_Format format,
    RetainPtr<CFX_DIBitmap> backdrop) {
#if defined(PDF_USE_SKIA)
  if (CFX_GEModule::Get()->UseSkiaRenderer()) {
    return CreateSkia(width, height, format, backdrop);
  }
#endif
#if defined(PDF_USE_AGG)
  return CreateAgg(width, height, format, backdrop);
#else
  return false;
#endif
}

void CFX_DefaultRenderDevice::Clear(uint32_t color) {
#if defined(PDF_USE_SKIA)
  if (CFX_GEModule::Get()->UseSkiaRenderer()) {
    static_cast<CFX_SkiaDeviceDriver*>(GetDeviceDriver())->Clear(color);
    return;
  }
#endif
#if defined(PDF_USE_AGG)
  static_cast<pdfium::CFX_AggDeviceDriver*>(GetDeviceDriver())->Clear(color);
#endif
}
