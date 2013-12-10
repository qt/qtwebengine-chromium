// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/filters/chunk_demuxer.h"
#include "media/webm/cluster_builder.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_crypto_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace media {

static const uint8 kTracksHeader[] = {
  0x16, 0x54, 0xAE, 0x6B,  // Tracks ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // tracks(size = 0)
};

// WebM Block bytes that represent a VP8 keyframe.
static const uint8 kVP8Keyframe[] = {
  0x010, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x00, 0x10, 0x00, 0x10, 0x00
};

// WebM Block bytes that represent a VP8 interframe.
static const uint8 kVP8Interframe[] = { 0x11, 0x00, 0x00 };

static const int kTracksHeaderSize = sizeof(kTracksHeader);
static const int kTracksSizeOffset = 4;

// The size of TrackEntry element in test file "webm_vorbis_track_entry" starts
// at index 1 and spans 8 bytes.
static const int kAudioTrackSizeOffset = 1;
static const int kAudioTrackSizeWidth = 8;
static const int kAudioTrackEntryHeaderSize = kAudioTrackSizeOffset +
                                              kAudioTrackSizeWidth;

// The size of TrackEntry element in test file "webm_vp8_track_entry" starts at
// index 1 and spans 8 bytes.
static const int kVideoTrackSizeOffset = 1;
static const int kVideoTrackSizeWidth = 8;
static const int kVideoTrackEntryHeaderSize = kVideoTrackSizeOffset +
                                              kVideoTrackSizeWidth;

static const int kVideoTrackNum = 1;
static const int kAudioTrackNum = 2;

static const int kAudioBlockDuration = 23;
static const int kVideoBlockDuration = 33;
static const int kBlockSize = 10;

static const char kSourceId[] = "SourceId";
static const char kDefaultFirstClusterRange[] = "{ [0,46) }";
static const int kDefaultFirstClusterEndTimestamp = 66;
static const int kDefaultSecondClusterEndTimestamp = 132;

base::TimeDelta kDefaultDuration() {
  return base::TimeDelta::FromMilliseconds(201224);
}

// Write an integer into buffer in the form of vint that spans 8 bytes.
// The data pointed by |buffer| should be at least 8 bytes long.
// |number| should be in the range 0 <= number < 0x00FFFFFFFFFFFFFF.
static void WriteInt64(uint8* buffer, int64 number) {
  DCHECK(number >= 0 && number < GG_LONGLONG(0x00FFFFFFFFFFFFFF));
  buffer[0] = 0x01;
  int64 tmp = number;
  for (int i = 7; i > 0; i--) {
    buffer[i] = tmp & 0xff;
    tmp >>= 8;
  }
}

MATCHER_P(HasTimestamp, timestamp_in_ms, "") {
  return arg.get() && !arg->end_of_stream() &&
         arg->timestamp().InMilliseconds() == timestamp_in_ms;
}

MATCHER(IsEndOfStream, "") { return arg.get() && arg->end_of_stream(); }

static void OnReadDone(const base::TimeDelta& expected_time,
                       bool* called,
                       DemuxerStream::Status status,
                       const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);
  EXPECT_EQ(expected_time, buffer->timestamp());
  *called = true;
}

static void OnReadDone_AbortExpected(
    bool* called, DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kAborted);
  EXPECT_EQ(NULL, buffer.get());
  *called = true;
}

static void OnReadDone_EOSExpected(bool* called,
                                   DemuxerStream::Status status,
                                   const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);
  EXPECT_TRUE(buffer->end_of_stream());
  *called = true;
}

static void OnSeekDone_OKExpected(bool* called, PipelineStatus status) {
  EXPECT_EQ(status, PIPELINE_OK);
  *called = true;
}

class ChunkDemuxerTest : public testing::Test {
 protected:
  enum CodecsIndex {
    AUDIO,
    VIDEO,
    MAX_CODECS_INDEX
  };

  // Default cluster to append first for simple tests.
  scoped_ptr<Cluster> kDefaultFirstCluster() {
    return GenerateCluster(0, 4);
  }

  // Default cluster to append after kDefaultFirstCluster()
  // has been appended. This cluster starts with blocks that
  // have timestamps consistent with the end times of the blocks
  // in kDefaultFirstCluster() so that these two clusters represent
  // a continuous region.
  scoped_ptr<Cluster> kDefaultSecondCluster() {
    return GenerateCluster(46, 66, 5);
  }

  ChunkDemuxerTest() {
    CreateNewDemuxer();
  }

  void CreateNewDemuxer() {
    base::Closure open_cb =
        base::Bind(&ChunkDemuxerTest::DemuxerOpened, base::Unretained(this));
    Demuxer::NeedKeyCB need_key_cb =
        base::Bind(&ChunkDemuxerTest::DemuxerNeedKey, base::Unretained(this));
    AddTextTrackCB add_text_track_cb =
        base::Bind(&ChunkDemuxerTest::OnTextTrack, base::Unretained(this));
    demuxer_.reset(new ChunkDemuxer(open_cb, need_key_cb,
                                    add_text_track_cb, LogCB()));
  }

  virtual ~ChunkDemuxerTest() {
    ShutdownDemuxer();
  }

  void CreateInitSegment(bool has_audio, bool has_video,
                         bool is_audio_encrypted, bool is_video_encrypted,
                         scoped_ptr<uint8[]>* buffer,
                         int* size) {
    scoped_refptr<DecoderBuffer> ebml_header;
    scoped_refptr<DecoderBuffer> info;
    scoped_refptr<DecoderBuffer> audio_track_entry;
    scoped_refptr<DecoderBuffer> video_track_entry;
    scoped_refptr<DecoderBuffer> audio_content_encodings;
    scoped_refptr<DecoderBuffer> video_content_encodings;

    ebml_header = ReadTestDataFile("webm_ebml_element");

    info = ReadTestDataFile("webm_info_element");

    int tracks_element_size = 0;

    if (has_audio) {
      audio_track_entry = ReadTestDataFile("webm_vorbis_track_entry");
      tracks_element_size += audio_track_entry->data_size();
      if (is_audio_encrypted) {
        audio_content_encodings = ReadTestDataFile("webm_content_encodings");
        tracks_element_size += audio_content_encodings->data_size();
      }
    }

    if (has_video) {
      video_track_entry = ReadTestDataFile("webm_vp8_track_entry");
      tracks_element_size += video_track_entry->data_size();
      if (is_video_encrypted) {
        video_content_encodings = ReadTestDataFile("webm_content_encodings");
        tracks_element_size += video_content_encodings->data_size();
      }
    }

    *size = ebml_header->data_size() + info->data_size() +
        kTracksHeaderSize + tracks_element_size;

    buffer->reset(new uint8[*size]);

    uint8* buf = buffer->get();
    memcpy(buf, ebml_header->data(), ebml_header->data_size());
    buf += ebml_header->data_size();

    memcpy(buf, info->data(), info->data_size());
    buf += info->data_size();

    memcpy(buf, kTracksHeader, kTracksHeaderSize);
    WriteInt64(buf + kTracksSizeOffset, tracks_element_size);
    buf += kTracksHeaderSize;

    // TODO(xhwang): Simplify this! Probably have test data files that contain
    // ContentEncodings directly instead of trying to create one at run-time.
    if (has_audio) {
      memcpy(buf, audio_track_entry->data(),
             audio_track_entry->data_size());
      if (is_audio_encrypted) {
        memcpy(buf + audio_track_entry->data_size(),
               audio_content_encodings->data(),
               audio_content_encodings->data_size());
        WriteInt64(buf + kAudioTrackSizeOffset,
                   audio_track_entry->data_size() +
                   audio_content_encodings->data_size() -
                   kAudioTrackEntryHeaderSize);
        buf += audio_content_encodings->data_size();
      }
      buf += audio_track_entry->data_size();
    }

    if (has_video) {
      memcpy(buf, video_track_entry->data(),
             video_track_entry->data_size());
      if (is_video_encrypted) {
        memcpy(buf + video_track_entry->data_size(),
               video_content_encodings->data(),
               video_content_encodings->data_size());
        WriteInt64(buf + kVideoTrackSizeOffset,
                   video_track_entry->data_size() +
                   video_content_encodings->data_size() -
                   kVideoTrackEntryHeaderSize);
        buf += video_content_encodings->data_size();
      }
      buf += video_track_entry->data_size();
    }
  }

  ChunkDemuxer::Status AddId() {
    return AddId(kSourceId, true, true);
  }

  ChunkDemuxer::Status AddId(const std::string& source_id,
                             bool has_audio, bool has_video) {
    std::vector<std::string> codecs;
    std::string type;

    if (has_audio) {
      codecs.push_back("vorbis");
      type = "audio/webm";
    }

    if (has_video) {
      codecs.push_back("vp8");
      type = "video/webm";
    }

    if (!has_audio && !has_video) {
      return AddId(kSourceId, true, true);
    }

    return demuxer_->AddId(source_id, type, codecs);
  }

  void AppendData(const uint8* data, size_t length) {
    AppendData(kSourceId, data, length);
  }

  void AppendCluster(const std::string& source_id,
                     scoped_ptr<Cluster> cluster) {
    AppendData(source_id, cluster->data(), cluster->size());
  }

  void AppendCluster(scoped_ptr<Cluster> cluster) {
    AppendCluster(kSourceId, cluster.Pass());
  }

  void AppendCluster(int timecode, int block_count) {
    AppendCluster(GenerateCluster(timecode, block_count));
  }

  void AppendSingleStreamCluster(const std::string& source_id, int track_number,
                                 int timecode, int block_count) {
    int block_duration = 0;
    switch(track_number) {
      case kVideoTrackNum:
        block_duration = kVideoBlockDuration;
        break;
      case kAudioTrackNum:
        block_duration = kAudioBlockDuration;
        break;
    }
    ASSERT_NE(block_duration, 0);
    int end_timecode = timecode + block_count * block_duration;
    AppendCluster(source_id,
                  GenerateSingleStreamCluster(
                      timecode, end_timecode, track_number, block_duration));
  }

  void AppendSingleStreamCluster(const std::string& source_id, int track_number,
                                 const std::string& cluster_description) {
    std::vector<std::string> timestamps;
    base::SplitString(cluster_description, ' ', &timestamps);

    ClusterBuilder cb;
    std::vector<uint8> data(10);
    for (size_t i = 0; i < timestamps.size(); ++i) {
      std::string timestamp_str = timestamps[i];
      int block_flags = 0;
      if (EndsWith(timestamp_str, "K", true)) {
        block_flags = kWebMFlagKeyframe;
        // Remove the "K" off of the token.
        timestamp_str = timestamp_str.substr(0, timestamps[i].length() - 1);
      }
      int timestamp_in_ms;
      CHECK(base::StringToInt(timestamp_str, &timestamp_in_ms));

      if (i == 0)
        cb.SetClusterTimecode(timestamp_in_ms);

      cb.AddSimpleBlock(track_number, timestamp_in_ms, block_flags,
                        &data[0], data.size());
    }
    AppendCluster(source_id, cb.Finish());
  }

  void AppendData(const std::string& source_id,
                  const uint8* data, size_t length) {
    EXPECT_CALL(host_, AddBufferedTimeRange(_, _)).Times(AnyNumber());
    demuxer_->AppendData(source_id, data, length);
  }

  void AppendDataInPieces(const uint8* data, size_t length) {
    AppendDataInPieces(data, length, 7);
  }

  void AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      AppendData(start, append_size);
      start += append_size;
    }
  }

  void AppendInitSegment(bool has_audio, bool has_video) {
    AppendInitSegmentWithSourceId(kSourceId, has_audio, has_video);
  }

  void AppendInitSegmentWithSourceId(const std::string& source_id,
                                     bool has_audio, bool has_video) {
    AppendInitSegmentWithEncryptedInfo(
        source_id, has_audio, has_video, false, false);
  }

  void AppendInitSegmentWithEncryptedInfo(const std::string& source_id,
                                          bool has_audio, bool has_video,
                                          bool is_audio_encrypted,
                                          bool is_video_encrypted) {
    scoped_ptr<uint8[]> info_tracks;
    int info_tracks_size = 0;
    CreateInitSegment(has_audio, has_video,
                      is_audio_encrypted, is_video_encrypted,
                      &info_tracks, &info_tracks_size);
    AppendData(source_id, info_tracks.get(), info_tracks_size);
  }

  void AppendGarbage() {
    // Fill up an array with gibberish.
    int garbage_cluster_size = 10;
    scoped_ptr<uint8[]> garbage_cluster(new uint8[garbage_cluster_size]);
    for (int i = 0; i < garbage_cluster_size; ++i)
      garbage_cluster[i] = i;
    AppendData(garbage_cluster.get(), garbage_cluster_size);
  }

  void InitDoneCalled(PipelineStatus expected_status,
                      PipelineStatus status) {
    EXPECT_EQ(status, expected_status);
  }

  void AppendEmptyCluster(int timecode) {
    AppendCluster(GenerateEmptyCluster(timecode));
  }

  PipelineStatusCB CreateInitDoneCB(const base::TimeDelta& expected_duration,
                                    PipelineStatus expected_status) {
    if (expected_duration != kNoTimestamp())
      EXPECT_CALL(host_, SetDuration(expected_duration));
    return CreateInitDoneCB(expected_status);
  }

  PipelineStatusCB CreateInitDoneCB(PipelineStatus expected_status) {
    return base::Bind(&ChunkDemuxerTest::InitDoneCalled,
                      base::Unretained(this),
                      expected_status);
  }

  bool InitDemuxer(bool has_audio, bool has_video) {
    return InitDemuxerWithEncryptionInfo(has_audio, has_video, false, false);
  }

  bool InitDemuxerWithEncryptionInfo(
      bool has_audio, bool has_video,
      bool is_audio_encrypted, bool is_video_encrypted) {
    PipelineStatus expected_status =
        (has_audio || has_video) ? PIPELINE_OK : DEMUXER_ERROR_COULD_NOT_OPEN;

    base::TimeDelta expected_duration = kNoTimestamp();
    if (expected_status == PIPELINE_OK)
      expected_duration = kDefaultDuration();

    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(expected_duration, expected_status));

    if (AddId(kSourceId, has_audio, has_video) != ChunkDemuxer::kOk)
      return false;

    AppendInitSegmentWithEncryptedInfo(
        kSourceId, has_audio, has_video,
        is_audio_encrypted, is_video_encrypted);
    return true;
  }

  bool InitDemuxerAudioAndVideoSources(const std::string& audio_id,
                                       const std::string& video_id) {
    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

    if (AddId(audio_id, true, false) != ChunkDemuxer::kOk)
      return false;
    if (AddId(video_id, false, true) != ChunkDemuxer::kOk)
      return false;

    AppendInitSegmentWithSourceId(audio_id, true, false);
    AppendInitSegmentWithSourceId(video_id, false, true);
    return true;
  }

  // Initializes the demuxer with data from 2 files with different
  // decoder configurations. This is used to test the decoder config change
  // logic.
  //
  // bear-320x240.webm VideoDecoderConfig returns 320x240 for its natural_size()
  // bear-640x360.webm VideoDecoderConfig returns 640x360 for its natural_size()
  // The resulting video stream returns data from each file for the following
  // time ranges.
  // bear-320x240.webm : [0-501)       [801-2737)
  // bear-640x360.webm :       [527-793)
  //
  // bear-320x240.webm AudioDecoderConfig returns 3863 for its extra_data_size()
  // bear-640x360.webm AudioDecoderConfig returns 3935 for its extra_data_size()
  // The resulting audio stream returns data from each file for the following
  // time ranges.
  // bear-320x240.webm : [0-524)       [779-2737)
  // bear-640x360.webm :       [527-759)
  bool InitDemuxerWithConfigChangeData() {
    scoped_refptr<DecoderBuffer> bear1 = ReadTestDataFile("bear-320x240.webm");
    scoped_refptr<DecoderBuffer> bear2 = ReadTestDataFile("bear-640x360.webm");

    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(base::TimeDelta::FromMilliseconds(2744),
                                 PIPELINE_OK));

    if (AddId(kSourceId, true, true) != ChunkDemuxer::kOk)
      return false;

    // Append the whole bear1 file.
    AppendData(bear1->data(), bear1->data_size());
    CheckExpectedRanges(kSourceId, "{ [0,2737) }");

    // Append initialization segment for bear2.
    // Note: Offsets here and below are derived from
    // media/test/data/bear-640x360-manifest.js and
    // media/test/data/bear-320x240-manifest.js which were
    // generated from media/test/data/bear-640x360.webm and
    // media/test/data/bear-320x240.webm respectively.
    AppendData(bear2->data(), 4340);

    // Append a media segment that goes from [0.527000, 1.014000).
    AppendData(bear2->data() + 55290, 18785);
    CheckExpectedRanges(kSourceId, "{ [0,1028) [1201,2737) }");

    // Append initialization segment for bear1 & fill gap with [779-1197)
    // segment.
    AppendData(bear1->data(), 4370);
    AppendData(bear1->data() + 72737, 28183);
    CheckExpectedRanges(kSourceId, "{ [0,2737) }");

    MarkEndOfStream(PIPELINE_OK);
    return true;
  }

  void ShutdownDemuxer() {
    if (demuxer_) {
      demuxer_->Shutdown();
      message_loop_.RunUntilIdle();
    }
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64 timecode) {
    uint8 data[] = { 0x00 };
    cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
  }

  scoped_ptr<Cluster> GenerateCluster(int timecode, int block_count) {
    return GenerateCluster(timecode, timecode, block_count);
  }

  void AddVideoBlockGroup(ClusterBuilder* cb, int track_num, int64 timecode,
                          int duration, int flags) {
    const uint8* data =
        (flags & kWebMFlagKeyframe) != 0 ? kVP8Keyframe : kVP8Interframe;
    int size = (flags & kWebMFlagKeyframe) != 0 ? sizeof(kVP8Keyframe) :
        sizeof(kVP8Interframe);
    cb->AddBlockGroup(track_num, timecode, duration, flags, data, size);
  }

  scoped_ptr<Cluster> GenerateCluster(int first_audio_timecode,
                                      int first_video_timecode,
                                      int block_count) {
    CHECK_GT(block_count, 0);

    int size = 10;
    scoped_ptr<uint8[]> data(new uint8[size]);

    ClusterBuilder cb;
    cb.SetClusterTimecode(std::min(first_audio_timecode, first_video_timecode));

    if (block_count == 1) {
      cb.AddBlockGroup(kAudioTrackNum, first_audio_timecode,
                       kAudioBlockDuration, kWebMFlagKeyframe,
                       data.get(), size);
      return cb.Finish();
    }

    int audio_timecode = first_audio_timecode;
    int video_timecode = first_video_timecode;

    // Create simple blocks for everything except the last 2 blocks.
    // The first video frame must be a keyframe.
    uint8 video_flag = kWebMFlagKeyframe;
    for (int i = 0; i < block_count - 2; i++) {
      if (audio_timecode <= video_timecode) {
        cb.AddSimpleBlock(kAudioTrackNum, audio_timecode, kWebMFlagKeyframe,
                          data.get(), size);
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      cb.AddSimpleBlock(kVideoTrackNum, video_timecode, video_flag, data.get(),
                        size);
      video_timecode += kVideoBlockDuration;
      video_flag = 0;
    }

    // Make the last 2 blocks BlockGroups so that they don't get delayed by the
    // block duration calculation logic.
    if (audio_timecode <= video_timecode) {
      cb.AddBlockGroup(kAudioTrackNum, audio_timecode, kAudioBlockDuration,
                       kWebMFlagKeyframe, data.get(), size);
      AddVideoBlockGroup(&cb, kVideoTrackNum, video_timecode,
                         kVideoBlockDuration, video_flag);
    } else {
      AddVideoBlockGroup(&cb, kVideoTrackNum, video_timecode,
                         kVideoBlockDuration, video_flag);
      cb.AddBlockGroup(kAudioTrackNum, audio_timecode, kAudioBlockDuration,
                       kWebMFlagKeyframe, data.get(), size);
    }

    return cb.Finish();
  }

  scoped_ptr<Cluster> GenerateSingleStreamCluster(int timecode,
                                                  int end_timecode,
                                                  int track_number,
                                                  int block_duration) {
    CHECK_GT(end_timecode, timecode);

    std::vector<uint8> data(kBlockSize);

    ClusterBuilder cb;
    cb.SetClusterTimecode(timecode);

    // Create simple blocks for everything except the last block.
    for (int i = 0; timecode < (end_timecode - block_duration); i++) {
      cb.AddSimpleBlock(track_number, timecode, kWebMFlagKeyframe,
                        &data[0], data.size());
      timecode += block_duration;
    }

    // Make the last block a BlockGroup so that it doesn't get delayed by the
    // block duration calculation logic.
    if (track_number == kVideoTrackNum) {
      AddVideoBlockGroup(&cb, track_number, timecode, block_duration,
                         kWebMFlagKeyframe);
    } else {
      cb.AddBlockGroup(track_number, timecode, block_duration,
                       kWebMFlagKeyframe, &data[0], data.size());
    }
    return cb.Finish();
  }

  void Read(DemuxerStream::Type type, const DemuxerStream::ReadCB& read_cb) {
    demuxer_->GetStream(type)->Read(read_cb);
    message_loop_.RunUntilIdle();
  }

  void ReadAudio(const DemuxerStream::ReadCB& read_cb) {
    Read(DemuxerStream::AUDIO, read_cb);
  }

  void ReadVideo(const DemuxerStream::ReadCB& read_cb) {
    Read(DemuxerStream::VIDEO, read_cb);
  }

  void GenerateExpectedReads(int timecode, int block_count) {
    GenerateExpectedReads(timecode, timecode, block_count);
  }

  void GenerateExpectedReads(int start_audio_timecode,
                             int start_video_timecode,
                             int block_count) {
    CHECK_GT(block_count, 0);

    if (block_count == 1) {
      ExpectRead(DemuxerStream::AUDIO, start_audio_timecode);
      return;
    }

    int audio_timecode = start_audio_timecode;
    int video_timecode = start_video_timecode;

    for (int i = 0; i < block_count; i++) {
      if (audio_timecode <= video_timecode) {
        ExpectRead(DemuxerStream::AUDIO, audio_timecode);
        audio_timecode += kAudioBlockDuration;
        continue;
      }

      ExpectRead(DemuxerStream::VIDEO, video_timecode);
      video_timecode += kVideoBlockDuration;
    }
  }

  void GenerateSingleStreamExpectedReads(int timecode,
                                         int block_count,
                                         DemuxerStream::Type type,
                                         int block_duration) {
    CHECK_GT(block_count, 0);
    int stream_timecode = timecode;

    for (int i = 0; i < block_count; i++) {
      ExpectRead(type, stream_timecode);
      stream_timecode += block_duration;
    }
  }

  void GenerateAudioStreamExpectedReads(int timecode, int block_count) {
    GenerateSingleStreamExpectedReads(
        timecode, block_count, DemuxerStream::AUDIO, kAudioBlockDuration);
  }

  void GenerateVideoStreamExpectedReads(int timecode, int block_count) {
    GenerateSingleStreamExpectedReads(
        timecode, block_count, DemuxerStream::VIDEO, kVideoBlockDuration);
  }

  scoped_ptr<Cluster> GenerateEmptyCluster(int timecode) {
    ClusterBuilder cb;
    cb.SetClusterTimecode(timecode);
    return cb.Finish();
  }

  void CheckExpectedRanges(const std::string& expected) {
    CheckExpectedRanges(kSourceId, expected);
  }

  void CheckExpectedRanges(const std::string&  id,
                           const std::string& expected) {
    Ranges<base::TimeDelta> r = demuxer_->GetBufferedRanges(id);

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      ss << "[" << r.start(i).InMilliseconds() << ","
         << r.end(i).InMilliseconds() << ") ";
    }
    ss << "}";
    EXPECT_EQ(ss.str(), expected);
  }

  MOCK_METHOD2(ReadDone, void(DemuxerStream::Status status,
                              const scoped_refptr<DecoderBuffer>&));

  void StoreStatusAndBuffer(DemuxerStream::Status* status_out,
                            scoped_refptr<DecoderBuffer>* buffer_out,
                            DemuxerStream::Status status,
                            const scoped_refptr<DecoderBuffer>& buffer) {
    *status_out = status;
    *buffer_out = buffer;
  }

  void ReadUntilNotOkOrEndOfStream(DemuxerStream::Type type,
                                   DemuxerStream::Status* status,
                                   base::TimeDelta* last_timestamp) {
    DemuxerStream* stream = demuxer_->GetStream(type);
    scoped_refptr<DecoderBuffer> buffer;

    *last_timestamp = kNoTimestamp();
    do {
      stream->Read(base::Bind(&ChunkDemuxerTest::StoreStatusAndBuffer,
                              base::Unretained(this), status, &buffer));
      base::MessageLoop::current()->RunUntilIdle();
      if (*status == DemuxerStream::kOk && !buffer->end_of_stream())
        *last_timestamp = buffer->timestamp();
    } while (*status == DemuxerStream::kOk && !buffer->end_of_stream());
  }

  void ExpectEndOfStream(DemuxerStream::Type type) {
    EXPECT_CALL(*this, ReadDone(DemuxerStream::kOk, IsEndOfStream()));
    demuxer_->GetStream(type)->Read(base::Bind(
        &ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void ExpectRead(DemuxerStream::Type type, int64 timestamp_in_ms) {
    EXPECT_CALL(*this, ReadDone(DemuxerStream::kOk,
                                HasTimestamp(timestamp_in_ms)));
    demuxer_->GetStream(type)->Read(base::Bind(
        &ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void ExpectConfigChanged(DemuxerStream::Type type) {
    EXPECT_CALL(*this, ReadDone(DemuxerStream::kConfigChanged, _));
    demuxer_->GetStream(type)->Read(base::Bind(
        &ChunkDemuxerTest::ReadDone, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void CheckExpectedBuffers(DemuxerStream* stream,
                            const std::string& expected) {
    std::vector<std::string> timestamps;
    base::SplitString(expected, ' ', &timestamps);
    std::stringstream ss;
    for (size_t i = 0; i < timestamps.size(); ++i) {
      DemuxerStream::Status status;
      scoped_refptr<DecoderBuffer> buffer;
      stream->Read(base::Bind(&ChunkDemuxerTest::StoreStatusAndBuffer,
                              base::Unretained(this), &status, &buffer));
      base::MessageLoop::current()->RunUntilIdle();
      if (status != DemuxerStream::kOk || buffer->end_of_stream())
        break;

      if (i > 0)
        ss << " ";
      ss << buffer->timestamp().InMilliseconds();
    }
    EXPECT_EQ(expected, ss.str());
  }

  MOCK_METHOD1(Checkpoint, void(int id));

  struct BufferTimestamps {
    int video_time_ms;
    int audio_time_ms;
  };
  static const int kSkip = -1;

  // Test parsing a WebM file.
  // |filename| - The name of the file in media/test/data to parse.
  // |timestamps| - The expected timestamps on the parsed buffers.
  //    a timestamp of kSkip indicates that a Read() call for that stream
  //    shouldn't be made on that iteration of the loop. If both streams have
  //    a kSkip then the loop will terminate.
  bool ParseWebMFile(const std::string& filename,
                     const BufferTimestamps* timestamps,
                     const base::TimeDelta& duration) {
    return ParseWebMFile(filename, timestamps, duration, true, true);
  }

  bool ParseWebMFile(const std::string& filename,
                     const BufferTimestamps* timestamps,
                     const base::TimeDelta& duration,
                     bool has_audio, bool has_video) {
    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(duration, PIPELINE_OK));

    if (AddId(kSourceId, has_audio, has_video) != ChunkDemuxer::kOk)
      return false;

    // Read a WebM file into memory and send the data to the demuxer.
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    AppendDataInPieces(buffer->data(), buffer->data_size(), 512);

    // Verify that the timestamps on the first few packets match what we
    // expect.
    for (size_t i = 0;
         (timestamps[i].audio_time_ms != kSkip ||
          timestamps[i].video_time_ms != kSkip);
         i++) {
      bool audio_read_done = false;
      bool video_read_done = false;

      if (timestamps[i].audio_time_ms != kSkip) {
        ReadAudio(base::Bind(&OnReadDone,
                             base::TimeDelta::FromMilliseconds(
                                 timestamps[i].audio_time_ms),
                             &audio_read_done));
        EXPECT_TRUE(audio_read_done);
      }

      if (timestamps[i].video_time_ms != kSkip) {
        ReadVideo(base::Bind(&OnReadDone,
                             base::TimeDelta::FromMilliseconds(
                                 timestamps[i].video_time_ms),
                             &video_read_done));
        EXPECT_TRUE(video_read_done);
      }
    }

    return true;
  }

  MOCK_METHOD0(DemuxerOpened, void());
  // TODO(xhwang): This is a workaround of the issue that move-only parameters
  // are not supported in mocked methods. Remove this when the issue is fixed
  // (http://code.google.com/p/googletest/issues/detail?id=395) or when we use
  // std::string instead of scoped_ptr<uint8[]> (http://crbug.com/130689).
  MOCK_METHOD3(NeedKeyMock, void(const std::string& type,
                                 const uint8* init_data, int init_data_size));
  void DemuxerNeedKey(const std::string& type,
                      const std::vector<uint8>& init_data) {
    const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
    NeedKeyMock(type, init_data_ptr, init_data.size());
  }

  scoped_ptr<TextTrack> OnTextTrack(TextKind kind,
                                    const std::string& label,
                                    const std::string& language) {
    return scoped_ptr<TextTrack>();
  }

  void Seek(base::TimeDelta seek_time) {
    demuxer_->StartWaitingForSeek(seek_time);
    demuxer_->Seek(seek_time, NewExpectedStatusCB(PIPELINE_OK));
    message_loop_.RunUntilIdle();
  }

  void MarkEndOfStream(PipelineStatus status) {
    demuxer_->MarkEndOfStream(status);
    message_loop_.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  MockDemuxerHost host_;

  scoped_ptr<ChunkDemuxer> demuxer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, Init) {
  // Test no streams, audio-only, video-only, and audio & video scenarios.
  // Audio and video streams can be encrypted or not encrypted.
  for (int i = 0; i < 16; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;
    bool is_audio_encrypted = (i & 0x4) != 0;
    bool is_video_encrypted = (i & 0x8) != 0;

    // No test on invalid combination.
    if ((!has_audio && is_audio_encrypted) ||
        (!has_video && is_video_encrypted)) {
      continue;
    }

    CreateNewDemuxer();

    if (is_audio_encrypted || is_video_encrypted) {
      int need_key_count = (is_audio_encrypted ? 1 : 0) +
                           (is_video_encrypted ? 1 : 0);
      EXPECT_CALL(*this, NeedKeyMock(kWebMEncryptInitDataType, NotNull(),
                                     DecryptConfig::kDecryptionKeySize))
          .Times(Exactly(need_key_count));
    }

    ASSERT_TRUE(InitDemuxerWithEncryptionInfo(
        has_audio, has_video, is_audio_encrypted, is_video_encrypted));

    DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
    if (has_audio) {
      ASSERT_TRUE(audio_stream);

      const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
      EXPECT_EQ(kCodecVorbis, config.codec());
      EXPECT_EQ(32, config.bits_per_channel());
      EXPECT_EQ(CHANNEL_LAYOUT_STEREO, config.channel_layout());
      EXPECT_EQ(44100, config.samples_per_second());
      EXPECT_TRUE(config.extra_data());
      EXPECT_GT(config.extra_data_size(), 0u);
      EXPECT_EQ(kSampleFormatPlanarF32, config.sample_format());
      EXPECT_EQ(is_audio_encrypted,
                audio_stream->audio_decoder_config().is_encrypted());
    } else {
      EXPECT_FALSE(audio_stream);
    }

    DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);
    if (has_video) {
      EXPECT_TRUE(video_stream);
      EXPECT_EQ(is_video_encrypted,
                video_stream->video_decoder_config().is_encrypted());
    } else {
      EXPECT_FALSE(video_stream);
    }

    ShutdownDemuxer();
    demuxer_.reset();
  }
}

// Make sure that the demuxer reports an error if Shutdown()
// is called before all the initialization segments are appended.
TEST_F(ChunkDemuxerTest, ShutdownBeforeAllInitSegmentsAppended) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(
          kDefaultDuration(), DEMUXER_ERROR_COULD_NOT_OPEN));

  EXPECT_EQ(AddId("audio", true, false), ChunkDemuxer::kOk);
  EXPECT_EQ(AddId("video", false, true), ChunkDemuxer::kOk);

  AppendInitSegmentWithSourceId("audio", true, false);
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, AppendDataAfterSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  InSequence s;

  EXPECT_CALL(*this, Checkpoint(1));

  Seek(base::TimeDelta::FromMilliseconds(46));

  EXPECT_CALL(*this, Checkpoint(2));

  Checkpoint(1);

  AppendCluster(kDefaultSecondCluster());

  message_loop_.RunUntilIdle();

  Checkpoint(2);
}

// Test that parsing errors are handled for clusters appended after init.
TEST_F(ChunkDemuxerTest, ErrorWhileParsingClusterAfterInit) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  AppendGarbage();
}

// Test the case where a Seek() is requested while the parser
// is in the middle of cluster. This is to verify that the parser
// does not reset itself on a seek.
TEST_F(ChunkDemuxerTest, SeekWhileParsingCluster) {
  ASSERT_TRUE(InitDemuxer(true, true));

  InSequence s;

  scoped_ptr<Cluster> cluster_a(GenerateCluster(0, 6));

  // Split the cluster into two appends at an arbitrary point near the end.
  int first_append_size = cluster_a->size() - 11;
  int second_append_size = cluster_a->size() - first_append_size;

  // Append the first part of the cluster.
  AppendData(cluster_a->data(), first_append_size);

  ExpectRead(DemuxerStream::AUDIO, 0);
  ExpectRead(DemuxerStream::VIDEO, 0);
  ExpectRead(DemuxerStream::AUDIO, kAudioBlockDuration);
  // Note: We skip trying to read a video buffer here because computing
  // the duration for this block relies on successfully parsing the last block
  // in the cluster the cluster.
  ExpectRead(DemuxerStream::AUDIO, 2 * kAudioBlockDuration);

  Seek(base::TimeDelta::FromSeconds(5));

  // Append the rest of the cluster.
  AppendData(cluster_a->data() + first_append_size, second_append_size);

  // Append the new cluster and verify that only the blocks
  // in the new cluster are returned.
  AppendCluster(GenerateCluster(5000, 6));
  GenerateExpectedReads(5000, 6);
}

// Test the case where AppendData() is called before Init().
TEST_F(ChunkDemuxerTest, AppendDataBeforeInit) {
  scoped_ptr<uint8[]> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(true, true, false, false, &info_tracks, &info_tracks_size);

  demuxer_->AppendData(kSourceId, info_tracks.get(), info_tracks_size);
}

// Make sure Read() callbacks are dispatched with the proper data.
TEST_F(ChunkDemuxerTest, Read) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, OutOfOrderClusters) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());
  AppendCluster(GenerateCluster(10, 4));

  // Make sure that AppendCluster() does not fail with a cluster that has
  // overlaps with the previously appended cluster.
  AppendCluster(GenerateCluster(5, 4));

  // Verify that AppendData() can still accept more data.
  scoped_ptr<Cluster> cluster_c(GenerateCluster(45, 2));
  demuxer_->AppendData(kSourceId, cluster_c->data(), cluster_c->size());
}

TEST_F(ChunkDemuxerTest, NonMonotonicButAboveClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  ClusterBuilder cb;

  // Test the case where block timecodes are not monotonically
  // increasing but stay above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  AppendCluster(cb.Finish());

  // Verify that AppendData() ignores data after the error.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(20, 2));
  demuxer_->AppendData(kSourceId, cluster_b->data(), cluster_b->size());
}

TEST_F(ChunkDemuxerTest, BackwardsAndBeforeClusterTimecode) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  ClusterBuilder cb;

  // Test timecodes going backwards and including values less than the cluster
  // timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  AppendCluster(cb.Finish());

  // Verify that AppendData() ignores data after the error.
  scoped_ptr<Cluster> cluster_b(GenerateCluster(6, 2));
  demuxer_->AppendData(kSourceId, cluster_b->data(), cluster_b->size());
}


TEST_F(ChunkDemuxerTest, PerStreamMonotonicallyIncreasingTimestamps) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  ClusterBuilder cb;

  // Test monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 4);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  AppendCluster(cb.Finish());
}

// Test the case where a cluster is passed to AppendCluster() before
// INFO & TRACKS data.
TEST_F(ChunkDemuxerTest, ClusterBeforeInitSegment) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  AppendCluster(GenerateCluster(0, 1));
}

// Test cases where we get an MarkEndOfStream() call during initialization.
TEST_F(ChunkDemuxerTest, EOSDuringInit) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));
  MarkEndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, EndOfStreamWithNoAppend) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  CheckExpectedRanges("{ }");
  MarkEndOfStream(PIPELINE_OK);
  ShutdownDemuxer();
  CheckExpectedRanges("{ }");
  demuxer_->RemoveId(kSourceId);
  demuxer_.reset();
}

TEST_F(ChunkDemuxerTest, EndOfStreamWithNoMediaAppend) {
  ASSERT_TRUE(InitDemuxer(true, true));

  CheckExpectedRanges("{ }");
  MarkEndOfStream(PIPELINE_OK);
  CheckExpectedRanges("{ }");
}

TEST_F(ChunkDemuxerTest, DecodeErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  MarkEndOfStream(PIPELINE_ERROR_DECODE);
  CheckExpectedRanges(kDefaultFirstClusterRange);
}

TEST_F(ChunkDemuxerTest, NetworkErrorEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());
  CheckExpectedRanges(kDefaultFirstClusterRange);

  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_NETWORK));
  MarkEndOfStream(PIPELINE_ERROR_NETWORK);
}

// Helper class to reduce duplicate code when testing end of stream
// Read() behavior.
class EndOfStreamHelper {
 public:
  explicit EndOfStreamHelper(Demuxer* demuxer)
      : demuxer_(demuxer),
        audio_read_done_(false),
        video_read_done_(false) {
  }

  // Request a read on the audio and video streams.
  void RequestReads() {
    EXPECT_FALSE(audio_read_done_);
    EXPECT_FALSE(video_read_done_);

    DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
    DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

    audio->Read(base::Bind(&OnEndOfStreamReadDone, &audio_read_done_));
    video->Read(base::Bind(&OnEndOfStreamReadDone, &video_read_done_));
    base::MessageLoop::current()->RunUntilIdle();
  }

  // Check to see if |audio_read_done_| and |video_read_done_| variables
  // match |expected|.
  void CheckIfReadDonesWereCalled(bool expected) {
    base::MessageLoop::current()->RunUntilIdle();
    EXPECT_EQ(expected, audio_read_done_);
    EXPECT_EQ(expected, video_read_done_);
  }

 private:
  static void OnEndOfStreamReadDone(
      bool* called,
      DemuxerStream::Status status,
      const scoped_refptr<DecoderBuffer>& buffer) {
    EXPECT_EQ(status, DemuxerStream::kOk);
    EXPECT_TRUE(buffer->end_of_stream());
    *called = true;
  }

  Demuxer* demuxer_;
  bool audio_read_done_;
  bool video_read_done_;

  DISALLOW_COPY_AND_ASSIGN(EndOfStreamHelper);
};

// Make sure that all pending reads that we don't have media data for get an
// "end of stream" buffer when MarkEndOfStream() is called.
TEST_F(ChunkDemuxerTest, EndOfStreamWithPendingReads) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(GenerateCluster(0, 2));

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_2(demuxer_.get());

  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done_1));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &video_read_done_1));
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);

  end_of_stream_helper_1.RequestReads();

  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(kVideoBlockDuration)));
  MarkEndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);
}

// Make sure that all Read() calls after we get an MarkEndOfStream()
// call return an "end of stream" buffer.
TEST_F(ChunkDemuxerTest, ReadsAfterEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(GenerateCluster(0, 2));

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_2(demuxer_.get());
  EndOfStreamHelper end_of_stream_helper_3(demuxer_.get());

  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done_1));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &video_read_done_1));

  end_of_stream_helper_1.RequestReads();

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);

  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(kVideoBlockDuration)));
  MarkEndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  // Request a few more reads and make sure we immediately get
  // end of stream buffers.
  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_3.RequestReads();
  end_of_stream_helper_3.CheckIfReadDonesWereCalled(true);
}

TEST_F(ChunkDemuxerTest, EndOfStreamDuringCanceledSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(0, 10);
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(138)));
  MarkEndOfStream(PIPELINE_OK);

  // Start the first seek.
  Seek(base::TimeDelta::FromMilliseconds(20));

  // Simulate another seek being requested before the first
  // seek has finished prerolling.
  base::TimeDelta seek_time2 = base::TimeDelta::FromMilliseconds(30);
  demuxer_->CancelPendingSeek(seek_time2);

  // Finish second seek.
  Seek(seek_time2);

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  // Make sure audio can reach end of stream.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);

  // Make sure video can reach end of stream.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
}

// Make sure AppendData() will accept elements that span multiple calls.
TEST_F(ChunkDemuxerTest, AppendingInPieces) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  scoped_ptr<uint8[]> info_tracks;
  int info_tracks_size = 0;
  CreateInitSegment(true, true, false, false, &info_tracks, &info_tracks_size);

  scoped_ptr<Cluster> cluster_a(kDefaultFirstCluster());
  scoped_ptr<Cluster> cluster_b(kDefaultSecondCluster());

  size_t buffer_size = info_tracks_size + cluster_a->size() + cluster_b->size();
  scoped_ptr<uint8[]> buffer(new uint8[buffer_size]);
  uint8* dst = buffer.get();
  memcpy(dst, info_tracks.get(), info_tracks_size);
  dst += info_tracks_size;

  memcpy(dst, cluster_a->data(), cluster_a->size());
  dst += cluster_a->size();

  memcpy(dst, cluster_b->data(), cluster_b->size());
  dst += cluster_b->size();

  AppendDataInPieces(buffer.get(), buffer_size);

  GenerateExpectedReads(0, 9);
}

TEST_F(ChunkDemuxerTest, WebMFile_AudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {67, 6},
    {100, 9},
    {133, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744)));
}

TEST_F(ChunkDemuxerTest, WebMFile_LiveAudioAndVideo) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {67, 6},
    {100, 9},
    {133, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-live.webm", buffer_timestamps,
                            kInfiniteDuration()));
}

TEST_F(ChunkDemuxerTest, WebMFile_AudioOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
    {kSkip, 0},
    {kSkip, 3},
    {kSkip, 6},
    {kSkip, 9},
    {kSkip, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-audio-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2744),
                            true, false));
}

TEST_F(ChunkDemuxerTest, WebMFile_VideoOnly) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, kSkip},
    {33, kSkip},
    {67, kSkip},
    {100, kSkip},
    {133, kSkip},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-video-only.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2703),
                            false, true));
}

TEST_F(ChunkDemuxerTest, WebMFile_AltRefFrames) {
  struct BufferTimestamps buffer_timestamps[] = {
    {0, 0},
    {33, 3},
    {33, 6},
    {67, 9},
    {100, 12},
    {kSkip, kSkip},
  };

  ASSERT_TRUE(ParseWebMFile("bear-320x240-altref.webm", buffer_timestamps,
                            base::TimeDelta::FromMilliseconds(2767)));
}

// Verify that we output buffers before the entire cluster has been parsed.
TEST_F(ChunkDemuxerTest, IncrementalClusterParsing) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendEmptyCluster(0);

  scoped_ptr<Cluster> cluster(GenerateCluster(0, 6));

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append data one byte at a time until the audio read completes.
  int i = 0;
  for (; i < cluster->size() && !audio_read_done; ++i) {
    AppendData(cluster->data() + i, 1);
    message_loop_.RunUntilIdle();
  }

  EXPECT_TRUE(audio_read_done);
  EXPECT_FALSE(video_read_done);
  EXPECT_GT(i, 0);
  EXPECT_LT(i, cluster->size());

  // Append data one byte at a time until the video read completes.
  for (; i < cluster->size() && !video_read_done; ++i) {
    AppendData(cluster->data() + i, 1);
    message_loop_.RunUntilIdle();
  }

  EXPECT_TRUE(video_read_done);
  EXPECT_LT(i, cluster->size());

  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(23),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(33),
                       &video_read_done));

  // Make sure the reads haven't completed yet.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Append the remaining data.
  ASSERT_LT(i, cluster->size());
  AppendData(cluster->data() + i, cluster->size() - i);

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, ParseErrorDuringInit) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(
          kNoTimestamp(), DEMUXER_ERROR_COULD_NOT_OPEN));

  ASSERT_EQ(AddId(), ChunkDemuxer::kOk);

  uint8 tmp = 0;
  demuxer_->AppendData(kSourceId, &tmp, 1);
}

TEST_F(ChunkDemuxerTest, AVHeadersWithAudioOnlyType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp(),
                               DEMUXER_ERROR_COULD_NOT_OPEN));

  std::vector<std::string> codecs(1);
  codecs[0] = "vorbis";
  ASSERT_EQ(demuxer_->AddId(kSourceId, "audio/webm", codecs),
            ChunkDemuxer::kOk);

  AppendInitSegment(true, true);
}

TEST_F(ChunkDemuxerTest, AVHeadersWithVideoOnlyType) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kNoTimestamp(),
                               DEMUXER_ERROR_COULD_NOT_OPEN));

  std::vector<std::string> codecs(1);
  codecs[0] = "vp8";
  ASSERT_EQ(demuxer_->AddId(kSourceId, "video/webm", codecs),
            ChunkDemuxer::kOk);

  AppendInitSegment(true, true);
}

TEST_F(ChunkDemuxerTest, MultipleHeaders) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());

  // Append another identical initialization segment.
  AppendInitSegment(true, true);

  AppendCluster(kDefaultSecondCluster());

  GenerateExpectedReads(0, 9);
}

TEST_F(ChunkDemuxerTest, AddSeparateSourcesForAudioAndVideo) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Append audio and video data into separate source ids.
  AppendCluster(audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));
  GenerateAudioStreamExpectedReads(0, 4);
  AppendCluster(video_id,
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));
  GenerateVideoStreamExpectedReads(0, 4);
}

TEST_F(ChunkDemuxerTest, AddIdFailures) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  std::string audio_id = "audio1";
  std::string video_id = "video1";

  ASSERT_EQ(AddId(audio_id, true, false), ChunkDemuxer::kOk);

  // Adding an id with audio/video should fail because we already added audio.
  ASSERT_EQ(AddId(), ChunkDemuxer::kReachedIdLimit);

  AppendInitSegmentWithSourceId(audio_id, true, false);

  // Adding an id after append should fail.
  ASSERT_EQ(AddId(video_id, false, true), ChunkDemuxer::kReachedIdLimit);
}

// Test that Read() calls after a RemoveId() return "end of stream" buffers.
TEST_F(ChunkDemuxerTest, RemoveId) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Append audio and video data into separate source ids.
  AppendCluster(audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id,
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  // Read() from audio should return normal buffers.
  GenerateAudioStreamExpectedReads(0, 4);

  // Remove the audio id.
  demuxer_->RemoveId(audio_id);

  // Read() from audio should return "end of stream" buffers.
  bool audio_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_EOSExpected, &audio_read_done));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(audio_read_done);

  // Read() from video should still return normal buffers.
  GenerateVideoStreamExpectedReads(0, 4);
}

// Test that removing an ID immediately after adding it does not interfere with
// quota for new IDs in the future.
TEST_F(ChunkDemuxerTest, RemoveAndAddId) {
  std::string audio_id_1 = "audio1";
  ASSERT_TRUE(AddId(audio_id_1, true, false) == ChunkDemuxer::kOk);
  demuxer_->RemoveId(audio_id_1);

  std::string audio_id_2 = "audio2";
  ASSERT_TRUE(AddId(audio_id_2, true, false) == ChunkDemuxer::kOk);
}

TEST_F(ChunkDemuxerTest, SeekCanceled) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Append cluster at the beginning of the stream.
  AppendCluster(GenerateCluster(0, 4));

  // Seek to an unbuffered region.
  Seek(base::TimeDelta::FromSeconds(50));

  // Attempt to read in unbuffered area; should not fulfill the read.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_AbortExpected, &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_AbortExpected, &video_read_done));
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  // Now cancel the pending seek, which should flush the reads with empty
  // buffers.
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(0);
  demuxer_->CancelPendingSeek(seek_time);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // A seek back to the buffered region should succeed.
  Seek(seek_time);
  GenerateExpectedReads(0, 4);
}

TEST_F(ChunkDemuxerTest, SeekCanceledWhileWaitingForSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Append cluster at the beginning of the stream.
  AppendCluster(GenerateCluster(0, 4));

  // Start waiting for a seek.
  base::TimeDelta seek_time1 = base::TimeDelta::FromSeconds(50);
  base::TimeDelta seek_time2 = base::TimeDelta::FromSeconds(0);
  demuxer_->StartWaitingForSeek(seek_time1);

  // Now cancel the upcoming seek to an unbuffered region.
  demuxer_->CancelPendingSeek(seek_time2);
  demuxer_->Seek(seek_time1, NewExpectedStatusCB(PIPELINE_OK));

  // Read requests should be fulfilled with empty buffers.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_AbortExpected, &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_AbortExpected, &video_read_done));
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // A seek back to the buffered region should succeed.
  Seek(seek_time2);
  GenerateExpectedReads(0, 4);
}

// Test that Seek() successfully seeks to all source IDs.
TEST_F(ChunkDemuxerTest, SeekAudioAndVideoSources) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  AppendCluster(
      audio_id,
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(
      video_id,
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  // Read() should return buffers at 0.
  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(0),
                       &video_read_done));
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // Seek to 3 (an unbuffered region).
  Seek(base::TimeDelta::FromSeconds(3));

  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromSeconds(3),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromSeconds(3),
                       &video_read_done));
  // Read()s should not return until after data is appended at the Seek point.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  AppendCluster(audio_id,
                GenerateSingleStreamCluster(
                    3000, 3092, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id,
                GenerateSingleStreamCluster(
                    3000, 3132, kVideoTrackNum, kVideoBlockDuration));

  message_loop_.RunUntilIdle();

  // Read() should return buffers at 3.
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

// Test that Seek() completes successfully when EndOfStream
// is called before data is available for that seek point.
// This scenario might be useful if seeking past the end of stream
// of either audio or video (or both).
TEST_F(ChunkDemuxerTest, EndOfStreamAfterPastEosSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(GenerateSingleStreamCluster(0, 120, kAudioTrackNum, 10));
  AppendCluster(GenerateSingleStreamCluster(0, 100, kVideoTrackNum, 5));

  // Seeking past the end of video.
  // Note: audio data is available for that seek point.
  bool seek_cb_was_called = false;
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(110);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 base::Bind(OnSeekDone_OKExpected, &seek_cb_was_called));
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(120)));
  MarkEndOfStream(PIPELINE_OK);
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(seek_cb_was_called);

  ShutdownDemuxer();
}

// Test that EndOfStream is ignored if coming during a pending seek
// whose seek time is before some existing ranges.
TEST_F(ChunkDemuxerTest, EndOfStreamDuringPendingSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(GenerateSingleStreamCluster(0, 120, kAudioTrackNum, 10));
  AppendCluster(GenerateSingleStreamCluster(0, 100, kVideoTrackNum, 5));
  AppendCluster(GenerateSingleStreamCluster(200, 300, kAudioTrackNum, 10));
  AppendCluster(GenerateSingleStreamCluster(200, 300, kVideoTrackNum, 5));

  bool seek_cb_was_called = false;
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(160);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 base::Bind(OnSeekDone_OKExpected, &seek_cb_was_called));
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(300)));
  MarkEndOfStream(PIPELINE_OK);
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(seek_cb_was_called);

  demuxer_->UnmarkEndOfStream();

  AppendCluster(GenerateSingleStreamCluster(140, 180, kAudioTrackNum, 10));
  AppendCluster(GenerateSingleStreamCluster(140, 180, kVideoTrackNum, 5));

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(seek_cb_was_called);

  ShutdownDemuxer();
}

// Test ranges in an audio-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioIdOnly) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(kSourceId, true, false), ChunkDemuxer::kOk);
  AppendInitSegment(true, false);

  // Test a simple cluster.
  AppendCluster(
      GenerateSingleStreamCluster(0, 92, kAudioTrackNum, kAudioBlockDuration));

  CheckExpectedRanges("{ [0,92) }");

  // Append a disjoint cluster to check for two separate ranges.
  AppendCluster(GenerateSingleStreamCluster(
      150, 219, kAudioTrackNum, kAudioBlockDuration));

  CheckExpectedRanges("{ [0,92) [150,219) }");
}

// Test ranges in a video-only stream.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_VideoIdOnly) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(kDefaultDuration(), PIPELINE_OK));

  ASSERT_EQ(AddId(kSourceId, false, true), ChunkDemuxer::kOk);
  AppendInitSegment(false, true);

  // Test a simple cluster.
  AppendCluster(
      GenerateSingleStreamCluster(0, 132, kVideoTrackNum, kVideoBlockDuration));

  CheckExpectedRanges("{ [0,132) }");

  // Append a disjoint cluster to check for two separate ranges.
  AppendCluster(GenerateSingleStreamCluster(
      200, 299, kVideoTrackNum, kVideoBlockDuration));

  CheckExpectedRanges("{ [0,132) [200,299) }");
}

TEST_F(ChunkDemuxerTest, GetBufferedRanges_AudioVideo) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Audio: 0 -> 23
  // Video: 0 -> 33
  // Buffered Range: 0 -> 23
  // Audio block duration is smaller than video block duration,
  // so the buffered ranges should correspond to the audio blocks.
  AppendCluster(GenerateSingleStreamCluster(
      0, kAudioBlockDuration, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(GenerateSingleStreamCluster(
      0, kVideoBlockDuration, kVideoTrackNum, kVideoBlockDuration));

  CheckExpectedRanges("{ [0,23) }");

  // Audio: 300 -> 400
  // Video: 320 -> 420
  // Buffered Range: 320 -> 400  (end overlap)
  AppendCluster(GenerateSingleStreamCluster(300, 400, kAudioTrackNum, 50));
  AppendCluster(GenerateSingleStreamCluster(320, 420, kVideoTrackNum, 50));

  CheckExpectedRanges("{ [0,23) [320,400) }");

  // Audio: 520 -> 590
  // Video: 500 -> 570
  // Buffered Range: 520 -> 570  (front overlap)
  AppendCluster(GenerateSingleStreamCluster(520, 590, kAudioTrackNum, 70));
  AppendCluster(GenerateSingleStreamCluster(500, 570, kVideoTrackNum, 70));

  CheckExpectedRanges("{ [0,23) [320,400) [520,570) }");

  // Audio: 720 -> 750
  // Video: 700 -> 770
  // Buffered Range: 720 -> 750  (complete overlap, audio)
  AppendCluster(GenerateSingleStreamCluster(720, 750, kAudioTrackNum, 30));
  AppendCluster(GenerateSingleStreamCluster(700, 770, kVideoTrackNum, 70));

  CheckExpectedRanges("{ [0,23) [320,400) [520,570) [720,750) }");

  // Audio: 900 -> 970
  // Video: 920 -> 950
  // Buffered Range: 920 -> 950  (complete overlap, video)
  AppendCluster(GenerateSingleStreamCluster(900, 970, kAudioTrackNum, 70));
  AppendCluster(GenerateSingleStreamCluster(920, 950, kVideoTrackNum, 30));

  CheckExpectedRanges("{ [0,23) [320,400) [520,570) [720,750) [920,950) }");

  // Appending within buffered range should not affect buffered ranges.
  AppendCluster(GenerateSingleStreamCluster(930, 950, kAudioTrackNum, 20));
  CheckExpectedRanges("{ [0,23) [320,400) [520,570) [720,750) [920,950) }");

  // Appending to single stream outside buffered ranges should not affect
  // buffered ranges.
  AppendCluster(GenerateSingleStreamCluster(1230, 1240, kVideoTrackNum, 10));
  CheckExpectedRanges("{ [0,23) [320,400) [520,570) [720,750) [920,950) }");
}

// Once MarkEndOfStream() is called, GetBufferedRanges should not cut off any
// over-hanging tails at the end of the ranges as this is likely due to block
// duration differences.
TEST_F(ChunkDemuxerTest, GetBufferedRanges_EndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(GenerateSingleStreamCluster(0, 90, kAudioTrackNum, 90));
  AppendCluster(GenerateSingleStreamCluster(0, 100, kVideoTrackNum, 100));

  CheckExpectedRanges("{ [0,90) }");

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(100)));
  MarkEndOfStream(PIPELINE_OK);

  CheckExpectedRanges("{ [0,100) }");
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodes) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Create a cluster where the video timecode begins 25ms after the audio.
  AppendCluster(GenerateCluster(0, 25, 8));

  Seek(base::TimeDelta::FromSeconds(0));
  GenerateExpectedReads(0, 25, 8);

  // Seek to 5 seconds.
  Seek(base::TimeDelta::FromSeconds(5));

  // Generate a cluster to fulfill this seek, where audio timecode begins 25ms
  // after the video.
  AppendCluster(GenerateCluster(5025, 5000, 8));
  GenerateExpectedReads(5025, 5000, 8);
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodesSeparateSources) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Generate two streams where the video stream starts 5ms after the audio
  // stream and append them.
  AppendCluster(audio_id, GenerateSingleStreamCluster(
      25, 4 * kAudioBlockDuration + 25, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id, GenerateSingleStreamCluster(
      30, 4 * kVideoBlockDuration + 30, kVideoTrackNum, kVideoBlockDuration));

  // Both streams should be able to fulfill a seek to 25.
  Seek(base::TimeDelta::FromMilliseconds(25));
  GenerateAudioStreamExpectedReads(25, 4);
  GenerateVideoStreamExpectedReads(30, 4);
}

TEST_F(ChunkDemuxerTest, DifferentStreamTimecodesOutOfRange) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  // Generate two streams where the video stream starts 10s after the audio
  // stream and append them.
  AppendCluster(audio_id, GenerateSingleStreamCluster(0,
      4 * kAudioBlockDuration + 0, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id, GenerateSingleStreamCluster(10000,
      4 * kVideoBlockDuration + 10000, kVideoTrackNum, kVideoBlockDuration));

  // Should not be able to fulfill a seek to 0.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(0);
  demuxer_->StartWaitingForSeek(seek_time);
  demuxer_->Seek(seek_time,
                 NewExpectedStatusCB(PIPELINE_ERROR_ABORT));
  ExpectRead(DemuxerStream::AUDIO, 0);
  ExpectEndOfStream(DemuxerStream::VIDEO);
}

TEST_F(ChunkDemuxerTest, ClusterWithNoBuffers) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Generate and append an empty cluster beginning at 0.
  AppendEmptyCluster(0);

  // Sanity check that data can be appended after this cluster correctly.
  AppendCluster(GenerateCluster(0, 2));
  ExpectRead(DemuxerStream::AUDIO, 0);
  ExpectRead(DemuxerStream::VIDEO, 0);
}

TEST_F(ChunkDemuxerTest, CodecPrefixMatching) {
  ChunkDemuxer::Status expected = ChunkDemuxer::kNotSupported;

#if defined(USE_PROPRIETARY_CODECS)
  expected = ChunkDemuxer::kOk;
#endif

  std::vector<std::string> codecs;
  codecs.push_back("avc1.4D4041");

  EXPECT_EQ(demuxer_->AddId("source_id", "video/mp4", codecs), expected);
}

// Test codec ID's that are not compliant with RFC6381, but have been
// seen in the wild.
TEST_F(ChunkDemuxerTest, CodecIDsThatAreNotRFC6381Compliant) {
  ChunkDemuxer::Status expected = ChunkDemuxer::kNotSupported;

#if defined(USE_PROPRIETARY_CODECS)
  expected = ChunkDemuxer::kOk;
#endif
  const char* codec_ids[] = {
    // GPAC places leading zeros on the audio object type.
    "mp4a.40.02",
    "mp4a.40.05"
  };

  for (size_t i = 0; i < arraysize(codec_ids); ++i) {
    std::vector<std::string> codecs;
    codecs.push_back(codec_ids[i]);

    ChunkDemuxer::Status result =
        demuxer_->AddId("source_id", "audio/mp4", codecs);

    EXPECT_EQ(result, expected)
        << "Fail to add codec_id '" << codec_ids[i] << "'";

    if (result == ChunkDemuxer::kOk)
      demuxer_->RemoveId("source_id");
  }
}

TEST_F(ChunkDemuxerTest, EndOfStreamStillSetAfterSeek) {
  ASSERT_TRUE(InitDemuxer(true, true));

  EXPECT_CALL(host_, SetDuration(_))
      .Times(AnyNumber());

  base::TimeDelta kLastAudioTimestamp = base::TimeDelta::FromMilliseconds(92);
  base::TimeDelta kLastVideoTimestamp = base::TimeDelta::FromMilliseconds(99);

  AppendCluster(kDefaultFirstCluster());
  AppendCluster(kDefaultSecondCluster());
  MarkEndOfStream(PIPELINE_OK);

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  // Verify that we can read audio & video to the end w/o problems.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastAudioTimestamp, last_timestamp);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastVideoTimestamp, last_timestamp);

  // Seek back to 0 and verify that we can read to the end again..
  Seek(base::TimeDelta::FromMilliseconds(0));

  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastAudioTimestamp, last_timestamp);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  EXPECT_EQ(DemuxerStream::kOk, status);
  EXPECT_EQ(kLastVideoTimestamp, last_timestamp);
}

TEST_F(ChunkDemuxerTest, GetBufferedRangesBeforeInitSegment) {
  EXPECT_CALL(*this, DemuxerOpened());
  demuxer_->Initialize(&host_, CreateInitDoneCB(PIPELINE_OK));
  ASSERT_EQ(AddId("audio", true, false), ChunkDemuxer::kOk);
  ASSERT_EQ(AddId("video", false, true), ChunkDemuxer::kOk);

  CheckExpectedRanges("audio", "{ }");
  CheckExpectedRanges("video", "{ }");
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, EndOfStreamDuringSeek) {
  InSequence s;

  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(0);
  demuxer_->StartWaitingForSeek(seek_time);

  AppendCluster(kDefaultSecondCluster());
  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(kDefaultSecondClusterEndTimestamp)));
  MarkEndOfStream(PIPELINE_OK);

  demuxer_->Seek(seek_time, NewExpectedStatusCB(PIPELINE_OK));

  GenerateExpectedReads(0, 4);
  GenerateExpectedReads(46, 66, 5);

  EndOfStreamHelper end_of_stream_helper(demuxer_.get());
  end_of_stream_helper.RequestReads();
  end_of_stream_helper.CheckIfReadDonesWereCalled(true);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Video) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Fetch initial video config and verify it matches what we expect.
  const VideoDecoderConfig& video_config_1 = video->video_decoder_config();
  ASSERT_TRUE(video_config_1.IsValidConfig());
  EXPECT_EQ(video_config_1.natural_size().width(), 320);
  EXPECT_EQ(video_config_1.natural_size().height(), 240);

  ExpectRead(DemuxerStream::VIDEO, 0);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);

  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 501);

  // Fetch the new decoder config.
  const VideoDecoderConfig& video_config_2 = video->video_decoder_config();
  ASSERT_TRUE(video_config_2.IsValidConfig());
  EXPECT_EQ(video_config_2.natural_size().width(), 640);
  EXPECT_EQ(video_config_2.natural_size().height(), 360);

  ExpectRead(DemuxerStream::VIDEO, 527);

  // Read until the next config change.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 793);

  // Get the new config and verify that it matches the first one.
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));

  ExpectRead(DemuxerStream::VIDEO, 801);

  // Read until the end of the stream just to make sure there aren't any other
  // config changes.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::VIDEO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Audio) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream::Status status;
  base::TimeDelta last_timestamp;

  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Fetch initial audio config and verify it matches what we expect.
  const AudioDecoderConfig& audio_config_1 = audio->audio_decoder_config();
  ASSERT_TRUE(audio_config_1.IsValidConfig());
  EXPECT_EQ(audio_config_1.samples_per_second(), 44100);
  EXPECT_EQ(audio_config_1.extra_data_size(), 3863u);

  ExpectRead(DemuxerStream::AUDIO, 0);

  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);

  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 524);

  // Fetch the new decoder config.
  const AudioDecoderConfig& audio_config_2 = audio->audio_decoder_config();
  ASSERT_TRUE(audio_config_2.IsValidConfig());
  EXPECT_EQ(audio_config_2.samples_per_second(), 44100);
  EXPECT_EQ(audio_config_2.extra_data_size(), 3935u);

  ExpectRead(DemuxerStream::AUDIO, 527);

  // Read until the next config change.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kConfigChanged);
  EXPECT_EQ(last_timestamp.InMilliseconds(), 759);

  // Get the new config and verify that it matches the first one.
  ASSERT_TRUE(audio_config_1.Matches(audio->audio_decoder_config()));

  ExpectRead(DemuxerStream::AUDIO, 779);

  // Read until the end of the stream just to make sure there aren't any other
  // config changes.
  ReadUntilNotOkOrEndOfStream(DemuxerStream::AUDIO, &status, &last_timestamp);
  ASSERT_EQ(status, DemuxerStream::kOk);
}

TEST_F(ChunkDemuxerTest, ConfigChange_Seek) {
  InSequence s;

  ASSERT_TRUE(InitDemuxerWithConfigChangeData());

  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Fetch initial video config and verify it matches what we expect.
  const VideoDecoderConfig& video_config_1 = video->video_decoder_config();
  ASSERT_TRUE(video_config_1.IsValidConfig());
  EXPECT_EQ(video_config_1.natural_size().width(), 320);
  EXPECT_EQ(video_config_1.natural_size().height(), 240);

  ExpectRead(DemuxerStream::VIDEO, 0);

  // Seek to a location with a different config.
  Seek(base::TimeDelta::FromMilliseconds(527));

  // Verify that the config change is signalled.
  ExpectConfigChanged(DemuxerStream::VIDEO);

  // Fetch the new decoder config and verify it is what we expect.
  const VideoDecoderConfig& video_config_2 = video->video_decoder_config();
  ASSERT_TRUE(video_config_2.IsValidConfig());
  EXPECT_EQ(video_config_2.natural_size().width(), 640);
  EXPECT_EQ(video_config_2.natural_size().height(), 360);

  // Verify that Read() will return a buffer now.
  ExpectRead(DemuxerStream::VIDEO, 527);

  // Seek back to the beginning and verify we get another config change.
  Seek(base::TimeDelta::FromMilliseconds(0));
  ExpectConfigChanged(DemuxerStream::VIDEO);
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));
  ExpectRead(DemuxerStream::VIDEO, 0);

  // Seek to a location that requires a config change and then
  // seek to a new location that has the same configuration as
  // the start of the file without a Read() in the middle.
  Seek(base::TimeDelta::FromMilliseconds(527));
  Seek(base::TimeDelta::FromMilliseconds(801));

  // Verify that no config change is signalled.
  ExpectRead(DemuxerStream::VIDEO, 801);
  ASSERT_TRUE(video_config_1.Matches(video->video_decoder_config()));
}

TEST_F(ChunkDemuxerTest, TimestampPositiveOffset) {
  ASSERT_TRUE(InitDemuxer(true, true));

  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      kSourceId, base::TimeDelta::FromSeconds(30)));
  AppendCluster(GenerateCluster(0, 2));

  Seek(base::TimeDelta::FromMilliseconds(30000));

  GenerateExpectedReads(30000, 2);
}

TEST_F(ChunkDemuxerTest, TimestampNegativeOffset) {
  ASSERT_TRUE(InitDemuxer(true, true));

  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      kSourceId, base::TimeDelta::FromSeconds(-1)));
  AppendCluster(GenerateCluster(1000, 2));

  GenerateExpectedReads(0, 2);
}

TEST_F(ChunkDemuxerTest, TimestampOffsetSeparateStreams) {
  std::string audio_id = "audio1";
  std::string video_id = "video1";
  ASSERT_TRUE(InitDemuxerAudioAndVideoSources(audio_id, video_id));

  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      audio_id, base::TimeDelta::FromMilliseconds(-2500)));
  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      video_id, base::TimeDelta::FromMilliseconds(-2500)));
  AppendCluster(audio_id, GenerateSingleStreamCluster(2500,
      2500 + kAudioBlockDuration * 4, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id, GenerateSingleStreamCluster(2500,
      2500 + kVideoBlockDuration * 4, kVideoTrackNum, kVideoBlockDuration));
  GenerateAudioStreamExpectedReads(0, 4);
  GenerateVideoStreamExpectedReads(0, 4);

  Seek(base::TimeDelta::FromMilliseconds(27300));

  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      audio_id, base::TimeDelta::FromMilliseconds(27300)));
  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      video_id, base::TimeDelta::FromMilliseconds(27300)));
  AppendCluster(audio_id, GenerateSingleStreamCluster(
      0, kAudioBlockDuration * 4, kAudioTrackNum, kAudioBlockDuration));
  AppendCluster(video_id, GenerateSingleStreamCluster(
      0, kVideoBlockDuration * 4, kVideoTrackNum, kVideoBlockDuration));
  GenerateVideoStreamExpectedReads(27300, 4);
  GenerateAudioStreamExpectedReads(27300, 4);
}

TEST_F(ChunkDemuxerTest, TimestampOffsetMidParse) {
  ASSERT_TRUE(InitDemuxer(true, true));

  scoped_ptr<Cluster> cluster = GenerateCluster(0, 2);
  // Append only part of the cluster data.
  AppendData(cluster->data(), cluster->size() - 13);

  // Setting a timestamp should fail because we're in the middle of a cluster.
  ASSERT_FALSE(demuxer_->SetTimestampOffset(
      kSourceId, base::TimeDelta::FromSeconds(25)));

  demuxer_->Abort(kSourceId);
  // After Abort(), setting a timestamp should succeed since we're no longer
  // in the middle of a cluster
  ASSERT_TRUE(demuxer_->SetTimestampOffset(
      kSourceId, base::TimeDelta::FromSeconds(25)));
}

TEST_F(ChunkDemuxerTest, DurationChange) {
  ASSERT_TRUE(InitDemuxer(true, true));
  static const int kStreamDuration = kDefaultDuration().InMilliseconds();

  // Add data leading up to the currently set duration.
  AppendCluster(GenerateCluster(kStreamDuration - kAudioBlockDuration,
                                kStreamDuration - kVideoBlockDuration,
                                2));

  CheckExpectedRanges(kSourceId, "{ [201191,201224) }");

  // Add data at the currently set duration. The duration should not increase.
  AppendCluster(GenerateCluster(kDefaultDuration().InMilliseconds(), 2));

  // Range should not be affected.
  CheckExpectedRanges(kSourceId, "{ [201191,201224) }");

  // Now add data past the duration and expect a new duration to be signalled.
  static const int kNewStreamDuration =
      kStreamDuration + kAudioBlockDuration * 2;
  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(kNewStreamDuration)));
  AppendCluster(GenerateCluster(kStreamDuration + kAudioBlockDuration,
                                kStreamDuration + kVideoBlockDuration,
                                2));

  // See that the range has increased appropriately.
  CheckExpectedRanges(kSourceId, "{ [201191,201270) }");
}

TEST_F(ChunkDemuxerTest, DurationChangeTimestampOffset) {
  ASSERT_TRUE(InitDemuxer(true, true));

  ASSERT_TRUE(demuxer_->SetTimestampOffset(kSourceId, kDefaultDuration()));

  EXPECT_CALL(host_, SetDuration(
      kDefaultDuration() + base::TimeDelta::FromMilliseconds(
          kAudioBlockDuration * 2)));
  AppendCluster(GenerateCluster(0, 4));
}

TEST_F(ChunkDemuxerTest, EndOfStreamTruncateDuration) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(kDefaultFirstCluster());

  EXPECT_CALL(host_, SetDuration(
      base::TimeDelta::FromMilliseconds(kDefaultFirstClusterEndTimestamp)));
  MarkEndOfStream(PIPELINE_OK);
}


TEST_F(ChunkDemuxerTest, ZeroLengthAppend) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendData(NULL, 0);
}

TEST_F(ChunkDemuxerTest, AppendAfterEndOfStream) {
  ASSERT_TRUE(InitDemuxer(true, true));

  EXPECT_CALL(host_, SetDuration(_))
      .Times(AnyNumber());

  AppendCluster(kDefaultFirstCluster());
  MarkEndOfStream(PIPELINE_OK);

  demuxer_->UnmarkEndOfStream();

  AppendCluster(kDefaultSecondCluster());
  MarkEndOfStream(PIPELINE_OK);
}

// Test receiving a Shutdown() call before we get an Initialize()
// call. This can happen if video element gets destroyed before
// the pipeline has a chance to initialize the demuxer.
TEST_F(ChunkDemuxerTest, ShutdownBeforeInitialize) {
  demuxer_->Shutdown();
  demuxer_->Initialize(
      &host_, CreateInitDoneCB(DEMUXER_ERROR_COULD_NOT_OPEN));
  message_loop_.RunUntilIdle();
}

TEST_F(ChunkDemuxerTest, ReadAfterAudioDisabled) {
  ASSERT_TRUE(InitDemuxer(true, true));
  AppendCluster(kDefaultFirstCluster());

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);

  // The stream should no longer be present.
  demuxer_->OnAudioRendererDisabled();
  ASSERT_FALSE(demuxer_->GetStream(DemuxerStream::AUDIO));

  // Normally this would return an audio buffer at timestamp zero, but
  // all reads should return EOS buffers when disabled.
  bool audio_read_done = false;
  stream->Read(base::Bind(&OnReadDone_EOSExpected, &audio_read_done));
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(audio_read_done);
}

// Verifies that signalling end of stream while stalled at a gap
// boundary does not trigger end of stream buffers to be returned.
TEST_F(ChunkDemuxerTest, EndOfStreamWhileWaitingForGapToBeFilled) {
  ASSERT_TRUE(InitDemuxer(true, true));

  AppendCluster(0, 10);
  AppendCluster(300, 10);
  CheckExpectedRanges(kSourceId, "{ [0,132) [300,432) }");


  GenerateExpectedReads(0, 10);

  bool audio_read_done = false;
  bool video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(138),
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone,
                       base::TimeDelta::FromMilliseconds(138),
                       &video_read_done));

  // Verify that the reads didn't complete
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(438)));
  MarkEndOfStream(PIPELINE_OK);

  // Verify that the reads still haven't completed.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  demuxer_->UnmarkEndOfStream();

  AppendCluster(138, 24);

  message_loop_.RunUntilIdle();

  CheckExpectedRanges(kSourceId, "{ [0,438) }");

  // Verify that the reads have completed.
  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  // Read the rest of the buffers.
  GenerateExpectedReads(161, 171, 22);

  // Verify that reads block because the append cleared the end of stream state.
  audio_read_done = false;
  video_read_done = false;
  ReadAudio(base::Bind(&OnReadDone_EOSExpected,
                       &audio_read_done));
  ReadVideo(base::Bind(&OnReadDone_EOSExpected,
                       &video_read_done));

  // Verify that the reads don't complete.
  EXPECT_FALSE(audio_read_done);
  EXPECT_FALSE(video_read_done);

  MarkEndOfStream(PIPELINE_OK);

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, CanceledSeekDuringInitialPreroll) {
  ASSERT_TRUE(InitDemuxer(true, true));

  // Cancel preroll.
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(200);
  demuxer_->CancelPendingSeek(seek_time);

  // Initiate the seek to the new location.
  Seek(seek_time);

  // Append data to satisfy the seek.
  AppendCluster(seek_time.InMilliseconds(), 10);
}

TEST_F(ChunkDemuxerTest, GCDuringSeek) {
  ASSERT_TRUE(InitDemuxer(true, false));

  demuxer_->SetMemoryLimitsForTesting(5 * kBlockSize);

  base::TimeDelta seek_time1 = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta seek_time2 = base::TimeDelta::FromMilliseconds(500);

  // Initiate a seek to |seek_time1|.
  Seek(seek_time1);

  // Append data to satisfy the first seek request.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum,
                            seek_time1.InMilliseconds(), 5);
  CheckExpectedRanges(kSourceId, "{ [1000,1115) }");

  // Signal that the second seek is starting.
  demuxer_->StartWaitingForSeek(seek_time2);

  // Append data to satisfy the second seek. This append triggers
  // the garbage collection logic since we set the memory limit to
  // 5 blocks.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum,
                            seek_time2.InMilliseconds(), 5);

  // Verify that the buffers that cover |seek_time2| do not get
  // garbage collected.
  CheckExpectedRanges(kSourceId, "{ [500,615) }");

  // Complete the seek.
  demuxer_->Seek(seek_time2, NewExpectedStatusCB(PIPELINE_OK));


  // Append more data and make sure that the blocks for |seek_time2|
  // don't get removed.
  //
  // NOTE: The current GC algorithm tries to preserve the GOP at the
  //  current position as well as the last appended GOP. This is
  //  why there are 2 ranges in the expectations.
  AppendSingleStreamCluster(kSourceId, kAudioTrackNum, 700, 5);
  CheckExpectedRanges(kSourceId, "{ [500,592) [792,815) }");
}

TEST_F(ChunkDemuxerTest, RemoveBeforeInitSegment) {
    EXPECT_CALL(*this, DemuxerOpened());
    demuxer_->Initialize(
        &host_, CreateInitDoneCB(kNoTimestamp(), PIPELINE_OK));

    EXPECT_EQ(ChunkDemuxer::kOk, AddId(kSourceId, true, true));

    demuxer_->Remove(kSourceId, base::TimeDelta::FromMilliseconds(0),
                     base::TimeDelta::FromMilliseconds(1));
}

TEST_F(ChunkDemuxerTest, AppendWindow) {
  ASSERT_TRUE(InitDemuxer(false, true));
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);

  // Set the append window to [20,280).
  demuxer_->SetAppendWindowStart(kSourceId,
                                 base::TimeDelta::FromMilliseconds(20));
  demuxer_->SetAppendWindowEnd(kSourceId,
                               base::TimeDelta::FromMilliseconds(280));

  // Append a cluster that starts before and ends after the append window.
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum,
                            "0K 30 60 90 120K 150 180 210 240K 270 300 330K");

  // Verify that GOPs that start outside the window are not included
  // in the buffer. Also verify that buffers that extend beyond the
  // window are not included.
  CheckExpectedRanges(kSourceId, "{ [120,300) }");
  CheckExpectedBuffers(stream, "120 150 180 210 240 270");

  // Extend the append window to [20,650).
  demuxer_->SetAppendWindowEnd(kSourceId,
                               base::TimeDelta::FromMilliseconds(650));

  // Append more data and verify that adding buffers start at the next
  // keyframe.
  AppendSingleStreamCluster(kSourceId, kVideoTrackNum,
                            "360 390 420K 450 480 510 540K 570 600 630K");
  CheckExpectedRanges(kSourceId, "{ [120,300) [420,660) }");
}

TEST_F(ChunkDemuxerTest, StartWaitingForSeekAfterParseError) {
  ASSERT_TRUE(InitDemuxer(true, true));
  EXPECT_CALL(host_, OnDemuxerError(PIPELINE_ERROR_DECODE));
  AppendGarbage();
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(50);
  demuxer_->StartWaitingForSeek(seek_time);
}

}  // namespace media
