// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "content/browser/browser_thread_impl.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_input_controller_factory.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoopProxy;
using media::AudioInputController;
using media::AudioInputStream;
using media::AudioManager;
using media::AudioOutputStream;
using media::AudioParameters;
using media::TestAudioInputController;
using media::TestAudioInputControllerFactory;

namespace content {

class SpeechRecognizerImplTest : public SpeechRecognitionEventListener,
                                 public testing::Test {
 public:
  SpeechRecognizerImplTest()
      : io_thread_(BrowserThread::IO, &message_loop_),
        recognition_started_(false),
        recognition_ended_(false),
        result_received_(false),
        audio_started_(false),
        audio_ended_(false),
        sound_started_(false),
        sound_ended_(false),
        error_(SPEECH_RECOGNITION_ERROR_NONE),
        volume_(-1.0f) {
    // SpeechRecognizer takes ownership of sr_engine.
    SpeechRecognitionEngine* sr_engine =
        new GoogleOneShotRemoteEngine(NULL /* URLRequestContextGetter */);
    SpeechRecognitionEngineConfig config;
    config.audio_num_bits_per_sample =
        SpeechRecognizerImpl::kNumBitsPerAudioSample;
    config.audio_sample_rate = SpeechRecognizerImpl::kAudioSampleRate;
    config.filter_profanities = false;
    sr_engine->SetConfig(config);

    const int kTestingSessionId = 1;
    const bool kOneShotMode = true;
    recognizer_ = new SpeechRecognizerImpl(
        this, kTestingSessionId, kOneShotMode, sr_engine);
    audio_manager_.reset(new media::MockAudioManager(
        base::MessageLoop::current()->message_loop_proxy().get()));
    recognizer_->SetAudioManagerForTesting(audio_manager_.get());

    int audio_packet_length_bytes =
        (SpeechRecognizerImpl::kAudioSampleRate *
         GoogleOneShotRemoteEngine::kAudioPacketIntervalMs *
         ChannelLayoutToChannelCount(SpeechRecognizerImpl::kChannelLayout) *
         SpeechRecognizerImpl::kNumBitsPerAudioSample) / (8 * 1000);
    audio_packet_.resize(audio_packet_length_bytes);
  }

  void CheckEventsConsistency() {
    // Note: "!x || y" == "x implies y".
    EXPECT_TRUE(!recognition_ended_ || recognition_started_);
    EXPECT_TRUE(!audio_ended_ || audio_started_);
    EXPECT_TRUE(!sound_ended_ || sound_started_);
    EXPECT_TRUE(!audio_started_ || recognition_started_);
    EXPECT_TRUE(!sound_started_ || audio_started_);
    EXPECT_TRUE(!audio_ended_ || (sound_ended_ || !sound_started_));
    EXPECT_TRUE(!recognition_ended_ || (audio_ended_ || !audio_started_));
  }

  void CheckFinalEventsConsistency() {
    // Note: "!(x ^ y)" == "(x && y) || (!x && !x)".
    EXPECT_FALSE(recognition_started_ ^ recognition_ended_);
    EXPECT_FALSE(audio_started_ ^ audio_ended_);
    EXPECT_FALSE(sound_started_ ^ sound_ended_);
  }

  // Overridden from SpeechRecognitionEventListener:
  virtual void OnAudioStart(int session_id) OVERRIDE {
    audio_started_ = true;
    CheckEventsConsistency();
  }

  virtual void OnAudioEnd(int session_id) OVERRIDE {
    audio_ended_ = true;
    CheckEventsConsistency();
  }

  virtual void OnRecognitionResults(
      int session_id, const SpeechRecognitionResults& results) OVERRIDE {
    result_received_ = true;
  }

  virtual void OnRecognitionError(
      int session_id, const SpeechRecognitionError& error) OVERRIDE {
    EXPECT_TRUE(recognition_started_);
    EXPECT_FALSE(recognition_ended_);
    error_ = error.code;
  }

  virtual void OnAudioLevelsChange(int session_id, float volume,
                                   float noise_volume) OVERRIDE {
    volume_ = volume;
    noise_volume_ = noise_volume;
  }

  virtual void OnRecognitionEnd(int session_id) OVERRIDE {
    recognition_ended_ = true;
    CheckEventsConsistency();
  }

  virtual void OnRecognitionStart(int session_id) OVERRIDE {
    recognition_started_ = true;
    CheckEventsConsistency();
  }

  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE {}

  virtual void OnSoundStart(int session_id) OVERRIDE {
    sound_started_ = true;
    CheckEventsConsistency();
  }

  virtual void OnSoundEnd(int session_id) OVERRIDE {
    sound_ended_ = true;
    CheckEventsConsistency();
  }

  // testing::Test methods.
  virtual void SetUp() OVERRIDE {
    AudioInputController::set_factory_for_testing(
        &audio_input_controller_factory_);
  }

  virtual void TearDown() OVERRIDE {
    AudioInputController::set_factory_for_testing(NULL);
  }

  void FillPacketWithTestWaveform() {
    // Fill the input with a simple pattern, a 125Hz sawtooth waveform.
    for (size_t i = 0; i < audio_packet_.size(); ++i)
      audio_packet_[i] = static_cast<uint8>(i);
  }

  void FillPacketWithNoise() {
    int value = 0;
    int factor = 175;
    for (size_t i = 0; i < audio_packet_.size(); ++i) {
      value += factor;
      audio_packet_[i] = value % 100;
    }
  }

 protected:
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
  scoped_refptr<SpeechRecognizerImpl> recognizer_;
  scoped_ptr<AudioManager> audio_manager_;
  bool recognition_started_;
  bool recognition_ended_;
  bool result_received_;
  bool audio_started_;
  bool audio_ended_;
  bool sound_started_;
  bool sound_ended_;
  SpeechRecognitionErrorCode error_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  TestAudioInputControllerFactory audio_input_controller_factory_;
  std::vector<uint8> audio_packet_;
  float volume_;
  float noise_volume_;
};

TEST_F(SpeechRecognizerImplTest, StopNoData) {
  // Check for callbacks when stopping record before any audio gets recorded.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  recognizer_->StopAudioCapture();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, CancelNoData) {
  // Check for callbacks when canceling recognition before any audio gets
  // recorded.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  recognizer_->AbortRecognition();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_ABORTED, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, StopWithData) {
  // Start recording, give some data and then stop. This should wait for the
  // network callback to arrive before completion.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);

  // Try sending 5 chunks of mock audio data and verify that each of them
  // resulted immediately in a packet sent out via the network. This verifies
  // that we are streaming out encoded data as chunks without waiting for the
  // full recording to complete.
  const size_t kNumChunks = 5;
  for (size_t i = 0; i < kNumChunks; ++i) {
    controller->event_handler()->OnData(controller, &audio_packet_[0],
                                        audio_packet_.size());
    base::MessageLoop::current()->RunUntilIdle();
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(i + 1, fetcher->upload_chunks().size());
  }

  recognizer_->StopAudioCapture();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);

  // Issue the network callback to complete the process.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->set_url(fetcher->GetOriginalURL());
  net::URLRequestStatus status;
  status.set_status(net::URLRequestStatus::SUCCESS);
  fetcher->set_status(status);
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      "{\"status\":0,\"hypotheses\":[{\"utterance\":\"123\"}]}");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_ended_);
  EXPECT_TRUE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, CancelWithData) {
  // Start recording, give some data and then cancel.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  base::MessageLoop::current()->RunUntilIdle();
  recognizer_->AbortRecognition();
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_TRUE(url_fetcher_factory_.GetFetcherByID(0));
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_ABORTED, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, ConnectionError) {
  // Start recording, give some data and then stop. Issue the network callback
  // with a connection error and verify that the recognizer bubbles the error up
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  base::MessageLoop::current()->RunUntilIdle();
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  recognizer_->StopAudioCapture();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);

  // Issue the network callback to complete the process.
  fetcher->set_url(fetcher->GetOriginalURL());
  net::URLRequestStatus status;
  status.set_status(net::URLRequestStatus::FAILED);
  status.set_error(net::ERR_CONNECTION_REFUSED);
  fetcher->set_status(status);
  fetcher->set_response_code(0);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NETWORK, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, ServerError) {
  // Start recording, give some data and then stop. Issue the network callback
  // with a 500 error and verify that the recognizer bubbles the error up
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  base::MessageLoop::current()->RunUntilIdle();
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  recognizer_->StopAudioCapture();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);

  // Issue the network callback to complete the process.
  fetcher->set_url(fetcher->GetOriginalURL());
  net::URLRequestStatus status;
  status.set_status(net::URLRequestStatus::SUCCESS);
  fetcher->set_status(status);
  fetcher->set_response_code(500);
  fetcher->SetResponseString("Internal Server Error");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NETWORK, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, AudioControllerErrorNoData) {
  // Check if things tear down properly if AudioInputController threw an error.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnError(controller);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_AUDIO, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, AudioControllerErrorWithData) {
  // Check if things tear down properly if AudioInputController threw an error
  // after giving some audio data.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  controller->event_handler()->OnError(controller);
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_TRUE(url_fetcher_factory_.GetFetcherByID(0));
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_AUDIO, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, NoSpeechCallbackIssued) {
  // Start recording and give a lot of packets with audio samples set to zero.
  // This should trigger the no-speech detector and issue a callback.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);

  int num_packets = (SpeechRecognizerImpl::kNoSpeechTimeoutMs) /
                     GoogleOneShotRemoteEngine::kAudioPacketIntervalMs + 1;
  // The vector is already filled with zero value samples on create.
  for (int i = 0; i < num_packets; ++i) {
    controller->event_handler()->OnData(controller, &audio_packet_[0],
                                        audio_packet_.size());
  }
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NO_SPEECH, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, NoSpeechCallbackNotIssued) {
  // Start recording and give a lot of packets with audio samples set to zero
  // and then some more with reasonably loud audio samples. This should be
  // treated as normal speech input and the no-speech detector should not get
  // triggered.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller = audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);

  int num_packets = (SpeechRecognizerImpl::kNoSpeechTimeoutMs) /
                     GoogleOneShotRemoteEngine::kAudioPacketIntervalMs;

  // The vector is already filled with zero value samples on create.
  for (int i = 0; i < num_packets / 2; ++i) {
    controller->event_handler()->OnData(controller, &audio_packet_[0],
                                        audio_packet_.size());
  }

  FillPacketWithTestWaveform();
  for (int i = 0; i < num_packets / 2; ++i) {
    controller->event_handler()->OnData(controller, &audio_packet_[0],
                                        audio_packet_.size());
  }

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  recognizer_->AbortRecognition();
  base::MessageLoop::current()->RunUntilIdle();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, SetInputVolumeCallback) {
  // Start recording and give a lot of packets with audio samples set to zero
  // and then some more with reasonably loud audio samples. Check that we don't
  // get the callback during estimation phase, then get zero for the silence
  // samples and proper volume for the loud audio.
  recognizer_->StartRecognition(media::AudioManagerBase::kDefaultDeviceId);
  base::MessageLoop::current()->RunUntilIdle();
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller = audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);

  // Feed some samples to begin with for the endpointer to do noise estimation.
  int num_packets = SpeechRecognizerImpl::kEndpointerEstimationTimeMs /
                    GoogleOneShotRemoteEngine::kAudioPacketIntervalMs;
  FillPacketWithNoise();
  for (int i = 0; i < num_packets; ++i) {
    controller->event_handler()->OnData(controller, &audio_packet_[0],
                                        audio_packet_.size());
  }
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(-1.0f, volume_);  // No audio volume set yet.

  // The vector is already filled with zero value samples on create.
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FLOAT_EQ(0.74939233f, volume_);

  FillPacketWithTestWaveform();
  controller->event_handler()->OnData(controller, &audio_packet_[0],
                                      audio_packet_.size());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_NEAR(0.89926866f, volume_, 0.00001f);
  EXPECT_FLOAT_EQ(0.75071919f, noise_volume_);

  EXPECT_EQ(SPEECH_RECOGNITION_ERROR_NONE, error_);
  EXPECT_FALSE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  recognizer_->AbortRecognition();
  base::MessageLoop::current()->RunUntilIdle();
  CheckFinalEventsConsistency();
}

}  // namespace content
