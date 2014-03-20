// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_drag_utils_win.h"

#include <oleidl.h>
#include "base/logging.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebDragOperationNone;
using blink::WebDragOperationCopy;
using blink::WebDragOperationLink;
using blink::WebDragOperationMove;
using blink::WebDragOperationGeneric;

namespace content {

WebDragOperation WinDragOpToWebDragOp(DWORD effect) {
  DCHECK(effect == DROPEFFECT_NONE || effect == DROPEFFECT_COPY ||
         effect == DROPEFFECT_LINK || effect == DROPEFFECT_MOVE);

  return WinDragOpMaskToWebDragOpMask(effect);
}

WebDragOperationsMask WinDragOpMaskToWebDragOpMask(DWORD effects) {
  WebDragOperationsMask ops = WebDragOperationNone;
  if (effects & DROPEFFECT_COPY)
    ops = static_cast<WebDragOperationsMask>(ops | WebDragOperationCopy);
  if (effects & DROPEFFECT_LINK)
    ops = static_cast<WebDragOperationsMask>(ops | WebDragOperationLink);
  if (effects & DROPEFFECT_MOVE)
    ops = static_cast<WebDragOperationsMask>(ops | WebDragOperationMove |
                                             WebDragOperationGeneric);
  return ops;
}

DWORD WebDragOpToWinDragOp(WebDragOperation op) {
  DCHECK(op == WebDragOperationNone ||
         op == WebDragOperationCopy ||
         op == WebDragOperationLink ||
         op == WebDragOperationMove ||
         op == (WebDragOperationMove | WebDragOperationGeneric));

  return WebDragOpMaskToWinDragOpMask(op);
}

DWORD WebDragOpMaskToWinDragOpMask(WebDragOperationsMask ops) {
  DWORD win_ops = DROPEFFECT_NONE;
  if (ops & WebDragOperationCopy)
    win_ops |= DROPEFFECT_COPY;
  if (ops & WebDragOperationLink)
    win_ops |= DROPEFFECT_LINK;
  if (ops & (WebDragOperationMove | WebDragOperationGeneric))
    win_ops |= DROPEFFECT_MOVE;
  return win_ops;
}

}  // namespace content
