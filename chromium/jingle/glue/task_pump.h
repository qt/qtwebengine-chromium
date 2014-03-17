// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_TASK_PUMP_H_
#define JINGLE_GLUE_TASK_PUMP_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "third_party/libjingle/source/talk/base/taskrunner.h"

namespace jingle_glue {

// talk_base::TaskRunner implementation that works on chromium threads.
class TaskPump : public talk_base::TaskRunner, public base::NonThreadSafe {
 public:
  TaskPump();

  virtual ~TaskPump();

  // talk_base::TaskRunner implementation.
  virtual void WakeTasks() OVERRIDE;
  virtual int64 CurrentTime() OVERRIDE;

  // No tasks will be processed after this is called, even if
  // WakeTasks() is called.
  void Stop();

 private:
  void CheckAndRunTasks();

  bool posted_wake_;
  bool stopped_;

  base::WeakPtrFactory<TaskPump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskPump);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_TASK_PUMP_H_
