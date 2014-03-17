// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecoderAccelerator
// that utilizes the hardware video decoder present on the Exynos SoC.

#ifndef CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_

#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/video_decode_accelerator_impl.h"
#include "media/base/limits.h"
#include "media/base/video_decoder_config.h"
#include "media/video/picture.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

namespace base {
class MessageLoopProxy;
}

namespace content {
class H264Parser;

// This class handles Exynos video acceleration directly through the V4L2
// device exported by the Multi Format Codec hardware block.
//
// The threading model of this class is driven by the fact that it needs to
// interface two fundamentally different event queues -- the one Chromium
// provides through MessageLoop, and the one driven by the V4L2 devices which
// is waited on with epoll().  There are three threads involved in this class:
//
// * The child thread, which is the main GPU process thread which calls the
//   media::VideoDecodeAccelerator entry points.  Calls from this thread
//   generally do not block (with the exception of Initialize() and Destroy()).
//   They post tasks to the decoder_thread_, which actually services the task
//   and calls back when complete through the
//   media::VideoDecodeAccelerator::Client interface.
// * The decoder_thread_, owned by this class.  It services API tasks, through
//   the *Task() routines, as well as V4L2 device events, through
//   ServiceDeviceTask().  Almost all state modification is done on this thread
//   (this doesn't include buffer (re)allocation sequence, see below).
// * The device_poll_thread_, owned by this class.  All it does is epoll() on
//   the V4L2 in DevicePollTask() and schedule a ServiceDeviceTask() on the
//   decoder_thread_ when something interesting happens.
//   TODO(sheu): replace this thread with an TYPE_IO decoder_thread_.
//
// Note that this class has (almost) no locks, apart from the pictures_assigned_
// WaitableEvent. Everything (apart from buffer (re)allocation) is serviced on
// the decoder_thread_, so there are no synchronization issues.
// ... well, there are, but it's a matter of getting messages posted in the
// right order, not fiddling with locks.
// Buffer creation is a two-step process that is serviced partially on the
// Child thread, because we need to wait for the client to provide textures
// for the buffers we allocate. We cannot keep the decoder thread running while
// the client allocates Pictures for us, because we need to REQBUFS first to get
// the required number of output buffers from the device and that cannot be done
// unless we free the previous set of buffers, leaving the decoding in a
// inoperable state for the duration of the wait for Pictures. So to prevent
// subtle races (esp. if we get Reset() in the meantime), we block the decoder
// thread while we wait for AssignPictureBuffers from the client.
class CONTENT_EXPORT ExynosVideoDecodeAccelerator
    : public VideoDecodeAcceleratorImpl {
 public:
  ExynosVideoDecodeAccelerator(
      EGLDisplay egl_display,
      EGLContext egl_context,
      Client* client,
      const base::WeakPtr<Client>& io_client_,
      const base::Callback<bool(void)>& make_context_current,
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy);
  virtual ~ExynosVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  // Note: Initialize() and Destroy() are synchronous.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // VideoDecodeAcceleratorImpl implementation.
  virtual bool CanDecodeOnIOThread() OVERRIDE;

 private:
  // These are rather subjectively tuned.
  enum {
    kMfcInputBufferCount = 8,
    // TODO(posciak): determine MFC input buffer size based on level limits.
    // See http://crbug.com/255116.
    kMfcInputBufferMaxSize = 1024 * 1024,
    // Number of output buffers to use for each VDA stage above what's required
    // by the decoder (e.g. DPB size, in H264).  We need
    // media::limits::kMaxVideoFrames to fill up the GpuVideoDecode pipeline,
    // and +1 for a frame in transit.
    kDpbOutputBufferExtraCount = media::limits::kMaxVideoFrames + 1,
  };

  // Internal state of the decoder.
  enum State {
    kUninitialized,      // Initialize() not yet called.
    kInitialized,        // Initialize() returned true; ready to start decoding.
    kDecoding,           // DecodeBufferInitial() successful; decoding frames.
    kResetting,          // Presently resetting.
    kAfterReset,         // After Reset(), ready to start decoding again.
    kChangingResolution, // Performing resolution change, all remaining
                         // pre-change frames decoded and processed.
    kError,              // Error in kDecoding state.
  };

  enum BufferId {
    kFlushBufferId = -2  // Buffer id for flush buffer, queued by FlushTask().
  };

  // File descriptors we need to poll.
  enum PollFds {
    kPollMfc = (1 << 0),
  };

  // Auto-destruction reference for BitstreamBuffer, for message-passing from
  // Decode() to DecodeTask().
  struct BitstreamBufferRef;

  // Auto-destruction reference for an array of PictureBuffer, for
  // simpler EGLImage cleanup if any calls fail in AssignPictureBuffers().
  struct PictureBufferArrayRef;

  // Auto-destruction reference for EGLSync (for message-passing).
  struct EGLSyncKHRRef;

  // Record for decoded pictures that can be sent to PictureReady.
  struct PictureRecord;

  // Record for MFC input buffers.
  struct MfcInputRecord {
    MfcInputRecord();
    ~MfcInputRecord();
    bool at_device;         // held by device.
    void* address;          // mmap() address.
    size_t length;          // mmap() length.
    off_t bytes_used;       // bytes filled in the mmap() segment.
    int32 input_id;         // triggering input_id as given to Decode().
  };

  // Record for MFC output buffers.
  struct MfcOutputRecord {
    MfcOutputRecord();
    ~MfcOutputRecord();
    bool at_device;         // held by device.
    bool at_client;         // held by client.
    int fds[2];             // file descriptors for each plane.
    EGLImageKHR egl_image;  // EGLImageKHR for the output buffer.
    EGLSyncKHR egl_sync;    // sync the compositor's use of the EGLImage.
    int32 picture_id;       // picture buffer id as returned to PictureReady().
    bool cleared;           // Whether the texture is cleared and safe to render
                            // from. See TextureManager for details.
  };

  //
  // Decoding tasks, to be run on decode_thread_.
  //

  // Enqueue a BitstreamBuffer to decode.  This will enqueue a buffer to the
  // decoder_input_queue_, then queue a DecodeBufferTask() to actually decode
  // the buffer.
  void DecodeTask(const media::BitstreamBuffer& bitstream_buffer);

  // Decode from the buffers queued in decoder_input_queue_.  Calls
  // DecodeBufferInitial() or DecodeBufferContinue() as appropriate.
  void DecodeBufferTask();
  // Advance to the next fragment that begins a frame.
  bool AdvanceFrameFragment(const uint8* data, size_t size, size_t* endpos);
  // Schedule another DecodeBufferTask() if we're behind.
  void ScheduleDecodeBufferTaskIfNeeded();

  // Return true if we should continue to schedule DecodeBufferTask()s after
  // completion.  Store the amount of input actually consumed in |endpos|.
  bool DecodeBufferInitial(const void* data, size_t size, size_t* endpos);
  bool DecodeBufferContinue(const void* data, size_t size);

  // Accumulate data for the next frame to decode.  May return false in
  // non-error conditions; for example when pipeline is full and should be
  // retried later.
  bool AppendToInputFrame(const void* data, size_t size);
  // Flush data for one decoded frame.
  bool FlushInputFrame();

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().  If |mfc_event_pending| is true, one or more events
  // on MFC file descriptor are pending.
  void ServiceDeviceTask(bool mfc_event_pending);
  // Handle the various device queues.
  void EnqueueMfc();
  void DequeueMfc();
  // Handle incoming MFC events.
  void DequeueMfcEvents();
  // Enqueue a buffer on the corresponding queue.
  bool EnqueueMfcInputRecord();
  bool EnqueueMfcOutputRecord();

  // Process a ReusePictureBuffer() API call.  The API call create an EGLSync
  // object on the main (GPU process) thread; we will record this object so we
  // can wait on it before reusing the buffer.
  void ReusePictureBufferTask(int32 picture_buffer_id,
                              scoped_ptr<EGLSyncKHRRef> egl_sync_ref);

  // Flush() task.  Child thread should not submit any more buffers until it
  // receives the NotifyFlushDone callback.  This task will schedule an empty
  // BitstreamBufferRef (with input_id == kFlushBufferId) to perform the flush.
  void FlushTask();
  // Notify the client of a flush completion, if required.  This should be
  // called any time a relevant queue could potentially be emptied: see
  // function definition.
  void NotifyFlushDoneIfNeeded();

  // Reset() task.  This task will schedule a ResetDoneTask() that will send
  // the NotifyResetDone callback, then set the decoder state to kResetting so
  // that all intervening tasks will drain.
  void ResetTask();
  // ResetDoneTask() will set the decoder state back to kAfterReset, so
  // subsequent decoding can continue.
  void ResetDoneTask();

  // Device destruction task.
  void DestroyTask();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  // If |keep_mfc_input_state| is true, don't reset MFC input state; used during
  // resolution change.
  bool StopDevicePoll(bool keep_mfc_input_state);
  // Set/clear the device poll interrupt (using device_poll_interrupt_fd_).
  bool SetDevicePollInterrupt();
  bool ClearDevicePollInterrupt();

  void StartResolutionChangeIfNeeded();
  void FinishResolutionChange();
  void ResumeAfterResolutionChange();

  // Try to get output format from MFC, detected after parsing the beginning
  // of the stream. Sets |again| to true if more parsing is needed.
  bool GetFormatInfo(struct v4l2_format* format, bool* again);
  // Create MFC output buffers for the given |format|.
  bool CreateBuffersForFormat(const struct v4l2_format& format);

  //
  // Device tasks, to be run on device_poll_thread_.
  //

  // The device task.
  void DevicePollTask(unsigned int poll_fds);

  //
  // Safe from any thread.
  //

  // Error notification (using PostTask() to child thread, if necessary).
  void NotifyError(Error error);

  // Set the decoder_thread_ state (using PostTask to decoder thread, if
  // necessary).
  void SetDecoderState(State state);

  //
  // Other utility functions.  Called on decoder_thread_, unless
  // decoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Create the buffers we need.
  bool CreateMfcInputBuffers();
  bool CreateMfcOutputBuffers();

  //
  // Methods run on child thread.
  //

  // Destroy buffers.
  void DestroyMfcInputBuffers();
  void DestroyMfcOutputBuffers();
  void ResolutionChangeDestroyBuffers();

  // Send decoded pictures to PictureReady.
  void SendPictureReady();

  // Callback that indicates a picture has been cleared.
  void PictureCleared();

  // Our original calling message loop for the child thread.
  scoped_refptr<base::MessageLoopProxy> child_message_loop_proxy_;

  // Message loop of the IO thread.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder or
  // device worker threads back to the child thread.  Because the worker threads
  // are members of this class, any task running on those threads is guaranteed
  // that this object is still alive.  As a result, tasks posted from the child
  // thread to the decoder or device thread should use base::Unretained(this),
  // and tasks posted the other way should use |weak_this_|.
  base::WeakPtr<ExynosVideoDecodeAccelerator> weak_this_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_message_loop_proxy_.
  base::WeakPtrFactory<Client> client_ptr_factory_;
  base::WeakPtr<Client> client_;
  // Callbacks to |io_client_| must be executed on |io_message_loop_proxy_|.
  base::WeakPtr<Client> io_client_;

  //
  // Decoder state, owned and operated by decoder_thread_.
  // Before decoder_thread_ has started, the decoder state is managed by
  // the child (main) thread.  After decoder_thread_ has started, the decoder
  // thread should be the only one managing these.
  //

  // This thread services tasks posted from the VDA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread decoder_thread_;
  // Decoder state machine state.
  State decoder_state_;
  // BitstreamBuffer we're presently reading.
  scoped_ptr<BitstreamBufferRef> decoder_current_bitstream_buffer_;
  // FlushTask() and ResetTask() should not affect buffers that have been
  // queued afterwards.  For flushing or resetting the pipeline then, we will
  // delay these buffers until after the flush or reset completes.
  int decoder_delay_bitstream_buffer_id_;
  // MFC input buffer we're presently filling.
  int decoder_current_input_buffer_;
  // We track the number of buffer decode tasks we have scheduled, since each
  // task execution should complete one buffer.  If we fall behind (due to
  // resource backpressure, etc.), we'll have to schedule more to catch up.
  int decoder_decode_buffer_tasks_scheduled_;
  // Picture buffers held by the client.
  int decoder_frames_at_client_;
  // Are we flushing?
  bool decoder_flushing_;
  // Got a notification from driver that it reached resolution change point
  // in the stream.
  bool resolution_change_pending_;
  // Got a reset request while we were performing resolution change.
  bool resolution_change_reset_pending_;
  // Input queue for decoder_thread_: BitstreamBuffers in.
  std::queue<linked_ptr<BitstreamBufferRef> > decoder_input_queue_;
  // For H264 decode, hardware requires that we send it frame-sized chunks.
  // We'll need to parse the stream.
  scoped_ptr<content::H264Parser> decoder_h264_parser_;
  // Set if the decoder has a pending incomplete frame in an input buffer.
  bool decoder_partial_frame_pending_;

  //
  // Hardware state and associated queues.  Since decoder_thread_ services
  // the hardware, decoder_thread_ owns these too.
  // mfc_output_buffer_map_ and free_output_buffers_ are an exception during the
  // buffer (re)allocation sequence, when the decoder_thread_ is blocked briefly
  // while the Child thread manipulates them.
  //

  // Completed decode buffers, waiting for MFC.
  std::queue<int> mfc_input_ready_queue_;

  // MFC decode device.
  int mfc_fd_;

  // MFC input buffer state.
  bool mfc_input_streamon_;
  // MFC input buffers enqueued to device.
  int mfc_input_buffer_queued_count_;
  // Input buffers ready to use, as a LIFO since we don't care about ordering.
  std::vector<int> mfc_free_input_buffers_;
  // Mapping of int index to MFC input buffer record.
  std::vector<MfcInputRecord> mfc_input_buffer_map_;

  // MFC output buffer state.
  bool mfc_output_streamon_;
  // MFC output buffers enqueued to device.
  int mfc_output_buffer_queued_count_;
  // Output buffers ready to use, as a FIFO since we want oldest-first to hide
  // synchronization latency with GL.
  std::queue<int> mfc_free_output_buffers_;
  // Mapping of int index to MFC output buffer record.
  std::vector<MfcOutputRecord> mfc_output_buffer_map_;
  // MFC output pixel format.
  uint32 mfc_output_buffer_pixelformat_;
  // Required size of DPB for decoding.
  int mfc_output_dpb_size_;

  // Pictures that are ready but not sent to PictureReady yet.
  std::queue<PictureRecord> pending_picture_ready_;

  // The number of pictures that are sent to PictureReady and will be cleared.
  int picture_clearing_count_;

  // Used by the decoder thread to wait for AssignPictureBuffers to arrive
  // to avoid races with potential Reset requests.
  base::WaitableEvent pictures_assigned_;

  // Output picture size.
  gfx::Size frame_buffer_size_;

  //
  // The device polling thread handles notifications of V4L2 device changes.
  //

  // The thread.
  base::Thread device_poll_thread_;
  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  //
  // Other state, held by the child (main) thread.
  //

  // Make our context current before running any EGL entry points.
  base::Callback<bool(void)> make_context_current_;

  // EGL state
  EGLDisplay egl_display_;
  EGLContext egl_context_;

  // The codec we'll be decoding for.
  media::VideoCodecProfile video_profile_;

  DISALLOW_COPY_AND_ASSIGN(ExynosVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_
