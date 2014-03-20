// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::SaveArg;
using testing::Return;

namespace content {

class MockAudioInputDeviceManagerListener
    : public MediaStreamProviderListener {
 public:
  MockAudioInputDeviceManagerListener() {}
  virtual ~MockAudioInputDeviceManagerListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, const int));
  MOCK_METHOD2(Closed, void(MediaStreamType, const int));
  MOCK_METHOD2(DevicesEnumerated, void(MediaStreamType,
                                       const StreamDeviceInfoArray&));
  MOCK_METHOD3(Error, void(MediaStreamType, int, MediaStreamProviderError));

  StreamDeviceInfoArray devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputDeviceManagerListener);
};

class AudioInputDeviceManagerTest : public testing::Test {
 public:
  AudioInputDeviceManagerTest() {}

  // Returns true iff machine has an audio input device.
  bool CanRunAudioInputDeviceTests() {
    return audio_manager_->HasAudioInputDevices();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // The test must run on Browser::IO.
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    audio_manager_.reset(media::AudioManager::CreateForTesting());
    manager_ = new AudioInputDeviceManager(audio_manager_.get());
    audio_input_listener_.reset(new MockAudioInputDeviceManagerListener());
    manager_->Register(audio_input_listener_.get(),
                       message_loop_->message_loop_proxy().get());

    // Gets the enumerated device list from the AudioInputDeviceManager.
    manager_->EnumerateDevices(MEDIA_DEVICE_AUDIO_CAPTURE);
    EXPECT_CALL(*audio_input_listener_,
                DevicesEnumerated(MEDIA_DEVICE_AUDIO_CAPTURE, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&devices_));

    // Wait until we get the list.
    message_loop_->RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    manager_->Unregister();
    io_thread_.reset();
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_refptr<AudioInputDeviceManager> manager_;
  scoped_ptr<MockAudioInputDeviceManagerListener> audio_input_listener_;
  scoped_ptr<media::AudioManager> audio_manager_;
  StreamDeviceInfoArray devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManagerTest);
};

// Opens and closes the devices.
TEST_F(AudioInputDeviceManagerTest, OpenAndCloseDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;

  ASSERT_FALSE(devices_.empty());

  InSequence s;

  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    // Opens/closes the devices.
    int session_id = manager_->Open(*iter);

    // Expected mock call with expected return value.
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);
    // Waits for the callback.
    message_loop_->RunUntilIdle();

    manager_->Close(session_id);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunUntilIdle();
  }
}

// Opens multiple devices at one time and closes them later.
TEST_F(AudioInputDeviceManagerTest, OpenMultipleDevices) {
  if (!CanRunAudioInputDeviceTests())
    return;

  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  scoped_ptr<int[]> session_id(new int[devices_.size()]);

  // Opens the devices in a loop.
  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Opens the devices.
    session_id[index] = manager_->Open(*iter);

    // Expected mock call with expected returned value.
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunUntilIdle();
  }

  // Checks if the session_ids are unique.
  for (size_t i = 0; i < devices_.size() - 1; ++i) {
    for (size_t k = i + 1; k < devices_.size(); ++k) {
      EXPECT_TRUE(session_id[i] != session_id[k]);
    }
  }

  for (size_t i = 0; i < devices_.size(); ++i) {
    // Closes the devices.
    manager_->Close(session_id[i]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[i]))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunUntilIdle();
  }
}

// Opens a non-existing device.
TEST_F(AudioInputDeviceManagerTest, OpenNotExistingDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  MediaStreamType stream_type = MEDIA_DEVICE_AUDIO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  int sample_rate(0);
  int channel_config(0);
  StreamDeviceInfo dummy_device(
      stream_type, device_name, device_id, sample_rate, channel_config, 2048);

  int session_id = manager_->Open(dummy_device);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);

  // Waits for the callback.
  message_loop_->RunUntilIdle();
}

// Opens default device twice.
TEST_F(AudioInputDeviceManagerTest, OpenDeviceTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;

  ASSERT_FALSE(devices_.empty());

  InSequence s;

  // Opens and closes the default device twice.
  int first_session_id = manager_->Open(devices_.front());
  int second_session_id = manager_->Open(devices_.front());

  // Expected mock calls with expected returned values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, second_session_id))
      .Times(1);
  // Waits for the callback.
  message_loop_->RunUntilIdle();

  manager_->Close(first_session_id);
  manager_->Close(second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, second_session_id))
      .Times(1);
  // Waits for the callback.
  message_loop_->RunUntilIdle();
}

// Accesses then closes the sessions after opening the devices.
TEST_F(AudioInputDeviceManagerTest, AccessAndCloseSession) {
  if (!CanRunAudioInputDeviceTests())
    return;

  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  scoped_ptr<int[]> session_id(new int[devices_.size()]);

  // Loops through the devices and calls Open()/Close()/GetOpenedDeviceInfoById
  // for each device.
  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Note that no DeviceStopped() notification for Event Handler as we have
    // stopped the device before calling close.
    session_id[index] = manager_->Open(*iter);
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);
    message_loop_->RunUntilIdle();

    const StreamDeviceInfo* info = manager_->GetOpenedDeviceInfoById(
        session_id[index]);
    DCHECK(info);
    EXPECT_EQ(iter->device.id, info->device.id);
    manager_->Close(session_id[index]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);
    message_loop_->RunUntilIdle();
  }
}

// Access an invalid session.
TEST_F(AudioInputDeviceManagerTest, AccessInvalidSession) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Opens the first device.
  StreamDeviceInfoArray::const_iterator iter = devices_.begin();
  int session_id = manager_->Open(*iter);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  message_loop_->RunUntilIdle();

  // Access a non-opened device.
  // This should fail and return an empty StreamDeviceInfo.
  int invalid_session_id = session_id + 1;
  const StreamDeviceInfo* info =
      manager_->GetOpenedDeviceInfoById(invalid_session_id);
  DCHECK(!info);

  manager_->Close(session_id);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  message_loop_->RunUntilIdle();
}

}  // namespace content
