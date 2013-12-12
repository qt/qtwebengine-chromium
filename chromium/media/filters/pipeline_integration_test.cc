// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_keys.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/aes_decryptor.h"
#include "media/filters/chunk_demuxer.h"

using testing::AnyNumber;
using testing::AtMost;

namespace media {

static const char kSourceId[] = "SourceId";
static const char kClearKeySystem[] = "org.w3.clearkey";
static const uint8 kInitData[] = { 0x69, 0x6e, 0x69, 0x74 };

static const char kWebM[] = "video/webm; codecs=\"vp8,vorbis\"";
static const char kWebMVP9[] = "video/webm; codecs=\"vp9\"";
static const char kAudioOnlyWebM[] = "video/webm; codecs=\"vorbis\"";
static const char kOpusAudioOnlyWebM[] = "video/webm; codecs=\"opus\"";
static const char kVideoOnlyWebM[] = "video/webm; codecs=\"vp8\"";
static const char kMP4[] = "video/mp4; codecs=\"avc1.4D4041,mp4a.40.2\"";
static const char kMP4Video[] = "video/mp4; codecs=\"avc1.4D4041\"";
static const char kMP4Audio[] = "audio/mp4; codecs=\"mp4a.40.2\"";
static const char kMP4AudioType[] = "audio/mp4";
static const char kMP4VideoType[] = "video/mp4";
static const char kMP3[] = "audio/mpeg";

// Key used to encrypt test files.
static const uint8 kSecretKey[] = {
  0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
  0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c
};

// The key ID for all encrypted files.
static const uint8 kKeyId[] = {
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35
};

static const int kAppendWholeFile = -1;

// Constants for the Media Source config change tests.
static const int kAppendTimeSec = 1;
static const int kAppendTimeMs = kAppendTimeSec * 1000;
static const int k320WebMFileDurationMs = 2737;
static const int k640WebMFileDurationMs = 2763;
static const int k640IsoFileDurationMs = 2737;
static const int k640IsoCencFileDurationMs = 2736;
static const int k1280IsoFileDurationMs = 2736;
static const int kOpusEndTrimmingWebMFileDurationMs = 2771;
static const uint32 kOpusEndTrimmingWebMFileAudioBytes = 528676;
static const int kVP9WebMFileDurationMs = 2735;
static const int kVP8AWebMFileDurationMs = 2700;

// Command line switch for runtime adjustment of audio file to be benchmarked.
static const char kBenchmarkAudioFile[] = "benchmark-audio-file";

// Note: Tests using this class only exercise the DecryptingDemuxerStream path.
// They do not exercise the Decrypting{Audio|Video}Decoder path.
class FakeEncryptedMedia {
 public:
  // Defines the behavior of the "app" that responds to EME events.
  class AppBase {
   public:
    virtual ~AppBase() {}

    virtual void KeyAdded(const std::string& session_id) = 0;

    // Errors are not expected unless overridden.
    virtual void KeyError(const std::string& session_id,
                          MediaKeys::KeyError error_code,
                          int system_code) {
      FAIL() << "Unexpected Key Error";
    }

    virtual void KeyMessage(const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& default_url) = 0;

    virtual void NeedKey(const std::string& session_id,
                         const std::string& type,
                         const std::vector<uint8>& init_data,
                         AesDecryptor* decryptor) = 0;
  };

  FakeEncryptedMedia(AppBase* app)
      : decryptor_(base::Bind(&FakeEncryptedMedia::KeyAdded,
                              base::Unretained(this)),
                   base::Bind(&FakeEncryptedMedia::KeyError,
                              base::Unretained(this)),
                   base::Bind(&FakeEncryptedMedia::KeyMessage,
                              base::Unretained(this))),
        app_(app) {
  }

  AesDecryptor* decryptor() {
    return &decryptor_;
  }

  // Callbacks for firing key events. Delegate to |app_|.
  void KeyAdded(const std::string& session_id) {
    app_->KeyAdded(session_id);
  }

  void KeyError(const std::string& session_id,
                MediaKeys::KeyError error_code,
                int system_code) {
    app_->KeyError(session_id, error_code, system_code);
  }

  void KeyMessage(const std::string& session_id,
                  const std::vector<uint8>& message,
                  const std::string& default_url) {
    app_->KeyMessage(session_id, message, default_url);
  }

  void NeedKey(const std::string& session_id,
               const std::string& type,
               const std::vector<uint8>& init_data) {
    app_->NeedKey(session_id, type, init_data, &decryptor_);
  }

 private:
  AesDecryptor decryptor_;
  scoped_ptr<AppBase> app_;
};

// Provides |kSecretKey| in response to needkey.
class KeyProvidingApp : public FakeEncryptedMedia::AppBase {
 public:
  virtual void KeyAdded(const std::string& session_id) OVERRIDE {
    EXPECT_FALSE(session_id.empty());
  }

  virtual void KeyMessage(const std::string& session_id,
                          const std::vector<uint8>& message,
                          const std::string& default_url) OVERRIDE {
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(message.empty());

    current_session_id_ = session_id;
  }

  virtual void NeedKey(const std::string& session_id,
                       const std::string& type,
                       const std::vector<uint8>& init_data,
                       AesDecryptor* decryptor) OVERRIDE {
    current_session_id_ = session_id;

    if (current_session_id_.empty()) {
      EXPECT_TRUE(decryptor->GenerateKeyRequest(type, kInitData,
                                                arraysize(kInitData)));
    }

    EXPECT_FALSE(current_session_id_.empty());

    // Clear Key really needs the key ID in |init_data|. For WebM, they are the
    // same, but this is not the case for ISO CENC. Therefore, provide the
    // correct key ID.
    const uint8* key_id = init_data.empty() ? NULL : &init_data[0];
    size_t key_id_length = init_data.size();
    if (type == kMP4AudioType || type == kMP4VideoType) {
      key_id = kKeyId;
      key_id_length = arraysize(kKeyId);
    }

    decryptor->AddKey(kSecretKey, arraysize(kSecretKey),
                      key_id, key_id_length, current_session_id_);
  }

  std::string current_session_id_;
};

// Ignores needkey and does not perform a license request
class NoResponseApp : public FakeEncryptedMedia::AppBase {
 public:
  virtual void KeyAdded(const std::string& session_id) OVERRIDE {
    EXPECT_FALSE(session_id.empty());
    FAIL() << "Unexpected KeyAdded";
  }

  virtual void KeyMessage(const std::string& session_id,
                          const std::vector<uint8>& message,
                          const std::string& default_url) OVERRIDE {
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(message.empty());
    FAIL() << "Unexpected KeyMessage";
  }

  virtual void NeedKey(const std::string& session_id,
                       const std::string& type,
                       const std::vector<uint8>& init_data,
                       AesDecryptor* decryptor) OVERRIDE {
  }
};

// Helper class that emulates calls made on the ChunkDemuxer by the
// Media Source API.
class MockMediaSource {
 public:
  MockMediaSource(const std::string& filename, const std::string& mimetype,
                  int initial_append_size)
      : file_path_(GetTestDataFilePath(filename)),
        current_position_(0),
        initial_append_size_(initial_append_size),
        mimetype_(mimetype),
        chunk_demuxer_(new ChunkDemuxer(
            base::Bind(&MockMediaSource::DemuxerOpened,
                       base::Unretained(this)),
            base::Bind(&MockMediaSource::DemuxerNeedKey,
                       base::Unretained(this)),
            base::Bind(&MockMediaSource::OnTextTrack,
                       base::Unretained(this)),
            LogCB())),
        owned_chunk_demuxer_(chunk_demuxer_) {

    file_data_ = ReadTestDataFile(filename);

    if (initial_append_size_ == kAppendWholeFile)
      initial_append_size_ = file_data_->data_size();

    DCHECK_GT(initial_append_size_, 0);
    DCHECK_LE(initial_append_size_, file_data_->data_size());
  }

  virtual ~MockMediaSource() {}

  scoped_ptr<Demuxer> GetDemuxer() { return owned_chunk_demuxer_.Pass(); }

  void set_need_key_cb(const NeedKeyCB& need_key_cb) {
    need_key_cb_ = need_key_cb;
  }

  void Seek(base::TimeDelta seek_time, int new_position, int seek_append_size) {
    chunk_demuxer_->StartWaitingForSeek(seek_time);

    chunk_demuxer_->Abort(kSourceId);

    DCHECK_GE(new_position, 0);
    DCHECK_LT(new_position, file_data_->data_size());
    current_position_ = new_position;

    AppendData(seek_append_size);
  }

  void AppendData(int size) {
    DCHECK(chunk_demuxer_);
    DCHECK_LT(current_position_, file_data_->data_size());
    DCHECK_LE(current_position_ + size, file_data_->data_size());
    chunk_demuxer_->AppendData(
        kSourceId, file_data_->data() + current_position_, size);
    current_position_ += size;
  }

  void AppendAtTime(const base::TimeDelta& timestampOffset,
                    const uint8* pData, int size) {
    CHECK(chunk_demuxer_->SetTimestampOffset(kSourceId, timestampOffset));
    chunk_demuxer_->AppendData(kSourceId, pData, size);
    CHECK(chunk_demuxer_->SetTimestampOffset(kSourceId, base::TimeDelta()));
  }

  void EndOfStream() {
    chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
  }

  void Abort() {
    if (!chunk_demuxer_)
      return;
    chunk_demuxer_->Shutdown();
    chunk_demuxer_ = NULL;
  }

  void DemuxerOpened() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&MockMediaSource::DemuxerOpenedTask,
                              base::Unretained(this)));
  }

  void DemuxerOpenedTask() {
    // This code assumes that |mimetype_| is one of the following forms.
    // 1. audio/mpeg
    // 2. video/webm;codec="vorbis,vp8".
    size_t semicolon = mimetype_.find(";");
    std::string type = mimetype_;
    std::vector<std::string> codecs;
    if (semicolon != std::string::npos) {
      type = mimetype_.substr(0, semicolon);
      size_t codecs_param_start = mimetype_.find("codecs=\"", semicolon);

      CHECK_NE(codecs_param_start, std::string::npos);

      codecs_param_start += 8; // Skip over the codecs=".

      size_t codecs_param_end = mimetype_.find("\"", codecs_param_start);

      CHECK_NE(codecs_param_end, std::string::npos);

      std::string codecs_param =
          mimetype_.substr(codecs_param_start,
                           codecs_param_end - codecs_param_start);
      Tokenize(codecs_param, ",", &codecs);
    }

    CHECK_EQ(chunk_demuxer_->AddId(kSourceId, type, codecs), ChunkDemuxer::kOk);
    AppendData(initial_append_size_);
  }

  void DemuxerNeedKey(const std::string& type,
                      const std::vector<uint8>& init_data) {
    DCHECK(!init_data.empty());
    CHECK(!need_key_cb_.is_null());
    need_key_cb_.Run(std::string(), type, init_data);
  }

  scoped_ptr<TextTrack> OnTextTrack(TextKind kind,
                                    const std::string& label,
                                    const std::string& language) {
    return scoped_ptr<TextTrack>();
  }

 private:
  base::FilePath file_path_;
  scoped_refptr<DecoderBuffer> file_data_;
  int current_position_;
  int initial_append_size_;
  std::string mimetype_;
  ChunkDemuxer* chunk_demuxer_;
  scoped_ptr<Demuxer> owned_chunk_demuxer_;
  NeedKeyCB need_key_cb_;
};

class PipelineIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  void StartPipelineWithMediaSource(MockMediaSource* source) {
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata))
        .Times(AtMost(1));
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted))
        .Times(AtMost(1));
    pipeline_->Start(
        CreateFilterCollection(source->GetDemuxer(), NULL),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK),
        base::Bind(&PipelineIntegrationTest::OnBufferingState,
                   base::Unretained(this)),
        base::Closure());

    message_loop_.Run();
  }

  void StartPipelineWithEncryptedMedia(
      MockMediaSource* source,
      FakeEncryptedMedia* encrypted_media) {
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata))
        .Times(AtMost(1));
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted))
        .Times(AtMost(1));
    pipeline_->Start(
        CreateFilterCollection(source->GetDemuxer(),
                               encrypted_media->decryptor()),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK),
        base::Bind(&PipelineIntegrationTest::OnBufferingState,
                   base::Unretained(this)),
        base::Closure());

    source->set_need_key_cb(base::Bind(&FakeEncryptedMedia::NeedKey,
                                       base::Unretained(encrypted_media)));

    message_loop_.Run();
  }

  // Verifies that seeking works properly for ChunkDemuxer when the
  // seek happens while there is a pending read on the ChunkDemuxer
  // and no data is available.
  bool TestSeekDuringRead(const std::string& filename,
                          const std::string& mimetype,
                          int initial_append_size,
                          base::TimeDelta start_seek_time,
                          base::TimeDelta seek_time,
                          int seek_file_position,
                          int seek_append_size) {
    MockMediaSource source(filename, mimetype, initial_append_size);
    StartPipelineWithMediaSource(&source);

    if (pipeline_status_ != PIPELINE_OK)
      return false;

    Play();
    if (!WaitUntilCurrentTimeIsAfter(start_seek_time))
      return false;

    source.Seek(seek_time, seek_file_position, seek_append_size);
    if (!Seek(seek_time))
      return false;

    source.EndOfStream();

    source.Abort();
    Stop();
    return true;
  }
};

TEST_F(PipelineIntegrationTest, BasicPlayback) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-320x240.webm"), PIPELINE_OK));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackHashed) {
  ASSERT_TRUE(Start(
      GetTestDataFilePath("bear-320x240.webm"), PIPELINE_OK, kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_EQ("f0be120a90a811506777c99a2cdf7cc1", GetVideoHash());
  EXPECT_EQ("-3.59,-2.06,-0.43,2.15,0.77,-0.95,", GetAudioHash());
}

TEST_F(PipelineIntegrationTest, AudioPlaybackBenchmark) {
  // Audio-only files are all that is allowed for clockless playback.
  // Audio file can be specified on the command line
  // (--benchmark-audio-file=id3_png_test.mp3), so check for it.
  std::string filename(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      kBenchmarkAudioFile));
  if (filename.empty())
    filename = "sfx_f32le.wav";

  ASSERT_TRUE(Start(GetTestDataFilePath(filename), PIPELINE_OK, kClockless));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  // Call Stop() to ensure that the rendering is complete.
  Stop();
  printf("Clockless playback of %s took %.2f ms.\n",
         filename.c_str(),
         GetAudioTime().InMillisecondsF());
}

TEST_F(PipelineIntegrationTest, F32PlaybackHashed) {
  ASSERT_TRUE(
      Start(GetTestDataFilePath("sfx_f32le.wav"), PIPELINE_OK, kHashed));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_EQ(std::string(kNullVideoHash), GetVideoHash());
  EXPECT_EQ("3.03,2.86,2.99,3.31,3.57,4.06,", GetAudioHash());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackEncrypted) {
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  set_need_key_cb(base::Bind(&FakeEncryptedMedia::NeedKey,
                             base::Unretained(&encrypted_media)));

  ASSERT_TRUE(Start(GetTestDataFilePath("bear-320x240-av_enc-av.webm"),
                    encrypted_media.decryptor()));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource) {
  MockMediaSource source("bear-320x240.webm", kWebM, 219229);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// TODO(fgalligan): Enable after new vp9 files are landed.
// http://crbug.com/259116
TEST_F(PipelineIntegrationTest,
       DISABLED_BasicPlayback_MediaSource_VideoOnly_VP9_WebM) {
  MockMediaSource source("bear-vp9.webm", kWebMVP9, 32393);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kVP9WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_VP8A_WebM) {
  EXPECT_CALL(*this, OnSetOpaque(false)).Times(AnyNumber());
  MockMediaSource source("bear-vp8a.webm", kVideoOnlyWebM, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kVP8AWebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_Opus_WebM) {
  EXPECT_CALL(*this, OnSetOpaque(false)).Times(AnyNumber());
  MockMediaSource source("bear-opus-end-trimming.webm", kOpusAudioOnlyWebM,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kOpusEndTrimmingWebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());
  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_EQ(kOpusEndTrimmingWebMFileAudioBytes,
            pipeline_->GetStatistics().audio_bytes_decoded);
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect.webm", kWebM,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k640WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_Encrypted_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect-av_enc-av.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360-av_enc-av.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k640WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// Config changes from encrypted to clear are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_ClearThenEncrypted_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360-av_enc-av.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  message_loop_.Run();
  EXPECT_EQ(PIPELINE_ERROR_DECODE, pipeline_status_);

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

// Config changes from clear to encrypted are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_EncryptedThenClear_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect-av_enc-av.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(PipelineIntegrationTest, MediaSource_MP3) {
  MockMediaSource source("sfx.mp3", kMP3, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
}


TEST_F(PipelineIntegrationTest, MediaSource_MP3_Icecast) {
  MockMediaSource source("icy_sfx.mp3", kMP3, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_MP4) {
  MockMediaSource source("bear-640x360-av_frag.mp4", kMP4, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k1280IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_Encrypted_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-640x360-v_frag-cenc.mp4",
                         kMP4Video, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k1280IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// Config changes from clear to encrypted are not currently supported.
// TODO(ddorwin): Figure out why this CHECKs in AppendAtTime().
TEST_F(PipelineIntegrationTest,
       DISABLED_MediaSource_ConfigChange_ClearThenEncrypted_MP4_CENC) {
  MockMediaSource source("bear-640x360-av_frag.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  message_loop_.Run();
  EXPECT_EQ(PIPELINE_ERROR_DECODE, pipeline_status_);

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k640IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

// Config changes from encrypted to clear are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_EncryptedThenClear_MP4_CENC) {
  MockMediaSource source("bear-640x360-v_frag-cenc.mp4",
                         kMP4Video, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k640IsoCencFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

// Verify files which change configuration midstream fail gracefully.
TEST_F(PipelineIntegrationTest, MidStreamConfigChangesFail) {
  ASSERT_TRUE(Start(
      GetTestDataFilePath("midstream_config_change.mp3"), PIPELINE_OK));
  Play();
  ASSERT_EQ(WaitUntilEndedOrError(), PIPELINE_ERROR_DECODE);
}

#endif

TEST_F(PipelineIntegrationTest, BasicPlayback_16x9AspectRatio) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-320x240-16x9-aspect.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av.webm", kWebM, 219816);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_ClearStart_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av_clear-1s.webm",
                         kWebM, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av_clear-all.webm",
                         kWebM, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-1280x720-v_frag-cenc.mp4",
                         kMP4Video, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_AudioOnly) {
  MockMediaSource source("bear-1280x720-a_frag-cenc.mp4",
                         kMP4Audio, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-1280x720-v_frag-cenc_clear-all.mp4",
                         kMP4Video, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       EncryptedPlayback_NoEncryptedFrames_MP4_CENC_AudioOnly) {
  MockMediaSource source("bear-1280x720-a_frag-cenc_clear-all.mp4",
                         kMP4Audio, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}
#endif

// TODO(acolwell): Fix flakiness http://crbug.com/117921
TEST_F(PipelineIntegrationTest, DISABLED_SeekWhilePaused) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-320x240.webm"), PIPELINE_OK));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// TODO(acolwell): Fix flakiness http://crbug.com/117921
TEST_F(PipelineIntegrationTest, DISABLED_SeekWhilePlaying) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-320x240.webm"), PIPELINE_OK));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify audio decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_AudioOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-audio-only.webm", kAudioOnlyWebM,
                                 8192,
                                 base::TimeDelta::FromMilliseconds(464),
                                 base::TimeDelta::FromMilliseconds(617),
                                 0x10CA, 19730));
}

// Verify video decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_VideoOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-video-only.webm", kVideoOnlyWebM,
                                 32768,
                                 base::TimeDelta::FromMilliseconds(200),
                                 base::TimeDelta::FromMilliseconds(1668),
                                 0x1C896, 65536));
}

// Verify that Opus audio in WebM containers can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_AudioOnly_Opus_WebM) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-opus.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP9 video in WebM containers can be played back.
// TODO(fgalligan): Enable after new vp9 files are landed.
// http://crbug.com/259116
TEST_F(PipelineIntegrationTest, DISABLED_BasicPlayback_VideoOnly_VP9_WebM) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-vp9.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP9 video and Opus audio in the same WebM container can be played
// back.
// TODO(fgalligan): Enable after new vp9 files are landed.
// http://crbug.com/259116
TEST_F(PipelineIntegrationTest, DISABLED_BasicPlayback_VP9_Opus_WebM) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-vp9-opus.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP8 video with alpha channel can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP8A_WebM) {
  EXPECT_CALL(*this, OnSetOpaque(false)).Times(AnyNumber());
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-vp8a.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_EQ(last_video_frame_format_, VideoFrame::YV12A);
}

// Verify that VP8 video with inband text track can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP8_WebVTT_WebM) {
  ASSERT_TRUE(Start(GetTestDataFilePath("bear-vp8-webvtt.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

}  // namespace media
