// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_text_track_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/filters/webvtt_util.h"

namespace media {

FakeTextTrackStream::FakeTextTrackStream()
    : message_loop_(base::MessageLoopProxy::current()),
      stopping_(false) {
}

FakeTextTrackStream::~FakeTextTrackStream() {
  DCHECK(read_cb_.is_null());
}

void FakeTextTrackStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null());
  OnRead();
  read_cb_ = read_cb;

  if (stopping_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FakeTextTrackStream::AbortPendingRead, base::Unretained(this)));
  }
}

DemuxerStream::Type FakeTextTrackStream::type() {
  return DemuxerStream::TEXT;
}

void FakeTextTrackStream::SatisfyPendingRead(
    const base::TimeDelta& start,
    const base::TimeDelta& duration,
    const std::string& id,
    const std::string& content,
    const std::string& settings) {
  DCHECK(!read_cb_.is_null());

  const uint8* const data_buf = reinterpret_cast<const uint8*>(content.data());
  const int data_len = static_cast<int>(content.size());

  std::vector<uint8> side_data;
  MakeSideData(id.begin(), id.end(),
                settings.begin(), settings.end(),
                &side_data);

  const uint8* const sd_buf = &side_data[0];
  const int sd_len = static_cast<int>(side_data.size());

  scoped_refptr<DecoderBuffer> buffer;
  buffer = DecoderBuffer::CopyFrom(data_buf, data_len, sd_buf, sd_len);

  buffer->set_timestamp(start);
  buffer->set_duration(duration);

  base::ResetAndReturn(&read_cb_).Run(kOk, buffer);
}

void FakeTextTrackStream::AbortPendingRead() {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
}

void FakeTextTrackStream::SendEosNotification() {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(kOk, DecoderBuffer::CreateEOSBuffer());
}

void FakeTextTrackStream::Stop() {
  stopping_ = true;
  if (!read_cb_.is_null())
    AbortPendingRead();
}

}  // namespace media
