// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_codec_bridge.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

// Class for managing all the decoding tasks. Each decoding task will be posted
// onto the same thread. The thread will be stopped once Stop() is called.
class MediaDecoderJob {
 public:
  struct Deleter {
    inline void operator()(MediaDecoderJob* ptr) const { ptr->Release(); }
  };

  // Callback when a decoder job finishes its work. Args: whether decode
  // finished successfully, presentation time, audio output bytes.
  // If the presentation time is equal to kNoTimestamp(), the decoder job
  // skipped rendering of the decoded output and the callback target should
  // update its clock to avoid introducing extra delays to the next frame.
  typedef base::Callback<void(MediaCodecStatus, const base::TimeDelta&,
                              size_t)> DecoderCallback;
  // Callback when a decoder job finishes releasing the output buffer.
  // Args: audio output bytes, must be 0 for video.
  typedef base::Callback<void(size_t)> ReleaseOutputCompletionCallback;

  virtual ~MediaDecoderJob();

  // Called by MediaSourcePlayer when more data for this object has arrived.
  void OnDataReceived(const DemuxerData& data);

  // Prefetch so we know the decoder job has data when we call Decode().
  // |prefetch_cb| - Run when prefetching has completed.
  void Prefetch(const base::Closure& prefetch_cb);

  // Called by MediaSourcePlayer to decode some data.
  // |callback| - Run when decode operation has completed.
  //
  // Returns true if the next decode was started and |callback| will be
  // called when the decode operation is complete.
  // Returns false if a config change is needed. |callback| is ignored
  // and will not be called.
  bool Decode(const base::TimeTicks& start_time_ticks,
              const base::TimeDelta& start_presentation_timestamp,
              const DecoderCallback& callback);

  // Called to stop the last Decode() early.
  // If the decoder is in the process of decoding the next frame, then
  // this method will just allow the decode to complete as normal. If
  // this object is waiting for a data request to complete, then this method
  // will wait for the data to arrive and then call the |callback|
  // passed to Decode() with a status of MEDIA_CODEC_STOPPED. This ensures that
  // the |callback| passed to Decode() is always called and the status
  // reflects whether data was actually decoded or the decode terminated early.
  void StopDecode();

  // Flush the decoder.
  void Flush();

  // Enter prerolling state. The job must not currently be decoding.
  void BeginPrerolling(const base::TimeDelta& preroll_timestamp);

  bool prerolling() const { return prerolling_; }

  bool is_decoding() const { return !decode_cb_.is_null(); }

 protected:
  MediaDecoderJob(const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
                  MediaCodecBridge* media_codec_bridge,
                  const base::Closure& request_data_cb);

  // Release the output buffer at index |output_buffer_index| and render it if
  // |render_output| is true. Upon completion, |callback| will be called.
  virtual void ReleaseOutputBuffer(
      int output_buffer_index,
      size_t size,
      bool render_output,
      const ReleaseOutputCompletionCallback& callback) = 0;

  // Returns true if the "time to render" needs to be computed for frames in
  // this decoder job.
  virtual bool ComputeTimeToRender() const = 0;

 private:
  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  MediaCodecStatus QueueInputBuffer(const AccessUnit& unit);

  // Returns true if this object has data to decode.
  bool HasData() const;

  // Initiates a request for more data.
  // |done_cb| is called when more data is available in |received_data_|.
  void RequestData(const base::Closure& done_cb);

  // Posts a task to start decoding the next access unit in |received_data_|.
  void DecodeNextAccessUnit(
      const base::TimeTicks& start_time_ticks,
      const base::TimeDelta& start_presentation_timestamp);

  // Helper function to decoder data on |thread_|. |unit| contains all the data
  // to be decoded. |start_time_ticks| and |start_presentation_timestamp|
  // represent the system time and the presentation timestamp when the first
  // frame is rendered. We use these information to estimate when the current
  // frame should be rendered. If |needs_flush| is true, codec needs to be
  // flushed at the beginning of this call.
  void DecodeInternal(const AccessUnit& unit,
                      const base::TimeTicks& start_time_ticks,
                      const base::TimeDelta& start_presentation_timestamp,
                      bool needs_flush,
                      const DecoderCallback& callback);

  // Called on the UI thread to indicate that one decode cycle has completed.
  // Completes any pending job destruction or any pending decode stop. If
  // destruction was not pending, passes its arguments to |decode_cb_|.
  void OnDecodeCompleted(MediaCodecStatus status,
                         const base::TimeDelta& presentation_timestamp,
                         size_t audio_output_bytes);

  // The UI message loop where callbacks should be dispatched.
  scoped_refptr<base::MessageLoopProxy> ui_loop_;

  // The message loop that decoder job runs on.
  scoped_refptr<base::MessageLoopProxy> decoder_loop_;

  // The media codec bridge used for decoding. Owned by derived class.
  // NOTE: This MUST NOT be accessed in the destructor.
  MediaCodecBridge* media_codec_bridge_;

  // Whether the decoder needs to be flushed.
  bool needs_flush_;

  // Whether input EOS is encountered.
  // TODO(wolenetz/qinmin): Protect with a lock. See http://crbug.com/320043.
  bool input_eos_encountered_;

  // Whether output EOS is encountered.
  bool output_eos_encountered_;

  // Tracks whether DecodeInternal() should skip decoding if the first access
  // unit is EOS or empty, and report |MEDIA_CODEC_OUTPUT_END_OF_STREAM|. This
  // is to work around some decoders that could crash otherwise. See
  // http://b/11696552.
  bool skip_eos_enqueue_;

  // The timestamp the decoder needs to preroll to. If an access unit's
  // timestamp is smaller than |preroll_timestamp_|, don't render it.
  // TODO(qinmin): Comparing access unit's timestamp with |preroll_timestamp_|
  // is not very accurate.
  base::TimeDelta preroll_timestamp_;

  // Indicates prerolling state. If true, this job has not yet decoded output
  // that it will render, since the most recent of job construction or
  // BeginPrerolling(). If false, |preroll_timestamp_| has been reached.
  // TODO(qinmin): Comparing access unit's timestamp with |preroll_timestamp_|
  // is not very accurate.
  bool prerolling_;

  // Weak pointer passed to media decoder jobs for callbacks. It is bounded to
  // the decoder thread.
  base::WeakPtrFactory<MediaDecoderJob> weak_this_;

  // Callback used to request more data.
  base::Closure request_data_cb_;

  // Callback to run when new data has been received.
  base::Closure on_data_received_cb_;

  // Callback to run when the current Decode() operation completes.
  DecoderCallback decode_cb_;

  // The current access unit being processed.
  size_t access_unit_index_;

  // Data received over IPC from last RequestData() operation.
  DemuxerData received_data_;

  // The index of input buffer that can be used by QueueInputBuffer().
  // If the index is uninitialized or invalid, it must be -1.
  int input_buf_index_;

  bool stop_decode_pending_;

  // Indicates that this object should be destroyed once the current
  // Decode() has completed. This gets set when Release() gets called
  // while there is a decode in progress.
  bool destroy_pending_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
