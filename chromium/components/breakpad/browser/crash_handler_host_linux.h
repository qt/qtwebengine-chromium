// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREAKPAD_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#define COMPONENTS_BREAKPAD_BROWSER_CRASH_HANDLER_HOST_LINUX_H_

#include <sys/types.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"

namespace base {
class Thread;
}

namespace breakpad {

struct BreakpadInfo;

// This is the host for processes which run breakpad inside the sandbox on
// Linux or Android. We perform the crash dump from the browser because it
// allows us to be outside the sandbox.
//
// Processes signal that they need to be dumped by sending a datagram over a
// UNIX domain socket. All processes of the same type share the client end of
// this socket which is installed in their descriptor table before exec.
class CrashHandlerHostLinux : public base::MessageLoopForIO::Watcher,
                              public base::MessageLoop::DestructionObserver {
 public:
  CrashHandlerHostLinux(const std::string& process_type,
                        const base::FilePath& dumps_path,
                        bool upload);
  virtual ~CrashHandlerHostLinux();

  // Starts the uploader thread. Must be called immediately after creating the
  // class.
  void StartUploaderThread();

  // Get the file descriptor which processes should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket() const {
    return process_socket_;
  }

  // MessagePumbLibevent::Watcher impl:
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;

  // MessageLoop::DestructionObserver impl:
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Whether we are shutting down or not.
  bool IsShuttingDown() const;

 private:
  void Init();

  // Do work on the FILE thread for OnFileCanReadWithoutBlocking().
  void WriteDumpFile(BreakpadInfo* info,
                     pid_t crashing_pid,
                     char* crash_context,
                     int signal_fd);

  // Continue OnFileCanReadWithoutBlocking()'s work on the IO thread.
  void QueueCrashDumpTask(BreakpadInfo* info, int signal_fd);

  std::string process_type_;
  base::FilePath dumps_path_;
  bool upload_;

  int process_socket_;
  int browser_socket_;

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;
  scoped_ptr<base::Thread> uploader_thread_;
  bool shutting_down_;

  // Unique sequence token so that writing crash dump won't be blocked
  // by other tasks.
  base::SequencedWorkerPool::SequenceToken worker_pool_token_;

#if defined(ADDRESS_SANITIZER)
  char* asan_report_str_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHostLinux);
};

}  // namespace breakpad

#endif  // COMPONENTS_BREAKPAD_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
