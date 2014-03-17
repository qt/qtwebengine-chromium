// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/channel_layout.h"

namespace content {

namespace {

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

#if defined(ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 32000;
#endif
const int kAudioProcessingNumberOfChannel = 1;

const int kMaxNumberOfBuffersInFifo = 2;

}  // namespace

class MediaStreamAudioProcessor::MediaStreamAudioConverter
    : public media::AudioConverter::InputCallback {
 public:
  MediaStreamAudioConverter(const media::AudioParameters& source_params,
                            const media::AudioParameters& sink_params)
     : source_params_(source_params),
       sink_params_(sink_params),
       audio_converter_(source_params, sink_params_, false) {
    audio_converter_.AddInput(this);
    // Create and initialize audio fifo and audio bus wrapper.
    // The size of the FIFO should be at least twice of the source buffer size
    // or twice of the sink buffer size.
    int buffer_size = std::max(
        kMaxNumberOfBuffersInFifo * source_params_.frames_per_buffer(),
        kMaxNumberOfBuffersInFifo * sink_params_.frames_per_buffer());
    fifo_.reset(new media::AudioFifo(source_params_.channels(), buffer_size));
    // TODO(xians): Use CreateWrapper to save one memcpy.
    audio_wrapper_ = media::AudioBus::Create(sink_params_.channels(),
                                             sink_params_.frames_per_buffer());
  }

  virtual ~MediaStreamAudioConverter() {
    DCHECK(thread_checker_.CalledOnValidThread());
    audio_converter_.RemoveInput(this);
  }

  void Push(media::AudioBus* audio_source) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    // And it must be the same thread as calling Convert().
    DCHECK(thread_checker_.CalledOnValidThread());
    fifo_->Push(audio_source);
  }

  bool Convert(webrtc::AudioFrame* out) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    // Return false if there is no 10ms data in the FIFO.
    DCHECK(thread_checker_.CalledOnValidThread());
    if (fifo_->frames() < (source_params_.sample_rate() / 100))
      return false;

    // Convert 10ms data to the output format, this will trigger ProvideInput().
    audio_converter_.Convert(audio_wrapper_.get());

    // TODO(xians): Figure out a better way to handle the interleaved and
    // deinterleaved format switching.
    audio_wrapper_->ToInterleaved(audio_wrapper_->frames(),
                                  sink_params_.bits_per_sample() / 8,
                                  out->data_);

    out->samples_per_channel_ = sink_params_.frames_per_buffer();
    out->sample_rate_hz_ = sink_params_.sample_rate();
    out->speech_type_ = webrtc::AudioFrame::kNormalSpeech;
    out->vad_activity_ = webrtc::AudioFrame::kVadUnknown;
    out->num_channels_ = sink_params_.channels();

    return true;
  }

  const media::AudioParameters& source_parameters() const {
    return source_params_;
  }
  const media::AudioParameters& sink_parameters() const {
    return sink_params_;
  }

 private:
  // AudioConverter::InputCallback implementation.
  virtual double ProvideInput(media::AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE {
    // Called on realtime audio thread.
    // TODO(xians): Figure out why the first Convert() triggers ProvideInput
    // two times.
    if (fifo_->frames() < audio_bus->frames())
      return 0;

    fifo_->Consume(audio_bus, 0, audio_bus->frames());

    // Return 1.0 to indicate no volume scaling on the data.
    return 1.0;
  }

  base::ThreadChecker thread_checker_;
  const media::AudioParameters source_params_;
  const media::AudioParameters sink_params_;

  // TODO(xians): consider using SincResampler to save some memcpy.
  // Handles mixing and resampling between input and output parameters.
  media::AudioConverter audio_converter_;
  scoped_ptr<media::AudioBus> audio_wrapper_;
  scoped_ptr<media::AudioFifo> fifo_;
};

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const webrtc::MediaConstraintsInterface* constraints)
    : render_delay_ms_(0) {
  capture_thread_checker_.DetachFromThread();
  render_thread_checker_.DetachFromThread();
  InitializeAudioProcessingModule(constraints);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  StopAudioProcessing();
}

void MediaStreamAudioProcessor::PushCaptureData(media::AudioBus* audio_source) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  capture_converter_->Push(audio_source);
}

void MediaStreamAudioProcessor::PushRenderData(
    const int16* render_audio, int sample_rate, int number_of_channels,
    int number_of_frames, base::TimeDelta render_delay) {
  DCHECK(render_thread_checker_.CalledOnValidThread());

  // Return immediately if the echo cancellation is off.
  if (!audio_processing_ ||
      !audio_processing_->echo_cancellation()->is_enabled()) {
    return;
  }

  TRACE_EVENT0("audio",
               "MediaStreamAudioProcessor::FeedRenderDataToAudioProcessing");
  int64 new_render_delay_ms = render_delay.InMilliseconds();
  DCHECK_LT(new_render_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, new_render_delay_ms);

  InitializeRenderConverterIfNeeded(sample_rate, number_of_channels,
                                    number_of_frames);

  // TODO(xians): Avoid this extra interleave/deinterleave.
  render_data_bus_->FromInterleaved(render_audio,
                                    render_data_bus_->frames(),
                                    sizeof(render_audio[0]));
  render_converter_->Push(render_data_bus_.get());
  while (render_converter_->Convert(&render_frame_))
    audio_processing_->AnalyzeReverseStream(&render_frame_);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    base::TimeDelta capture_delay, int volume, bool key_pressed,
    int16** out) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("audio",
               "MediaStreamAudioProcessor::ProcessAndConsumeData");

  if (!capture_converter_->Convert(&capture_frame_))
    return false;

  ProcessData(&capture_frame_, capture_delay, volume, key_pressed);
  *out = capture_frame_.data_;

  return true;
}

void MediaStreamAudioProcessor::SetCaptureFormat(
    const media::AudioParameters& source_params) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(source_params.IsValid());

  // Create and initialize audio converter for the source data.
  // When the webrtc AudioProcessing is enabled, the sink format of the
  // converter will be the same as the post-processed data format, which is
  // 32k mono for desktops and 16k mono for Android. When the AudioProcessing
  // is disabled, the sink format will be the same as the source format.
  const int sink_sample_rate = audio_processing_ ?
      kAudioProcessingSampleRate : source_params.sample_rate();
  const media::ChannelLayout sink_channel_layout = audio_processing_ ?
      media::CHANNEL_LAYOUT_MONO : source_params.channel_layout();

  // WebRtc is using 10ms data as its native packet size.
  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, sink_channel_layout,
      sink_sample_rate, 16, sink_sample_rate / 100);
  capture_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return capture_converter_->sink_parameters();
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const webrtc::MediaConstraintsInterface* constraints) {
  DCHECK(!audio_processing_);
  DCHECK(constraints);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAudioTrackProcessing)) {
    return;
  }

  const bool enable_aec = GetPropertyFromConstraints(
      constraints, MediaConstraintsInterface::kEchoCancellation);
  const bool enable_ns = GetPropertyFromConstraints(
      constraints, MediaConstraintsInterface::kNoiseSuppression);
  const bool enable_high_pass_filter = GetPropertyFromConstraints(
      constraints, MediaConstraintsInterface::kHighpassFilter);
#if defined(IOS) || defined(ANDROID)
  const bool enable_experimental_aec = false;
  const bool enable_typing_detection = false;
#else
  const bool enable_experimental_aec = GetPropertyFromConstraints(
      constraints, MediaConstraintsInterface::kExperimentalEchoCancellation);
  const bool enable_typing_detection = GetPropertyFromConstraints(
      constraints, MediaConstraintsInterface::kTypingNoiseDetection);
#endif

  // Return immediately if no audio processing component is enabled.
  if (!enable_aec && !enable_experimental_aec && !enable_ns &&
      !enable_high_pass_filter && !enable_typing_detection) {
    return;
  }

  // Create and configure the webrtc::AudioProcessing.
  audio_processing_.reset(webrtc::AudioProcessing::Create(0));

  // Enable the audio processing components.
  if (enable_aec) {
    EnableEchoCancellation(audio_processing_.get());
    if (enable_experimental_aec)
      EnableExperimentalEchoCancellation(audio_processing_.get());
  }

  if (enable_ns)
    EnableNoiseSuppression(audio_processing_.get());

  if (enable_high_pass_filter)
    EnableHighPassFilter(audio_processing_.get());

  if (enable_typing_detection)
    EnableTypingDetection(audio_processing_.get());


  // Configure the audio format the audio processing is running on. This
  // has to be done after all the needed components are enabled.
  CHECK_EQ(audio_processing_->set_sample_rate_hz(kAudioProcessingSampleRate),
           0);
  CHECK_EQ(audio_processing_->set_num_channels(kAudioProcessingNumberOfChannel,
                                               kAudioProcessingNumberOfChannel),
           0);
}

void MediaStreamAudioProcessor::InitializeRenderConverterIfNeeded(
    int sample_rate, int number_of_channels, int frames_per_buffer) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // TODO(xians): Figure out if we need to handle the buffer size change.
  if (render_converter_.get() &&
      render_converter_->source_parameters().sample_rate() == sample_rate &&
      render_converter_->source_parameters().channels() == number_of_channels) {
    // Do nothing if the |render_converter_| has been setup properly.
    return;
  }

  // Create and initialize audio converter for the render data.
  // webrtc::AudioProcessing accepts the same format as what it uses to process
  // capture data, which is 32k mono for desktops and 16k mono for Android.
  media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(number_of_channels), sample_rate, 16,
      frames_per_buffer);
  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, kAudioProcessingSampleRate, 16,
      kAudioProcessingSampleRate / 100);
  render_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
  render_data_bus_ = media::AudioBus::Create(number_of_channels,
                                             frames_per_buffer);
}

void MediaStreamAudioProcessor::ProcessData(webrtc::AudioFrame* audio_frame,
                                            base::TimeDelta capture_delay,
                                            int volume,
                                            bool key_pressed) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  if (!audio_processing_)
    return;

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::Process10MsData");
  DCHECK_EQ(audio_processing_->sample_rate_hz(),
            capture_converter_->sink_parameters().sample_rate());
  DCHECK_EQ(audio_processing_->num_input_channels(),
            capture_converter_->sink_parameters().channels());
  DCHECK_EQ(audio_processing_->num_output_channels(),
            capture_converter_->sink_parameters().channels());

  base::subtle::Atomic32 render_delay_ms =
      base::subtle::Acquire_Load(&render_delay_ms_);
  int64 capture_delay_ms = capture_delay.InMilliseconds();
  DCHECK_LT(capture_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());
  int total_delay_ms =  capture_delay_ms + render_delay_ms;
  if (total_delay_ms > 1000) {
    LOG(WARNING) << "Large audio delay, capture delay: " << capture_delay_ms
                 << "ms; render delay: " << render_delay_ms << "ms";
  }

  audio_processing_->set_stream_delay_ms(total_delay_ms);
  webrtc::GainControl* agc = audio_processing_->gain_control();
  int err = agc->set_stream_analog_level(volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;
  err = audio_processing_->ProcessStream(audio_frame);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  // TODO(xians): Add support for AGC, typing detection, audio level
  // calculation, stereo swapping.
}

void MediaStreamAudioProcessor::StopAudioProcessing() {
  if (!audio_processing_.get())
    return;

  audio_processing_.reset();
}

}  // namespace content
