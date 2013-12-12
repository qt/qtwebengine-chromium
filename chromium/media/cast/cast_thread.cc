// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_thread.h"

#include "base/logging.h"

using base::TaskRunner;

namespace media {
namespace cast {

CastThread::CastThread(
    scoped_refptr<TaskRunner> main_thread_proxy,
    scoped_refptr<TaskRunner> audio_encode_thread_proxy,
    scoped_refptr<TaskRunner> audio_decode_thread_proxy,
    scoped_refptr<TaskRunner> video_encode_thread_proxy,
    scoped_refptr<TaskRunner> video_decode_thread_proxy)
    : main_thread_proxy_(main_thread_proxy),
      audio_encode_thread_proxy_(audio_encode_thread_proxy),
      audio_decode_thread_proxy_(audio_decode_thread_proxy),
      video_encode_thread_proxy_(video_encode_thread_proxy),
      video_decode_thread_proxy_(video_decode_thread_proxy) {
  DCHECK(main_thread_proxy) << "Main thread required";
}

bool CastThread::PostTask(ThreadId identifier,
                          const tracked_objects::Location& from_here,
                          const base::Closure& task) {
  scoped_refptr<TaskRunner> task_runner =
      GetMessageTaskRunnerForThread(identifier);

  return task_runner->PostTask(from_here, task);
}

bool CastThread::PostDelayedTask(ThreadId identifier,
                                 const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 base::TimeDelta delay) {
  scoped_refptr<TaskRunner> task_runner =
      GetMessageTaskRunnerForThread(identifier);

  return task_runner->PostDelayedTask(from_here, task, delay);
}

scoped_refptr<TaskRunner> CastThread::GetMessageTaskRunnerForThread(
      ThreadId identifier) {
  switch (identifier) {
    case CastThread::MAIN:
      return main_thread_proxy_;
    case CastThread::AUDIO_ENCODER:
      return audio_encode_thread_proxy_;
    case CastThread::AUDIO_DECODER:
      return audio_decode_thread_proxy_;
    case CastThread::VIDEO_ENCODER:
      return video_encode_thread_proxy_;
    case CastThread::VIDEO_DECODER:
      return video_decode_thread_proxy_;
  }
}

}  // namespace cast
}  // namespace media
