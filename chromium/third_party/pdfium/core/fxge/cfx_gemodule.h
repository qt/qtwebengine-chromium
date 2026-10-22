// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_GEMODULE_H_
#define CORE_FXGE_CFX_GEMODULE_H_

#include <stdint.h>

#include <memory>

#include "build/build_config.h"
#include "core/fxcrt/unowned_ptr_exclusion.h"
#include "core/fxge/cfx_fontmgr.h"

#if BUILDFLAG(IS_APPLE)
#include "core/fxcrt/span.h"
#endif

class SystemFontInfoIface;

class CFX_GEModule {
 public:
  class PlatformIface {
   public:
    static std::unique_ptr<PlatformIface> Create();
    virtual ~PlatformIface() = default;

    virtual void Init() = 0;
    virtual void Terminate() = 0;

    virtual std::unique_ptr<SystemFontInfoIface>
    CreateDefaultSystemFontInfo() = 0;
#if BUILDFLAG(IS_APPLE)
    virtual void* CreatePlatformFont(pdfium::span<const uint8_t> font_span) = 0;
#endif
  };

  // This internal definition of renderer types must stay updated with respect
  // to the public definition of `FPDF_RENDERER_TYPE` in `fpdfview.h`.
  enum class RendererType {
    kAgg = 0,
    kSkia = 1,
#if defined(PDF_USE_SKIA)
    kDefault = kSkia,
#else
    kDefault = kAgg,
#endif
  };

  static void Create(const char** pUserFontPaths,
                     RendererType renderer_type,
                     CFX_FontMgr::FontBackend backend);
  static void Destroy();
  static CFX_GEModule* Get();

  CFX_FontMgr* GetFontMgr() const { return font_mgr_.get(); }
  PlatformIface* GetPlatform() const { return platform_.get(); }
  const char** GetUserFontPaths() const { return user_font_paths_; }

#if defined(PDF_USE_SKIA)
  // Runtime check to see if Skia is the renderer variant in use.
  bool UseSkiaRenderer() const { return renderer_type_ == RendererType::kSkia; }
#endif

 private:
  CFX_GEModule(const char** pUserFontPaths,
               RendererType renderer_type,
               CFX_FontMgr::FontBackend backend);
  ~CFX_GEModule();

  const RendererType renderer_type_;

  std::unique_ptr<PlatformIface> const platform_;  // Must outlive `font_mgr_`.
  std::unique_ptr<CFX_FontMgr> const font_mgr_;

  // Exclude because taken from public API.
  UNOWNED_PTR_EXCLUSION const char** const user_font_paths_;
};

#endif  // CORE_FXGE_CFX_GEMODULE_H_
