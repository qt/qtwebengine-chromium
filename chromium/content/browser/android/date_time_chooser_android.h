// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class ContentViewCore;
class RenderViewHost;
struct DateTimeSuggestion;

// Android implementation for DateTimeChooser dialogs.
class DateTimeChooserAndroid {
 public:
  DateTimeChooserAndroid();
  ~DateTimeChooserAndroid();

  // DateTimeChooser implementation:
  // Shows the dialog. |dialog_value| is the date/time value converted to a
  // number as defined in HTML. (See blink::InputType::parseToNumber())
  void ShowDialog(ContentViewCore* content,
                  RenderViewHost* host,
                  ui::TextInputType dialog_type,
                  double dialog_value,
                  double min,
                  double max,
                  double step,
                  const std::vector<DateTimeSuggestion>& suggestions);

  // Replaces the current value
  void ReplaceDateTime(JNIEnv* env, jobject, jdouble value);

  // Closes the dialog without propagating any changes.
  void CancelDialog(JNIEnv* env, jobject);

  // Propagates the different types of accepted date/time values to the
  // java side.
  static void InitializeDateInputTypes(
       int text_input_type_date, int text_input_type_date_time,
       int text_input_type_date_time_local, int text_input_type_month,
       int text_input_type_time, int text_input_type_week);

 private:
  RenderViewHost* host_;

  base::android::ScopedJavaGlobalRef<jobject> j_date_time_chooser_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeChooserAndroid);
};

// Native JNI methods
bool RegisterDateTimeChooserAndroid(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
