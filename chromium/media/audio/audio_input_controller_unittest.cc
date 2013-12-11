// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 10;

// Posts base::MessageLoop::QuitClosure() on specified message loop.
ACTION_P(QuitMessageLoop, loop_or_proxy) {
  loop_or_proxy->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

// Posts base::MessageLoop::QuitClosure() on specified message loop after a
// certain number of calls given by |limit|.
ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop_or_proxy) {
  if (++*count >= limit) {
    loop_or_proxy->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }
}

// Closes AudioOutputController synchronously.
static void CloseAudioController(AudioInputController* controller) {
  controller->Close(base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();
}

class MockAudioInputControllerEventHandler
    : public AudioInputController::EventHandler {
 public:
  MockAudioInputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioInputController* controller));
  MOCK_METHOD1(OnRecording, void(AudioInputController* controller));
  MOCK_METHOD1(OnError, void(AudioInputController* controller));
  MOCK_METHOD3(OnData, void(AudioInputController* controller,
                            const uint8* data, uint32 size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputControllerEventHandler);
};

// Test fixture.
class AudioInputControllerTest : public testing::Test {
 public:
  AudioInputControllerTest() {}
  virtual ~AudioInputControllerTest() {}

 protected:
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputControllerTest);
};

// Test AudioInputController for create and close without recording audio.
TEST_F(AudioInputControllerTest, CreateAndClose) {
  MockAudioInputControllerEventHandler event_handler;

  // OnCreated() will be posted once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(QuitMessageLoop(&message_loop_));

  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(audio_manager.get(),
                                   &event_handler,
                                   params,
                                   AudioManagerBase::kDefaultDeviceId,
                                   NULL);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to fire.
  message_loop_.Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test a normal call sequence of create, record and close.
TEST_F(AudioInputControllerTest, RecordAndClose) {
  MockAudioInputControllerEventHandler event_handler;
  int count = 0;

  // OnCreated() will be called once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  // OnData() shall be called ten times.
  EXPECT_CALL(event_handler, OnData(NotNull(), NotNull(), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10,
          message_loop_.message_loop_proxy()));

  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  // Creating the AudioInputController should render an OnCreated() call.
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(audio_manager.get(),
                                   &event_handler,
                                   params,
                                   AudioManagerBase::kDefaultDeviceId,
                                   NULL);
  ASSERT_TRUE(controller.get());

  // Start recording and trigger one OnRecording() call.
  controller->Record();

  // Record and wait until ten OnData() callbacks are received.
  message_loop_.Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test that the AudioInputController reports an error when the input stream
// stops without an OnClose() callback. This can happen when the underlying
// audio layer stops feeding data as a result of a removed microphone device.
TEST_F(AudioInputControllerTest, RecordAndError) {
  MockAudioInputControllerEventHandler event_handler;
  int count = 0;

  // OnCreated() will be called once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  // OnData() shall be called ten times.
  EXPECT_CALL(event_handler, OnData(NotNull(), NotNull(), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10,
          message_loop_.message_loop_proxy()));

  // OnError() will be called after the data stream stops while the
  // controller is in a recording state.
  EXPECT_CALL(event_handler, OnError(NotNull()))
      .Times(Exactly(1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  // Creating the AudioInputController should render an OnCreated() call.
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(audio_manager.get(),
                                   &event_handler,
                                   params,
                                   AudioManagerBase::kDefaultDeviceId,
                                   NULL);
  ASSERT_TRUE(controller.get());

  // Start recording and trigger one OnRecording() call.
  controller->Record();

  // Record and wait until ten OnData() callbacks are received.
  message_loop_.Run();

  // Stop the stream and verify that OnError() is posted.
  AudioInputStream* stream = controller->stream_for_testing();
  stream->Stop();
  message_loop_.Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test that AudioInputController rejects insanely large packet sizes.
TEST_F(AudioInputControllerTest, SamplesPerPacketTooLarge) {
  // Create an audio device with a very large packet size.
  MockAudioInputControllerEventHandler event_handler;

  // OnCreated() shall not be called in this test.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
    .Times(Exactly(0));

  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket * 1000);
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(audio_manager.get(),
                                   &event_handler,
                                   params,
                                   AudioManagerBase::kDefaultDeviceId,
                                   NULL);
  ASSERT_FALSE(controller.get());
}

// Test calling AudioInputController::Close multiple times.
TEST_F(AudioInputControllerTest, CloseTwice) {
  MockAudioInputControllerEventHandler event_handler;

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(audio_manager.get(),
                                   &event_handler,
                                   params,
                                   AudioManagerBase::kDefaultDeviceId,
                                   NULL);
  ASSERT_TRUE(controller.get());

  controller->Record();

  controller->Close(base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();

  controller->Close(base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();
}

}  // namespace media
