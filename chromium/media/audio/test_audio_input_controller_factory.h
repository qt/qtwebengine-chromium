// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_
#define MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_

#include "base/bind.h"
#include "media/audio/audio_input_controller.h"

namespace media {

class UserInputMonitor;
class TestAudioInputControllerFactory;

// TestAudioInputController and TestAudioInputControllerFactory are used for
// testing consumers of AudioInputController. TestAudioInputControllerFactory
// is a AudioInputController::Factory that creates TestAudioInputControllers.
//
// TestAudioInputController::Record and Close are overriden to do nothing. It is
// expected that you'll grab the EventHandler from the TestAudioInputController
// and invoke the callback methods when appropriate. In this way it's easy to
// mock a AudioInputController.
//
// Typical usage:
//   // Create and register factory.
//   TestAudioInputControllerFactory factory;
//   AudioInputController::set_factory_for_testing(&factory);
//
//   // Do something that triggers creation of an AudioInputController.
//   TestAudioInputController* controller = factory.last_controller();
//   DCHECK(controller);
//
//   // Notify event handler with whatever data you want.
//   controller->event_handler()->OnCreated(...);
//
//   // Do something that triggers AudioInputController::Record to be called.
//   controller->event_handler()->OnData(...);
//   controller->event_handler()->OnError(...);
//
//   // Make sure consumer of AudioInputController does the right thing.
//   ...
//   // Reset factory.
//   AudioInputController::set_factory_for_testing(NULL);

class TestAudioInputController : public AudioInputController {
 public:
  class Delegate {
   public:
    virtual void TestAudioControllerOpened(
        TestAudioInputController* controller) = 0;
    virtual void TestAudioControllerClosed(
        TestAudioInputController* controller) = 0;
  };

  TestAudioInputController(TestAudioInputControllerFactory* factory,
                           AudioManager* audio_manager,
                           const AudioParameters& audio_parameters,
                           EventHandler* event_handler,
                           SyncWriter* sync_writer,
                           UserInputMonitor* user_input_monitor);

  // Returns the event handler installed on the AudioInputController.
  EventHandler* event_handler() const { return event_handler_; }

  // Notifies the TestAudioControllerOpened() event to the delegate (if any).
  virtual void Record() OVERRIDE;

  // Ensure that the closure is run on the audio-manager thread.
  virtual void Close(const base::Closure& closed_task) OVERRIDE;

 protected:
  virtual ~TestAudioInputController();

 private:
  AudioParameters audio_parameters_;

  // These are not owned by us and expected to be valid for this object's
  // lifetime.
  TestAudioInputControllerFactory* factory_;
  EventHandler* event_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputController);
};

typedef TestAudioInputController::Delegate TestAudioInputControllerDelegate;

// Simple AudioInputController::Factory method that creates
// TestAudioInputControllers.
class TestAudioInputControllerFactory : public AudioInputController::Factory {
 public:
  TestAudioInputControllerFactory();
  virtual ~TestAudioInputControllerFactory();

  // AudioInputController::Factory methods.
  virtual AudioInputController* Create(
      AudioManager* audio_manager,
      AudioInputController::EventHandler* event_handler,
      AudioParameters params,
      UserInputMonitor* user_input_monitor) OVERRIDE;

  void SetDelegateForTests(TestAudioInputControllerDelegate* delegate);

  TestAudioInputController* controller() const { return controller_; }

 private:
  friend class TestAudioInputController;

  // Invoked by a TestAudioInputController when it gets destroyed.
  void OnTestAudioInputControllerDestroyed(
      TestAudioInputController* controller);

  // The caller of Create owns this object.
  TestAudioInputController* controller_;

  // The delegate for tests for receiving audio controller events.
  TestAudioInputControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputControllerFactory);
};

}  // namespace media

#endif  // MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_
