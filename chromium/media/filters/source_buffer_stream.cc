// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace media {
// Helper class representing a range of buffered data. All buffers in a
// SourceBufferRange are ordered sequentially in presentation order with no
// gaps.
class SourceBufferRange {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  // Returns the maximum distance in time between any buffer seen in this
  // stream. Used to estimate the duration of a buffer if its duration is not
  // known.
  typedef base::Callback<base::TimeDelta()> InterbufferDistanceCB;

  // Creates a source buffer range with |new_buffers|. |new_buffers| cannot be
  // empty and the front of |new_buffers| must be a keyframe.
  // |media_segment_start_time| refers to the starting timestamp for the media
  // segment to which these buffers belong.
  SourceBufferRange(const BufferQueue& new_buffers,
                    base::TimeDelta media_segment_start_time,
                    const InterbufferDistanceCB& interbuffer_distance_cb);

  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes. Assumes |buffers| belongs at the end of the
  // range.
  void AppendBuffersToEnd(const BufferQueue& buffers);
  bool CanAppendBuffersToEnd(const BufferQueue& buffers) const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_index_|
  // is transfered to this SourceBufferRange.
  void AppendRangeToEnd(const SourceBufferRange& range,
                        bool transfer_current_position);
  bool CanAppendRangeToEnd(const SourceBufferRange& range) const;

  // Updates |next_buffer_index_| to point to the Buffer containing |timestamp|.
  // Assumes |timestamp| is valid and in this range.
  void Seek(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe after or equal to
  // |timestamp|.
  void SeekAheadTo(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe strictly after
  // |timestamp|.
  void SeekAheadPast(base::TimeDelta timestamp);

  // Seeks to the beginning of the range.
  void SeekToStart();

  // Finds the next keyframe from |buffers_| after |timestamp| (or at
  // |timestamp| if |is_exclusive| is false) and creates and returns a new
  // SourceBufferRange with the buffers from that keyframe onward.
  // The buffers in the new SourceBufferRange are moved out of this range. If
  // there is no keyframe after |timestamp|, SplitRange() returns null and this
  // range is unmodified.
  SourceBufferRange* SplitRange(base::TimeDelta timestamp, bool is_exclusive);

  // Deletes the buffers from this range starting at |timestamp|, exclusive if
  // |is_exclusive| is true, inclusive otherwise.
  // Resets |next_buffer_index_| if the buffer at |next_buffer_index_| was
  // deleted, and deletes the |keyframe_map_| entries for the buffers that
  // were removed.
  // |deleted_buffers| contains the buffers that were deleted from this range,
  // starting at the buffer that had been at |next_buffer_index_|.
  void TruncateAt(base::TimeDelta timestamp,
                  BufferQueue* deleted_buffers, bool is_exclusive);
  // Deletes all buffers in range.
  void DeleteAll(BufferQueue* deleted_buffers);

  // Deletes a GOP from the front or back of the range and moves these
  // buffers into |deleted_buffers|. Returns the number of bytes deleted from
  // the range (i.e. the size in bytes of |deleted_buffers|).
  int DeleteGOPFromFront(BufferQueue* deleted_buffers);
  int DeleteGOPFromBack(BufferQueue* deleted_buffers);

  // Indicates whether the GOP at the beginning or end of the range contains the
  // next buffer position.
  bool FirstGOPContainsNextBufferPosition() const;
  bool LastGOPContainsNextBufferPosition() const;

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  bool HasNextBuffer() const;

  // Returns the config ID for the buffer that will be returned by
  // GetNextBuffer().
  int GetNextConfigId() const;

  // Returns true if the range knows the position of the next buffer it should
  // return, i.e. it has been Seek()ed. This does not necessarily mean that it
  // has the next buffer yet.
  bool HasNextBufferPosition() const;

  // Resets this range to an "unseeked" state.
  void ResetNextBufferPosition();

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(), or kNoTimestamp() if the timestamp is unknown.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // Returns the timestamp of the last buffer in the range.
  base::TimeDelta GetEndTimestamp() const;

  // Returns the timestamp for the end of the buffered region in this range.
  // This is an approximation if the duration for the last buffer in the range
  // is unset.
  base::TimeDelta GetBufferedEndTimestamp() const;

  // Gets the timestamp for the keyframe that is after |timestamp|. If
  // there isn't a keyframe in the range after |timestamp| then kNoTimestamp()
  // is returned.
  base::TimeDelta NextKeyframeTimestamp(base::TimeDelta timestamp);

  // Gets the timestamp for the closest keyframe that is <= |timestamp|. If
  // there isn't a keyframe before |timestamp| or |timestamp| is outside
  // this range, then kNoTimestamp() is returned.
  base::TimeDelta KeyframeBeforeTimestamp(base::TimeDelta timestamp);

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  bool BelongsToRange(base::TimeDelta timestamp) const;

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Returns true if this range's buffered timespan completely overlaps the
  // buffered timespan of |range|.
  bool CompletelyOverlaps(const SourceBufferRange& range) const;

  // Returns true if the end of this range contains buffers that overlaps with
  // the beginning of |range|.
  bool EndOverlaps(const SourceBufferRange& range) const;

  // Returns true if |timestamp| is the timestamp of the next buffer in
  // sequence after |buffer|, false otherwise.
  bool IsNextInSequence(
      const scoped_refptr<media::StreamParserBuffer>& buffer,
      base::TimeDelta timestamp) const;

  int size_in_bytes() const { return size_in_bytes_; }

 private:
  typedef std::map<base::TimeDelta, int> KeyframeMap;

  // Seeks the range to the next keyframe after |timestamp|. If
  // |skip_given_timestamp| is true, the seek will go to a keyframe with a
  // timestamp strictly greater than |timestamp|.
  void SeekAhead(base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |buffers_| pointing to the buffer at |timestamp|.
  // If |skip_given_timestamp| is true, this returns the first buffer with
  // timestamp greater than |timestamp|.
  BufferQueue::iterator GetBufferItrAt(
      base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |keyframe_map_| pointing to the next keyframe after
  // |timestamp|. If |skip_given_timestamp| is true, this returns the first
  // keyframe with a timestamp strictly greater than |timestamp|.
  KeyframeMap::iterator GetFirstKeyframeAt(
      base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |keyframe_map_| pointing to the first keyframe
  // before or at |timestamp|.
  KeyframeMap::iterator GetFirstKeyframeBefore(base::TimeDelta timestamp);

  // Helper method to delete buffers in |buffers_| starting at
  // |starting_point|, an iterator in |buffers_|.
  void TruncateAt(const BufferQueue::iterator& starting_point,
                  BufferQueue* deleted_buffers);

  // Frees the buffers in |buffers_| from [|start_point|,|ending_point|) and
  // updates the |size_in_bytes_| accordingly. Does not update |keyframe_map_|.
  void FreeBufferRange(const BufferQueue::iterator& starting_point,
                       const BufferQueue::iterator& ending_point);

  // Returns the distance in time estimating how far from the beginning or end
  // of this range a buffer can be to considered in the range.
  base::TimeDelta GetFudgeRoom() const;

  // Returns the approximate duration of a buffer in this range.
  base::TimeDelta GetApproximateDuration() const;

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  KeyframeMap keyframe_map_;

  // Index base of all positions in |keyframe_map_|. In other words, the
  // real position of entry |k| of |keyframe_map_| in the range is:
  //   keyframe_map_[k] - keyframe_map_index_base_
  int keyframe_map_index_base_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetNextBuffer(), set to -1 before Seek().
  int next_buffer_index_;

  // If the first buffer in this range is the beginning of a media segment,
  // |media_segment_start_time_| is the time when the media segment begins.
  // |media_segment_start_time_| may be <= the timestamp of the first buffer in
  // |buffers_|. |media_segment_start_time_| is kNoTimestamp() if this range
  // does not start at the beginning of a media segment, which can only happen
  // garbage collection or after an end overlap that results in a split range
  // (we don't have a way of knowing the media segment timestamp for the new
  // range).
  base::TimeDelta media_segment_start_time_;

  // Called to get the largest interbuffer distance seen so far in the stream.
  InterbufferDistanceCB interbuffer_distance_cb_;

  // Stores the amount of memory taken up by the data in |buffers_|.
  int size_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferRange);
};

}  // namespace media

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
static bool IsRangeListSorted(
    const std::list<media::SourceBufferRange*>& ranges) {
  base::TimeDelta prev = media::kNoTimestamp();
  for (std::list<media::SourceBufferRange*>::const_iterator itr =
       ranges.begin(); itr != ranges.end(); ++itr) {
    if (prev != media::kNoTimestamp() && prev >= (*itr)->GetStartTimestamp())
      return false;
    prev = (*itr)->GetEndTimestamp();
  }
  return true;
}

// Comparison function for two Buffers based on timestamp.
static bool BufferComparator(
    const scoped_refptr<media::StreamParserBuffer>& first,
    const scoped_refptr<media::StreamParserBuffer>& second) {
  return first->GetDecodeTimestamp() < second->GetDecodeTimestamp();
}

// Returns an estimate of how far from the beginning or end of a range a buffer
// can be to still be considered in the range, given the |approximate_duration|
// of a buffer in the stream.
static base::TimeDelta ComputeFudgeRoom(base::TimeDelta approximate_duration) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * approximate_duration;
}

// An arbitrarily-chosen number to estimate the duration of a buffer if none
// is set and there's not enough information to get a better estimate.
static int kDefaultBufferDurationInMs = 125;

// The amount of time the beginning of the buffered data can differ from the
// start time in order to still be considered the start of stream.
static base::TimeDelta kSeekToStartFudgeRoom() {
  return base::TimeDelta::FromMilliseconds(1000);
}
// The maximum amount of data in bytes the stream will keep in memory.
#if defined(GOOGLE_TV)
// In Google TV, set the size of the buffer to 1 min because of
// the limited memory of the embedded system.
// 2MB: approximately 1 minutes of 256Kbps content.
// 30MB: approximately 1 minutes of 4Mbps content.
static int kDefaultAudioMemoryLimit = 2 * 1024 * 1024;
static int kDefaultVideoMemoryLimit = 30 * 1024 * 1024;
#else
// 12MB: approximately 5 minutes of 320Kbps content.
// 150MB: approximately 5 minutes of 4Mbps content.
static int kDefaultAudioMemoryLimit = 12 * 1024 * 1024;
static int kDefaultVideoMemoryLimit = 150 * 1024 * 1024;
#endif

namespace media {

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config,
                                       const LogCB& log_cb)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kDefaultAudioMemoryLimit),
      config_change_pending_(false) {
  DCHECK(audio_config.IsValidConfig());
  audio_configs_.push_back(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config,
                                       const LogCB& log_cb)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kDefaultVideoMemoryLimit),
      config_change_pending_(false) {
  DCHECK(video_config.IsValidConfig());
  video_configs_.push_back(video_config);
}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

void SourceBufferStream::OnNewMediaSegment(
    base::TimeDelta media_segment_start_time) {
  DCHECK(!end_of_stream_);
  media_segment_start_time_ = media_segment_start_time;
  new_media_segment_ = true;

  RangeList::iterator last_range = range_for_next_append_;
  range_for_next_append_ = FindExistingRangeFor(media_segment_start_time);

  // Only reset |last_appended_buffer_timestamp_| if this new media segment is
  // not adjacent to the previous media segment appended to the stream.
  if (range_for_next_append_ == ranges_.end() ||
      !AreAdjacentInSequence(last_appended_buffer_timestamp_,
                             media_segment_start_time)) {
    last_appended_buffer_timestamp_ = kNoTimestamp();
    last_appended_buffer_is_keyframe_ = false;
  } else {
    DCHECK(last_range == range_for_next_append_);
  }
}

bool SourceBufferStream::Append(
    const SourceBufferStream::BufferQueue& buffers) {
  TRACE_EVENT2("mse", "SourceBufferStream::Append",
               "stream type", GetStreamTypeName(),
               "buffers to append", buffers.size());

  DCHECK(!buffers.empty());
  DCHECK(media_segment_start_time_ != kNoTimestamp());
  DCHECK(!end_of_stream_);

  // New media segments must begin with a keyframe.
  if (new_media_segment_ && !buffers.front()->IsKeyframe()) {
    MEDIA_LOG(log_cb_) << "Media segment did not begin with keyframe.";
    return false;
  }

  // Buffers within a media segment should be monotonically increasing.
  if (!IsMonotonicallyIncreasing(buffers))
    return false;

  if (media_segment_start_time_ < base::TimeDelta() ||
      buffers.front()->GetDecodeTimestamp() < base::TimeDelta()) {
    MEDIA_LOG(log_cb_)
        << "Cannot append a media segment with negative timestamps.";
    return false;
  }

  UpdateMaxInterbufferDistance(buffers);
  SetConfigIds(buffers);

  // Save a snapshot of stream state before range modifications are made.
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  BufferQueue deleted_buffers;

  RangeList::iterator range_for_new_buffers = range_for_next_append_;
  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_new_buffers != ranges_.end()) {
    if (!InsertIntoExistingRange(range_for_new_buffers, buffers,
                                 &deleted_buffers)) {
      return false;
    }
  } else {
    DCHECK(new_media_segment_);
    range_for_new_buffers =
        AddToRanges(new SourceBufferRange(
            buffers, media_segment_start_time_,
            base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                       base::Unretained(this))));
  }

  range_for_next_append_ = range_for_new_buffers;
  new_media_segment_ = false;
  last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
  last_appended_buffer_is_keyframe_ = buffers.back()->IsKeyframe();

  // Resolve overlaps.
  ResolveCompleteOverlaps(range_for_new_buffers, &deleted_buffers);
  ResolveEndOverlap(range_for_new_buffers, &deleted_buffers);
  MergeWithAdjacentRangeIfNecessary(range_for_new_buffers);

  // Seek to try to fulfill a previous call to Seek().
  if (seek_pending_) {
    DCHECK(!selected_range_);
    DCHECK(deleted_buffers.empty());
    Seek(seek_buffer_timestamp_);
  }

  if (!deleted_buffers.empty()) {
    base::TimeDelta start_of_deleted =
        deleted_buffers.front()->GetDecodeTimestamp();

    DCHECK(track_buffer_.empty() ||
           track_buffer_.back()->GetDecodeTimestamp() < start_of_deleted)
        << "decode timestamp "
        << track_buffer_.back()->GetDecodeTimestamp().InSecondsF() << " sec"
        << ", start_of_deleted " << start_of_deleted.InSecondsF()<< " sec";

    track_buffer_.insert(track_buffer_.end(), deleted_buffers.begin(),
                         deleted_buffers.end());
  }

  // Prune any extra buffers in |track_buffer_| if new keyframes
  // are appended to the range covered by |track_buffer_|.
  if (!track_buffer_.empty()) {
    base::TimeDelta keyframe_timestamp =
        FindKeyframeAfterTimestamp(track_buffer_.front()->GetDecodeTimestamp());
    if (keyframe_timestamp != kNoTimestamp())
      PruneTrackBuffer(keyframe_timestamp);
  }

  SetSelectedRangeIfNeeded(next_buffer_timestamp);

  GarbageCollectIfNeeded();

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  return true;
}

void SourceBufferStream::Remove(base::TimeDelta start, base::TimeDelta end,
                                base::TimeDelta duration) {
  DVLOG(1) << __FUNCTION__ << "(" << start.InSecondsF()
           << ", " << end.InSecondsF()
           << ", " << duration.InSecondsF() << ")";
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration != kNoTimestamp());

  base::TimeDelta remove_end_timestamp = duration;
  base::TimeDelta keyframe_timestamp = FindKeyframeAfterTimestamp(end);
  if (keyframe_timestamp != kNoTimestamp()) {
    remove_end_timestamp = keyframe_timestamp;
  } else if (end < remove_end_timestamp) {
    remove_end_timestamp = end;
  }

  RangeList::iterator itr = ranges_.begin();

  while (itr != ranges_.end()) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= remove_end_timestamp)
      break;

    // Split off any remaining end piece and add it to |ranges_|.
    SourceBufferRange* new_range =
        range->SplitRange(remove_end_timestamp, false);
    if (new_range) {
      itr = ranges_.insert(++itr, new_range);
      --itr;

      // Update the selected range if the next buffer position was transferred
      // to |new_range|.
      if (new_range->HasNextBufferPosition())
        SetSelectedRange(new_range);
    }

    // If the current range now is completely covered by the removal
    // range then delete it and move on.
    if (start <= range->GetStartTimestamp()) {
      if (selected_range_ == range)
        SetSelectedRange(NULL);

      delete range;
      itr = ranges_.erase(itr);
      continue;
    }

    // Truncate the current range so that it only contains data before
    // the removal range.
    BufferQueue saved_buffers;
    range->TruncateAt(start, &saved_buffers, false);

    // Check to see if the current playback position was removed and
    // update the selected range appropriately.
    if (!saved_buffers.empty()) {
      DCHECK(!range->HasNextBufferPosition());
      SetSelectedRange(NULL);
      SetSelectedRangeIfNeeded(saved_buffers.front()->GetDecodeTimestamp());
    }

    // Move on to the next range.
    ++itr;
  }

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
}

void SourceBufferStream::ResetSeekState() {
  SetSelectedRange(NULL);
  track_buffer_.clear();
  config_change_pending_ = false;
  last_output_buffer_timestamp_ = kNoTimestamp();
}

bool SourceBufferStream::ShouldSeekToStartOfBuffered(
    base::TimeDelta seek_timestamp) const {
  if (ranges_.empty())
    return false;
  base::TimeDelta beginning_of_buffered =
      ranges_.front()->GetStartTimestamp();
  return (seek_timestamp <= beginning_of_buffered &&
          beginning_of_buffered < kSeekToStartFudgeRoom());
}

// Buffers with the same timestamp are only allowed under certain conditions.
// Video: Allowed when the previous frame and current frame are NOT keyframes.
//        This is the situation for VP8 Alt-Ref frames.
// Otherwise: Allowed in all situations except where a non-keyframe is followed
//            by a keyframe.
// Returns true if |prev_is_keyframe| and |current_is_keyframe| indicate a
// same timestamp situation that is allowed. False is returned otherwise.
bool SourceBufferStream::AllowSameTimestamp(
    bool prev_is_keyframe, bool current_is_keyframe) const {
  if (video_configs_.size() > 0)
    return !prev_is_keyframe && !current_is_keyframe;

  return prev_is_keyframe || !current_is_keyframe;
}

bool SourceBufferStream::IsMonotonicallyIncreasing(
    const BufferQueue& buffers) const {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    bool current_is_keyframe = (*itr)->IsKeyframe();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp()) {
      if (current_timestamp < prev_timestamp) {
        MEDIA_LOG(log_cb_) << "Buffers were not monotonically increasing.";
        return false;
      }

      if (current_timestamp == prev_timestamp &&
          !AllowSameTimestamp(prev_is_keyframe, current_is_keyframe)) {
        MEDIA_LOG(log_cb_) << "Unexpected combination of buffers with the"
                           << " same timestamp detected at "
                           << current_timestamp.InSecondsF();
        return false;
      }
    }

    prev_timestamp = current_timestamp;
    prev_is_keyframe = current_is_keyframe;
  }
  return true;
}

bool SourceBufferStream::OnlySelectedRangeIsSeeked() const {
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    if ((*itr)->HasNextBufferPosition() && (*itr) != selected_range_)
      return false;
  }
  return !selected_range_ || selected_range_->HasNextBufferPosition();
}

void SourceBufferStream::UpdateMaxInterbufferDistance(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp()) {
      base::TimeDelta interbuffer_distance = current_timestamp - prev_timestamp;
      if (max_interbuffer_distance_ == kNoTimestamp()) {
        max_interbuffer_distance_ = interbuffer_distance;
      } else {
        max_interbuffer_distance_ =
            std::max(max_interbuffer_distance_, interbuffer_distance);
      }
    }
    prev_timestamp = current_timestamp;
  }
}

void SourceBufferStream::SetConfigIds(const BufferQueue& buffers) {
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetConfigId(append_config_index_);
  }
}

void SourceBufferStream::GarbageCollectIfNeeded() {
  // Compute size of |ranges_|.
  int ranges_size = 0;
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr)
    ranges_size += (*itr)->size_in_bytes();

  // Return if we're under or at the memory limit.
  if (ranges_size <= memory_limit_)
    return;

  int bytes_to_free = ranges_size - memory_limit_;

  // Begin deleting from the front.
  int bytes_freed = FreeBuffers(bytes_to_free, false);

  // Begin deleting from the back.
  if (bytes_to_free - bytes_freed > 0)
    FreeBuffers(bytes_to_free - bytes_freed, true);
}

int SourceBufferStream::FreeBuffers(int total_bytes_to_free,
                                    bool reverse_direction) {
  TRACE_EVENT2("mse", "SourceBufferStream::FreeBuffers",
               "total bytes to free", total_bytes_to_free,
               "reverse direction", reverse_direction);

  DCHECK_GT(total_bytes_to_free, 0);
  int bytes_to_free = total_bytes_to_free;
  int bytes_freed = 0;

  // This range will save the last GOP appended to |range_for_next_append_|
  // if the buffers surrounding it get deleted during garbage collection.
  SourceBufferRange* new_range_for_append = NULL;

  while (!ranges_.empty() && bytes_to_free > 0) {
    SourceBufferRange* current_range = NULL;
    BufferQueue buffers;
    int bytes_deleted = 0;

    if (reverse_direction) {
      current_range = ranges_.back();
      if (current_range->LastGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromBack(&buffers);
    } else {
      current_range = ranges_.front();
      if (current_range->FirstGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromFront(&buffers);
    }

    // Check to see if we've just deleted the GOP that was last appended.
    base::TimeDelta end_timestamp = buffers.back()->GetDecodeTimestamp();
    if (end_timestamp == last_appended_buffer_timestamp_) {
      DCHECK(last_appended_buffer_timestamp_ != kNoTimestamp());
      DCHECK(!new_range_for_append);
      // Create a new range containing these buffers.
      new_range_for_append = new SourceBufferRange(
            buffers, kNoTimestamp(),
            base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                       base::Unretained(this)));
      range_for_next_append_ = ranges_.end();
    } else {
      bytes_to_free -= bytes_deleted;
      bytes_freed += bytes_deleted;
    }

    if (current_range->size_in_bytes() == 0) {
      DCHECK_NE(current_range, selected_range_);
      DCHECK(range_for_next_append_ == ranges_.end() ||
             *range_for_next_append_ != current_range);
      delete current_range;
      reverse_direction ? ranges_.pop_back() : ranges_.pop_front();
    }
  }

  // Insert |new_range_for_append| into |ranges_|, if applicable.
  if (new_range_for_append) {
    range_for_next_append_ = AddToRanges(new_range_for_append);
    DCHECK(range_for_next_append_ != ranges_.end());

    // Check to see if we need to merge |new_range_for_append| with the range
    // before or after it. |new_range_for_append| is created whenever the last
    // GOP appended is encountered, regardless of whether any buffers after it
    // are ultimately deleted. Merging is necessary if there were no buffers
    // (or very few buffers) deleted after creating |new_range_for_append|.
    if (range_for_next_append_ != ranges_.begin()) {
      RangeList::iterator range_before_next = range_for_next_append_;
      --range_before_next;
      MergeWithAdjacentRangeIfNecessary(range_before_next);
    }
    MergeWithAdjacentRangeIfNecessary(range_for_next_append_);
  }
  return bytes_freed;
}

bool SourceBufferStream::InsertIntoExistingRange(
    const RangeList::iterator& range_for_new_buffers_itr,
    const BufferQueue& new_buffers, BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  SourceBufferRange* range_for_new_buffers = *range_for_new_buffers_itr;

  bool temporarily_select_range = false;
  if (!track_buffer_.empty()) {
    base::TimeDelta tb_timestamp = track_buffer_.back()->GetDecodeTimestamp();
    base::TimeDelta seek_timestamp = FindKeyframeAfterTimestamp(tb_timestamp);
    if (seek_timestamp != kNoTimestamp() &&
        seek_timestamp < new_buffers.front()->GetDecodeTimestamp() &&
        range_for_new_buffers->BelongsToRange(seek_timestamp)) {
      DCHECK(tb_timestamp < seek_timestamp);
      DCHECK(!selected_range_);
      DCHECK(!range_for_new_buffers->HasNextBufferPosition());

      // If there are GOPs between the end of the track buffer and the
      // beginning of the new buffers, then temporarily seek the range
      // so that the buffers between these two times will be deposited in
      // |deleted_buffers| as if they were part of the current playback
      // position.
      // TODO(acolwell): Figure out a more elegant way to do this.
      SeekAndSetSelectedRange(range_for_new_buffers, seek_timestamp);
      temporarily_select_range = true;
    }
  }

  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  base::TimeDelta next_timestamp = new_buffers.front()->GetDecodeTimestamp();
  bool next_is_keyframe = new_buffers.front()->IsKeyframe();

  if (prev_timestamp != kNoTimestamp() && prev_timestamp != next_timestamp) {
    // Clean up the old buffers between the last appended buffer and the
    // beginning of |new_buffers|.
    DeleteBetween(
        range_for_new_buffers_itr, prev_timestamp, next_timestamp, true,
        deleted_buffers);
  }

  bool is_exclusive = false;
  if (prev_timestamp == next_timestamp) {
    if (!new_media_segment_ &&
        !AllowSameTimestamp(prev_is_keyframe, next_is_keyframe)) {
      MEDIA_LOG(log_cb_) << "Invalid same timestamp construct detected at time "
                         << prev_timestamp.InSecondsF();
      return false;
    }

    // Make the delete range exclusive if we are dealing with an allowed same
    // timestamp situation so that the buffer with the same timestamp that is
    // already stored in |*range_for_new_buffers_itr| doesn't get deleted.
    is_exclusive = AllowSameTimestamp(prev_is_keyframe, next_is_keyframe);
  }

  // If we cannot append the |new_buffers| to the end of the existing range,
  // this is either a start overlap or a middle overlap. Delete the buffers
  // that |new_buffers| overlaps.
  if (!range_for_new_buffers->CanAppendBuffersToEnd(new_buffers)) {
    DeleteBetween(
        range_for_new_buffers_itr, new_buffers.front()->GetDecodeTimestamp(),
        new_buffers.back()->GetDecodeTimestamp(), is_exclusive,
        deleted_buffers);
  }

  // Restore the range seek state if necessary.
  if (temporarily_select_range)
    SetSelectedRange(NULL);

  range_for_new_buffers->AppendBuffersToEnd(new_buffers);
  return true;
}

void SourceBufferStream::DeleteBetween(
    const RangeList::iterator& range_itr, base::TimeDelta start_timestamp,
    base::TimeDelta end_timestamp, bool is_range_exclusive,
    BufferQueue* deleted_buffers) {
  SourceBufferRange* new_next_range =
      (*range_itr)->SplitRange(end_timestamp, is_range_exclusive);

  // Insert the |new_next_range| into |ranges_| after |range|.
  if (new_next_range) {
    RangeList::iterator next_range_itr = range_itr;
    ranges_.insert(++next_range_itr, new_next_range);
  }

  BufferQueue saved_buffers;
  (*range_itr)->TruncateAt(start_timestamp, &saved_buffers, is_range_exclusive);

  if (selected_range_ != *range_itr)
    return;

  DCHECK(deleted_buffers->empty());
  *deleted_buffers = saved_buffers;

  // If the next buffer position has transferred to the split range, set the
  // selected range accordingly.
  if (new_next_range && new_next_range->HasNextBufferPosition()) {
    DCHECK(!(*range_itr)->HasNextBufferPosition());
    SetSelectedRange(new_next_range);
  } else if (!selected_range_->HasNextBufferPosition()) {
    SetSelectedRange(NULL);
  }
}

bool SourceBufferStream::AreAdjacentInSequence(
    base::TimeDelta first_timestamp, base::TimeDelta second_timestamp) const {
  return first_timestamp < second_timestamp &&
      second_timestamp <=
      first_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
}

void SourceBufferStream::ResolveCompleteOverlaps(
    const RangeList::iterator& range_with_new_buffers_itr,
    BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  while (next_range_itr != ranges_.end() &&
         range_with_new_buffers->CompletelyOverlaps(**next_range_itr)) {
    if (*next_range_itr == selected_range_) {
      DCHECK(deleted_buffers->empty());
      selected_range_->DeleteAll(deleted_buffers);
      SetSelectedRange(NULL);
    }
    delete *next_range_itr;
    next_range_itr = ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::ResolveEndOverlap(
    const RangeList::iterator& range_with_new_buffers_itr,
    BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr == ranges_.end() ||
      !range_with_new_buffers->EndOverlaps(**next_range_itr)) {
    return;
  }

  // Split the overlapped range after |range_with_new_buffers|'s last buffer
  // overlaps. Now |overlapped_range| contains only the buffers that do not
  // belong in |ranges_| anymore, and |new_next_range| contains buffers that
  // go after |range_with_new_buffers| (without overlap).
  scoped_ptr<SourceBufferRange> overlapped_range(*next_range_itr);
  next_range_itr = ranges_.erase(next_range_itr);

  SourceBufferRange* new_next_range =
      overlapped_range->SplitRange(
          range_with_new_buffers->GetEndTimestamp(), true);

  // If there were non-overlapped buffers, add the new range to |ranges_|.
  if (new_next_range)
    AddToRanges(new_next_range);

  // If we didn't overlap a selected range, return.
  if (selected_range_ != overlapped_range)
    return;

  // If the |overlapped_range| transfers its next buffer position to
  // |new_next_range|, make |new_next_range| the |selected_range_|.
  if (new_next_range && new_next_range->HasNextBufferPosition()) {
    DCHECK(!overlapped_range->HasNextBufferPosition());
    SetSelectedRange(new_next_range);
    return;
  }

  // Save the buffers in |overlapped_range|.
  DCHECK(deleted_buffers->empty());
  DCHECK_EQ(overlapped_range.get(), selected_range_);
  overlapped_range->DeleteAll(deleted_buffers);

  // |overlapped_range| will be deleted, so set |selected_range_| to NULL.
  SetSelectedRange(NULL);
}

void SourceBufferStream::PruneTrackBuffer(const base::TimeDelta timestamp) {
  // If we don't have the next timestamp, we don't have anything to delete.
  if (timestamp == kNoTimestamp())
    return;

  while (!track_buffer_.empty() &&
         track_buffer_.back()->GetDecodeTimestamp() >= timestamp) {
    track_buffer_.pop_back();
  }
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr != ranges_.end() &&
      range_with_new_buffers->CanAppendRangeToEnd(**next_range_itr)) {
    bool transfer_current_position = selected_range_ == *next_range_itr;
    range_with_new_buffers->AppendRangeToEnd(**next_range_itr,
                                             transfer_current_position);
    // Update |selected_range_| pointer if |range| has become selected after
    // merges.
    if (transfer_current_position)
      SetSelectedRange(range_with_new_buffers);

    if (next_range_itr == range_for_next_append_)
      range_for_next_append_ = range_with_new_buffers_itr;

    delete *next_range_itr;
    ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  DCHECK(timestamp >= base::TimeDelta());
  ResetSeekState();

  if (ShouldSeekToStartOfBuffered(timestamp)) {
    ranges_.front()->SeekToStart();
    SetSelectedRange(ranges_.front());
    seek_pending_ = false;
    return;
  }

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(timestamp))
      break;
  }

  if (itr == ranges_.end())
    return;

  SeekAndSetSelectedRange(*itr, timestamp);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return !(end_of_stream_ && IsEndSelected()) && seek_pending_;
}

void SourceBufferStream::OnSetDuration(base::TimeDelta duration) {
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() > duration)
      break;
  }
  if (itr == ranges_.end())
    return;

  // Need to partially truncate this range.
  if ((*itr)->GetStartTimestamp() < duration) {
    (*itr)->TruncateAt(duration, NULL, false);
    if ((*itr == selected_range_) && !selected_range_->HasNextBufferPosition())
      SetSelectedRange(NULL);
    ++itr;
  }

  // Delete all ranges that begin after |duration|.
  while (itr != ranges_.end()) {
    // If we're about to delete the selected range, also reset the seek state.
    DCHECK((*itr)->GetStartTimestamp() >= duration);
    if (*itr== selected_range_)
      ResetSeekState();
    delete *itr;
    itr = ranges_.erase(itr);
  }
}

SourceBufferStream::Status SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  CHECK(!config_change_pending_);

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    if (track_buffer_.front()->GetConfigId() != current_config_index_) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (track buffer config ID does not match).";
      return kConfigChange;
    }

    *out_buffer = track_buffer_.front();
    track_buffer_.pop_front();
    last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();

    // If the track buffer becomes empty, then try to set the selected range
    // based on the timestamp of this buffer being returned.
    if (track_buffer_.empty())
      SetSelectedRangeIfNeeded(last_output_buffer_timestamp_);

    return kSuccess;
  }

  if (!selected_range_ || !selected_range_->HasNextBuffer()) {
    if (end_of_stream_ && IsEndSelected())
      return kEndOfStream;
    return kNeedBuffer;
  }

  if (selected_range_->GetNextConfigId() != current_config_index_) {
    config_change_pending_ = true;
    DVLOG(1) << "Config change (selected range config ID does not match).";
    return kConfigChange;
  }

  CHECK(selected_range_->GetNextBuffer(out_buffer));
  last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();
  return kSuccess;
}

base::TimeDelta SourceBufferStream::GetNextBufferTimestamp() {
  if (!track_buffer_.empty())
    return track_buffer_.front()->GetDecodeTimestamp();

  if (!selected_range_)
    return kNoTimestamp();

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

base::TimeDelta SourceBufferStream::GetEndBufferTimestamp() {
  if (!selected_range_)
    return kNoTimestamp();
  return selected_range_->GetEndTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(base::TimeDelta start_timestamp) {
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::AddToRanges(SourceBufferRange* new_range) {
  base::TimeDelta start_timestamp = new_range->GetStartTimestamp();
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, new_range);
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::GetSelectedRangeItr() {
  DCHECK(selected_range_);
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if (*itr == selected_range_)
      break;
  }
  DCHECK(itr != ranges_.end());
  return itr;
}

void SourceBufferStream::SeekAndSetSelectedRange(
    SourceBufferRange* range, base::TimeDelta seek_timestamp) {
  if (range)
    range->Seek(seek_timestamp);
  SetSelectedRange(range);
}

void SourceBufferStream::SetSelectedRange(SourceBufferRange* range) {
  if (selected_range_)
    selected_range_->ResetNextBufferPosition();
  DCHECK(!range || range->HasNextBufferPosition());
  selected_range_ = range;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    ranges.Add((*itr)->GetStartTimestamp(), (*itr)->GetBufferedEndTimestamp());
  }
  return ranges;
}

void SourceBufferStream::MarkEndOfStream() {
  DCHECK(!end_of_stream_);
  end_of_stream_ = true;
}

void SourceBufferStream::UnmarkEndOfStream() {
  DCHECK(end_of_stream_);
  end_of_stream_ = false;
}

bool SourceBufferStream::IsEndSelected() const {
  if (ranges_.empty())
    return true;

  if (seek_pending_)
    return seek_buffer_timestamp_ >= ranges_.back()->GetBufferedEndTimestamp();

  return selected_range_ == ranges_.back();
}

const AudioDecoderConfig& SourceBufferStream::GetCurrentAudioDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return audio_configs_[current_config_index_];
}

const VideoDecoderConfig& SourceBufferStream::GetCurrentVideoDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return video_configs_[current_config_index_];
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  if (max_interbuffer_distance_ == kNoTimestamp())
    return base::TimeDelta::FromMilliseconds(kDefaultBufferDurationInMs);
  return max_interbuffer_distance_;
}

bool SourceBufferStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(!audio_configs_.empty());
  DCHECK(video_configs_.empty());
  DVLOG(3) << "UpdateAudioConfig.";

  if (audio_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Audio codec changes not allowed.";
    return false;
  }

  if (audio_configs_[0].samples_per_second() != config.samples_per_second()) {
    MEDIA_LOG(log_cb_) << "Audio sample rate changes not allowed.";
    return false;
  }

  if (audio_configs_[0].channel_layout() != config.channel_layout()) {
    MEDIA_LOG(log_cb_) << "Audio channel layout changes not allowed.";
    return false;
  }

  if (audio_configs_[0].bits_per_channel() != config.bits_per_channel()) {
    MEDIA_LOG(log_cb_) << "Audio bits per channel changes not allowed.";
    return false;
  }

  if (audio_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Audio encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < audio_configs_.size(); ++i) {
    if (config.Matches(audio_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = audio_configs_.size();
  DVLOG(2) << "New audio config - index: " << append_config_index_;
  audio_configs_.resize(audio_configs_.size() + 1);
  audio_configs_[append_config_index_] = config;
  return true;
}

bool SourceBufferStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(!video_configs_.empty());
  DCHECK(audio_configs_.empty());
  DVLOG(3) << "UpdateVideoConfig.";

  if (video_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Video Encryption changes not allowed.";
    return false;
  }

  if (video_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Video codec changes not allowed.";
    return false;
  }

  if (video_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Video encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < video_configs_.size(); ++i) {
    if (config.Matches(video_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = video_configs_.size();
  DVLOG(2) << "New video config - index: " << append_config_index_;
  video_configs_.resize(video_configs_.size() + 1);
  video_configs_[append_config_index_] = config;
  return true;
}

void SourceBufferStream::CompleteConfigChange() {
  config_change_pending_ = false;

  if (!track_buffer_.empty()) {
    current_config_index_ = track_buffer_.front()->GetConfigId();
    return;
  }

  if (selected_range_ && selected_range_->HasNextBuffer())
    current_config_index_ = selected_range_->GetNextConfigId();
}

void SourceBufferStream::SetSelectedRangeIfNeeded(
    const base::TimeDelta timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  if (selected_range_) {
    DCHECK(track_buffer_.empty());
    return;
  }

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    return;
  }

  base::TimeDelta start_timestamp = timestamp;

  // If the next buffer timestamp is not known then use a timestamp just after
  // the timestamp on the last buffer returned by GetNextBuffer().
  if (start_timestamp == kNoTimestamp()) {
    if (last_output_buffer_timestamp_ == kNoTimestamp())
      return;

    start_timestamp = last_output_buffer_timestamp_ +
        base::TimeDelta::FromInternalValue(1);
  }

  base::TimeDelta seek_timestamp =
      FindNewSelectedRangeSeekTimestamp(start_timestamp);

  // If we don't have buffered data to seek to, then return.
  if (seek_timestamp == kNoTimestamp())
    return;

  DCHECK(track_buffer_.empty());
  SeekAndSetSelectedRange(*FindExistingRangeFor(seek_timestamp),
                          seek_timestamp);
}

base::TimeDelta SourceBufferStream::FindNewSelectedRangeSeekTimestamp(
    const base::TimeDelta start_timestamp) {
  DCHECK(start_timestamp != kNoTimestamp());
  DCHECK(start_timestamp >= base::TimeDelta());

  RangeList::iterator itr = ranges_.begin();

  for (; itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() >= start_timestamp) {
      break;
    }
  }

  if (itr == ranges_.end())
    return kNoTimestamp();

  // First check for a keyframe timestamp >= |start_timestamp|
  // in the current range.
  base::TimeDelta keyframe_timestamp =
      (*itr)->NextKeyframeTimestamp(start_timestamp);

  if (keyframe_timestamp != kNoTimestamp())
    return keyframe_timestamp;

  // If a keyframe was not found then look for a keyframe that is
  // "close enough" in the current or next range.
  base::TimeDelta end_timestamp =
      start_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
  DCHECK(start_timestamp < end_timestamp);

  // Make sure the current range doesn't start beyond |end_timestamp|.
  if ((*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  // If |end_timestamp| is within this range, then no other checks are
  // necessary.
  if (end_timestamp <= (*itr)->GetEndTimestamp())
    return kNoTimestamp();

  // Move on to the next range.
  ++itr;

  // Return early if the next range does not contain |end_timestamp|.
  if (itr == ranges_.end() || (*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  return kNoTimestamp();
}

base::TimeDelta SourceBufferStream::FindKeyframeAfterTimestamp(
    const base::TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp());

  RangeList::iterator itr = FindExistingRangeFor(timestamp);

  if (itr == ranges_.end())
    return kNoTimestamp();

  // First check for a keyframe timestamp >= |timestamp|
  // in the current range.
  return (*itr)->NextKeyframeTimestamp(timestamp);
}

std::string SourceBufferStream::GetStreamTypeName() const {
  if (!video_configs_.empty()) {
    DCHECK(audio_configs_.empty());
    return "VIDEO";
  }

  DCHECK(!audio_configs_.empty());
  return "AUDIO";
}

SourceBufferRange::SourceBufferRange(
    const BufferQueue& new_buffers, base::TimeDelta media_segment_start_time,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : keyframe_map_index_base_(0),
      next_buffer_index_(-1),
      media_segment_start_time_(media_segment_start_time),
      interbuffer_distance_cb_(interbuffer_distance_cb),
      size_in_bytes_(0) {
  DCHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->IsKeyframe());
  DCHECK(!interbuffer_distance_cb.is_null());
  AppendBuffersToEnd(new_buffers);
}

void SourceBufferRange::AppendBuffersToEnd(const BufferQueue& new_buffers) {
  DCHECK(buffers_.empty() ||
         buffers_.back()->GetDecodeTimestamp() <=
         new_buffers.front()->GetDecodeTimestamp());

  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end(); ++itr) {
    DCHECK((*itr)->GetDecodeTimestamp() != kNoTimestamp());
    buffers_.push_back(*itr);
    size_in_bytes_ += (*itr)->data_size();

    if ((*itr)->IsKeyframe()) {
      keyframe_map_.insert(
          std::make_pair((*itr)->GetDecodeTimestamp(),
                         buffers_.size() - 1 + keyframe_map_index_base_));
    }
  }
}

void SourceBufferRange::Seek(base::TimeDelta timestamp) {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  KeyframeMap::iterator result = GetFirstKeyframeBefore(timestamp);
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekAheadTo(base::TimeDelta timestamp) {
  SeekAhead(timestamp, false);
}

void SourceBufferRange::SeekAheadPast(base::TimeDelta timestamp) {
  SeekAhead(timestamp, true);
}

void SourceBufferRange::SeekAhead(base::TimeDelta timestamp,
                                  bool skip_given_timestamp) {
  DCHECK(!keyframe_map_.empty());

  KeyframeMap::iterator result =
      GetFirstKeyframeAt(timestamp, skip_given_timestamp);

  // If there isn't a keyframe after |timestamp|, then seek to end and return
  // kNoTimestamp to signal such.
  if (result == keyframe_map_.end()) {
    next_buffer_index_ = -1;
    return;
  }
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekToStart() {
  DCHECK(!buffers_.empty());
  next_buffer_index_ = 0;
}

SourceBufferRange* SourceBufferRange::SplitRange(
    base::TimeDelta timestamp, bool is_exclusive) {
  // Find the first keyframe after |timestamp|. If |is_exclusive|, do not
  // include keyframes at |timestamp|.
  KeyframeMap::iterator new_beginning_keyframe =
      GetFirstKeyframeAt(timestamp, is_exclusive);

  // If there is no keyframe after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end())
    return NULL;

  // Remove the data beginning at |keyframe_index| from |buffers_| and save it
  // into |removed_buffers|.
  int keyframe_index =
      new_beginning_keyframe->second - keyframe_map_index_base_;
  DCHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));
  BufferQueue::iterator starting_point = buffers_.begin() + keyframe_index;
  BufferQueue removed_buffers(starting_point, buffers_.end());
  keyframe_map_.erase(new_beginning_keyframe, keyframe_map_.end());
  FreeBufferRange(starting_point, buffers_.end());

  // Create a new range with |removed_buffers|.
  SourceBufferRange* split_range =
      new SourceBufferRange(
          removed_buffers, kNoTimestamp(), interbuffer_distance_cb_);

  // If the next buffer position is now in |split_range|, update the state of
  // this range and |split_range| accordingly.
  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    split_range->next_buffer_index_ = next_buffer_index_ - keyframe_index;
    ResetNextBufferPosition();
  }

  return split_range;
}

SourceBufferRange::BufferQueue::iterator SourceBufferRange::GetBufferItrAt(
    base::TimeDelta timestamp, bool skip_given_timestamp) {
  // Need to make a dummy buffer with timestamp |timestamp| in order to search
  // the |buffers_| container.
  scoped_refptr<StreamParserBuffer> dummy_buffer =
      StreamParserBuffer::CopyFrom(NULL, 0, false);
  dummy_buffer->SetDecodeTimestamp(timestamp);

  if (skip_given_timestamp) {
    return std::upper_bound(
        buffers_.begin(), buffers_.end(), dummy_buffer, BufferComparator);
  }
  return std::lower_bound(
      buffers_.begin(), buffers_.end(), dummy_buffer, BufferComparator);
}

SourceBufferRange::KeyframeMap::iterator
SourceBufferRange::GetFirstKeyframeAt(base::TimeDelta timestamp,
                                      bool skip_given_timestamp) {
  return skip_given_timestamp ?
      keyframe_map_.upper_bound(timestamp) :
      keyframe_map_.lower_bound(timestamp);
}

SourceBufferRange::KeyframeMap::iterator
SourceBufferRange::GetFirstKeyframeBefore(base::TimeDelta timestamp) {
  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);
  // lower_bound() returns the first element >= |timestamp|, so we want the
  // previous element if it did not return the element exactly equal to
  // |timestamp|.
  if (result != keyframe_map_.begin() &&
      (result == keyframe_map_.end() || result->first != timestamp)) {
    --result;
  }
  return result;
}

void SourceBufferRange::DeleteAll(BufferQueue* removed_buffers) {
  TruncateAt(buffers_.begin(), removed_buffers);
}

void SourceBufferRange::TruncateAt(
    base::TimeDelta timestamp, BufferQueue* removed_buffers,
    bool is_exclusive) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::iterator starting_point =
      GetBufferItrAt(timestamp, is_exclusive);
  TruncateAt(starting_point, removed_buffers);
}

int SourceBufferRange::DeleteGOPFromFront(BufferQueue* deleted_buffers) {
  DCHECK(!FirstGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  int buffers_deleted = 0;
  int total_bytes_deleted = 0;

  KeyframeMap::iterator front = keyframe_map_.begin();
  DCHECK(front != keyframe_map_.end());

  // Delete the keyframe at the start of |keyframe_map_|.
  keyframe_map_.erase(front);

  // Now we need to delete all the buffers that depend on the keyframe we've
  // just deleted.
  int end_index = keyframe_map_.size() > 0 ?
      keyframe_map_.begin()->second - keyframe_map_index_base_ :
      buffers_.size();

  // Delete buffers from the beginning of the buffered range up until (but not
  // including) the next keyframe.
  for (int i = 0; i < end_index; i++) {
    int bytes_deleted = buffers_.front()->data_size();
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    deleted_buffers->push_back(buffers_.front());
    buffers_.pop_front();
    ++buffers_deleted;
  }

  // Update |keyframe_map_index_base_| to account for the deleted buffers.
  keyframe_map_index_base_ += buffers_deleted;

  if (next_buffer_index_ > -1) {
    next_buffer_index_ -= buffers_deleted;
    DCHECK_GE(next_buffer_index_, 0);
  }

  // Invalidate media segment start time if we've deleted the first buffer of
  // the range.
  if (buffers_deleted > 0)
    media_segment_start_time_ = kNoTimestamp();

  return total_bytes_deleted;
}

int SourceBufferRange::DeleteGOPFromBack(BufferQueue* deleted_buffers) {
  DCHECK(!LastGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  // Remove the last GOP's keyframe from the |keyframe_map_|.
  KeyframeMap::iterator back = keyframe_map_.end();
  DCHECK_GT(keyframe_map_.size(), 0u);
  --back;

  // The index of the first buffer in the last GOP is equal to the new size of
  // |buffers_| after that GOP is deleted.
  size_t goal_size = back->second - keyframe_map_index_base_;
  keyframe_map_.erase(back);

  int total_bytes_deleted = 0;
  while (buffers_.size() != goal_size) {
    int bytes_deleted = buffers_.back()->data_size();
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    // We're removing buffers from the back, so push each removed buffer to the
    // front of |deleted_buffers| so that |deleted_buffers| are in nondecreasing
    // order.
    deleted_buffers->push_front(buffers_.back());
    buffers_.pop_back();
  }

  return total_bytes_deleted;
}

bool SourceBufferRange::FirstGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  KeyframeMap::const_iterator second_gop = keyframe_map_.begin();
  ++second_gop;
  return next_buffer_index_ < second_gop->second - keyframe_map_index_base_;
}

bool SourceBufferRange::LastGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  KeyframeMap::const_iterator last_gop = keyframe_map_.end();
  --last_gop;
  return last_gop->second - keyframe_map_index_base_ <= next_buffer_index_;
}

void SourceBufferRange::FreeBufferRange(
    const BufferQueue::iterator& starting_point,
    const BufferQueue::iterator& ending_point) {
  for (BufferQueue::iterator itr = starting_point;
       itr != ending_point; ++itr) {
    size_in_bytes_ -= (*itr)->data_size();
    DCHECK_GE(size_in_bytes_, 0);
  }
  buffers_.erase(starting_point, ending_point);
}

void SourceBufferRange::TruncateAt(
    const BufferQueue::iterator& starting_point, BufferQueue* removed_buffers) {
  DCHECK(!removed_buffers || removed_buffers->empty());

  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return;

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  if (HasNextBufferPosition()) {
    base::TimeDelta next_buffer_timestamp = GetNextTimestamp();
    if (next_buffer_timestamp == kNoTimestamp() ||
        next_buffer_timestamp >= (*starting_point)->GetDecodeTimestamp()) {
      if (HasNextBuffer() && removed_buffers) {
        int starting_offset = starting_point - buffers_.begin();
        int next_buffer_offset = next_buffer_index_ - starting_offset;
        DCHECK_GE(next_buffer_offset, 0);
        BufferQueue saved(starting_point + next_buffer_offset, buffers_.end());
        removed_buffers->swap(saved);
      }
      ResetNextBufferPosition();
    }
  }

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetDecodeTimestamp());
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  FreeBufferRange(starting_point, buffers_.end());
}

bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!HasNextBuffer())
    return false;

  *out_buffer = buffers_.at(next_buffer_index_);
  next_buffer_index_++;
  return true;
}

bool SourceBufferRange::HasNextBuffer() const {
  return next_buffer_index_ >= 0 &&
      next_buffer_index_ < static_cast<int>(buffers_.size());
}

int SourceBufferRange::GetNextConfigId() const {
  DCHECK(HasNextBuffer());
  return buffers_.at(next_buffer_index_)->GetConfigId();
}


base::TimeDelta SourceBufferRange::GetNextTimestamp() const {
  DCHECK(!buffers_.empty());
  DCHECK(HasNextBufferPosition());

  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    return kNoTimestamp();
  }

  return buffers_.at(next_buffer_index_)->GetDecodeTimestamp();
}

bool SourceBufferRange::HasNextBufferPosition() const {
  return next_buffer_index_ >= 0;
}

void SourceBufferRange::ResetNextBufferPosition() {
  next_buffer_index_ = -1;
}

void SourceBufferRange::AppendRangeToEnd(const SourceBufferRange& range,
                                         bool transfer_current_position) {
  DCHECK(CanAppendRangeToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position && range.next_buffer_index_ >= 0)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendRangeToEnd(
    const SourceBufferRange& range) const {
  return CanAppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendBuffersToEnd(
    const BufferQueue& buffers) const {
  DCHECK(!buffers_.empty());
  return IsNextInSequence(buffers_.back(),
                          buffers.front()->GetDecodeTimestamp());
}

bool SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  DCHECK(!buffers_.empty());

  return (IsNextInSequence(buffers_.back(), timestamp) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  base::TimeDelta start_timestamp =
      std::max(base::TimeDelta(), GetStartTimestamp() - GetFudgeRoom());
  return !keyframe_map_.empty() && start_timestamp <= timestamp &&
      timestamp < GetBufferedEndTimestamp();
}

bool SourceBufferRange::CompletelyOverlaps(
    const SourceBufferRange& range) const {
  return GetStartTimestamp() <= range.GetStartTimestamp() &&
      GetEndTimestamp() >= range.GetEndTimestamp();
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.GetStartTimestamp() <= GetEndTimestamp() &&
      GetEndTimestamp() < range.GetEndTimestamp();
}

base::TimeDelta SourceBufferRange::GetStartTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta start_timestamp = media_segment_start_time_;
  if (start_timestamp == kNoTimestamp())
    start_timestamp = buffers_.front()->GetDecodeTimestamp();
  return start_timestamp;
}

base::TimeDelta SourceBufferRange::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetDecodeTimestamp();
}

base::TimeDelta SourceBufferRange::GetBufferedEndTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta duration = buffers_.back()->duration();
  if (duration == kNoTimestamp() || duration == base::TimeDelta())
    duration = GetApproximateDuration();
  return GetEndTimestamp() + duration;
}

base::TimeDelta SourceBufferRange::NextKeyframeTimestamp(
    base::TimeDelta timestamp) {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp();

  KeyframeMap::iterator itr = GetFirstKeyframeAt(timestamp, false);
  if (itr == keyframe_map_.end())
    return kNoTimestamp();
  return itr->first;
}

base::TimeDelta SourceBufferRange::KeyframeBeforeTimestamp(
    base::TimeDelta timestamp) {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp();

  return GetFirstKeyframeBefore(timestamp)->first;
}

bool SourceBufferRange::IsNextInSequence(
    const scoped_refptr<media::StreamParserBuffer>& buffer,
    base::TimeDelta timestamp) const {
  return buffer->GetDecodeTimestamp() < timestamp &&
      timestamp <= buffer->GetDecodeTimestamp() + GetFudgeRoom();
}

base::TimeDelta SourceBufferRange::GetFudgeRoom() const {
  return ComputeFudgeRoom(GetApproximateDuration());
}

base::TimeDelta SourceBufferRange::GetApproximateDuration() const {
  base::TimeDelta max_interbuffer_distance = interbuffer_distance_cb_.Run();
  DCHECK(max_interbuffer_distance != kNoTimestamp());
  return max_interbuffer_distance;
}

}  // namespace media
