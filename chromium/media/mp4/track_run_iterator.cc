// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/track_run_iterator.h"

#include <algorithm>

#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"
#include "media/mp4/rcheck.h"

namespace {
static const uint32 kSampleIsDifferenceSampleFlagMask = 0x10000;
}

namespace media {
namespace mp4 {

struct SampleInfo {
  int size;
  int duration;
  int cts_offset;
  bool is_keyframe;
};

struct TrackRunInfo {
  uint32 track_id;
  std::vector<SampleInfo> samples;
  int64 timescale;
  int64 start_dts;
  int64 sample_start_offset;

  bool is_audio;
  const AudioSampleEntry* audio_description;
  const VideoSampleEntry* video_description;

  int64 aux_info_start_offset;  // Only valid if aux_info_total_size > 0.
  int aux_info_default_size;
  std::vector<uint8> aux_info_sizes;  // Populated if default_size == 0.
  int aux_info_total_size;

  TrackRunInfo();
  ~TrackRunInfo();
};

TrackRunInfo::TrackRunInfo()
    : track_id(0),
      timescale(-1),
      start_dts(-1),
      sample_start_offset(-1),
      is_audio(false),
      aux_info_start_offset(-1),
      aux_info_default_size(-1),
      aux_info_total_size(-1) {
}
TrackRunInfo::~TrackRunInfo() {}

TimeDelta TimeDeltaFromRational(int64 numer, int64 denom) {
  DCHECK_LT((numer > 0 ? numer : -numer),
            kint64max / base::Time::kMicrosecondsPerSecond);
  return TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * numer / denom);
}

TrackRunIterator::TrackRunIterator(const Movie* moov,
                                   const LogCB& log_cb)
    : moov_(moov), log_cb_(log_cb), sample_offset_(0) {
  CHECK(moov);
}

TrackRunIterator::~TrackRunIterator() {}

static void PopulateSampleInfo(const TrackExtends& trex,
                               const TrackFragmentHeader& tfhd,
                               const TrackFragmentRun& trun,
                               const int64 edit_list_offset,
                               const uint32 i,
                               SampleInfo* sample_info,
                               const SampleDependsOn sample_depends_on) {
  if (i < trun.sample_sizes.size()) {
    sample_info->size = trun.sample_sizes[i];
  } else if (tfhd.default_sample_size > 0) {
    sample_info->size = tfhd.default_sample_size;
  } else {
    sample_info->size = trex.default_sample_size;
  }

  if (i < trun.sample_durations.size()) {
    sample_info->duration = trun.sample_durations[i];
  } else if (tfhd.default_sample_duration > 0) {
    sample_info->duration = tfhd.default_sample_duration;
  } else {
    sample_info->duration = trex.default_sample_duration;
  }

  if (i < trun.sample_composition_time_offsets.size()) {
    sample_info->cts_offset = trun.sample_composition_time_offsets[i];
  } else {
    sample_info->cts_offset = 0;
  }
  sample_info->cts_offset += edit_list_offset;

  uint32 flags;
  if (i < trun.sample_flags.size()) {
    flags = trun.sample_flags[i];
  } else if (tfhd.has_default_sample_flags) {
    flags = tfhd.default_sample_flags;
  } else {
    flags = trex.default_sample_flags;
  }

  switch (sample_depends_on) {
    case kSampleDependsOnUnknown:
      sample_info->is_keyframe = !(flags & kSampleIsDifferenceSampleFlagMask);
      break;

    case kSampleDependsOnOthers:
      sample_info->is_keyframe = false;
      break;

    case kSampleDependsOnNoOther:
      sample_info->is_keyframe = true;
      break;

    case kSampleDependsOnReserved:
      CHECK(false);
  }
}

// In well-structured encrypted media, each track run will be immediately
// preceded by its auxiliary information; this is the only optimal storage
// pattern in terms of minimum number of bytes from a serial stream needed to
// begin playback. It also allows us to optimize caching on memory-constrained
// architectures, because we can cache the relatively small auxiliary
// information for an entire run and then discard data from the input stream,
// instead of retaining the entire 'mdat' box.
//
// We optimize for this situation (with no loss of generality) by sorting track
// runs during iteration in order of their first data offset (either sample data
// or auxiliary data).
class CompareMinTrackRunDataOffset {
 public:
  bool operator()(const TrackRunInfo& a, const TrackRunInfo& b) {
    int64 a_aux = a.aux_info_total_size ? a.aux_info_start_offset : kint64max;
    int64 b_aux = b.aux_info_total_size ? b.aux_info_start_offset : kint64max;

    int64 a_lesser = std::min(a_aux, a.sample_start_offset);
    int64 a_greater = std::max(a_aux, a.sample_start_offset);
    int64 b_lesser = std::min(b_aux, b.sample_start_offset);
    int64 b_greater = std::max(b_aux, b.sample_start_offset);

    if (a_lesser == b_lesser) return a_greater < b_greater;
    return a_lesser < b_lesser;
  }
};

bool TrackRunIterator::Init(const MovieFragment& moof) {
  runs_.clear();

  for (size_t i = 0; i < moof.tracks.size(); i++) {
    const TrackFragment& traf = moof.tracks[i];

    const Track* trak = NULL;
    for (size_t t = 0; t < moov_->tracks.size(); t++) {
      if (moov_->tracks[t].header.track_id == traf.header.track_id)
        trak = &moov_->tracks[t];
    }
    RCHECK(trak);

    const TrackExtends* trex = NULL;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      if (moov_->extends.tracks[t].track_id == traf.header.track_id)
        trex = &moov_->extends.tracks[t];
    }
    RCHECK(trex);

    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type != kAudio && stsd.type != kVideo) {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }
    size_t desc_idx = traf.header.sample_description_index;
    if (!desc_idx) desc_idx = trex->default_sample_description_index;
    RCHECK(desc_idx > 0);  // Descriptions are one-indexed in the file
    desc_idx -= 1;

    // Process edit list to remove CTS offset introduced in the presence of
    // B-frames (those that contain a single edit with a nonnegative media
    // time). Other uses of edit lists are not supported, as they are
    // both uncommon and better served by higher-level protocols.
    int64 edit_list_offset = 0;
    const std::vector<EditListEntry>& edits = trak->edit.list.edits;
    if (!edits.empty()) {
      if (edits.size() > 1)
        DVLOG(1) << "Multi-entry edit box detected; some components ignored.";

      if (edits[0].media_time < 0) {
        DVLOG(1) << "Empty edit list entry ignored.";
      } else {
        edit_list_offset = -edits[0].media_time;
      }
    }

    int64 run_start_dts = traf.decode_time.decode_time;
    int sample_count_sum = 0;

    for (size_t j = 0; j < traf.runs.size(); j++) {
      const TrackFragmentRun& trun = traf.runs[j];
      TrackRunInfo tri;
      tri.track_id = traf.header.track_id;
      tri.timescale = trak->media.header.timescale;
      tri.start_dts = run_start_dts;
      tri.sample_start_offset = trun.data_offset;

      tri.is_audio = (stsd.type == kAudio);
      if (tri.is_audio) {
        RCHECK(!stsd.audio_entries.empty());
        if (desc_idx > stsd.audio_entries.size())
          desc_idx = 0;
        tri.audio_description = &stsd.audio_entries[desc_idx];
      } else {
        RCHECK(!stsd.video_entries.empty());
        if (desc_idx > stsd.video_entries.size())
          desc_idx = 0;
        tri.video_description = &stsd.video_entries[desc_idx];
      }

      // Collect information from the auxiliary_offset entry with the same index
      // in the 'saiz' container as the current run's index in the 'trun'
      // container, if it is present.
      if (traf.auxiliary_offset.offsets.size() > j) {
        // There should be an auxiliary info entry corresponding to each sample
        // in the auxiliary offset entry's corresponding track run.
        RCHECK(traf.auxiliary_size.sample_count >=
               sample_count_sum + trun.sample_count);
        tri.aux_info_start_offset = traf.auxiliary_offset.offsets[j];
        tri.aux_info_default_size =
            traf.auxiliary_size.default_sample_info_size;
        if (tri.aux_info_default_size == 0) {
          const std::vector<uint8>& sizes =
              traf.auxiliary_size.sample_info_sizes;
          tri.aux_info_sizes.insert(tri.aux_info_sizes.begin(),
              sizes.begin() + sample_count_sum,
              sizes.begin() + sample_count_sum + trun.sample_count);
        }

        // If the default info size is positive, find the total size of the aux
        // info block from it, otherwise sum over the individual sizes of each
        // aux info entry in the aux_offset entry.
        if (tri.aux_info_default_size) {
          tri.aux_info_total_size =
              tri.aux_info_default_size * trun.sample_count;
        } else {
          tri.aux_info_total_size = 0;
          for (size_t k = 0; k < trun.sample_count; k++) {
            tri.aux_info_total_size += tri.aux_info_sizes[k];
          }
        }
      } else {
        tri.aux_info_start_offset = -1;
        tri.aux_info_total_size = 0;
      }

      tri.samples.resize(trun.sample_count);
      for (size_t k = 0; k < trun.sample_count; k++) {
        PopulateSampleInfo(*trex, traf.header, trun, edit_list_offset,
                           k, &tri.samples[k], traf.sdtp.sample_depends_on(k));
        run_start_dts += tri.samples[k].duration;
      }
      runs_.push_back(tri);
      sample_count_sum += trun.sample_count;
    }
  }

  std::sort(runs_.begin(), runs_.end(), CompareMinTrackRunDataOffset());
  run_itr_ = runs_.begin();
  ResetRun();
  return true;
}

void TrackRunIterator::AdvanceRun() {
  ++run_itr_;
  ResetRun();
}

void TrackRunIterator::ResetRun() {
  if (!IsRunValid()) return;
  sample_dts_ = run_itr_->start_dts;
  sample_offset_ = run_itr_->sample_start_offset;
  sample_itr_ = run_itr_->samples.begin();
  cenc_info_.clear();
}

void TrackRunIterator::AdvanceSample() {
  DCHECK(IsSampleValid());
  sample_dts_ += sample_itr_->duration;
  sample_offset_ += sample_itr_->size;
  ++sample_itr_;
}

// This implementation only indicates a need for caching if CENC auxiliary
// info is available in the stream.
bool TrackRunIterator::AuxInfoNeedsToBeCached() {
  DCHECK(IsRunValid());
  return is_encrypted() && aux_info_size() > 0 && cenc_info_.size() == 0;
}

// This implementation currently only caches CENC auxiliary info.
bool TrackRunIterator::CacheAuxInfo(const uint8* buf, int buf_size) {
  RCHECK(AuxInfoNeedsToBeCached() && buf_size >= aux_info_size());

  cenc_info_.resize(run_itr_->samples.size());
  int64 pos = 0;
  for (size_t i = 0; i < run_itr_->samples.size(); i++) {
    int info_size = run_itr_->aux_info_default_size;
    if (!info_size)
      info_size = run_itr_->aux_info_sizes[i];

    BufferReader reader(buf + pos, info_size);
    RCHECK(cenc_info_[i].Parse(track_encryption().default_iv_size, &reader));
    pos += info_size;
  }

  return true;
}

bool TrackRunIterator::IsRunValid() const {
  return run_itr_ != runs_.end();
}

bool TrackRunIterator::IsSampleValid() const {
  return IsRunValid() && (sample_itr_ != run_itr_->samples.end());
}

// Because tracks are in sorted order and auxiliary information is cached when
// returning samples, it is guaranteed that no data will be required before the
// lesser of the minimum data offset of this track and the next in sequence.
// (The stronger condition - that no data is required before the minimum data
// offset of this track alone - is not guaranteed, because the BMFF spec does
// not have any inter-run ordering restrictions.)
int64 TrackRunIterator::GetMaxClearOffset() {
  int64 offset = kint64max;

  if (IsSampleValid()) {
    offset = std::min(offset, sample_offset_);
    if (AuxInfoNeedsToBeCached())
      offset = std::min(offset, aux_info_offset());
  }
  if (run_itr_ != runs_.end()) {
    std::vector<TrackRunInfo>::const_iterator next_run = run_itr_ + 1;
    if (next_run != runs_.end()) {
      offset = std::min(offset, next_run->sample_start_offset);
      if (next_run->aux_info_total_size)
        offset = std::min(offset, next_run->aux_info_start_offset);
    }
  }
  if (offset == kint64max) return 0;
  return offset;
}

uint32 TrackRunIterator::track_id() const {
  DCHECK(IsRunValid());
  return run_itr_->track_id;
}

bool TrackRunIterator::is_encrypted() const {
  DCHECK(IsRunValid());
  return track_encryption().is_encrypted;
}

int64 TrackRunIterator::aux_info_offset() const {
  return run_itr_->aux_info_start_offset;
}

int TrackRunIterator::aux_info_size() const {
  return run_itr_->aux_info_total_size;
}

bool TrackRunIterator::is_audio() const {
  DCHECK(IsRunValid());
  return run_itr_->is_audio;
}

const AudioSampleEntry& TrackRunIterator::audio_description() const {
  DCHECK(is_audio());
  DCHECK(run_itr_->audio_description);
  return *run_itr_->audio_description;
}

const VideoSampleEntry& TrackRunIterator::video_description() const {
  DCHECK(!is_audio());
  DCHECK(run_itr_->video_description);
  return *run_itr_->video_description;
}

int64 TrackRunIterator::sample_offset() const {
  DCHECK(IsSampleValid());
  return sample_offset_;
}

int TrackRunIterator::sample_size() const {
  DCHECK(IsSampleValid());
  return sample_itr_->size;
}

TimeDelta TrackRunIterator::dts() const {
  DCHECK(IsSampleValid());
  return TimeDeltaFromRational(sample_dts_, run_itr_->timescale);
}

TimeDelta TrackRunIterator::cts() const {
  DCHECK(IsSampleValid());
  return TimeDeltaFromRational(sample_dts_ + sample_itr_->cts_offset,
                               run_itr_->timescale);
}

TimeDelta TrackRunIterator::duration() const {
  DCHECK(IsSampleValid());
  return TimeDeltaFromRational(sample_itr_->duration, run_itr_->timescale);
}

bool TrackRunIterator::is_keyframe() const {
  DCHECK(IsSampleValid());
  return sample_itr_->is_keyframe;
}

const TrackEncryption& TrackRunIterator::track_encryption() const {
  if (is_audio())
    return audio_description().sinf.info.track_encryption;
  return video_description().sinf.info.track_encryption;
}

scoped_ptr<DecryptConfig> TrackRunIterator::GetDecryptConfig() {
  size_t sample_idx = sample_itr_ - run_itr_->samples.begin();
  DCHECK(sample_idx < cenc_info_.size());
  const FrameCENCInfo& cenc_info = cenc_info_[sample_idx];
  DCHECK(is_encrypted() && !AuxInfoNeedsToBeCached());

  size_t total_size = 0;
  if (!cenc_info.subsamples.empty() &&
      (!cenc_info.GetTotalSizeOfSubsamples(&total_size) ||
       total_size != static_cast<size_t>(sample_size()))) {
    MEDIA_LOG(log_cb_) << "Incorrect CENC subsample size.";
    return scoped_ptr<DecryptConfig>();
  }

  const std::vector<uint8>& kid = track_encryption().default_kid;
  return scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(&kid[0]), kid.size()),
      std::string(reinterpret_cast<const char*>(cenc_info.iv),
                  arraysize(cenc_info.iv)),
      0,  // No offset to start of media data in MP4 using CENC.
      cenc_info.subsamples));
}

}  // namespace mp4
}  // namespace media
