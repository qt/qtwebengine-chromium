// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_source_provider.h"

#include "base/logging.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProviderClient.h"

using blink::WebVector;

namespace content {

static const size_t kMaxNumberOfBuffers = 10;

// Size of the buffer that WebAudio processes each time, it is the same value
// as AudioNode::ProcessingSizeInFrames in WebKit.
// static
const size_t WebRtcLocalAudioSourceProvider::kWebAudioRenderBufferSize = 128;

WebRtcLocalAudioSourceProvider::WebRtcLocalAudioSourceProvider()
    : is_enabled_(false) {
  // Get the native audio output hardware sample-rate for the sink.
  // We need to check if RenderThreadImpl is valid here since the unittests
  // do not have one and they will inject their own |sink_params_| for testing.
  if (RenderThreadImpl::current()) {
    media::AudioHardwareConfig* hardware_config =
        RenderThreadImpl::current()->GetAudioHardwareConfig();
    int sample_rate = hardware_config->GetOutputSampleRate();
    sink_params_.Reset(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO, 2, 0, sample_rate, 16,
        kWebAudioRenderBufferSize);
  }
}

WebRtcLocalAudioSourceProvider::~WebRtcLocalAudioSourceProvider() {
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);
}

void WebRtcLocalAudioSourceProvider::OnSetFormat(
    const media::AudioParameters& params) {
  // We need detach the thread here because it will be a new capture thread
  // calling OnSetFormat() and OnData() if the source is restarted.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(params.IsValid());
  DCHECK(sink_params_.IsValid());

  base::AutoLock auto_lock(lock_);
  source_params_ = params;
  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request source_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_.reset(
      new media::AudioConverter(params, sink_params_, false));
  audio_converter_->AddInput(this);
  fifo_.reset(new media::AudioFifo(
      params.channels(),
      kMaxNumberOfBuffers * params.frames_per_buffer()));
  input_bus_ = media::AudioBus::Create(params.channels(),
                                       params.frames_per_buffer());
}

void WebRtcLocalAudioSourceProvider::OnData(
    const int16* audio_data,
    int sample_rate,
    int number_of_channels,
    int number_of_frames) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (!is_enabled_)
    return;

  DCHECK(fifo_.get());

  // TODO(xians): A better way to handle the interleaved and deinterleaved
  // format switching, see issue/317710.
  DCHECK(input_bus_->frames() == number_of_frames);
  DCHECK(input_bus_->channels() == number_of_channels);
  input_bus_->FromInterleaved(audio_data, number_of_frames, 2);

  if (fifo_->frames() + number_of_frames <= fifo_->max_frames()) {
    fifo_->Push(input_bus_.get());
  } else {
    // This can happen if the data in FIFO is too slowed to be consumed or
    // WebAudio stops consuming data.
    DLOG(WARNING) << "Local source provicer FIFO is full" << fifo_->frames();
  }
}

void WebRtcLocalAudioSourceProvider::setClient(
    blink::WebAudioSourceProviderClient* client) {
  NOTREACHED();
}

void WebRtcLocalAudioSourceProvider::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  DCHECK_EQ(number_of_frames, kWebAudioRenderBufferSize);
  if (!output_wrapper_ ||
      static_cast<size_t>(output_wrapper_->channels()) != audio_data.size()) {
    output_wrapper_ = media::AudioBus::CreateWrapper(audio_data.size());
  }

  output_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    output_wrapper_->SetChannelData(i, audio_data[i]);

  base::AutoLock auto_lock(lock_);
  if (!audio_converter_)
    return;

  is_enabled_ = true;
  audio_converter_->Convert(output_wrapper_.get());
}

double WebRtcLocalAudioSourceProvider::ProvideInput(
    media::AudioBus* audio_bus, base::TimeDelta buffer_delay) {
  if (fifo_->frames() >= audio_bus->frames()) {
    fifo_->Consume(audio_bus, 0, audio_bus->frames());
  } else {
    audio_bus->Zero();
    DVLOG(1) << "WARNING: Underrun, FIFO has data " << fifo_->frames()
             << " samples but " << audio_bus->frames()
             << " samples are needed";
  }

  return 1.0;
}

void WebRtcLocalAudioSourceProvider::SetSinkParamsForTesting(
    const media::AudioParameters& sink_params) {
  sink_params_ = sink_params;
}

}  // namespace content
