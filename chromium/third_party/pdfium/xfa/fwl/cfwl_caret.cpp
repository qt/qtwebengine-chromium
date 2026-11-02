// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_caret.h"

#include <utility>

#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/ifwl_themeprovider.h"

namespace pdfium {

namespace {

const uint32_t kBlinkPeriodMs = 600;

}  // namespace

CFWL_Caret::CFWL_Caret(CFWL_App* app,
                       const Properties& properties,
                       CFWL_Widget* pOuter)
    : CFWL_Widget(app, properties, pOuter) {
  SetStates({WidgetState::kCaretHighlight});
}

CFWL_Caret::~CFWL_Caret() = default;

void CFWL_Caret::PreFinalize() {
  timer_.reset();
  CFWL_Widget::PreFinalize();
}

FWL_Type CFWL_Caret::GetClassID() const {
  return FWL_Type::Caret;
}

void CFWL_Caret::Update() {}

void CFWL_Caret::DrawWidget(CFGAS_GEGraphics* pGraphics,
                            const CFX_Matrix& matrix) {
  if (!pGraphics) {
    return;
  }

  DrawCaretBK(pGraphics, matrix);
}

void CFWL_Caret::ShowCaret() {
  timer_ = std::make_unique<CFX_Timer>(GetFWLApp()->GetTimerHandler(), this,
                                       kBlinkPeriodMs);
  ClearStates(WidgetState::kInvisible);
  SetStates({WidgetState::kCaretHighlight});
}

void CFWL_Caret::HideCaret() {
  timer_.reset();
  SetStates(WidgetState::kInvisible);
}

void CFWL_Caret::DrawCaretBK(CFGAS_GEGraphics* pGraphics,
                             const CFX_Matrix& mtMatrix) {
  if (!(properties_.states_ & WidgetState::kCaretHighlight)) {
    return;
  }

  CFWL_ThemeBackground param(CFWL_ThemePart::Part::kBackground, this,
                             pGraphics);
  param.part_rect_ = CFX_RectF(0, 0, GetWidgetRect().Size());
  param.states_ = CFWL_PartState::kHightLight;
  param.matrix_ = mtMatrix;
  GetThemeProvider()->DrawBackground(param);
}

void CFWL_Caret::OnProcessMessage(CFWL_Message* pMessage) {}

void CFWL_Caret::OnDrawWidget(CFGAS_GEGraphics* pGraphics,
                              const CFX_Matrix& matrix) {
  DrawWidget(pGraphics, matrix);
}

void CFWL_Caret::OnTimerFired() {
  if (!(GetStates() & WidgetState::kCaretHighlight)) {
    SetStates({WidgetState::kCaretHighlight});
  } else {
    ClearStates(WidgetState::kCaretHighlight);
  }

  CFX_RectF rt = GetWidgetRect();
  RepaintRect(CFX_RectF(0, 0, rt.width + 1, rt.height));
}

}  // namespace pdfium
