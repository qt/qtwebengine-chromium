// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_gemodule.h"

#include "core/fxcrt/check.h"
#include "core/fxge/cfx_folderfontinfo.h"

namespace {

CFX_GEModule* g_GEModule = nullptr;

}  // namespace

// static
void CFX_GEModule::Create(const char** pUserFontPaths,
                          RendererType renderer_type,
                          CFX_FontMgr::FontBackend backend) {
  DCHECK(!g_GEModule);
  g_GEModule = new CFX_GEModule(pUserFontPaths, renderer_type, backend);
  g_GEModule->platform_->Init();
  g_GEModule->font_mgr_->GetBuiltinMapper()->SetSystemFontInfo(
      g_GEModule->platform_->CreateDefaultSystemFontInfo());
}

// static
void CFX_GEModule::Destroy() {
  DCHECK(g_GEModule);
  g_GEModule->platform_->Terminate();
  delete g_GEModule;
  g_GEModule = nullptr;
}

// static
CFX_GEModule* CFX_GEModule::Get() {
  DCHECK(g_GEModule);
  return g_GEModule;
}

CFX_GEModule::CFX_GEModule(const char** pUserFontPaths,
                           RendererType renderer_type,
                           CFX_FontMgr::FontBackend backend)
    : renderer_type_(renderer_type),
      platform_(PlatformIface::Create()),
      font_mgr_(std::make_unique<CFX_FontMgr>(backend)),
      user_font_paths_(pUserFontPaths) {}

CFX_GEModule::~CFX_GEModule() = default;
