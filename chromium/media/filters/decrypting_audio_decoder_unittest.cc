// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/audio_buffer.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::IsNull;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

// Make sure the kFakeAudioFrameSize is a valid frame size for all audio decoder
// configs used in this test.
static const int kFakeAudioFrameSize = 48;
static const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  arraysize(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), arraysize(kFakeIv)),
      0,
      std::vector<SubsampleEntry>())));
  return buffer;
}

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer.get() ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

ACTION_P(RunCallbackIfNotNull, param) {
  if (!arg0.is_null())
    arg0.Run(param);
}

ACTION_P2(ResetAndRunCallback, callback, param) {
  base::ResetAndReturn(callback).Run(param);
}

MATCHER(IsEndOfStream, "end of stream") {
  return (arg->end_of_stream());
}

}  // namespace

class DecryptingAudioDecoderTest : public testing::Test {
 public:
  DecryptingAudioDecoderTest()
      : decoder_(new DecryptingAudioDecoder(
            message_loop_.message_loop_proxy(),
            base::Bind(
                &DecryptingAudioDecoderTest::RequestDecryptorNotification,
                base::Unretained(this)))),
        decryptor_(new StrictMock<MockDecryptor>()),
        demuxer_(new StrictMock<MockDemuxerStream>(DemuxerStream::AUDIO)),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_frame_(NULL),
        end_of_stream_frame_(AudioBuffer::CreateEOSBuffer()),
        decoded_frame_list_() {
  }

  void InitializeAndExpectStatus(const AudioDecoderConfig& config,
                                 PipelineStatus status) {
    // Initialize data now that the config is known. Since the code uses
    // invalid values (that CreateEmptyBuffer() doesn't support), tweak them
    // just for CreateEmptyBuffer().
    int channels = ChannelLayoutToChannelCount(config.channel_layout());
    if (channels < 1)
      channels = 1;
    decoded_frame_ = AudioBuffer::CreateEmptyBuffer(
        channels, kFakeAudioFrameSize, kNoTimestamp(), kNoTimestamp());
    decoded_frame_list_.push_back(decoded_frame_);

    demuxer_->set_audio_decoder_config(config);
    decoder_->Initialize(demuxer_.get(), NewExpectedStatusCB(status),
                         base::Bind(&MockStatisticsCB::OnStatistics,
                                    base::Unretained(&statistics_cb_)));
    message_loop_.RunUntilIdle();
  }

  void Initialize() {
    EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
        .Times(AtMost(1))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*this, RequestDecryptorNotification(_))
        .WillOnce(RunCallbackIfNotNull(decryptor_.get()));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    config_.Initialize(kCodecVorbis, kSampleFormatPlanarF32,
                       CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, true, true,
                       base::TimeDelta(), base::TimeDelta());
    InitializeAndExpectStatus(config_, PIPELINE_OK);

    EXPECT_EQ(DecryptingAudioDecoder::kSupportedBitsPerChannel,
              decoder_->bits_per_channel());
    EXPECT_EQ(config_.channel_layout(), decoder_->channel_layout());
    EXPECT_EQ(config_.samples_per_second(), decoder_->samples_per_second());
  }

  void ReadAndExpectFrameReadyWith(
      AudioDecoder::Status status,
      const scoped_refptr<AudioBuffer>& audio_frame) {
    if (status != AudioDecoder::kOk)
      EXPECT_CALL(*this, FrameReady(status, IsNull()));
    else if (audio_frame->end_of_stream())
      EXPECT_CALL(*this, FrameReady(status, IsEndOfStream()));
    else
      EXPECT_CALL(*this, FrameReady(status, audio_frame));

    decoder_->Read(base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                              base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    Decryptor::AudioBuffers end_of_stream_frames_(1, end_of_stream_frame_);

    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(encrypted_buffer_))
        .WillRepeatedly(ReturnBuffer(DecoderBuffer::CreateEOSBuffer()));
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNeedMoreData,
                                       Decryptor::AudioBuffers()));
    EXPECT_CALL(statistics_cb_, OnStatistics(_));

    ReadAndExpectFrameReadyWith(AudioDecoder::kOk, decoded_frame_);
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    ReadAndExpectFrameReadyWith(AudioDecoder::kOk, end_of_stream_frame_);
  }

  // Make the read callback pending by saving and not firing it.
  void EnterPendingReadState() {
    EXPECT_TRUE(pending_demuxer_read_cb_.is_null());
    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(SaveArg<0>(&pending_demuxer_read_cb_));
    decoder_->Read(base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                              base::Unretained(this)));
    message_loop_.RunUntilIdle();
    // Make sure the Read() on the decoder triggers a Read() on the demuxer.
    EXPECT_FALSE(pending_demuxer_read_cb_.is_null());
  }

  // Make the audio decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(pending_audio_decode_cb_.is_null());
    EXPECT_CALL(*demuxer_, Read(_))
        .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_audio_decode_cb_));

    decoder_->Read(base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                              base::Unretained(this)));
    message_loop_.RunUntilIdle();
    // Make sure the Read() on the decoder triggers a DecryptAndDecode() on the
    // decryptor.
    EXPECT_FALSE(pending_audio_decode_cb_.is_null());
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*demuxer_, Read(_))
        .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNoKey,
                                       Decryptor::AudioBuffers()));
    decoder_->Read(base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                              base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void AbortPendingAudioDecodeCB() {
    if (!pending_audio_decode_cb_.is_null()) {
      base::ResetAndReturn(&pending_audio_decode_cb_).Run(
          Decryptor::kSuccess, Decryptor::AudioBuffers());
    }
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingAudioDecoderTest::AbortPendingAudioDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(RequestDecryptorNotification, void(const DecryptorReadyCB&));

  MOCK_METHOD2(FrameReady,
               void(AudioDecoder::Status, const scoped_refptr<AudioBuffer>&));

  base::MessageLoop message_loop_;
  scoped_ptr<DecryptingAudioDecoder> decoder_;
  scoped_ptr<StrictMock<MockDecryptor> > decryptor_;
  scoped_ptr<StrictMock<MockDemuxerStream> > demuxer_;
  MockStatisticsCB statistics_cb_;
  AudioDecoderConfig config_;

  DemuxerStream::ReadCB pending_demuxer_read_cb_;
  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::AudioDecodeCB pending_audio_decode_cb_;

  // Constant buffer/frames to be returned by the |demuxer_| and |decryptor_|.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<AudioBuffer> decoded_frame_;
  scoped_refptr<AudioBuffer> end_of_stream_frame_;
  Decryptor::AudioBuffers decoded_frame_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingAudioDecoderTest);
};

TEST_F(DecryptingAudioDecoderTest, Initialize_Normal) {
  Initialize();
}

// Ensure that DecryptingAudioDecoder only accepts encrypted audio.
TEST_F(DecryptingAudioDecoderTest, Initialize_UnencryptedAudioConfig) {
  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, false);

  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

// Ensure decoder handles invalid audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_InvalidAudioConfig) {
  AudioDecoderConfig config(kUnknownAudioCodec, kUnknownSampleFormat,
                            CHANNEL_LAYOUT_STEREO, 0, NULL, 0, true);

  InitializeAndExpectStatus(config, PIPELINE_ERROR_DECODE);
}

// Ensure decoder handles unsupported audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_UnsupportedAudioConfig) {
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(false));
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillOnce(RunCallbackIfNotNull(decryptor_.get()));

  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, true);
  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingAudioDecoderTest, Initialize_NullDecryptor) {
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillRepeatedly(RunCallbackIfNotNull(static_cast<Decryptor*>(NULL)));

  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, true);
  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kError,
                                     Decryptor::AudioBuffers()));

  ReadAndExpectFrameReadyWith(AudioDecoder::kDecodeError, NULL);
}

// Test the case where the decryptor returns kNeedMoreData to ask for more
// buffers before it can produce a frame.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_NeedMoreData) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .Times(2)
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kNeedMoreData,
                               Decryptor::AudioBuffers()))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(statistics_cb_, OnStatistics(_))
      .Times(2);

  ReadAndExpectFrameReadyWith(AudioDecoder::kOk, decoded_frame_);
}

// Test the case where the decryptor returns multiple decoded frames.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_MultipleFrames) {
  Initialize();

  scoped_refptr<AudioBuffer> frame_a = AudioBuffer::CreateEmptyBuffer(
      ChannelLayoutToChannelCount(config_.channel_layout()),
      kFakeAudioFrameSize,
      kNoTimestamp(),
      kNoTimestamp());
  scoped_refptr<AudioBuffer> frame_b = AudioBuffer::CreateEmptyBuffer(
      ChannelLayoutToChannelCount(config_.channel_layout()),
      kFakeAudioFrameSize,
      kNoTimestamp(),
      kNoTimestamp());
  decoded_frame_list_.push_back(frame_a);
  decoded_frame_list_.push_back(frame_b);

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(statistics_cb_, OnStatistics(_));

  ReadAndExpectFrameReadyWith(AudioDecoder::kOk, decoded_frame_);
  ReadAndExpectFrameReadyWith(AudioDecoder::kOk, frame_a);
  ReadAndExpectFrameReadyWith(AudioDecoder::kOk, frame_b);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test aborted read on the demuxer stream.
TEST_F(DecryptingAudioDecoderTest, DemuxerRead_Aborted) {
  Initialize();

  // ReturnBuffer() with NULL triggers aborted demuxer read.
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(scoped_refptr<DecoderBuffer>()));

  ReadAndExpectFrameReadyWith(AudioDecoder::kAborted, NULL);
}

// Test config change on the demuxer stream.
TEST_F(DecryptingAudioDecoderTest, DemuxerRead_ConfigChange) {
  Initialize();

  // The new config is different from the initial config in bits-per-channel,
  // channel layout and samples_per_second.
  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarS16,
                                CHANNEL_LAYOUT_5_1, 88200, NULL, 0, false);
  EXPECT_NE(new_config.bits_per_channel(), config_.bits_per_channel());
  EXPECT_NE(new_config.channel_layout(), config_.channel_layout());
  EXPECT_NE(new_config.samples_per_second(), config_.samples_per_second());

  demuxer_->set_audio_decoder_config(new_config);
  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(true));
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(RunCallback<0>(DemuxerStream::kConfigChanged,
                               scoped_refptr<DecoderBuffer>()))
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(statistics_cb_, OnStatistics(_));

  ReadAndExpectFrameReadyWith(AudioDecoder::kOk, decoded_frame_);

  EXPECT_EQ(new_config.bits_per_channel(), decoder_->bits_per_channel());
  EXPECT_EQ(new_config.channel_layout(), decoder_->channel_layout());
  EXPECT_EQ(new_config.samples_per_second(), decoder_->samples_per_second());
}

// Test config change failure.
TEST_F(DecryptingAudioDecoderTest, DemuxerRead_ConfigChangeFailed) {
  Initialize();

  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(false));
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(RunCallback<0>(DemuxerStream::kConfigChanged,
                               scoped_refptr<DecoderBuffer>()))
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));

  ReadAndExpectFrameReadyWith(AudioDecoder::kDecodeError, NULL);
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(statistics_cb_, OnStatistics(_));
  EXPECT_CALL(*this, FrameReady(AudioDecoder::kOk, decoded_frame_));
  key_added_cb_.Run();
  message_loop_.RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DruingPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(statistics_cb_, OnStatistics(_));
  EXPECT_CALL(*this, FrameReady(AudioDecoder::kOk, decoded_frame_));
  // The audio decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  base::ResetAndReturn(&pending_audio_decode_cb_).Run(
      Decryptor::kNoKey, Decryptor::AudioBuffers());
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDemuxerRead state and the read
// callback is returned with kOk.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringDemuxerRead_Ok) {
  Initialize();
  EnterPendingReadState();

  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  Reset();
  base::ResetAndReturn(&pending_demuxer_read_cb_).Run(DemuxerStream::kOk,
                                                      encrypted_buffer_);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kPendingDemuxerRead state and the read
// callback is returned with kAborted.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringDemuxerRead_Aborted) {
  Initialize();
  EnterPendingReadState();

  // Make sure we get a NULL audio frame returned.
  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  Reset();
  base::ResetAndReturn(&pending_demuxer_read_cb_).Run(DemuxerStream::kAborted,
                                                      NULL);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kPendingDemuxerRead state and the read
// callback is returned with kConfigChanged.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingReadState();

  Reset();

  // The new config is different from the initial config in bits-per-channel,
  // channel layout and samples_per_second.
  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarS16,
                                CHANNEL_LAYOUT_5_1, 88200, NULL, 0, false);
  EXPECT_NE(new_config.bits_per_channel(), config_.bits_per_channel());
  EXPECT_NE(new_config.channel_layout(), config_.channel_layout());
  EXPECT_NE(new_config.samples_per_second(), config_.samples_per_second());

  // Even during pending reset, the decoder still needs to be initialized with
  // the new config.
  demuxer_->set_audio_decoder_config(new_config);
  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(true));
  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  base::ResetAndReturn(&pending_demuxer_read_cb_)
      .Run(DemuxerStream::kConfigChanged, NULL);
  message_loop_.RunUntilIdle();

  EXPECT_EQ(new_config.bits_per_channel(), decoder_->bits_per_channel());
  EXPECT_EQ(new_config.channel_layout(), decoder_->channel_layout());
  EXPECT_EQ(new_config.samples_per_second(), decoder_->samples_per_second());
}

// Test resetting when the decoder is in kPendingDemuxerRead state, the read
// callback is returned with kConfigChanged and the config change fails.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringDemuxerRead_ConfigChangeFailed) {
  Initialize();
  EnterPendingReadState();

  Reset();

  // Even during pending reset, the decoder still needs to be initialized with
  // the new config.
  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(false));
  EXPECT_CALL(*this, FrameReady(AudioDecoder::kDecodeError, IsNull()));

  base::ResetAndReturn(&pending_demuxer_read_cb_)
      .Run(DemuxerStream::kConfigChanged, NULL);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kPendingConfigChange state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringPendingConfigChange) {
  Initialize();
  EnterNormalDecodingState();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(RunCallback<0>(DemuxerStream::kConfigChanged,
                               scoped_refptr<DecoderBuffer>()));
  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(SaveArg<1>(&pending_init_cb_));

  decoder_->Read(base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                            base::Unretained(this)));
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(pending_init_cb_.is_null());

  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  Reset();
  base::ResetAndReturn(&pending_init_cb_).Run(true);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, FrameReady(AudioDecoder::kAborted, IsNull()));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

}  // namespace media
