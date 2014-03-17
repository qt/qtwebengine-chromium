// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include "base/memory/scoped_ptr.h"
#include "content/common/text_input_client_messages.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/mac/WebSubstringUtil.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/rect.h"

namespace content {

TextInputClientObserver::TextInputClientObserver(RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      render_view_impl_(render_view) {
}

TextInputClientObserver::~TextInputClientObserver() {
}

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_CharacterIndexForPoint,
                        OnCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_FirstRectForCharacterRange,
                        OnFirstRectForCharacterRange)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

blink::WebView* TextInputClientObserver::webview() {
  return render_view()->GetWebView();
}

void TextInputClientObserver::OnCharacterIndexForPoint(gfx::Point point) {
  blink::WebPoint web_point(point);
  size_t index = webview()->focusedFrame()->characterIndexForPoint(web_point);
  Send(new TextInputClientReplyMsg_GotCharacterIndexForPoint(routing_id(),
      index));
}

void TextInputClientObserver::OnFirstRectForCharacterRange(gfx::Range range) {
  gfx::Rect rect;
#if defined(ENABLE_PLUGINS)
  if (render_view_impl_->focused_pepper_plugin()) {
    rect = render_view_impl_->focused_pepper_plugin()->GetCaretBounds();
  } else
#endif
  {
    blink::WebFrame* frame = webview()->focusedFrame();
    if (frame) {
      blink::WebRect web_rect;
      frame->firstRectForCharacterRange(range.start(), range.length(),
                                        web_rect);
      rect = web_rect;
    }
  }
  Send(new TextInputClientReplyMsg_GotFirstRectForRange(routing_id(), rect));
}

void TextInputClientObserver::OnStringForRange(gfx::Range range) {
#if defined(OS_MACOSX)
  NSAttributedString* string = nil;
  blink::WebFrame* frame = webview()->focusedFrame();
  if (frame) {
    string = blink::WebSubstringUtil::attributedSubstringInRange(
        frame, range.start(), range.length());
  }
  scoped_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(routing_id(),
      *encoded.get()));
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace content
