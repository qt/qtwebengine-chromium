// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "media/audio/audio_parameters.h"
#include "media/base/limits.h"
#include "media/video/capture/video_capture_types.h"

using media::AudioParameters;
using media::ChannelLayout;
using media::VideoCaptureParams;
using media::VideoCaptureSessionId;

namespace IPC {

void ParamTraits<AudioParameters>::Write(Message* m,
                                         const AudioParameters& p) {
  m->WriteInt(static_cast<int>(p.format()));
  m->WriteInt(static_cast<int>(p.channel_layout()));
  m->WriteInt(p.sample_rate());
  m->WriteInt(p.bits_per_sample());
  m->WriteInt(p.frames_per_buffer());
  m->WriteInt(p.channels());
  m->WriteInt(p.input_channels());
}

bool ParamTraits<AudioParameters>::Read(const Message* m,
                                        PickleIterator* iter,
                                        AudioParameters* r) {
  int format, channel_layout, sample_rate, bits_per_sample,
      frames_per_buffer, channels, input_channels;

  if (!m->ReadInt(iter, &format) ||
      !m->ReadInt(iter, &channel_layout) ||
      !m->ReadInt(iter, &sample_rate) ||
      !m->ReadInt(iter, &bits_per_sample) ||
      !m->ReadInt(iter, &frames_per_buffer) ||
      !m->ReadInt(iter, &channels) ||
      !m->ReadInt(iter, &input_channels))
    return false;
  r->Reset(static_cast<AudioParameters::Format>(format),
           static_cast<ChannelLayout>(channel_layout), channels,
           input_channels, sample_rate, bits_per_sample, frames_per_buffer);
  if (!r->IsValid())
    return false;
  return true;
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

void ParamTraits<VideoCaptureParams>::Write(Message* m,
                                            const VideoCaptureParams& p) {
  m->WriteInt(p.width);
  m->WriteInt(p.height);
  m->WriteInt(p.frame_rate);
  m->WriteInt(static_cast<int>(p.session_id));
  m->WriteInt(static_cast<int>(p.frame_size_type));
}

bool ParamTraits<VideoCaptureParams>::Read(const Message* m,
                                           PickleIterator* iter,
                                           VideoCaptureParams* r) {
  int session_id, frame_size_type;
  if (!m->ReadInt(iter, &r->width) ||
      !m->ReadInt(iter, &r->height) ||
      !m->ReadInt(iter, &r->frame_rate) ||
      !m->ReadInt(iter, &session_id) ||
      !m->ReadInt(iter, &frame_size_type))
    return false;

  r->session_id = static_cast<VideoCaptureSessionId>(session_id);
  r->frame_size_type =
      static_cast<media::VideoCaptureResolutionType>(
          frame_size_type);
  if (!r->IsValid())
    return false;
  return true;
}

void ParamTraits<VideoCaptureParams>::Log(const VideoCaptureParams& p,
                                          std::string* l) {
  l->append(base::StringPrintf("<VideoCaptureParams>"));
}

}
