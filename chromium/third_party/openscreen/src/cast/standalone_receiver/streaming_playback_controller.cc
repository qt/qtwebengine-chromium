// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/streaming_playback_controller.h"

#include <string>

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_receiver/sdl_audio_player.h"  // nogncheck
#include "cast/standalone_receiver/sdl_glue.h"          // nogncheck
#include "cast/standalone_receiver/sdl_video_player.h"  // nogncheck
#else
#include "cast/standalone_receiver/dummy_player.h"  // nogncheck
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

#include "util/trace_logging.h"

namespace openscreen::cast {

StreamingPlaybackController::Client::~Client() = default;

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
StreamingPlaybackController::StreamingPlaybackController(
    TaskRunner& task_runner,
    StreamingPlaybackController::Client* client,
    bool enable_input_events)
    : client_(client),
      task_runner_(task_runner),
      enable_input_events_(enable_input_events),
      sdl_event_loop_(task_runner_, [this] {
        client_->OnPlaybackError(this,
                                 Error{Error::Code::kOperationCancelled,
                                       std::string("SDL event loop closed.")});
      }) {
  OSP_CHECK(client_);
  constexpr int kDefaultWindowWidth = 1280;
  constexpr int kDefaultWindowHeight = 720;
  window_ = MakeUniqueSDLWindow(
      "Cast Streaming Receiver Demo",
      SDL_WINDOWPOS_UNDEFINED /* initial X position */,
      SDL_WINDOWPOS_UNDEFINED /* initial Y position */, kDefaultWindowWidth,
      kDefaultWindowHeight, SDL_WINDOW_RESIZABLE);
  OSP_CHECK(window_) << "Failed to create SDL window: " << SDL_GetError();
  renderer_ = MakeUniqueSDLRenderer(window_.get(), -1, 0);
  OSP_CHECK(renderer_) << "Failed to create SDL renderer: " << SDL_GetError();

  sdl_event_loop_.RegisterForKeyboardEvent(
      [this](const SDL_KeyboardEvent& event) {
        this->HandleKeyboardEvent(event);
      });

  if (enable_input_events_) {
    sdl_event_loop_.RegisterForMouseButtonEvent(
        [this](const SDL_MouseButtonEvent& event) {
          this->HandleMouseButtonEvent(event);
        });
  }
}
#else
StreamingPlaybackController::StreamingPlaybackController(
    StreamingPlaybackController::Client* client,
    bool enable_input_events)
    : client_(client) {
  OSP_CHECK(client_);
}
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

void StreamingPlaybackController::OnNegotiated(
    const ReceiverSession* session,
    ReceiverSession::ConfiguredReceivers receivers) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneReceiver);
  session_ = session;
  Initialize(receivers);
}

void StreamingPlaybackController::OnRemotingNegotiated(
    const ReceiverSession* session,
    ReceiverSession::RemotingNegotiation negotiation) {
  session_ = session;
  remoting_receiver_ =
      std::make_unique<SimpleRemotingReceiver>(negotiation.messenger);
  remoting_receiver_->SendInitializeMessage(
      [this, receivers = negotiation.receivers](AudioCodec audio_codec,
                                                VideoCodec video_codec) {
        // The configurations in `negotiation` do not have the actual codecs,
        // only REMOTE_AUDIO and REMOTE_VIDEO. Once we receive the
        // initialization callback method, we can override with the actual
        // codecs here.
        auto mutable_receivers = receivers;
        mutable_receivers.audio_config.codec = audio_codec;
        mutable_receivers.video_config.codec = video_codec;
        Initialize(mutable_receivers);
      });
}

void StreamingPlaybackController::OnReceiversDestroying(
    const ReceiverSession* session,
    ReceiversDestroyingReason reason) {
  OSP_LOG_INFO << "Receivers are currently destroying, resetting SDL players.";
  audio_player_.reset();
  video_player_.reset();
  session_ = nullptr;
}

void StreamingPlaybackController::OnError(const ReceiverSession* session,
                                          const Error& error) {
  client_->OnPlaybackError(this, error);
}

void StreamingPlaybackController::Initialize(
    ReceiverSession::ConfiguredReceivers receivers) {
#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  OSP_LOG_INFO << "Successfully negotiated a session, creating SDL players.";
  if (receivers.video_receiver && !receivers.video_config.resolutions.empty()) {
    const auto& res = receivers.video_config.resolutions[0];
    SDL_RenderSetLogicalSize(renderer_.get(), res.width, res.height);
  } else {
    // Default to a logical size of 1920x1080.
    SDL_RenderSetLogicalSize(renderer_.get(), 1920, 1080);
  }
  if (receivers.audio_receiver) {
    audio_player_ = std::make_unique<SDLAudioPlayer>(
        &Clock::now, task_runner_, *receivers.audio_receiver,
        receivers.audio_config.codec, [this] {
          client_->OnPlaybackError(this, audio_player_->error_status());
        });
  }
  if (receivers.video_receiver) {
    video_player_ = std::make_unique<SDLVideoPlayer>(
        &Clock::now, task_runner_, *receivers.video_receiver,
        receivers.video_config.codec, *renderer_, [this] {
          client_->OnPlaybackError(this, video_player_->error_status());
        });
  }
#else
  if (receivers.audio_receiver) {
    audio_player_ = std::make_unique<DummyPlayer>(*receivers.audio_receiver);
  }

  if (receivers.video_receiver) {
    video_player_ = std::make_unique<DummyPlayer>(*receivers.video_receiver);
  }
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
}

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
void StreamingPlaybackController::HandleKeyboardEvent(
    const SDL_KeyboardEvent& event) {
  // We only handle keyboard events if we are remoting.
  if (!remoting_receiver_) {
    return;
  }

  switch (event.keysym.sym) {
    // See codes here: https://wiki.libsdl.org/SDL_Scancode
    case SDLK_KP_SPACE:  // fallthrough, "Keypad Space"
    case SDLK_SPACE:     // "Space"
      is_playing_ = !is_playing_;
      remoting_receiver_->SendPlaybackRateMessage(is_playing_ ? 1.0 : 0.0);
      break;
  }
}

void StreamingPlaybackController::HandleMouseButtonEvent(
    const SDL_MouseButtonEvent& event) {
  if (!session_) {
    return;
  }

  int w, h;
  SDL_RenderGetLogicalSize(renderer_.get(), &w, &h);
  if (w <= 0 || h <= 0) {
    return;
  }

  // If the click is outside the logical area (e.g. in letterboxing), skip it.
  // Note: SDL2 automatically scales mouse event coordinates to the logical
  // size if it is set on the renderer.
  if (event.x < 0 || event.x >= w || event.y < 0 || event.y >= h) {
    return;
  }

  InputMessage message;
  auto* input_event = message.add_events();
  auto* ts = input_event->mutable_timestamp();
  ts->set_seconds(event.timestamp / 1000);
  ts->set_nanos((event.timestamp % 1000) * 1000000);

  if (event.type == SDL_MOUSEBUTTONDOWN) {
    input_event->set_type(InputMessage::INPUT_TYPE_MOUSE_DOWN);
  } else {
    input_event->set_type(InputMessage::INPUT_TYPE_MOUSE_UP);
  }

  auto* mouse_event = input_event->mutable_mouse_event();
  auto* loc = mouse_event->mutable_location();
  loc->set_x(static_cast<float>(event.x) / static_cast<float>(w));
  loc->set_y(static_cast<float>(event.y) / static_cast<float>(h));
  if (event.button == SDL_BUTTON_LEFT) {
    mouse_event->add_buttons(InputMessage::MOUSE_BUTTON_PRIMARY);
  } else if (event.button == SDL_BUTTON_RIGHT) {
    mouse_event->add_buttons(InputMessage::MOUSE_BUTTON_SECONDARY);
  } else if (event.button == SDL_BUTTON_MIDDLE) {
    mouse_event->add_buttons(InputMessage::MOUSE_BUTTON_AUXILIARY);
  }

  message.set_viewport_width(w);
  message.set_viewport_height(h);

  const_cast<ReceiverSession*>(session_)->SendInputMessage(message);
}
#endif

}  // namespace openscreen::cast
