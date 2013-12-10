// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser.h"
#include "media/base/text_track.h"
#include "media/filters/source_buffer_stream.h"

namespace media {

class ChunkDemuxerStream;
class FFmpegURLProtocol;
class SourceState;

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  enum Status {
    kOk,              // ID added w/o error.
    kNotSupported,    // Type specified is not supported.
    kReachedIdLimit,  // Reached ID limit. We can't handle any more IDs.
  };

  // |open_cb| Run when Initialize() is called to signal that the demuxer
  //   is ready to receive media data via AppenData().
  // |need_key_cb| Run when the demuxer determines that an encryption key is
  //   needed to decrypt the content.
  // |add_text_track_cb| Run when demuxer detects the presence of an inband
  //   text track.
  // |log_cb| Run when parsing error messages need to be logged to the error
  //   console.
  ChunkDemuxer(const base::Closure& open_cb,
               const NeedKeyCB& need_key_cb,
               const AddTextTrackCB& add_text_track_cb,
               const LogCB& log_cb);
  virtual ~ChunkDemuxer();

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& cb) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB&  cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual DemuxerStream* GetStream(DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;

  // Methods used by an external object to control this demuxer.
  //
  // Indicates that a new Seek() call is on its way. Any pending Reads on the
  // DemuxerStream objects should be aborted immediately inside this call and
  // future Read calls should return kAborted until the Seek() call occurs.
  // This method MUST ALWAYS be called before Seek() is called to signal that
  // the next Seek() call represents the seek point we actually want to return
  // data for.
  // |seek_time| - The presentation timestamp for the seek that triggered this
  // call. It represents the most recent position the caller is trying to seek
  // to.
  void StartWaitingForSeek(base::TimeDelta seek_time);

  // Indicates that a Seek() call is on its way, but another seek has been
  // requested that will override the impending Seek() call. Any pending Reads
  // on the DemuxerStream objects should be aborted immediately inside this call
  // and future Read calls should return kAborted until the next
  // StartWaitingForSeek() call. This method also arranges for the next Seek()
  // call received before a StartWaitingForSeek() call to immediately call its
  // callback without waiting for any data.
  // |seek_time| - The presentation timestamp for the seek request that
  // triggered this call. It represents the most recent position the caller is
  // trying to seek to.
  void CancelPendingSeek(base::TimeDelta seek_time);

  // Registers a new |id| to use for AppendData() calls. |type| indicates
  // the MIME type for the data that we intend to append for this ID.
  // kOk is returned if the demuxer has enough resources to support another ID
  //    and supports the format indicated by |type|.
  // kNotSupported is returned if |type| is not a supported format.
  // kReachedIdLimit is returned if the demuxer cannot handle another ID right
  //    now.
  Status AddId(const std::string& id, const std::string& type,
               std::vector<std::string>& codecs);

  // Removed an ID & associated resources that were previously added with
  // AddId().
  void RemoveId(const std::string& id);

  // Gets the currently buffered ranges for the specified ID.
  Ranges<base::TimeDelta> GetBufferedRanges(const std::string& id) const;

  // Appends media data to the source buffer associated with |id|.
  void AppendData(const std::string& id, const uint8* data, size_t length);

  // Aborts parsing the current segment and reset the parser to a state where
  // it can accept a new segment.
  void Abort(const std::string& id);

  // Remove buffers between |start| and |end| for the source buffer
  // associated with |id|.
  void Remove(const std::string& id, base::TimeDelta start,
              base::TimeDelta end);

  // Returns the current presentation duration.
  double GetDuration();
  double GetDuration_Locked();

  // Notifies the demuxer that the duration of the media has changed to
  // |duration|.
  void SetDuration(double duration);

  // Sets a time |offset| to be applied to subsequent buffers appended to the
  // source buffer associated with |id|. Returns true if the offset is set
  // properly, false if the offset cannot be applied because we're in the
  // middle of parsing a media segment.
  bool SetTimestampOffset(const std::string& id, base::TimeDelta offset);

  // Called to signal changes in the "end of stream"
  // state. UnmarkEndOfStream() must not be called if a matching
  // MarkEndOfStream() has not come before it.
  void MarkEndOfStream(PipelineStatus status);
  void UnmarkEndOfStream();

  // Set the append window start and end values for the source buffer
  // associated with |id|.
  void SetAppendWindowStart(const std::string& id, base::TimeDelta start);
  void SetAppendWindowEnd(const std::string& id, base::TimeDelta end);

  void Shutdown();

  void SetMemoryLimitsForTesting(int memory_limit);

 private:
  enum State {
    WAITING_FOR_INIT,
    INITIALIZING,
    INITIALIZED,
    ENDED,
    PARSE_ERROR,
    SHUTDOWN,
  };

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  // Returns true if any stream has seeked to a time without buffered data.
  bool IsSeekWaitingForData_Locked() const;

  // Returns true if all streams can successfully call EndOfStream,
  // false if any can not.
  bool CanEndOfStream_Locked() const;

  // SourceState callbacks.
  void OnSourceInitDone(bool success, base::TimeDelta duration);

  // Creates a DemuxerStream for the specified |type|.
  // Returns a new ChunkDemuxerStream instance if a stream of this type
  // has not been created before. Returns NULL otherwise.
  ChunkDemuxerStream* CreateDemuxerStream(DemuxerStream::Type type);

  bool OnTextBuffers(TextTrack* text_track,
                     const StreamParser::BufferQueue& buffers);
  void OnNewMediaSegment(const std::string& source_id,
                         base::TimeDelta start_timestamp);

  // Computes the intersection between the video & audio
  // buffered ranges.
  Ranges<base::TimeDelta> ComputeIntersection() const;

  // Applies |time_offset| to the timestamps of |buffers|.
  void AdjustBufferTimestamps(const StreamParser::BufferQueue& buffers,
                              base::TimeDelta timestamp_offset);

  // Returns true if |source_id| is valid, false otherwise.
  bool IsValidId(const std::string& source_id) const;

  // Increases |duration_| if |last_appended_buffer_timestamp| exceeds the
  // current  |duration_|. The |duration_| is set to the end buffered timestamp
  // of |stream|.
  void IncreaseDurationIfNecessary(
      base::TimeDelta last_appended_buffer_timestamp,
      ChunkDemuxerStream* stream);

  // Decreases |duration_| if the buffered region is less than |duration_| when
  // EndOfStream() is called.
  void DecreaseDurationIfNecessary();

  // Sets |duration_| to |new_duration|, sets |user_specified_duration_| to -1
  // and notifies |host_|.
  void UpdateDuration(base::TimeDelta new_duration);

  // Returns the ranges representing the buffered data in the demuxer.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

  // Start returning data on all DemuxerStreams.
  void StartReturningData();

  // Aborts pending reads on all DemuxerStreams.
  void AbortPendingReads();

  // Completes any pending reads if it is possible to do so.
  void CompletePendingReadsIfPossible();

  // Seeks all SourceBufferStreams to |seek_time|.
  void SeekAllSources(base::TimeDelta seek_time);

  mutable base::Lock lock_;
  State state_;
  bool cancel_next_seek_;

  DemuxerHost* host_;
  base::Closure open_cb_;
  NeedKeyCB need_key_cb_;
  AddTextTrackCB add_text_track_cb_;
  // Callback used to report error strings that can help the web developer
  // figure out what is wrong with the content.
  LogCB log_cb_;

  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;

  scoped_ptr<ChunkDemuxerStream> audio_;
  scoped_ptr<ChunkDemuxerStream> video_;

  // Keeps |audio_| alive when audio has been disabled.
  scoped_ptr<ChunkDemuxerStream> disabled_audio_;

  base::TimeDelta duration_;

  // The duration passed to the last SetDuration(). If
  // SetDuration() is never called or an AppendData() call or
  // a EndOfStream() call changes |duration_|, then this
  // variable is set to < 0 to indicate that the |duration_| represents
  // the actual duration instead of a user specified value.
  double user_specified_duration_;

  typedef std::map<std::string, SourceState*> SourceStateMap;
  SourceStateMap source_state_map_;

  // Used to ensure that (1) config data matches the type and codec provided in
  // AddId(), (2) only 1 audio and 1 video sources are added, and (3) ids may be
  // removed with RemoveID() but can not be re-added (yet).
  std::string source_id_audio_;
  std::string source_id_video_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
