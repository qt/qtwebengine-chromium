// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TOUCH_POINT_H_
#define CONTENT_BROWSER_ANDROID_TOUCH_POINT_H_

#include <jni.h>

#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// This class provides a helper method to convert a java object array of touch
// events (in physical pixdels) into a blink::WebTouchEvent (in dip).
class TouchPoint {
 public:
  static void BuildWebTouchEvent(JNIEnv* env,
                                 jint type,
                                 jlong time_ms,
                                 float dpi_scale,
                                 jobjectArray pts,
                                 blink::WebTouchEvent& event);
};

bool RegisterTouchPoint(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CHROME_VIEW_H_
