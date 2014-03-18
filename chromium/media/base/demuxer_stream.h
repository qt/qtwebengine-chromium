// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_STREAM_H_
#define MEDIA_BASE_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;

class MEDIA_EXPORT DemuxerStream {
 public:
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
    TEXT,
    NUM_TYPES,  // Always keep this entry as the last one!
  };

  // Status returned in the Read() callback.
  //  kOk : Indicates the second parameter is Non-NULL and contains media data
  //        or the end of the stream.
  //  kAborted : Indicates an aborted Read(). This can happen if the
  //             DemuxerStream gets flushed and doesn't have any more data to
  //             return. The second parameter MUST be NULL when this status is
  //             returned.
  //  kConfigChange : Indicates that the AudioDecoderConfig or
  //                  VideoDecoderConfig for the stream has changed.
  //                  The DemuxerStream expects an audio_decoder_config() or
  //                  video_decoder_config() call before Read() will start
  //                  returning DecoderBuffers again. The decoder will need this
  //                  new configuration to properly decode the buffers read
  //                  from this point forward. The second parameter MUST be NULL
  //                  when this status is returned.
  enum Status {
    kOk,
    kAborted,
    kConfigChanged,
  };

  // Request a buffer to returned via the provided callback.
  //
  // The first parameter indicates the status of the read.
  // The second parameter is non-NULL and contains media data
  // or the end of the stream if the first parameter is kOk. NULL otherwise.
  typedef base::Callback<void(Status,
                              const scoped_refptr<DecoderBuffer>&)>ReadCB;
  virtual void Read(const ReadCB& read_cb) = 0;

  // Returns the audio decoder configuration. It is an error to call this method
  // if type() != AUDIO.
  virtual AudioDecoderConfig audio_decoder_config() = 0;

  // Returns the video decoder configuration. It is an error to call this method
  // if type() != VIDEO.
  virtual VideoDecoderConfig video_decoder_config() = 0;

  // Returns the type of stream.
  virtual Type type() = 0;

  virtual void EnableBitstreamConverter() = 0;

 protected:
  // Only allow concrete implementations to get deleted.
  virtual ~DemuxerStream();
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_STREAM_H_
