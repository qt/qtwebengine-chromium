// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/zygote/zygote_linux.h"

#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/kill.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/set_process_title.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"

// See http://code.google.com/p/chromium/wiki/LinuxZygote

namespace content {

namespace {

// NOP function. See below where this handler is installed.
void SIGCHLDHandler(int signal) {
}

int LookUpFd(const base::GlobalDescriptors::Mapping& fd_mapping, uint32_t key) {
  for (size_t index = 0; index < fd_mapping.size(); ++index) {
    if (fd_mapping[index].first == key)
      return fd_mapping[index].second;
  }
  return -1;
}

}  // namespace

Zygote::Zygote(int sandbox_flags,
               ZygoteForkDelegate* helper)
    : sandbox_flags_(sandbox_flags),
      helper_(helper),
      initial_uma_sample_(0),
      initial_uma_boundary_value_(0) {
  if (helper_) {
    helper_->InitialUMA(&initial_uma_name_,
                        &initial_uma_sample_,
                        &initial_uma_boundary_value_);
  }
}

Zygote::~Zygote() {
}

bool Zygote::ProcessRequests() {
  // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
  // browser on it.
  // A SOCK_DGRAM is installed in fd 5. This is the sandbox IPC channel.
  // See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = &SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  if (UsingSUIDSandbox()) {
    // Let the ZygoteHost know we are ready to go.
    // The receiving code is in content/browser/zygote_host_linux.cc.
    std::vector<int> empty;
    bool r = UnixDomainSocket::SendMsg(kZygoteSocketPairFd,
                                       kZygoteHelloMessage,
                                       sizeof(kZygoteHelloMessage), empty);
#if defined(OS_CHROMEOS)
    LOG_IF(WARNING, !r) << "Sending zygote magic failed";
    // Exit normally on chromeos because session manager may send SIGTERM
    // right after the process starts and it may fail to send zygote magic
    // number to browser process.
    if (!r)
      _exit(RESULT_CODE_NORMAL_EXIT);
#else
    CHECK(r) << "Sending zygote magic failed";
#endif
  }

  for (;;) {
    // This function call can return multiple times, once per fork().
    if (HandleRequestFromBrowser(kZygoteSocketPairFd))
      return true;
  }
}

bool Zygote::GetProcessInfo(base::ProcessHandle pid,
                            ZygoteProcessInfo* process_info) {
  DCHECK(process_info);
  const ZygoteProcessMap::const_iterator it = process_info_map_.find(pid);
  if (it == process_info_map_.end()) {
    return false;
  }
  *process_info = it->second;
  return true;
}

bool Zygote::UsingSUIDSandbox() const {
  return sandbox_flags_ & kSandboxLinuxSUID;
}

bool Zygote::HandleRequestFromBrowser(int fd) {
  std::vector<int> fds;
  char buf[kZygoteMaxMessageLength];
  const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);

  if (len == 0 || (len == -1 && errno == ECONNRESET)) {
    // EOF from the browser. We should die.
    _exit(0);
    return false;
  }

  if (len == -1) {
    PLOG(ERROR) << "Error reading message from browser";
    return false;
  }

  Pickle pickle(buf, len);
  PickleIterator iter(pickle);

  int kind;
  if (pickle.ReadInt(&iter, &kind)) {
    switch (kind) {
      case kZygoteCommandFork:
        // This function call can return multiple times, once per fork().
        return HandleForkRequest(fd, pickle, iter, fds);

      case kZygoteCommandReap:
        if (!fds.empty())
          break;
        HandleReapRequest(fd, pickle, iter);
        return false;
      case kZygoteCommandGetTerminationStatus:
        if (!fds.empty())
          break;
        HandleGetTerminationStatus(fd, pickle, iter);
        return false;
      case kZygoteCommandGetSandboxStatus:
        HandleGetSandboxStatus(fd, pickle, iter);
        return false;
      default:
        NOTREACHED();
        break;
    }
  }

  LOG(WARNING) << "Error parsing message from browser";
  for (std::vector<int>::const_iterator
       i = fds.begin(); i != fds.end(); ++i)
    close(*i);
  return false;
}

// TODO(jln): remove callers to this broken API. See crbug.com/274855.
void Zygote::HandleReapRequest(int fd,
                               const Pickle& pickle,
                               PickleIterator iter) {
  base::ProcessId child;

  if (!pickle.ReadInt(&iter, &child)) {
    LOG(WARNING) << "Error parsing reap request from browser";
    return;
  }

  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(child, &child_info)) {
    LOG(ERROR) << "Child not found!";
    NOTREACHED();
    return;
  }

  if (!child_info.started_from_helper) {
    // TODO(jln): this old code is completely broken. See crbug.com/274855.
    base::EnsureProcessTerminated(child_info.internal_pid);
  } else {
    // For processes from the helper, send a GetTerminationStatus request
    // with known_dead set to true.
    // This is not perfect, as the process may be killed instantly, but is
    // better than ignoring the request.
    base::TerminationStatus status;
    int exit_code;
    bool got_termination_status =
        GetTerminationStatus(child, true /* known_dead */, &status, &exit_code);
    DCHECK(got_termination_status);
  }
  process_info_map_.erase(child);
}

bool Zygote::GetTerminationStatus(base::ProcessHandle real_pid,
                                  bool known_dead,
                                  base::TerminationStatus* status,
                                  int* exit_code) {

  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(real_pid, &child_info)) {
    LOG(ERROR) << "Zygote::GetTerminationStatus for unknown PID "
               << real_pid;
    NOTREACHED();
    return false;
  }
  // We know about |real_pid|.
  const base::ProcessHandle child = child_info.internal_pid;
  if (child_info.started_from_helper) {
    // Let the helper handle the request.
    DCHECK(helper_);
    if (!helper_->GetTerminationStatus(child, known_dead, status, exit_code)) {
      return false;
    }
  } else {
    // Handle the request directly.
    if (known_dead) {
      *status = base::GetKnownDeadTerminationStatus(child, exit_code);
    } else {
      // We don't know if the process is dying, so get its status but don't
      // wait.
      *status = base::GetTerminationStatus(child, exit_code);
    }
  }
  // Successfully got a status for |real_pid|.
  if (*status != base::TERMINATION_STATUS_STILL_RUNNING) {
    // Time to forget about this process.
    process_info_map_.erase(real_pid);
  }
  return true;
}

void Zygote::HandleGetTerminationStatus(int fd,
                                        const Pickle& pickle,
                                        PickleIterator iter) {
  bool known_dead;
  base::ProcessHandle child_requested;

  if (!pickle.ReadBool(&iter, &known_dead) ||
      !pickle.ReadInt(&iter, &child_requested)) {
    LOG(WARNING) << "Error parsing GetTerminationStatus request "
                 << "from browser";
    return;
  }

  base::TerminationStatus status;
  int exit_code;

  bool got_termination_status =
      GetTerminationStatus(child_requested, known_dead, &status, &exit_code);
  if (!got_termination_status) {
    // Assume that if we can't find the child in the sandbox, then
    // it terminated normally.
    NOTREACHED();
    status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    exit_code = RESULT_CODE_NORMAL_EXIT;
  }

  Pickle write_pickle;
  write_pickle.WriteInt(static_cast<int>(status));
  write_pickle.WriteInt(exit_code);
  ssize_t written =
      HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));
  if (written != static_cast<ssize_t>(write_pickle.size()))
    PLOG(ERROR) << "write";
}

int Zygote::ForkWithRealPid(const std::string& process_type,
                            const base::GlobalDescriptors::Mapping& fd_mapping,
                            const std::string& channel_switch,
                            std::string* uma_name,
                            int* uma_sample,
                            int* uma_boundary_value) {
  const bool use_helper = (helper_ && helper_->CanHelp(process_type,
                                                       uma_name,
                                                       uma_sample,
                                                       uma_boundary_value));
  int dummy_fd;
  ino_t dummy_inode;
  int pipe_fds[2] = { -1, -1 };
  base::ProcessId pid = 0;

  dummy_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
  if (dummy_fd < 0) {
    LOG(ERROR) << "Failed to create dummy FD";
    goto error;
  }
  if (!base::FileDescriptorGetInode(&dummy_inode, dummy_fd)) {
    LOG(ERROR) << "Failed to get inode for dummy FD";
    goto error;
  }
  if (pipe(pipe_fds) != 0) {
    LOG(ERROR) << "Failed to create pipe";
    goto error;
  }

  if (use_helper) {
    std::vector<int> fds;
    int ipc_channel_fd = LookUpFd(fd_mapping, kPrimaryIPCChannel);
    if (ipc_channel_fd < 0) {
      DLOG(ERROR) << "Failed to find kPrimaryIPCChannel in FD mapping";
      goto error;
    }
    fds.push_back(ipc_channel_fd);  // kBrowserFDIndex
    fds.push_back(dummy_fd);  // kDummyFDIndex
    fds.push_back(pipe_fds[0]);  // kParentFDIndex
    pid = helper_->Fork(fds);
  } else {
    pid = fork();
  }
  if (pid < 0) {
    goto error;
  } else if (pid == 0) {
    // In the child process.
    close(pipe_fds[1]);
    base::ProcessId real_pid;
    // Wait until the parent process has discovered our PID.  We
    // should not fork any child processes (which the seccomp
    // sandbox does) until then, because that can interfere with the
    // parent's discovery of our PID.
    if (!base::ReadFromFD(pipe_fds[0], reinterpret_cast<char*>(&real_pid),
                          sizeof(real_pid))) {
      LOG(FATAL) << "Failed to synchronise with parent zygote process";
    }
    if (real_pid <= 0) {
      LOG(FATAL) << "Invalid pid from parent zygote";
    }
#if defined(OS_LINUX)
    // Sandboxed processes need to send the global, non-namespaced PID when
    // setting up an IPC channel to their parent.
    IPC::Channel::SetGlobalPid(real_pid);
    // Force the real PID so chrome event data have a PID that corresponds
    // to system trace event data.
    base::debug::TraceLog::GetInstance()->SetProcessID(
        static_cast<int>(real_pid));
#endif
    close(pipe_fds[0]);
    close(dummy_fd);
    return 0;
  } else {
    // In the parent process.
    close(dummy_fd);
    dummy_fd = -1;
    close(pipe_fds[0]);
    pipe_fds[0] = -1;
    base::ProcessId real_pid;
    if (UsingSUIDSandbox()) {
      uint8_t reply_buf[512];
      Pickle request;
      request.WriteInt(LinuxSandbox::METHOD_GET_CHILD_WITH_INODE);
      request.WriteUInt64(dummy_inode);

      const ssize_t r = UnixDomainSocket::SendRecvMsg(
          GetSandboxFD(), reply_buf, sizeof(reply_buf), NULL,
          request);
      if (r == -1) {
        LOG(ERROR) << "Failed to get child process's real PID";
        goto error;
      }

      Pickle reply(reinterpret_cast<char*>(reply_buf), r);
      PickleIterator iter(reply);
      if (!reply.ReadInt(&iter, &real_pid))
        goto error;
      if (real_pid <= 0) {
        // METHOD_GET_CHILD_WITH_INODE failed. Did the child die already?
        LOG(ERROR) << "METHOD_GET_CHILD_WITH_INODE failed";
        goto error;
      }
    } else {
      // If no SUID sandbox is involved then no pid translation is
      // necessary.
      real_pid = pid;
    }

    // Now set-up this process to be tracked by the Zygote.
    if (process_info_map_.find(real_pid) != process_info_map_.end()) {
      LOG(ERROR) << "Already tracking PID " << real_pid;
      NOTREACHED();
    }
    process_info_map_[real_pid].internal_pid = pid;
    process_info_map_[real_pid].started_from_helper = use_helper;

    if (use_helper) {
      if (!helper_->AckChild(pipe_fds[1], channel_switch)) {
        LOG(ERROR) << "Failed to synchronise with zygote fork helper";
        goto error;
      }
    } else {
      int written =
          HANDLE_EINTR(write(pipe_fds[1], &real_pid, sizeof(real_pid)));
      if (written != sizeof(real_pid)) {
        LOG(ERROR) << "Failed to synchronise with child process";
        goto error;
      }
    }
    close(pipe_fds[1]);
    return real_pid;
  }

 error:
  if (pid > 0) {
    if (waitpid(pid, NULL, WNOHANG) == -1)
      LOG(ERROR) << "Failed to wait for process";
  }
  if (dummy_fd >= 0)
    close(dummy_fd);
  if (pipe_fds[0] >= 0)
    close(pipe_fds[0]);
  if (pipe_fds[1] >= 0)
    close(pipe_fds[1]);
  return -1;
}

base::ProcessId Zygote::ReadArgsAndFork(const Pickle& pickle,
                                        PickleIterator iter,
                                        std::vector<int>& fds,
                                        std::string* uma_name,
                                        int* uma_sample,
                                        int* uma_boundary_value) {
  std::vector<std::string> args;
  int argc = 0;
  int numfds = 0;
  base::GlobalDescriptors::Mapping mapping;
  std::string process_type;
  std::string channel_id;
  const std::string channel_id_prefix = std::string("--")
      + switches::kProcessChannelID + std::string("=");

  if (!pickle.ReadString(&iter, &process_type))
    return -1;
  if (!pickle.ReadInt(&iter, &argc))
    return -1;

  for (int i = 0; i < argc; ++i) {
    std::string arg;
    if (!pickle.ReadString(&iter, &arg))
      return -1;
    args.push_back(arg);
    if (arg.compare(0, channel_id_prefix.length(), channel_id_prefix) == 0)
      channel_id = arg;
  }

  if (!pickle.ReadInt(&iter, &numfds))
    return -1;
  if (numfds != static_cast<int>(fds.size()))
    return -1;

  for (int i = 0; i < numfds; ++i) {
    base::GlobalDescriptors::Key key;
    if (!pickle.ReadUInt32(&iter, &key))
      return -1;
    mapping.push_back(std::make_pair(key, fds[i]));
  }

  mapping.push_back(std::make_pair(
      static_cast<uint32_t>(kSandboxIPCChannel), GetSandboxFD()));

  // Returns twice, once per process.
  base::ProcessId child_pid = ForkWithRealPid(process_type, mapping, channel_id,
                                              uma_name, uma_sample,
                                              uma_boundary_value);
  if (!child_pid) {
    // This is the child process.

    close(kZygoteSocketPairFd);  // Our socket from the browser.
    if (UsingSUIDSandbox())
      close(kZygoteIdFd);  // Another socket from the browser.
    base::GlobalDescriptors::GetInstance()->Reset(mapping);

    // Reset the process-wide command line to our new command line.
    CommandLine::Reset();
    CommandLine::Init(0, NULL);
    CommandLine::ForCurrentProcess()->InitFromArgv(args);

    // Update the process title. The argv was already cached by the call to
    // SetProcessTitleFromCommandLine in ChromeMain, so we can pass NULL here
    // (we don't have the original argv at this point).
    SetProcessTitleFromCommandLine(NULL);
  } else if (child_pid < 0) {
    LOG(ERROR) << "Zygote could not fork: process_type " << process_type
        << " numfds " << numfds << " child_pid " << child_pid;
  }
  return child_pid;
}

bool Zygote::HandleForkRequest(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               std::vector<int>& fds) {
  std::string uma_name;
  int uma_sample;
  int uma_boundary_value;
  base::ProcessId child_pid = ReadArgsAndFork(pickle, iter, fds,
                                              &uma_name, &uma_sample,
                                              &uma_boundary_value);
  if (child_pid == 0)
    return true;
  for (std::vector<int>::const_iterator
       i = fds.begin(); i != fds.end(); ++i)
    close(*i);
  if (uma_name.empty()) {
    // There is no UMA report from this particular fork.
    // Use the initial UMA report if any, and clear that record for next time.
    // Note the swap method here is the efficient way to do this, since
    // we know uma_name is empty.
    uma_name.swap(initial_uma_name_);
    uma_sample = initial_uma_sample_;
    uma_boundary_value = initial_uma_boundary_value_;
  }
  // Must always send reply, as ZygoteHost blocks while waiting for it.
  Pickle reply_pickle;
  reply_pickle.WriteInt(child_pid);
  reply_pickle.WriteString(uma_name);
  if (!uma_name.empty()) {
    reply_pickle.WriteInt(uma_sample);
    reply_pickle.WriteInt(uma_boundary_value);
  }
  if (HANDLE_EINTR(write(fd, reply_pickle.data(), reply_pickle.size())) !=
      static_cast<ssize_t> (reply_pickle.size()))
    PLOG(ERROR) << "write";
  return false;
}

bool Zygote::HandleGetSandboxStatus(int fd,
                                    const Pickle& pickle,
                                    PickleIterator iter) {
  if (HANDLE_EINTR(write(fd, &sandbox_flags_, sizeof(sandbox_flags_))) !=
                   sizeof(sandbox_flags_)) {
    PLOG(ERROR) << "write";
  }

  return false;
}

}  // namespace content
