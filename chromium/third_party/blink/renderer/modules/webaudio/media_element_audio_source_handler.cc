// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_handler.h"

#include <array>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_audio_source_provider_impl.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_element_audio_source_options.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

// Default to stereo. This could change depending on what the media element
// .src is set to.
constexpr unsigned kDefaultNumberOfOutputChannelsMEASH = 2;

}  // namespace

class MediaElementAudioSourceHandlerLocker final {
  STACK_ALLOCATED();

 public:
  explicit MediaElementAudioSourceHandlerLocker(
      MediaElementAudioSourceHandler& lockable)
      : lockable_(lockable) {
    lockable_.lock();
  }

  MediaElementAudioSourceHandlerLocker(
      const MediaElementAudioSourceHandlerLocker&) = delete;
  MediaElementAudioSourceHandlerLocker& operator=(
      const MediaElementAudioSourceHandlerLocker&) = delete;

  ~MediaElementAudioSourceHandlerLocker() { lockable_.unlock(); }

 private:
  MediaElementAudioSourceHandler& lockable_;
};

MediaElementAudioSourceHandler::MediaElementAudioSourceHandler(
    AudioNode& node,
    HTMLMediaElement& media_element)
    : AudioHandler(NodeType::kNodeTypeMediaElementAudioSource,
                   node,
                   node.context()->sampleRate()),
      audio_source_provider_(
          media_element.GetWebMediaPlayer()
              ? media_element.GetWebMediaPlayer()->GetAudioSourceProvider()
              : nullptr) {
  DCHECK(IsMainThread());

  AddOutput(kDefaultNumberOfOutputChannelsMEASH);

  if (Context()->GetExecutionContext()) {
    task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMediaElementEvent);
  }

  Initialize();
}

scoped_refptr<MediaElementAudioSourceHandler>
MediaElementAudioSourceHandler::Create(AudioNode& node,
                                       HTMLMediaElement& media_element) {
  return base::AdoptRef(
      new MediaElementAudioSourceHandler(node, media_element));
}

MediaElementAudioSourceHandler::~MediaElementAudioSourceHandler() {
  Uninitialize();
}

void MediaElementAudioSourceHandler::Dispose() {
  DCHECK(IsMainThread());
  {
    base::AutoLock locker(process_lock_);
    audio_source_provider_.reset();
    multi_channel_resampler_.reset();
  }
  AudioHandler::Dispose();
}

HTMLMediaElement* MediaElementAudioSourceHandler::GetMediaElement() {
  DCHECK(IsMainThread());
  if (AudioNode* node = GetNode()) {
    return static_cast<MediaElementAudioSourceNode*>(node)->mediaElement();
  }
  return nullptr;
}

void MediaElementAudioSourceHandler::SetFormat(uint32_t number_of_channels,
                                               float source_sample_rate) {
  DCHECK(IsMainThread());
  HTMLMediaElement* media_element = GetMediaElement();
  if (!media_element) {
    return;
  }
  bool is_tainted = WouldTaintOrigin();

  if (is_tainted) {
    PrintCorsMessage(media_element->currentSrc().GetString());
  }

  WebMediaPlayer* player = media_element->GetWebMediaPlayer();
  scoped_refptr<WebAudioSourceProviderImpl> provider =
      player ? player->GetAudioSourceProvider() : nullptr;

  const bool is_format_valid =
      number_of_channels > 0 &&
      number_of_channels <= BaseAudioContext::MaxNumberOfChannels() &&
      audio_utilities::IsValidAudioBufferSampleRate(source_sample_rate);
  if (!is_format_valid) {
    DLOG(ERROR) << "setFormat(" << number_of_channels << ", "
                << source_sample_rate << ") - unhandled format change";
  }

  {
    base::AutoLock locker(process_lock_);
    if (!is_format_valid) {
      // `Process()` will generate silence for these uninitialized values.
      source_number_of_channels_ = 0;
      source_sample_rate_ = 0;
      audio_source_provider_.reset();
      multi_channel_resampler_.reset();
      is_origin_tainted_ = is_tainted;
      return;
    }

    if (number_of_channels == source_number_of_channels_ &&
        source_sample_rate == source_sample_rate_) {
      if (provider) {
        audio_source_provider_ = std::move(provider);
      }
      is_origin_tainted_ = is_tainted;
      return;
    }
  }

  std::unique_ptr<MediaMultiChannelResampler> new_resampler;
  if (source_sample_rate != Context()->sampleRate()) {
    double scale_factor = source_sample_rate / Context()->sampleRate();
    new_resampler = std::make_unique<MediaMultiChannelResampler>(
        number_of_channels, scale_factor,
        GetDeferredTaskHandler().RenderQuantumFrames(),
        CrossThreadBindRepeating(
            &MediaElementAudioSourceHandler::ProvideResamplerInput,
            CrossThreadUnretained(this)));
  }

  {
    base::AutoLock locker(process_lock_);
    if (provider) {
      audio_source_provider_ = std::move(provider);
    }
    source_number_of_channels_ = number_of_channels;
    source_sample_rate_ = source_sample_rate;
    multi_channel_resampler_ = std::move(new_resampler);
    is_origin_tainted_ = is_tainted;
  }

  {
    // The context must be locked when changing the number of output channels.
    DeferredTaskHandler::GraphAutoLocker context_locker(Context());

    // Do any necesssary re-configuration to the output's number of channels.
    Output(0).SetNumberOfChannels(number_of_channels);
  }
}

bool MediaElementAudioSourceHandler::WouldTaintOrigin() {
  DCHECK(IsMainThread());
  HTMLMediaElement* media_element = GetMediaElement();
  if (!media_element) {
    return true;
  }
  WebMediaPlayer* player = media_element->GetWebMediaPlayer();
  return player ? player->WouldTaintOrigin() : false;
}

void MediaElementAudioSourceHandler::PrintCorsMessage(const String& message) {
  if (Context()->GetExecutionContext()) {
    Context()->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kSecurity,
            mojom::blink::ConsoleMessageLevel::kInfo,
            StrCat({"MediaElementAudioSource outputs zeroes due to CORS access "
                    "restrictions for ",
                    message})));
  }
}

void MediaElementAudioSourceHandler::ProvideInput(AudioBus* bus,
                                                  int frames_to_process) {
  DCHECK(bus);
  if (!audio_source_provider_) {
    bus->Zero();
    return;
  }

  const unsigned num_channels = bus->NumberOfChannels();
  if (num_channels == 0 ||
      num_channels > static_cast<unsigned>(media::limits::kMaxChannels)) {
    bus->Zero();
    return;
  }

  std::array<base::span<float>, media::limits::kMaxChannels> web_audio_data;
  const size_t frames = base::checked_cast<size_t>(frames_to_process);
  for (unsigned i = 0; i < num_channels; ++i) {
    web_audio_data[i] = bus->Channel(i)->MutableSpan().first(frames);
  }

  audio_source_provider_->ProvideInput(
      base::span(web_audio_data).first(num_channels), frames_to_process);
}

void MediaElementAudioSourceHandler::ProvideResamplerInput(
    int resampler_frame_delay,
    AudioBus* dest) {
  DCHECK(Context()->IsAudioThread());
  DCHECK(dest);
  ProvideInput(dest, base::checked_cast<int>(dest->length()));
}

void MediaElementAudioSourceHandler::Process(uint32_t number_of_frames) {
  AudioBus* output_bus = Output(0).Bus();

  // Use a tryLock() to avoid contention in the real-time audio thread.
  // If we fail to acquire the lock then the HTMLMediaElement must be in the
  // middle of reconfiguring its playback engine, so we output silence in this
  // case.
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    if (!audio_source_provider_ || !source_sample_rate_) {
      output_bus->Zero();
      return;
    }

    // TODO(crbug.com/811516): Although OnSetFormat() requested the output bus
    // channels, the actual channel count might have not been changed yet.
    // Output silence for such case until the channel count is resolved.
    if (source_number_of_channels_ != output_bus->NumberOfChannels()) {
      output_bus->Zero();
      return;
    }

    // Grab data from the provider so that the element continues to make
    // progress, even if we're going to output silence anyway.
    const int frames_int = base::checked_cast<int>(number_of_frames);
    if (multi_channel_resampler_.get()) {
      DCHECK_NE(source_sample_rate_, Context()->sampleRate());
      multi_channel_resampler_->Resample(frames_int, output_bus);
    } else {
      // Bypass the resampler completely if the source is at the context's
      // sample-rate.
      DCHECK_EQ(source_sample_rate_, Context()->sampleRate());
      ProvideInput(output_bus, frames_int);
    }
    // Output silence if we don't have access to the element.
    if (is_origin_tainted_) {
      output_bus->Zero();
    }
  } else {
    // We failed to acquire the lock.
    output_bus->Zero();
  }
}

void MediaElementAudioSourceHandler::lock() {
  process_lock_.Acquire();
}

void MediaElementAudioSourceHandler::unlock() {
  process_lock_.Release();
}

}  // namespace blink
