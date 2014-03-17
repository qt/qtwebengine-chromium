// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_renderer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/text_cue.h"

namespace media {

TextRenderer::TextRenderer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const AddTextTrackCB& add_text_track_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      add_text_track_cb_(add_text_track_cb),
      state_(kUninitialized),
      pending_read_count_(0) {
}

TextRenderer::~TextRenderer() {
  DCHECK(state_ == kUninitialized ||
         state_ == kStopped) << "state_ " << state_;
  DCHECK_EQ(pending_read_count_, 0);
  STLDeleteValues(&text_track_state_map_);
}

void TextRenderer::Initialize(const base::Closure& ended_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!ended_cb.is_null());
  DCHECK_EQ(kUninitialized, state_)  << "state_ " << state_;
  DCHECK(text_track_state_map_.empty());
  DCHECK_EQ(pending_read_count_, 0);
  DCHECK(pending_eos_set_.empty());
  DCHECK(ended_cb_.is_null());

  weak_this_ = weak_factory_.GetWeakPtr();
  ended_cb_ = ended_cb;
  state_ = kPaused;
}

void TextRenderer::Play(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPaused) << "state_ " << state_;

  for (TextTrackStateMap::iterator itr = text_track_state_map_.begin();
       itr != text_track_state_map_.end(); ++itr) {
    TextTrackState* state = itr->second;
    if (state->read_state == TextTrackState::kReadPending) {
      DCHECK_GT(pending_read_count_, 0);
      continue;
    }

    Read(state, itr->first);
  }

  state_ = kPlaying;
  callback.Run();
}

void TextRenderer::Pause(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kPlaying || state_ == kEnded) << "state_ " << state_;
  DCHECK_GE(pending_read_count_, 0);
  pause_cb_ = callback;

  if (pending_read_count_ == 0) {
    state_ = kPaused;
    base::ResetAndReturn(&pause_cb_).Run();
    return;
  }

  state_ = kPausePending;
}

void TextRenderer::Flush(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(pending_read_count_, 0);
  DCHECK(state_ == kPaused) << "state_ " << state_;

  for (TextTrackStateMap::iterator itr = text_track_state_map_.begin();
       itr != text_track_state_map_.end(); ++itr) {
    pending_eos_set_.insert(itr->first);
  }
  DCHECK_EQ(pending_eos_set_.size(), text_track_state_map_.size());
  callback.Run();
}

void TextRenderer::Stop(const base::Closure& cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!cb.is_null());
  DCHECK(state_ == kPlaying ||
         state_ == kPausePending ||
         state_ == kPaused ||
         state_ == kEnded) << "state_ " << state_;
  DCHECK_GE(pending_read_count_, 0);

  stop_cb_ = cb;

  if (pending_read_count_ == 0) {
    state_ = kStopped;
    base::ResetAndReturn(&stop_cb_).Run();
    return;
  }

  state_ = kStopPending;
}

void TextRenderer::AddTextStream(DemuxerStream* text_stream,
                                 const TextTrackConfig& config) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized) << "state_ " << state_;
  DCHECK_NE(state_, kStopPending);
  DCHECK_NE(state_, kStopped);
  DCHECK(text_track_state_map_.find(text_stream) ==
         text_track_state_map_.end());
  DCHECK(pending_eos_set_.find(text_stream) ==
         pending_eos_set_.end());

  media::AddTextTrackDoneCB done_cb =
      media::BindToLoop(message_loop_,
                        base::Bind(&TextRenderer::OnAddTextTrackDone,
                                   weak_this_,
                                   text_stream));

  add_text_track_cb_.Run(config, done_cb);
}

void TextRenderer::RemoveTextStream(DemuxerStream* text_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  TextTrackStateMap::iterator itr = text_track_state_map_.find(text_stream);
  DCHECK(itr != text_track_state_map_.end());

  TextTrackState* state = itr->second;
  DCHECK_EQ(state->read_state, TextTrackState::kReadIdle);
  delete state;
  text_track_state_map_.erase(itr);

  pending_eos_set_.erase(text_stream);
}

bool TextRenderer::HasTracks() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return !text_track_state_map_.empty();
}

void TextRenderer::BufferReady(
    DemuxerStream* stream,
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& input) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(status, DemuxerStream::kConfigChanged);

  if (status == DemuxerStream::kAborted) {
    DCHECK(!input);
    DCHECK_GT(pending_read_count_, 0);
    DCHECK(pending_eos_set_.find(stream) != pending_eos_set_.end());

    TextTrackStateMap::iterator itr = text_track_state_map_.find(stream);
    DCHECK(itr != text_track_state_map_.end());

    TextTrackState* state = itr->second;
    DCHECK_EQ(state->read_state, TextTrackState::kReadPending);

    --pending_read_count_;
    state->read_state = TextTrackState::kReadIdle;

    switch (state_) {
      case kPlaying:
        return;

      case kPausePending:
        if (pending_read_count_ == 0) {
          state_ = kPaused;
          base::ResetAndReturn(&pause_cb_).Run();
        }

        return;

      case kStopPending:
        if (pending_read_count_ == 0) {
          state_ = kStopped;
          base::ResetAndReturn(&stop_cb_).Run();
        }

        return;

      case kPaused:
      case kStopped:
      case kUninitialized:
      case kEnded:
        NOTREACHED();
        return;
    }

    NOTREACHED();
    return;
  }

  if (input->end_of_stream()) {
    CueReady(stream, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  DCHECK_GE(input->side_data_size(), 2);

  // The side data contains both the cue id and cue settings,
  // each terminated with a NUL.
  const char* id_ptr = reinterpret_cast<const char*>(input->side_data());
  size_t id_len = strlen(id_ptr);
  std::string id(id_ptr, id_len);

  const char* settings_ptr = id_ptr + id_len + 1;
  size_t settings_len = strlen(settings_ptr);
  std::string settings(settings_ptr, settings_len);

  // The cue payload is stored in the data-part of the input buffer.
  std::string text(input->data(), input->data() + input->data_size());

  scoped_refptr<TextCue> text_cue(
      new TextCue(input->timestamp(),
                  input->duration(),
                  id,
                  settings,
                  text));

  CueReady(stream, text_cue);
}

void TextRenderer::CueReady(
    DemuxerStream* text_stream,
    const scoped_refptr<TextCue>& text_cue) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized &&
         state_ != kStopped) << "state_ " << state_;
  DCHECK_GT(pending_read_count_, 0);
  DCHECK(pending_eos_set_.find(text_stream) != pending_eos_set_.end());

  TextTrackStateMap::iterator itr = text_track_state_map_.find(text_stream);
  DCHECK(itr != text_track_state_map_.end());

  TextTrackState* state = itr->second;
  DCHECK_EQ(state->read_state, TextTrackState::kReadPending);
  DCHECK(state->text_track);

  --pending_read_count_;
  state->read_state = TextTrackState::kReadIdle;

  switch (state_) {
    case kPlaying: {
      if (text_cue)
        break;

      const size_t count = pending_eos_set_.erase(text_stream);
      DCHECK_EQ(count, 1U);

      if (pending_eos_set_.empty()) {
        DCHECK_EQ(pending_read_count_, 0);
        state_ = kEnded;
        ended_cb_.Run();
        return;
      }

      DCHECK_GT(pending_read_count_, 0);
      return;
    }
    case kPausePending: {
      if (text_cue)
        break;

      const size_t count = pending_eos_set_.erase(text_stream);
      DCHECK_EQ(count, 1U);

      if (pending_read_count_ > 0) {
        DCHECK(!pending_eos_set_.empty());
        return;
      }

      state_ = kPaused;
      base::ResetAndReturn(&pause_cb_).Run();

      return;
    }
    case kStopPending:
      if (pending_read_count_ == 0) {
        state_ = kStopped;
        base::ResetAndReturn(&stop_cb_).Run();
      }

      return;

    case kPaused:
    case kStopped:
    case kUninitialized:
    case kEnded:
      NOTREACHED();
      return;
  }

  base::TimeDelta start = text_cue->timestamp();
  base::TimeDelta end = start + text_cue->duration();

  state->text_track->addWebVTTCue(start, end,
                                  text_cue->id(),
                                  text_cue->text(),
                                  text_cue->settings());

  if (state_ == kPlaying) {
    Read(state, text_stream);
    return;
  }

  if (pending_read_count_ == 0) {
      DCHECK_EQ(state_, kPausePending) << "state_ " << state_;
      state_ = kPaused;
      base::ResetAndReturn(&pause_cb_).Run();
  }
}

void TextRenderer::OnAddTextTrackDone(DemuxerStream* text_stream,
                                      scoped_ptr<TextTrack> text_track) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized &&
         state_ != kStopped &&
         state_ != kStopPending) << "state_ " << state_;
  DCHECK(text_stream);
  DCHECK(text_track);

  scoped_ptr<TextTrackState> state(new TextTrackState(text_track.Pass()));
  text_track_state_map_[text_stream] = state.release();
  pending_eos_set_.insert(text_stream);

  if (state_ == kPlaying)
    Read(text_track_state_map_[text_stream], text_stream);
}

void TextRenderer::Read(
    TextTrackState* state,
    DemuxerStream* text_stream) {
  DCHECK_NE(state->read_state, TextTrackState::kReadPending);

  state->read_state = TextTrackState::kReadPending;
  ++pending_read_count_;

  text_stream->Read(base::Bind(&TextRenderer::BufferReady,
                                weak_this_,
                                text_stream));
}

TextRenderer::TextTrackState::TextTrackState(scoped_ptr<TextTrack> tt)
    : read_state(kReadIdle),
      text_track(tt.Pass()) {
}

TextRenderer::TextTrackState::~TextTrackState() {
}

}  // namespace media
