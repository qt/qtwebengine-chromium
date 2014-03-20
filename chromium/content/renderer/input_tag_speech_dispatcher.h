// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_
#define CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_

#include "base/basictypes.h"
#include "content/public/common/speech_recognition_result.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebSpeechInputController.h"

namespace blink {
class WebSpeechInputListener;
}

namespace content {
class RenderViewImpl;
struct SpeechRecognitionResult;

// InputTagSpeechDispatcher is a delegate for messages used by WebKit. It's
// the complement of InputTagSpeechDispatcherHost (owned by RenderViewHost).
class InputTagSpeechDispatcher : public RenderViewObserver,
                                 public blink::WebSpeechInputController {
 public:
  InputTagSpeechDispatcher(RenderViewImpl* render_view,
                           blink::WebSpeechInputListener* listener);

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // blink::WebSpeechInputController.
  virtual bool startRecognition(int request_id,
                                const blink::WebRect& element_rect,
                                const blink::WebString& language,
                                const blink::WebString& grammar,
                                const blink::WebSecurityOrigin& origin);

  virtual void cancelRecognition(int request_id);
  virtual void stopRecording(int request_id);

  void OnSpeechRecognitionResults(
      int request_id, const SpeechRecognitionResults& results);
  void OnSpeechRecordingComplete(int request_id);
  void OnSpeechRecognitionComplete(int request_id);
  void OnSpeechRecognitionToggleSpeechInput();

  blink::WebSpeechInputListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(InputTagSpeechDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_
