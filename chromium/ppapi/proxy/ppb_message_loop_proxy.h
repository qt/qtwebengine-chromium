// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
#define PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"
#include "ppapi/thunk/ppb_message_loop_api.h"

struct PPB_MessageLoop_1_0;

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT MessageLoopResource : public MessageLoopShared {
 public:
  explicit MessageLoopResource(PP_Instance instance);
  // Construct the one MessageLoopResource for the main thread. This must be
  // invoked on the main thread.
  explicit MessageLoopResource(ForMainThread);
  virtual ~MessageLoopResource();

  // Resource overrides.
  virtual thunk::PPB_MessageLoop_API* AsPPB_MessageLoop_API() OVERRIDE;

  // PPB_MessageLoop_API implementation.
  virtual int32_t AttachToCurrentThread() OVERRIDE;
  virtual int32_t Run() OVERRIDE;
  virtual int32_t PostWork(PP_CompletionCallback callback,
                           int64_t delay_ms) OVERRIDE;
  virtual int32_t PostQuit(PP_Bool should_destroy) OVERRIDE;

  static MessageLoopResource* GetCurrent();
  void DetachFromThread();
  bool is_main_thread_loop() const {
    return is_main_thread_loop_;
  }

 private:
  struct TaskInfo {
    tracked_objects::Location from_here;
    base::Closure closure;
    int64 delay_ms;
  };

  // Returns true if the object is associated with the current thread.
  bool IsCurrent() const;

  // Handles posting to the message loop if there is one, or the pending queue
  // if there isn't.
  // NOTE: The given closure will be run *WITHOUT* acquiring the Proxy lock.
  //       This only makes sense for user code and completely thread-safe
  //       proxy operations (e.g., MessageLoop::QuitClosure).
  virtual void PostClosure(const tracked_objects::Location& from_here,
                           const base::Closure& closure,
                           int64 delay_ms) OVERRIDE;

  virtual base::MessageLoopProxy* GetMessageLoopProxy() OVERRIDE;

  // TLS destructor function.
  static void ReleaseMessageLoop(void* value);

  // Created when we attach to the current thread, since MessageLoop assumes
  // that it's created on the thread it will run on. NULL for the main thread
  // loop, since that's owned by somebody else. This is needed for Run and Quit.
  // Any time we post tasks, we should post them using loop_proxy_.
  scoped_ptr<base::MessageLoop> loop_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;

  // Number of invocations of Run currently on the stack.
  int nested_invocations_;

  // Set to true when the message loop is destroyed to prevent forther
  // posting of work.
  bool destroyed_;

  // Set to true if all message loop invocations should exit and that the
  // loop should be destroyed once it reaches the outermost Run invocation.
  bool should_destroy_;

  bool is_main_thread_loop_;

  // Since we allow tasks to be posted before the message loop is actually
  // created (when it's associated with a thread), we keep tasks posted here
  // until that happens. Once the loop_ is created, this is unused.
  std::vector<TaskInfo> pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopResource);
};

class PPB_MessageLoop_Proxy : public InterfaceProxy {
 public:
  explicit PPB_MessageLoop_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_MessageLoop_Proxy();

  static const PPB_MessageLoop_1_0* GetInterface();

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_MessageLoop_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
