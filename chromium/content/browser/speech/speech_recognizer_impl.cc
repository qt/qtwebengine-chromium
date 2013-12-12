// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "media/base/audio_converter.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "media/audio/win/core_audio_util_win.h"
#endif

using media::AudioBus;
using media::AudioConverter;
using media::AudioInputController;
using media::AudioManager;
using media::AudioParameters;
using media::ChannelLayout;

namespace content {

// Private class which encapsulates the audio converter and the
// AudioConverter::InputCallback. It handles resampling, buffering and
// channel mixing between input and output parameters.
class SpeechRecognizerImpl::OnDataConverter
    : public media::AudioConverter::InputCallback {
 public:
  OnDataConverter(const AudioParameters& input_params,
                  const AudioParameters& output_params);
  virtual ~OnDataConverter();

  // Converts input |data| buffer into an AudioChunk where the input format
  // is given by |input_parameters_| and the output format by
  // |output_parameters_|.
  scoped_refptr<AudioChunk> Convert(const uint8* data, size_t size);

 private:
  // media::AudioConverter::InputCallback implementation.
  virtual double ProvideInput(AudioBus* dest,
                              base::TimeDelta buffer_delay) OVERRIDE;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  scoped_ptr<AudioBus> input_bus_;
  scoped_ptr<AudioBus> output_bus_;
  const AudioParameters input_parameters_;
  const AudioParameters output_parameters_;
  bool waiting_for_input_;
  scoped_ptr<uint8[]> converted_data_;

  DISALLOW_COPY_AND_ASSIGN(OnDataConverter);
};

namespace {

// The following constants are related to the volume level indicator shown in
// the UI for recorded audio.
// Multiplier used when new volume is greater than previous level.
const float kUpSmoothingFactor = 1.0f;
// Multiplier used when new volume is lesser than previous level.
const float kDownSmoothingFactor = 0.7f;
// RMS dB value of a maximum (unclipped) sine wave for int16 samples.
const float kAudioMeterMaxDb = 90.31f;
// This value corresponds to RMS dB for int16 with 6 most-significant-bits = 0.
// Values lower than this will display as empty level-meter.
const float kAudioMeterMinDb = 30.0f;
const float kAudioMeterDbRange = kAudioMeterMaxDb - kAudioMeterMinDb;

// Maximum level to draw to display unclipped meter. (1.0f displays clipping.)
const float kAudioMeterRangeMaxUnclipped = 47.0f / 48.0f;

// Returns true if more than 5% of the samples are at min or max value.
bool DetectClipping(const AudioChunk& chunk) {
  const int num_samples = chunk.NumSamples();
  const int16* samples = chunk.SamplesData16();
  const int kThreshold = num_samples / 20;
  int clipping_samples = 0;

  for (int i = 0; i < num_samples; ++i) {
    if (samples[i] <= -32767 || samples[i] >= 32767) {
      if (++clipping_samples > kThreshold)
        return true;
    }
  }
  return false;
}

void KeepAudioControllerRefcountedForDtor(scoped_refptr<AudioInputController>) {
}

}  // namespace

const int SpeechRecognizerImpl::kAudioSampleRate = 16000;
const ChannelLayout SpeechRecognizerImpl::kChannelLayout =
    media::CHANNEL_LAYOUT_MONO;
const int SpeechRecognizerImpl::kNumBitsPerAudioSample = 16;
const int SpeechRecognizerImpl::kNoSpeechTimeoutMs = 8000;
const int SpeechRecognizerImpl::kEndpointerEstimationTimeMs = 300;
media::AudioManager* SpeechRecognizerImpl::audio_manager_for_tests_ = NULL;

COMPILE_ASSERT(SpeechRecognizerImpl::kNumBitsPerAudioSample % 8 == 0,
               kNumBitsPerAudioSample_must_be_a_multiple_of_8);

// SpeechRecognizerImpl::OnDataConverter implementation

SpeechRecognizerImpl::OnDataConverter::OnDataConverter(
    const AudioParameters& input_params, const AudioParameters& output_params)
    : audio_converter_(input_params, output_params, false),
      input_bus_(AudioBus::Create(input_params)),
      output_bus_(AudioBus::Create(output_params)),
      input_parameters_(input_params),
      output_parameters_(output_params),
      waiting_for_input_(false),
      converted_data_(new uint8[output_parameters_.GetBytesPerBuffer()]) {
  audio_converter_.AddInput(this);
}

SpeechRecognizerImpl::OnDataConverter::~OnDataConverter() {
  // It should now be safe to unregister the converter since no more OnData()
  // callbacks are outstanding at this point.
  audio_converter_.RemoveInput(this);
}

scoped_refptr<AudioChunk> SpeechRecognizerImpl::OnDataConverter::Convert(
    const uint8* data, size_t size) {
  CHECK_EQ(size, static_cast<size_t>(input_parameters_.GetBytesPerBuffer()));

  input_bus_->FromInterleaved(
      data, input_bus_->frames(), input_parameters_.bits_per_sample() / 8);

  waiting_for_input_ = true;
  audio_converter_.Convert(output_bus_.get());

  output_bus_->ToInterleaved(
      output_bus_->frames(), output_parameters_.bits_per_sample() / 8,
      converted_data_.get());

  // TODO(primiano): Refactor AudioChunk to avoid the extra-copy here
  // (see http://crbug.com/249316 for details).
  return scoped_refptr<AudioChunk>(new AudioChunk(
      converted_data_.get(),
      output_parameters_.GetBytesPerBuffer(),
      output_parameters_.bits_per_sample() / 8));
}

double SpeechRecognizerImpl::OnDataConverter::ProvideInput(
    AudioBus* dest, base::TimeDelta buffer_delay) {
  // The audio converted should never ask for more than one bus in each call
  // to Convert(). If so, we have a serious issue in our design since we might
  // miss recorded chunks of 100 ms audio data.
  CHECK(waiting_for_input_);

  // Read from the input bus to feed the converter.
  input_bus_->CopyTo(dest);

  // |input_bus_| should only be provide once.
  waiting_for_input_ = false;
  return 1;
}

// SpeechRecognizerImpl implementation

SpeechRecognizerImpl::SpeechRecognizerImpl(
    SpeechRecognitionEventListener* listener,
    int session_id,
    bool is_single_shot,
    SpeechRecognitionEngine* engine)
    : SpeechRecognizer(listener, session_id),
      recognition_engine_(engine),
      endpointer_(kAudioSampleRate),
      is_dispatching_event_(false),
      is_single_shot_(is_single_shot),
      state_(STATE_IDLE) {
  DCHECK(recognition_engine_ != NULL);
  if (is_single_shot) {
    // In single shot recognition, the session is automatically ended after:
    //  - 0.5 seconds of silence if time <  3 seconds
    //  - 1   seconds of silence if time >= 3 seconds
    endpointer_.set_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond / 2);
    endpointer_.set_long_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond);
    endpointer_.set_long_speech_length(3 * base::Time::kMicrosecondsPerSecond);
  } else {
    // In continuous recognition, the session is automatically ended after 15
    // seconds of silence.
    const int64 cont_timeout_us = base::Time::kMicrosecondsPerSecond * 15;
    endpointer_.set_speech_input_complete_silence_length(cont_timeout_us);
    endpointer_.set_long_speech_length(0);  // Use only a single timeout.
  }
  endpointer_.StartSession();
  recognition_engine_->set_delegate(this);
}

// -------  Methods that trigger Finite State Machine (FSM) events ------------

// NOTE:all the external events and requests should be enqueued (PostTask), even
// if they come from the same (IO) thread, in order to preserve the relationship
// of causality between events and avoid interleaved event processing due to
// synchronous callbacks.

void SpeechRecognizerImpl::StartRecognition(const std::string& device_id) {
  DCHECK(!device_id.empty());
  device_id_ = device_id;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_START)));
}

void SpeechRecognizerImpl::AbortRecognition() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_ABORT)));
}

void SpeechRecognizerImpl::StopAudioCapture() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_STOP_CAPTURE)));
}

bool SpeechRecognizerImpl::IsActive() const {
  // Checking the FSM state from another thread (thus, while the FSM is
  // potentially concurrently evolving) is meaningless.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return state_ != STATE_IDLE && state_ != STATE_ENDED;
}

bool SpeechRecognizerImpl::IsCapturingAudio() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)); // See IsActive().
  const bool is_capturing_audio = state_ >= STATE_STARTING &&
                                  state_ <= STATE_RECOGNIZING;
  DCHECK((is_capturing_audio && (audio_controller_.get() != NULL)) ||
         (!is_capturing_audio && audio_controller_.get() == NULL));
  return is_capturing_audio;
}

const SpeechRecognitionEngine&
SpeechRecognizerImpl::recognition_engine() const {
  return *(recognition_engine_.get());
}

SpeechRecognizerImpl::~SpeechRecognizerImpl() {
  endpointer_.EndSession();
  if (audio_controller_.get()) {
    audio_controller_->Close(
        base::Bind(&KeepAudioControllerRefcountedForDtor, audio_controller_));
  }
}

// Invoked in the audio thread.
void SpeechRecognizerImpl::OnError(AudioInputController* controller) {
  FSMEventArgs event_args(EVENT_AUDIO_ERROR);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnData(AudioInputController* controller,
                                  const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when audio capture stops and is normal.
    return;

  // Convert audio from native format to fixed format used by WebSpeech.
  FSMEventArgs event_args(EVENT_AUDIO_DATA);
  event_args.audio_data = audio_converter_->Convert(data, size);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnAudioClosed(AudioInputController*) {}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineResults(
    const SpeechRecognitionResults& results) {
  FSMEventArgs event_args(EVENT_ENGINE_RESULT);
  event_args.engine_results = results;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineError(
    const SpeechRecognitionError& error) {
  FSMEventArgs event_args(EVENT_ENGINE_ERROR);
  event_args.engine_error = error;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

// -----------------------  Core FSM implementation ---------------------------
// TODO(primiano): After the changes in the media package (r129173), this class
// slightly violates the SpeechRecognitionEventListener interface contract. In
// particular, it is not true anymore that this class can be freed after the
// OnRecognitionEnd event, since the audio_controller_.Close() asynchronous
// call can be still in progress after the end event. Currently, it does not
// represent a problem for the browser itself, since refcounting protects us
// against such race conditions. However, we should fix this in the next CLs.
// For instance, tests are currently working just because the
// TestAudioInputController is not closing asynchronously as the real controller
// does, but they will become flaky if TestAudioInputController will be fixed.

void SpeechRecognizerImpl::DispatchEvent(const FSMEventArgs& event_args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_LE(event_args.event, EVENT_MAX_VALUE);
  DCHECK_LE(state_, STATE_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;

  // Guard against the delegate freeing us until we finish processing the event.
  scoped_refptr<SpeechRecognizerImpl> me(this);

  if (event_args.event == EVENT_AUDIO_DATA) {
    DCHECK(event_args.audio_data.get() != NULL);
    ProcessAudioPipeline(*event_args.audio_data.get());
  }

  // The audio pipeline must be processed before the event dispatch, otherwise
  // it would take actions according to the future state instead of the current.
  state_ = ExecuteTransitionAndGetNextState(event_args);
  is_dispatching_event_ = false;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ExecuteTransitionAndGetNextState(
    const FSMEventArgs& event_args) {
  const FSMEvent event = event_args.event;
  switch (state_) {
    case STATE_IDLE:
      switch (event) {
        // TODO(primiano): restore UNREACHABLE_CONDITION on EVENT_ABORT and
        // EVENT_STOP_CAPTURE below once speech input extensions are fixed.
        case EVENT_ABORT:
          return AbortSilently(event_args);
        case EVENT_START:
          return StartRecording(event_args);
        case EVENT_STOP_CAPTURE:
          return AbortSilently(event_args);
        case EVENT_AUDIO_DATA:     // Corner cases related to queued messages
        case EVENT_ENGINE_RESULT:  // being lately dispatched.
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return DoNothing(event_args);
      }
      break;
    case STATE_STARTING:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return AbortSilently(event_args);
        case EVENT_AUDIO_DATA:
          return StartRecognitionEngine(event_args);
        case EVENT_ENGINE_RESULT:
          return NotFeasible(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_ESTIMATING_ENVIRONMENT:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return WaitEnvironmentEstimationCompletion(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_WAITING_FOR_SPEECH:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectUserSpeechOrTimeout(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_RECOGNIZING:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectEndOfSpeech(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_WAITING_FINAL_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
        case EVENT_AUDIO_DATA:
          return DoNothing(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessFinalResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;

    // TODO(primiano): remove this state when speech input extensions support
    // will be removed and STATE_IDLE.EVENT_ABORT,EVENT_STOP_CAPTURE will be
    // reset to NotFeasible (see TODO above).
    case STATE_ENDED:
      return DoNothing(event_args);
  }
  return NotFeasible(event_args);
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);
//  - event_args members are guaranteed to be stable during the call;
//  - The class won't be freed in the meanwhile due to callbacks;
//  - IsCapturingAudio() returns true if and only if audio_controller_ != NULL.

// TODO(primiano): the audio pipeline is currently serial. However, the
// clipper->endpointer->vumeter chain and the sr_engine could be parallelized.
// We should profile the execution to see if it would be worth or not.
void SpeechRecognizerImpl::ProcessAudioPipeline(const AudioChunk& raw_audio) {
  const bool route_to_endpointer = state_ >= STATE_ESTIMATING_ENVIRONMENT &&
                                   state_ <= STATE_RECOGNIZING;
  const bool route_to_sr_engine = route_to_endpointer;
  const bool route_to_vumeter = state_ >= STATE_WAITING_FOR_SPEECH &&
                                state_ <= STATE_RECOGNIZING;
  const bool clip_detected = DetectClipping(raw_audio);
  float rms = 0.0f;

  num_samples_recorded_ += raw_audio.NumSamples();

  if (route_to_endpointer)
    endpointer_.ProcessAudio(raw_audio, &rms);

  if (route_to_vumeter) {
    DCHECK(route_to_endpointer); // Depends on endpointer due to |rms|.
    UpdateSignalAndNoiseLevels(rms, clip_detected);
  }
  if (route_to_sr_engine) {
    DCHECK(recognition_engine_.get() != NULL);
    recognition_engine_->TakeAudioChunk(raw_audio);
  }
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecording(const FSMEventArgs&) {
  DCHECK(recognition_engine_.get() != NULL);
  DCHECK(!IsCapturingAudio());
  const bool unit_test_is_active = (audio_manager_for_tests_ != NULL);
  AudioManager* audio_manager = unit_test_is_active ?
                                audio_manager_for_tests_ :
                                AudioManager::Get();
  DCHECK(audio_manager != NULL);

  DVLOG(1) << "SpeechRecognizerImpl starting audio capture.";
  num_samples_recorded_ = 0;
  audio_level_ = 0;
  listener()->OnRecognitionStart(session_id());

  // TODO(xians): Check if the OS has the device with |device_id_|, return
  // |SPEECH_AUDIO_ERROR_DETAILS_NO_MIC| if the target device does not exist.
  if (!audio_manager->HasAudioInputDevices()) {
    return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_AUDIO,
                                        SPEECH_AUDIO_ERROR_DETAILS_NO_MIC));
  }

  int chunk_duration_ms = recognition_engine_->GetDesiredAudioChunkDurationMs();

  AudioParameters in_params = audio_manager->GetInputStreamParameters(
      device_id_);
  if (!in_params.IsValid() && !unit_test_is_active) {
    DLOG(ERROR) << "Invalid native audio input parameters";
    return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_AUDIO));
  }

  // Audio converter shall provide audio based on these parameters as output.
  // Hard coded, WebSpeech specific parameters are utilized here.
  int frames_per_buffer = (kAudioSampleRate * chunk_duration_ms) / 1000;
  AudioParameters output_parameters = AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, kChannelLayout, kAudioSampleRate,
      kNumBitsPerAudioSample, frames_per_buffer);

  // Audio converter will receive audio based on these parameters as input.
  // On Windows we start by verifying that Core Audio is supported. If not,
  // the WaveIn API is used and we might as well avoid all audio conversations
  // since WaveIn does the conversion for us.
  // TODO(henrika): this code should be moved to platform dependent audio
  // managers.
  bool use_native_audio_params = true;
#if defined(OS_WIN)
  use_native_audio_params = media::CoreAudioUtil::IsSupported();
  DVLOG_IF(1, !use_native_audio_params) << "Reverting to WaveIn for WebSpeech";
#endif

  AudioParameters input_parameters = output_parameters;
  if (use_native_audio_params && !unit_test_is_active) {
    // Use native audio parameters but avoid opening up at the native buffer
    // size. Instead use same frame size (in milliseconds) as WebSpeech uses.
    // We rely on internal buffers in the audio back-end to fulfill this request
    // and the idea is to simplify the audio conversion since each Convert()
    // call will then render exactly one ProvideInput() call.
    // Due to implementation details in the audio converter, 2 milliseconds
    // are added to the default frame size (100 ms) to ensure there is enough
    // data to generate 100 ms of output when resampling.
    frames_per_buffer =
        ((in_params.sample_rate() * (chunk_duration_ms + 2)) / 1000.0) + 0.5;
    input_parameters.Reset(in_params.format(),
                           in_params.channel_layout(),
                           in_params.channels(),
                           in_params.input_channels(),
                           in_params.sample_rate(),
                           in_params.bits_per_sample(),
                           frames_per_buffer);
  }

  // Create an audio converter which converts data between native input format
  // and WebSpeech specific output format.
  audio_converter_.reset(
      new OnDataConverter(input_parameters, output_parameters));

  audio_controller_ = AudioInputController::Create(
      audio_manager, this, input_parameters, device_id_, NULL);

  if (!audio_controller_.get()) {
    return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_AUDIO));
  }

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. We wait in the state
  // ESTIMATING_ENVIRONMENT until such interval has elapsed before switching
  // to user input mode.
  endpointer_.SetEnvironmentEstimationMode();
  audio_controller_->Record();
  return STATE_STARTING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecognitionEngine(const FSMEventArgs& event_args) {
  // This is the first audio packet captured, so the recognition engine is
  // started and the delegate notified about the event.
  DCHECK(recognition_engine_.get() != NULL);
  recognition_engine_->StartRecognition();
  listener()->OnAudioStart(session_id());

  // This is a little hack, since TakeAudioChunk() is already called by
  // ProcessAudioPipeline(). It is the best tradeoff, unless we allow dropping
  // the first audio chunk captured after opening the audio device.
  recognition_engine_->TakeAudioChunk(*(event_args.audio_data.get()));
  return STATE_ESTIMATING_ENVIRONMENT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::WaitEnvironmentEstimationCompletion(const FSMEventArgs&) {
  DCHECK(endpointer_.IsEstimatingEnvironment());
  if (GetElapsedTimeMs() >= kEndpointerEstimationTimeMs) {
    endpointer_.SetUserInputMode();
    listener()->OnEnvironmentEstimationComplete(session_id());
    return STATE_WAITING_FOR_SPEECH;
  } else {
    return STATE_ESTIMATING_ENVIRONMENT;
  }
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectUserSpeechOrTimeout(const FSMEventArgs&) {
  if (endpointer_.DidStartReceivingSpeech()) {
    listener()->OnSoundStart(session_id());
    return STATE_RECOGNIZING;
  } else if (GetElapsedTimeMs() >= kNoSpeechTimeoutMs) {
    return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_NO_SPEECH));
  }
  return STATE_WAITING_FOR_SPEECH;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectEndOfSpeech(const FSMEventArgs& event_args) {
  if (endpointer_.speech_input_complete())
    return StopCaptureAndWaitForResult(event_args);
  return STATE_RECOGNIZING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StopCaptureAndWaitForResult(const FSMEventArgs&) {
  DCHECK(state_ >= STATE_ESTIMATING_ENVIRONMENT && state_ <= STATE_RECOGNIZING);

  DVLOG(1) << "Concluding recognition";
  CloseAudioControllerAsynchronously();
  recognition_engine_->AudioChunksEnded();

  if (state_ > STATE_WAITING_FOR_SPEECH)
    listener()->OnSoundEnd(session_id());

  listener()->OnAudioEnd(session_id());
  return STATE_WAITING_FINAL_RESULT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::AbortSilently(const FSMEventArgs& event_args) {
  DCHECK_NE(event_args.event, EVENT_AUDIO_ERROR);
  DCHECK_NE(event_args.event, EVENT_ENGINE_ERROR);
  return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_NONE));
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::AbortWithError(const FSMEventArgs& event_args) {
  if (event_args.event == EVENT_AUDIO_ERROR) {
    return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_AUDIO));
  } else if (event_args.event == EVENT_ENGINE_ERROR) {
    return Abort(event_args.engine_error);
  }
  return Abort(SpeechRecognitionError(SPEECH_RECOGNITION_ERROR_ABORTED));
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::Abort(
    const SpeechRecognitionError& error) {
  if (IsCapturingAudio())
    CloseAudioControllerAsynchronously();

  DVLOG(1) << "SpeechRecognizerImpl canceling recognition. ";

  // The recognition engine is initialized only after STATE_STARTING.
  if (state_ > STATE_STARTING) {
    DCHECK(recognition_engine_.get() != NULL);
    recognition_engine_->EndRecognition();
  }

  if (state_ > STATE_WAITING_FOR_SPEECH && state_ < STATE_WAITING_FINAL_RESULT)
    listener()->OnSoundEnd(session_id());

  if (state_ > STATE_STARTING && state_ < STATE_WAITING_FINAL_RESULT)
    listener()->OnAudioEnd(session_id());

  if (error.code != SPEECH_RECOGNITION_ERROR_NONE)
    listener()->OnRecognitionError(session_id(), error);

  listener()->OnRecognitionEnd(session_id());

  return STATE_ENDED;
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::ProcessIntermediateResult(
    const FSMEventArgs& event_args) {
  // Provisional results can occur only during continuous (non one-shot) mode.
  // If this check is reached it means that a continuous speech recognition
  // engine is being used for a one shot recognition.
  DCHECK_EQ(false, is_single_shot_);

  // In continuous recognition, intermediate results can occur even when we are
  // in the ESTIMATING_ENVIRONMENT or WAITING_FOR_SPEECH states (if the
  // recognition engine is "faster" than our endpointer). In these cases we
  // skip the endpointer and fast-forward to the RECOGNIZING state, with respect
  // of the events triggering order.
  if (state_ == STATE_ESTIMATING_ENVIRONMENT) {
    DCHECK(endpointer_.IsEstimatingEnvironment());
    endpointer_.SetUserInputMode();
    listener()->OnEnvironmentEstimationComplete(session_id());
  } else if (state_ == STATE_WAITING_FOR_SPEECH) {
    listener()->OnSoundStart(session_id());
  } else {
    DCHECK_EQ(STATE_RECOGNIZING, state_);
  }

  listener()->OnRecognitionResults(session_id(), event_args.engine_results);
  return STATE_RECOGNIZING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ProcessFinalResult(const FSMEventArgs& event_args) {
  const SpeechRecognitionResults& results = event_args.engine_results;
  SpeechRecognitionResults::const_iterator i = results.begin();
  bool provisional_results_pending = false;
  bool results_are_empty = true;
  for (; i != results.end(); ++i) {
    const SpeechRecognitionResult& result = *i;
    if (result.is_provisional) {
      provisional_results_pending = true;
      DCHECK(!is_single_shot_);
    } else if (results_are_empty) {
      results_are_empty = result.hypotheses.empty();
    }
  }

  if (provisional_results_pending) {
    listener()->OnRecognitionResults(session_id(), results);
    // We don't end the recognition if a provisional result is received in
    // STATE_WAITING_FINAL_RESULT. A definitive result will come next and will
    // end the recognition.
    return state_;
  }

  recognition_engine_->EndRecognition();

  if (!results_are_empty) {
    // We could receive an empty result (which we won't propagate further)
    // in the following (continuous) scenario:
    //  1. The caller start pushing audio and receives some results;
    //  2. A |StopAudioCapture| is issued later;
    //  3. The final audio frames captured in the interval ]1,2] do not lead to
    //     any result (nor any error);
    //  4. The speech recognition engine, therefore, emits an empty result to
    //     notify that the recognition is ended with no error, yet neither any
    //     further result.
    listener()->OnRecognitionResults(session_id(), results);
  }

  listener()->OnRecognitionEnd(session_id());
  return STATE_ENDED;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DoNothing(const FSMEventArgs&) const {
  return state_;  // Just keep the current state.
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::NotFeasible(const FSMEventArgs& event_args) {
  NOTREACHED() << "Unfeasible event " << event_args.event
               << " in state " << state_;
  return state_;
}

void SpeechRecognizerImpl::CloseAudioControllerAsynchronously() {
  DCHECK(IsCapturingAudio());
  DVLOG(1) << "SpeechRecognizerImpl closing audio controller.";
  // Issues a Close on the audio controller, passing an empty callback. The only
  // purpose of such callback is to keep the audio controller refcounted until
  // Close has completed (in the audio thread) and automatically destroy it
  // afterwards (upon return from OnAudioClosed).
  audio_controller_->Close(base::Bind(&SpeechRecognizerImpl::OnAudioClosed,
                                      this, audio_controller_));
  audio_controller_ = NULL;  // The controller is still refcounted by Bind.
}

int SpeechRecognizerImpl::GetElapsedTimeMs() const {
  return (num_samples_recorded_ * 1000) / kAudioSampleRate;
}

void SpeechRecognizerImpl::UpdateSignalAndNoiseLevels(const float& rms,
                                                  bool clip_detected) {
  // Calculate the input volume to display in the UI, smoothing towards the
  // new level.
  // TODO(primiano): Do we really need all this floating point arith here?
  // Perhaps it might be quite expensive on mobile.
  float level = (rms - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  level = std::min(std::max(0.0f, level), kAudioMeterRangeMaxUnclipped);
  const float smoothing_factor = (level > audio_level_) ? kUpSmoothingFactor :
                                                          kDownSmoothingFactor;
  audio_level_ += (level - audio_level_) * smoothing_factor;

  float noise_level = (endpointer_.NoiseLevelDb() - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  noise_level = std::min(std::max(0.0f, noise_level),
                         kAudioMeterRangeMaxUnclipped);

  listener()->OnAudioLevelsChange(
      session_id(), clip_detected ? 1.0f : audio_level_, noise_level);
}

void SpeechRecognizerImpl::SetAudioManagerForTests(
    AudioManager* audio_manager) {
  audio_manager_for_tests_ = audio_manager;
}

SpeechRecognizerImpl::FSMEventArgs::FSMEventArgs(FSMEvent event_value)
    : event(event_value),
      audio_data(NULL),
      engine_error(SPEECH_RECOGNITION_ERROR_NONE) {
}

SpeechRecognizerImpl::FSMEventArgs::~FSMEventArgs() {
}

}  // namespace content
