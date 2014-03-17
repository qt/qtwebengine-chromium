// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SPEECH_UI_MODEL_OBSERVER_H_
#define UI_APP_LIST_SPEECH_UI_MODEL_OBSERVER_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {

enum SpeechRecognitionState {
  SPEECH_RECOGNITION_NOT_STARTED = 0,
  SPEECH_RECOGNITION_ON,
  SPEECH_RECOGNITION_IN_SPEECH,
};

class APP_LIST_EXPORT SpeechUIModelObserver {
 public:
  // Invoked when sound level for the speech recognition has changed.
  virtual void OnSpeechSoundLevelChanged(int16 level) {}

  // Invoked when a speech result arrives. |is_final| is true only when the
  // speech result is final.
  virtual void OnSpeechResult(const base::string16& result, bool is_final) {}

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) {}

 protected:
  virtual ~SpeechUIModelObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_SPEECH_UI_MODEL_OBSERVER_H_
