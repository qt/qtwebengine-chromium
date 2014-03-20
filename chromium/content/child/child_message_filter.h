// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CROSS_MESSAGE_FILTER_H_
#define CONTENT_CHILD_CROSS_MESSAGE_FILTER_H_

#include "ipc/ipc_channel_proxy.h"

namespace base {
class TaskRunner;
}

namespace content {

class ThreadSafeSender;

// A base class for implementing IPC MessageFilter's that run on a different
// thread or TaskRunner than the main thread.
class ChildMessageFilter
    : public base::RefCountedThreadSafe<ChildMessageFilter>,
      public IPC::Sender {
 public:
  // IPC::Sender implementation. Can be called on any threads.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // If implementers want to run OnMessageReceived on a different task
  // runner it should override this and return the TaskRunner for the message.
  // Returning NULL runs OnMessageReceived() on the current IPC thread.
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) = 0;

  // If OverrideTaskRunnerForMessage is overriden and returns non-null
  // this will be called on the returned TaskRunner.
  virtual bool OnMessageReceived(const IPC::Message& msg) = 0;

  // This method is called when WorkerTaskRunner::PostTask() returned false
  // for the target thread.  Note that there's still a little chance that
  // PostTask() returns true but OnMessageReceived() is never called on the
  // target thread.  By default this does nothing.
  virtual void OnStaleMessageReceived(const IPC::Message& msg) {}

 protected:
  ChildMessageFilter();
  virtual ~ChildMessageFilter();

 private:
  class Internal;
  friend class ChildThread;
  friend class RenderThreadImpl;
  friend class WorkerThread;

  friend class base::RefCountedThreadSafe<ChildMessageFilter>;

  IPC::ChannelProxy::MessageFilter* GetFilter();

  // This implements IPC::ChannelProxy::MessageFilter to hide the actual
  // filter methods from child classes.
  Internal* internal_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ChildMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_CROSS_MESSAGE_FILTER_H_
