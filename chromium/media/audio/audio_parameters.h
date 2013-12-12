// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_PARAMETERS_H_
#define MEDIA_AUDIO_AUDIO_PARAMETERS_H_

#include "base/basictypes.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT AudioInputBufferParameters {
  double volume;
  uint32 size;
  bool key_pressed;
};

// Use a struct-in-struct approach to ensure that we can calculate the required
// size as sizeof(AudioInputBufferParameters) + #(bytes in audio buffer) without
// using packing.
struct MEDIA_EXPORT AudioInputBuffer {
  AudioInputBufferParameters params;
  int8 audio[1];
};

class MEDIA_EXPORT AudioParameters {
 public:
  // TODO(miu): Rename this enum to something that correctly reflects its
  // semantics, such as "TransportScheme."
  enum Format {
    AUDIO_PCM_LINEAR = 0,     // PCM is 'raw' amplitude samples.
    AUDIO_PCM_LOW_LATENCY,    // Linear PCM, low latency requested.
    AUDIO_FAKE,               // Creates a fake AudioOutputStream object.
    AUDIO_LAST_FORMAT         // Only used for validation of format.
  };

  enum {
    // Telephone quality sample rate, mostly for speech-only audio.
    kTelephoneSampleRate = 8000,
    // CD sampling rate is 44.1 KHz or conveniently 2x2x3x3x5x5x7x7.
    kAudioCDSampleRate = 44100,
  };

  AudioParameters();
  AudioParameters(Format format, ChannelLayout channel_layout,
                  int sample_rate, int bits_per_sample,
                  int frames_per_buffer);
  AudioParameters(Format format, ChannelLayout channel_layout,
                  int input_channels,
                  int sample_rate, int bits_per_sample,
                  int frames_per_buffer);
  void Reset(Format format, ChannelLayout channel_layout,
             int channels, int input_channels,
             int sample_rate, int bits_per_sample,
             int frames_per_buffer);

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  // Returns size of audio buffer in bytes.
  int GetBytesPerBuffer() const;

  // Returns the number of bytes representing one second of audio.
  int GetBytesPerSecond() const;

  // Returns the number of bytes representing a frame of audio.
  int GetBytesPerFrame() const;

  Format format() const { return format_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int sample_rate() const { return sample_rate_; }
  int bits_per_sample() const { return bits_per_sample_; }
  int frames_per_buffer() const { return frames_per_buffer_; }
  int channels() const { return channels_; }
  int input_channels() const { return input_channels_; }

  // Set to CHANNEL_LAYOUT_DISCRETE with given number of channels.
  void SetDiscreteChannels(int channels);

  // Comparison with other AudioParams.
  bool operator==(const AudioParameters& other) const {
    return format_ == other.format() &&
           sample_rate_ == other.sample_rate() &&
           channel_layout_ == other.channel_layout() &&
           channels_ == other.channels() &&
           input_channels_ == other.input_channels() &&
           bits_per_sample_ == other.bits_per_sample() &&
           frames_per_buffer_ == other.frames_per_buffer();
  }

 private:
  Format format_;                 // Format of the stream.
  ChannelLayout channel_layout_;  // Order of surround sound channels.
  int sample_rate_;               // Sampling frequency/rate.
  int bits_per_sample_;           // Number of bits per sample.
  int frames_per_buffer_;         // Number of frames in a buffer.

  int channels_;                  // Number of channels. Value set based on
                                  // |channel_layout|.
  int input_channels_;            // Optional number of input channels.
                                  // Normally 0, but can be set to specify
                                  // synchronized I/O.
};

// Comparison is useful when AudioParameters is used with std structures.
inline bool operator<(const AudioParameters& a, const AudioParameters& b) {
  if (a.format() != b.format())
    return a.format() < b.format();
  if (a.channels() != b.channels())
    return a.channels() < b.channels();
  if (a.input_channels() != b.input_channels())
    return a.input_channels() < b.input_channels();
  if (a.sample_rate() != b.sample_rate())
    return a.sample_rate() < b.sample_rate();
  if (a.bits_per_sample() != b.bits_per_sample())
    return a.bits_per_sample() < b.bits_per_sample();
  return a.frames_per_buffer() < b.frames_per_buffer();
}

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_PARAMETERS_H_
