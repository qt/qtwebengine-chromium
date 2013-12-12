// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/common/gpu/gpu_channel.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/video/picture.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// Helper macros for dealing with failure.  If |result| evaluates false, emit
// |log| to ERROR, register |error| with the decoder, and return.
#define RETURN_ON_FAILURE(result, log, error)                       \
  do {                                                              \
    if (!(result)) {                                                \
      DLOG(ERROR) << log;                                           \
      base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind( \
          &AndroidVideoDecodeAccelerator::NotifyError,              \
          base::AsWeakPtr(this), error));                           \
      state_ = ERROR;                                               \
      return;                                                       \
    }                                                               \
  } while (0)

// TODO(dwkang): We only need kMaxVideoFrames to pass media stack's prerolling
// phase, but 1 is added due to crbug.com/176036. This should be tuned when we
// have actual use case.
enum { kNumPictureBuffers = media::limits::kMaxVideoFrames + 1 };

// Max number of bitstreams notified to the client with
// NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
enum { kMaxBitstreamsNotifiedInAdvance = 32 };

// static
static inline const base::TimeDelta DecodePollDelay() {
  return base::TimeDelta::FromMilliseconds(10);
}

static inline const base::TimeDelta NoWaitTimeOut() {
  return base::TimeDelta::FromMicroseconds(0);
}

AndroidVideoDecodeAccelerator::AndroidVideoDecodeAccelerator(
    media::VideoDecodeAccelerator::Client* client,
    const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
    const base::Callback<bool(void)>& make_context_current)
    : client_(client),
      make_context_current_(make_context_current),
      codec_(media::kCodecH264),
      state_(NO_ERROR),
      surface_texture_id_(0),
      picturebuffers_requested_(false),
      io_task_is_posted_(false),
      decoder_met_eos_(false),
      num_bytes_used_in_the_pending_buffer_(0),
      gl_decoder_(decoder) {
}

AndroidVideoDecodeAccelerator::~AndroidVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool AndroidVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile) {
  DCHECK(!media_codec_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!media::MediaCodecBridge::IsAvailable())
    return false;

  if (profile == media::VP8PROFILE_MAIN) {
    codec_ = media::kCodecVP8;
  } else {
    // TODO(dwkang): enable H264 once b/8125974 is fixed.
    LOG(ERROR) << "Unsupported profile: " << profile;
    return false;
  }

  if (!make_context_current_.Run()) {
    LOG(ERROR) << "Failed to make this decoder's GL context current.";
    return false;
  }

  if (!gl_decoder_) {
    LOG(ERROR) << "Failed to get gles2 decoder instance.";
    return false;
  }
  glGenTextures(1, &surface_texture_id_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, surface_texture_id_);

  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_decoder_->RestoreTextureUnitBindings(0);
  gl_decoder_->RestoreActiveTexture();

  surface_texture_ = new gfx::SurfaceTexture(surface_texture_id_);

  if (!ConfigureMediaCodec()) {
    LOG(ERROR) << "Failed to create MediaCodec instance.";
    return false;
  }

  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &AndroidVideoDecodeAccelerator::NotifyInitializeDone,
      base::AsWeakPtr(this)));
  return true;
}

void AndroidVideoDecodeAccelerator::DoIOTask() {
  io_task_is_posted_ = false;
  if (state_ == ERROR) {
    return;
  }

  DequeueOutput();
  QueueInput();

  if (!pending_bitstream_buffers_.empty() ||
      !free_picture_ids_.empty()) {
    io_task_is_posted_ = true;
    // TODO(dwkang): PostDelayedTask() does not guarantee the task will awake
    //               at the exact time. Need a better way for polling.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &AndroidVideoDecodeAccelerator::DoIOTask, base::AsWeakPtr(this)),
            DecodePollDelay());
  }
}

void AndroidVideoDecodeAccelerator::QueueInput() {
  if (bitstreams_notified_in_advance_.size() > kMaxBitstreamsNotifiedInAdvance)
    return;
  if (pending_bitstream_buffers_.empty())
    return;

  int input_buf_index = 0;
  media::MediaCodecStatus status = media_codec_->DequeueInputBuffer(
      NoWaitTimeOut(), &input_buf_index);
  if (status != media::MEDIA_CODEC_OK) {
    DCHECK(status == media::MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER ||
           status == media::MEDIA_CODEC_ERROR);
    return;
  }

  media::BitstreamBuffer& bitstream_buffer =
      pending_bitstream_buffers_.front();

  if (bitstream_buffer.id() == -1) {
    media_codec_->QueueEOS(input_buf_index);
    pending_bitstream_buffers_.pop();
    return;
  }
  // Abuse the presentation time argument to propagate the bitstream
  // buffer ID to the output, so we can report it back to the client in
  // PictureReady().
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(bitstream_buffer.id());

  int bytes_written = 0;
  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));

  RETURN_ON_FAILURE(shm->Map(bitstream_buffer.size()),
                    "Failed to SharedMemory::Map()",
                    UNREADABLE_INPUT);

  const size_t offset = num_bytes_used_in_the_pending_buffer_;
  bytes_written = media_codec_->QueueInputBuffer(
          input_buf_index,
          static_cast<const uint8*>(shm->memory()) + offset,
          bitstream_buffer.size() - offset, timestamp);
  num_bytes_used_in_the_pending_buffer_ += bytes_written;
  CHECK_LE(num_bytes_used_in_the_pending_buffer_, bitstream_buffer.size());

  if (num_bytes_used_in_the_pending_buffer_ == bitstream_buffer.size()) {
    num_bytes_used_in_the_pending_buffer_ = 0;
    pending_bitstream_buffers_.pop();

    // We should call NotifyEndOfBitstreamBuffer(), when no more decoded output
    // will be returned from the bitstream buffer. However, MediaCodec API is
    // not enough to guarantee it.
    // So, here, we calls NotifyEndOfBitstreamBuffer() in advance in order to
    // keep getting more bitstreams from the client, and throttle them by using
    // |bitstreams_notified_in_advance_|.
    // TODO(dwkang): check if there is a way to remove this workaround.
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
        base::AsWeakPtr(this), bitstream_buffer.id()));
    bitstreams_notified_in_advance_.push_back(bitstream_buffer.id());
  }
}

void AndroidVideoDecodeAccelerator::DequeueOutput() {
  if (picturebuffers_requested_ && output_picture_buffers_.empty())
    return;

  if (!output_picture_buffers_.empty() && free_picture_ids_.empty()) {
    // Don't have any picture buffer to send. Need to wait more.
    return;
  }

  bool eos = false;
  base::TimeDelta timestamp;
  int32 buf_index = 0;
  do {
    size_t offset = 0;
    size_t size = 0;
    int buf_index = 0;

    media::MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
        NoWaitTimeOut(), &buf_index, &offset, &size, &timestamp, &eos);
    switch (status) {
      case media::MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
      case media::MEDIA_CODEC_ERROR:
        return;

      case media::MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        int32 width, height;
        media_codec_->GetOutputFormat(&width, &height);

        if (!picturebuffers_requested_) {
          picturebuffers_requested_ = true;
          size_ = gfx::Size(width, height);
          base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
              &AndroidVideoDecodeAccelerator::RequestPictureBuffers,
              base::AsWeakPtr(this)));
        } else {
          // TODO(dwkang): support the dynamic resolution change.
          // Currently, we assume that there is no resolution change in the
          // input stream. So, INFO_OUTPUT_FORMAT_CHANGED should not happen
          // more than once. However, we allows it if resolution is the same
          // as the previous one because |media_codec_| can be reset in Reset().
          RETURN_ON_FAILURE(size_ == gfx::Size(width, height),
                            "Dynamic resolution change is not supported.",
                            PLATFORM_FAILURE);
        }
        return;
      }

      case media::MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        RETURN_ON_FAILURE(media_codec_->GetOutputBuffers(),
                          "Cannot get output buffer from MediaCodec.",
                          PLATFORM_FAILURE);
        break;

      case media::MEDIA_CODEC_OK:
        DCHECK_GE(buf_index, 0);
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (buf_index < 0);

  media_codec_->ReleaseOutputBuffer(buf_index, true);

  if (eos) {
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &AndroidVideoDecodeAccelerator::NotifyFlushDone,
        base::AsWeakPtr(this)));
    decoder_met_eos_ = true;
  } else {
    int64 bitstream_buffer_id = timestamp.InMicroseconds();
    SendCurrentSurfaceToClient(static_cast<int32>(bitstream_buffer_id));

    // Removes ids former or equal than the id from decoder. Note that
    // |bitstreams_notified_in_advance_| does not mean bitstream ids in decoder
    // because of frame reordering issue. We just maintain this roughly and use
    // for the throttling purpose.
    std::list<int32>::iterator it;
    for (it = bitstreams_notified_in_advance_.begin();
        it != bitstreams_notified_in_advance_.end();
        ++it) {
      if (*it == bitstream_buffer_id) {
        bitstreams_notified_in_advance_.erase(
            bitstreams_notified_in_advance_.begin(), ++it);
        break;
      }
    }
  }
}

void AndroidVideoDecodeAccelerator::SendCurrentSurfaceToClient(
    int32 bitstream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(bitstream_id, -1);
  DCHECK(!free_picture_ids_.empty());

  RETURN_ON_FAILURE(make_context_current_.Run(),
                    "Failed to make this decoder's GL context current.",
                    PLATFORM_FAILURE);

  int32 picture_buffer_id = free_picture_ids_.front();
  free_picture_ids_.pop();

  float transfrom_matrix[16];
  surface_texture_->UpdateTexImage();
  surface_texture_->GetTransformMatrix(transfrom_matrix);

  OutputBufferMap::const_iterator i =
      output_picture_buffers_.find(picture_buffer_id);
  RETURN_ON_FAILURE(i != output_picture_buffers_.end(),
                    "Can't find a PictureBuffer for " << picture_buffer_id,
                    PLATFORM_FAILURE);
  uint32 picture_buffer_texture_id = i->second.texture_id();

  RETURN_ON_FAILURE(gl_decoder_.get(),
                    "Failed to get gles2 decoder instance.",
                    ILLEGAL_STATE);
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copier_) {
    copier_.reset(new gpu::CopyTextureCHROMIUMResourceManager());
    copier_->Initialize(gl_decoder_.get());
  }

  // Here, we copy |surface_texture_id_| to the picture buffer instead of
  // setting new texture to |surface_texture_| by calling attachToGLContext()
  // because:
  // 1. Once we call detachFrameGLContext(), it deletes the texture previous
  //    attached.
  // 2. SurfaceTexture requires us to apply a transform matrix when we show
  //    the texture.
  copier_->DoCopyTexture(gl_decoder_.get(), GL_TEXTURE_EXTERNAL_OES,
                         GL_TEXTURE_2D, surface_texture_id_,
                         picture_buffer_texture_id, 0, size_.width(),
                         size_.height(), false, false, false);

  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &AndroidVideoDecodeAccelerator::NotifyPictureReady,
      base::AsWeakPtr(this), media::Picture(picture_buffer_id, bitstream_id)));
}

void AndroidVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bitstream_buffer.id() != -1 && bitstream_buffer.size() == 0) {
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
        base::AsWeakPtr(this), bitstream_buffer.id()));
    return;
  }

  pending_bitstream_buffers_.push(bitstream_buffer);

  if (!io_task_is_posted_)
    DoIOTask();
}

void AndroidVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_picture_buffers_.empty());

  for (size_t i = 0; i < buffers.size(); ++i) {
    RETURN_ON_FAILURE(buffers[i].size() == size_,
                      "Invalid picture buffer size was passed.",
                      INVALID_ARGUMENT);
    output_picture_buffers_.insert(std::make_pair(buffers[i].id(), buffers[i]));
    free_picture_ids_.push(buffers[i].id());
  }

  RETURN_ON_FAILURE(output_picture_buffers_.size() == kNumPictureBuffers,
                    "Invalid picture buffers were passed.",
                    INVALID_ARGUMENT);

  if (!io_task_is_posted_)
    DoIOTask();
}

void AndroidVideoDecodeAccelerator::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  free_picture_ids_.push(picture_buffer_id);

  if (!io_task_is_posted_)
    DoIOTask();
}

void AndroidVideoDecodeAccelerator::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Decode(media::BitstreamBuffer(-1, base::SharedMemoryHandle(), 0));
}

bool AndroidVideoDecodeAccelerator::ConfigureMediaCodec() {
  DCHECK(surface_texture_.get());
  media_codec_.reset(media::VideoCodecBridge::Create(codec_, false));

  if (!media_codec_)
    return false;

  gfx::ScopedJavaSurface surface(surface_texture_.get());
  // VDA does not pass the container indicated resolution in the initialization
  // phase. Here, we set 720p by default.
  // TODO(dwkang): find out a way to remove the following hard-coded value.
  media_codec_->Start(
      codec_, gfx::Size(1280, 720), surface.j_surface().obj(), NULL);
  return media_codec_->GetOutputBuffers();
}

void AndroidVideoDecodeAccelerator::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  while (!pending_bitstream_buffers_.empty()) {
    media::BitstreamBuffer& bitstream_buffer =
        pending_bitstream_buffers_.front();
    pending_bitstream_buffers_.pop();

    if (bitstream_buffer.id() != -1) {
      base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
          base::AsWeakPtr(this), bitstream_buffer.id()));
    }
  }
  bitstreams_notified_in_advance_.clear();

  if (!decoder_met_eos_) {
    media_codec_->Reset();
  } else {
    // MediaCodec should be usable after meeting EOS, but it is not on some
    // devices. b/8125974 To avoid the case, we recreate a new one.
    media_codec_->Stop();
    ConfigureMediaCodec();
  }
  decoder_met_eos_ = false;
  num_bytes_used_in_the_pending_buffer_ = 0;
  state_ = NO_ERROR;

  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &AndroidVideoDecodeAccelerator::NotifyResetDone, base::AsWeakPtr(this)));
}

void AndroidVideoDecodeAccelerator::Destroy() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (media_codec_)
    media_codec_->Stop();
  if (surface_texture_id_)
    glDeleteTextures(1, &surface_texture_id_);
  if (copier_)
    copier_->Destroy();
  delete this;
}

void AndroidVideoDecodeAccelerator::NotifyInitializeDone() {
  client_->NotifyInitializeDone();
}

void AndroidVideoDecodeAccelerator::RequestPictureBuffers() {
  client_->ProvidePictureBuffers(kNumPictureBuffers, size_, GL_TEXTURE_2D);
}

void AndroidVideoDecodeAccelerator::NotifyPictureReady(
    const media::Picture& picture) {
  client_->PictureReady(picture);
}

void AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int input_buffer_id) {
  client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void AndroidVideoDecodeAccelerator::NotifyFlushDone() {
  client_->NotifyFlushDone();
}

void AndroidVideoDecodeAccelerator::NotifyResetDone() {
  client_->NotifyResetDone();
}

void AndroidVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  client_->NotifyError(error);
}

}  // namespace content
