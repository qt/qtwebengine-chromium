// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MESSAGE_FILTER_H_

#include "content/common/mac/attributed_string_coder.h"
#include "content/public/browser/browser_message_filter.h"

namespace gfx {
class Range;
class Rect;
}

namespace content {

// This is a browser-side message filter that lives on the IO thread to handle
// replies to messages sent by the TextInputClientMac. See
// content/browser/renderer_host/text_input_client_mac.h for more information.
class CONTENT_EXPORT TextInputClientMessageFilter
    : public BrowserMessageFilter {
 public:
  explicit TextInputClientMessageFilter(int child_id);

  // BrowserMessageFilter override:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 protected:
  virtual ~TextInputClientMessageFilter();

 private:
  // IPC Message handlers:
  void OnGotCharacterIndexForPoint(size_t index);
  void OnGotFirstRectForRange(const gfx::Rect& rect);
  void OnGotStringFromRange(
      const mac::AttributedStringCoder::EncodedString& string);

  // Child process id.
  int child_process_id_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MESSAGE_FILTER_H_
