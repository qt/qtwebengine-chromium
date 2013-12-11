// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFileReaderTest : public testing::Test {
 public:
  AudioFileReaderTest() {}
  virtual ~AudioFileReaderTest() {}

  void Initialize(const char* filename) {
    data_ = ReadTestDataFile(filename);
    protocol_.reset(new InMemoryUrlProtocol(
        data_->data(), data_->data_size(), false));
    reader_.reset(new AudioFileReader(protocol_.get()));
  }

  // Reads and the entire file provided to Initialize().
  void ReadAndVerify(const char* expected_audio_hash, int expected_frames) {
    scoped_ptr<AudioBus> decoded_audio_data = AudioBus::Create(
        reader_->channels(), reader_->number_of_frames());
    int actual_frames = reader_->Read(decoded_audio_data.get());
    ASSERT_LE(actual_frames, decoded_audio_data->frames());
    ASSERT_EQ(expected_frames, actual_frames);

    AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);
    EXPECT_EQ(expected_audio_hash, audio_hash.ToString());
  }

  void RunTest(const char* fn, const char* hash, int channels, int sample_rate,
               base::TimeDelta duration, int frames, int trimmed_frames) {
    Initialize(fn);
    ASSERT_TRUE(reader_->Open());
    EXPECT_EQ(channels, reader_->channels());
    EXPECT_EQ(sample_rate, reader_->sample_rate());
    EXPECT_EQ(duration.InMicroseconds(), reader_->duration().InMicroseconds());
    EXPECT_EQ(frames, reader_->number_of_frames());
    ReadAndVerify(hash, trimmed_frames);
  }

  void RunTestFailingDemux(const char* fn) {
    Initialize(fn);
    EXPECT_FALSE(reader_->Open());
  }

  void RunTestFailingDecode(const char* fn) {
    Initialize(fn);
    EXPECT_TRUE(reader_->Open());
    scoped_ptr<AudioBus> decoded_audio_data = AudioBus::Create(
        reader_->channels(), reader_->number_of_frames());
    EXPECT_EQ(reader_->Read(decoded_audio_data.get()), 0);
  }

 protected:
  scoped_refptr<DecoderBuffer> data_;
  scoped_ptr<InMemoryUrlProtocol> protocol_;
  scoped_ptr<AudioFileReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(AudioFileReaderTest);
};

TEST_F(AudioFileReaderTest, WithoutOpen) {
  Initialize("bear.ogv");
}

TEST_F(AudioFileReaderTest, InvalidFile) {
  RunTestFailingDemux("ten_byte_file");
}

TEST_F(AudioFileReaderTest, WithVideo) {
  RunTest("bear.ogv", "-2.49,-0.75,0.38,1.60,-0.15,-1.22,", 2, 44100,
          base::TimeDelta::FromMicroseconds(1011520), 44608, 44608);
}

TEST_F(AudioFileReaderTest, Vorbis) {
  RunTest("sfx.ogg", "4.36,4.81,4.84,4.34,4.61,4.63,", 1, 44100,
          base::TimeDelta::FromMicroseconds(350001), 15435, 15435);
}

TEST_F(AudioFileReaderTest, WaveU8) {
  RunTest("sfx_u8.wav", "-1.23,-1.57,-1.14,-0.91,-0.87,-0.07,", 1, 44100,
          base::TimeDelta::FromMicroseconds(288414), 12719, 12719);
}

TEST_F(AudioFileReaderTest, WaveS16LE) {
  RunTest("sfx_s16le.wav", "3.05,2.87,3.00,3.32,3.58,4.08,", 1, 44100,
          base::TimeDelta::FromMicroseconds(288414), 12719, 12719);
}

TEST_F(AudioFileReaderTest, WaveS24LE) {
  RunTest("sfx_s24le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::TimeDelta::FromMicroseconds(288414), 12719, 12719);
}

TEST_F(AudioFileReaderTest, WaveF32LE) {
  RunTest("sfx_f32le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::TimeDelta::FromMicroseconds(288414), 12719, 12719);
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(AudioFileReaderTest, MP3) {
  RunTest("sfx.mp3", "3.05,2.87,3.00,3.32,3.58,4.08,", 1, 44100,
          base::TimeDelta::FromMicroseconds(313470), 13824, 12719);
}

TEST_F(AudioFileReaderTest, AAC) {
  RunTest("sfx.m4a", "1.81,1.66,2.32,3.27,4.46,3.36,", 1, 44100,
          base::TimeDelta::FromMicroseconds(312001), 13759, 13312);
}

TEST_F(AudioFileReaderTest, MidStreamConfigChangesFail) {
  RunTestFailingDecode("midstream_config_change.mp3");
}
#endif

TEST_F(AudioFileReaderTest, VorbisInvalidChannelLayout) {
  RunTestFailingDemux("9ch.ogg");
}

TEST_F(AudioFileReaderTest, WaveValidFourChannelLayout) {
  RunTest("4ch.wav", "131.71,38.02,130.31,44.89,135.98,42.52,", 4, 44100,
          base::TimeDelta::FromMicroseconds(100001), 4410, 4410);
}

}  // namespace media
