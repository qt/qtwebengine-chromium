// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_PUSHBUTTON_H_
#define XFA_FWL_CFWL_PUSHBUTTON_H_

#include "xfa/fwl/cfwl_widget.h"

namespace pdfium {

class CFWL_MessageKey;
class CFWL_MessageMouse;

class CFWL_PushButton final : public CFWL_Widget {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CFWL_PushButton() override;

  // CFWL_Widget
  FWL_Type GetClassID() const override;
  void SetStates(Mask<WidgetState> states) override;
  void Update() override;
  void DrawWidget(CFGAS_GEGraphics* pGraphics,
                  const CFX_Matrix& matrix) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFGAS_GEGraphics* pGraphics,
                    const CFX_Matrix& matrix) override;

 private:
  explicit CFWL_PushButton(CFWL_App* pApp);

  void DrawBkground(CFGAS_GEGraphics* pGraphics, const CFX_Matrix& mtMatrix);
  Mask<CFWL_PartState> GetPartStates();
  void UpdateTextOutStyles();
  void OnFocusGained();
  void OnFocusLost();
  void OnLButtonDown(CFWL_MessageMouse* pMsg);
  void OnLButtonUp(CFWL_MessageMouse* pMsg);
  void OnMouseMove(CFWL_MessageMouse* pMsg);
  void OnMouseLeave(CFWL_MessageMouse* pMsg);
  void OnKeyDown(CFWL_MessageKey* pMsg);

  bool btn_down_ = false;
  CFX_RectF client_rect_;
  CFX_RectF caption_rect_;
};

}  // namespace pdfium

// TODO(crbug.com/42271761): Remove.
using pdfium::CFWL_PushButton;

#endif  // XFA_FWL_CFWL_PUSHBUTTON_H_
