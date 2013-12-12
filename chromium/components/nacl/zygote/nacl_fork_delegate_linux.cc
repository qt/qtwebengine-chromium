// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/zygote/nacl_fork_delegate_linux.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "components/nacl/common/nacl_helper_linux.h"
#include "components/nacl/common/nacl_paths.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"

namespace {

// Note these need to match up with their counterparts in nacl_helper_linux.c
// and nacl_helper_bootstrap_linux.c.
const char kNaClHelperReservedAtZero[] =
    "--reserved_at_zero=0xXXXXXXXXXXXXXXXX";
const char kNaClHelperRDebug[] = "--r_debug=0xXXXXXXXXXXXXXXXX";

#if defined(ARCH_CPU_X86)
bool NonZeroSegmentBaseIsSlow() {
  base::CPU cpuid;
  // Using a non-zero segment base is known to be very slow on Intel
  // Atom CPUs.  See "Segmentation-based Memory Protection Mechanism
  // on Intel Atom Microarchitecture: Coding Optimizations" (Leonardo
  // Potenza, Intel).
  //
  // The following list of CPU model numbers is taken from:
  // "Intel 64 and IA-32 Architectures Software Developer's Manual"
  // (http://download.intel.com/products/processor/manual/325462.pdf),
  // "Table 35-1. CPUID Signature Values of DisplayFamily_DisplayModel"
  // (Volume 3C, 35-1), which contains:
  //   "06_36H - Intel Atom S Processor Family
  //    06_1CH, 06_26H, 06_27H, 06_35, 06_36 - Intel Atom Processor Family"
  if (cpuid.family() == 6) {
    switch (cpuid.model()) {
      case 0x1c:
      case 0x26:
      case 0x27:
      case 0x35:
      case 0x36:
        return true;
    }
  }
  return false;
}
#endif

// Send an IPC request on |ipc_channel|. The request is contained in
// |request_pickle| and can have file descriptors attached in |attached_fds|.
// |reply_data_buffer| must be allocated by the caller and will contain the
// reply. The size of the reply will be written to |reply_size|.
// This code assumes that only one thread can write to |ipc_channel| to make
// requests.
bool SendIPCRequestAndReadReply(int ipc_channel,
                                const std::vector<int>& attached_fds,
                                const Pickle& request_pickle,
                                char* reply_data_buffer,
                                size_t reply_data_buffer_size,
                                ssize_t* reply_size) {
  DCHECK_LE(static_cast<size_t>(kNaClMaxIPCMessageLength),
            reply_data_buffer_size);
  DCHECK(reply_size);

  if (!UnixDomainSocket::SendMsg(ipc_channel, request_pickle.data(),
                                 request_pickle.size(), attached_fds)) {
    LOG(ERROR) << "SendIPCRequestAndReadReply: SendMsg failed";
    return false;
  }

  // Then read the remote reply.
  std::vector<int> received_fds;
  const ssize_t msg_len =
      UnixDomainSocket::RecvMsg(ipc_channel, reply_data_buffer,
                                reply_data_buffer_size, &received_fds);
  if (msg_len <= 0) {
    LOG(ERROR) << "SendIPCRequestAndReadReply: RecvMsg failed";
    return false;
  }
  *reply_size = msg_len;
  return true;
}

}  // namespace.

NaClForkDelegate::NaClForkDelegate()
    : status_(kNaClHelperUnused),
      fd_(-1) {}

void NaClForkDelegate::Init(const int sandboxdesc) {
  VLOG(1) << "NaClForkDelegate::Init()";
  int fds[2];

  // Confirm a hard-wired assumption.
  // The NaCl constant is from chrome/nacl/nacl_linux_helper.h
  DCHECK(kNaClSandboxDescriptor == sandboxdesc);

  CHECK(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  base::FileHandleMappingVector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], kNaClZygoteDescriptor));
  fds_to_map.push_back(std::make_pair(sandboxdesc, kNaClSandboxDescriptor));

  // Using nacl_helper_bootstrap is not necessary on x86-64 because
  // NaCl's x86-64 sandbox is not zero-address-based.  Starting
  // nacl_helper through nacl_helper_bootstrap works on x86-64, but it
  // leaves nacl_helper_bootstrap mapped at a fixed address at the
  // bottom of the address space, which is undesirable because it
  // effectively defeats ASLR.
#if defined(ARCH_CPU_X86_64)
  bool kUseNaClBootstrap = false;
#elif defined(ARCH_CPU_X86)
  // Performance vs. security trade-off: We prefer using a
  // non-zero-address-based sandbox on x86-32 because it provides some
  // ASLR and so is more secure.  However, on Atom CPUs, using a
  // non-zero segment base is very slow, so we use a zero-based
  // sandbox on those.
  bool kUseNaClBootstrap = NonZeroSegmentBaseIsSlow();
#else
  bool kUseNaClBootstrap = true;
#endif

  status_ = kNaClHelperUnused;
  base::FilePath helper_exe;
  base::FilePath helper_bootstrap_exe;
  if (!PathService::Get(nacl::FILE_NACL_HELPER, &helper_exe)) {
    status_ = kNaClHelperMissing;
  } else if (kUseNaClBootstrap &&
             !PathService::Get(nacl::FILE_NACL_HELPER_BOOTSTRAP,
                               &helper_bootstrap_exe)) {
    status_ = kNaClHelperBootstrapMissing;
  } else if (RunningOnValgrind()) {
    status_ = kNaClHelperValgrind;
  } else {
    CommandLine::StringVector argv_to_launch;
    {
      CommandLine cmd_line(CommandLine::NO_PROGRAM);
      if (kUseNaClBootstrap)
        cmd_line.SetProgram(helper_bootstrap_exe);
      else
        cmd_line.SetProgram(helper_exe);

      // Append any switches that need to be forwarded to the NaCl helper.
      static const char* kForwardSwitches[] = {
        switches::kDisableSeccompFilterSandbox,
        switches::kNoSandbox,
      };
      const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();
      cmd_line.CopySwitchesFrom(current_cmd_line, kForwardSwitches,
                                arraysize(kForwardSwitches));

      // The command line needs to be tightly controlled to use
      // |helper_bootstrap_exe|. So from now on, argv_to_launch should be
      // modified directly.
      argv_to_launch = cmd_line.argv();
    }
    if (kUseNaClBootstrap) {
      // Arguments to the bootstrap helper which need to be at the start
      // of the command line, right after the helper's path.
      CommandLine::StringVector bootstrap_prepend;
      bootstrap_prepend.push_back(helper_exe.value());
      bootstrap_prepend.push_back(kNaClHelperReservedAtZero);
      bootstrap_prepend.push_back(kNaClHelperRDebug);
      argv_to_launch.insert(argv_to_launch.begin() + 1,
                            bootstrap_prepend.begin(),
                            bootstrap_prepend.end());
    }
    base::LaunchOptions options;
    options.fds_to_remap = &fds_to_map;
    options.clone_flags = CLONE_FS | SIGCHLD;

    // The NaCl processes spawned may need to exceed the ambient soft limit
    // on RLIMIT_AS to allocate the untrusted address space and its guard
    // regions.  The nacl_helper itself cannot just raise its own limit,
    // because the existing limit may prevent the initial exec of
    // nacl_helper_bootstrap from succeeding, with its large address space
    // reservation.
    std::set<int> max_these_limits;
    max_these_limits.insert(RLIMIT_AS);
    options.maximize_rlimits = &max_these_limits;

    if (!base::LaunchProcess(argv_to_launch, options, NULL))
      status_ = kNaClHelperLaunchFailed;
    // parent and error cases are handled below
  }
  if (HANDLE_EINTR(close(fds[1])) != 0)
    LOG(ERROR) << "close(fds[1]) failed";
  if (status_ == kNaClHelperUnused) {
    const ssize_t kExpectedLength = strlen(kNaClHelperStartupAck);
    char buf[kExpectedLength];

    // Wait for ack from nacl_helper, indicating it is ready to help
    const ssize_t nread = HANDLE_EINTR(read(fds[0], buf, sizeof(buf)));
    if (nread == kExpectedLength &&
        memcmp(buf, kNaClHelperStartupAck, nread) == 0) {
      // all is well
      status_ = kNaClHelperSuccess;
      fd_ = fds[0];
      return;
    }

    status_ = kNaClHelperAckFailed;
    LOG(ERROR) << "Bad NaCl helper startup ack (" << nread << " bytes)";
  }
  // TODO(bradchen): Make this LOG(ERROR) when the NaCl helper
  // becomes the default.
  fd_ = -1;
  if (HANDLE_EINTR(close(fds[0])) != 0)
    LOG(ERROR) << "close(fds[0]) failed";
}

void NaClForkDelegate::InitialUMA(std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value) {
  *uma_name = "NaCl.Client.Helper.InitState";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
}

NaClForkDelegate::~NaClForkDelegate() {
  // side effect of close: delegate process will terminate
  if (status_ == kNaClHelperSuccess) {
    if (HANDLE_EINTR(close(fd_)) != 0)
      LOG(ERROR) << "close(fd_) failed";
  }
}

bool NaClForkDelegate::CanHelp(const std::string& process_type,
                               std::string* uma_name,
                               int* uma_sample,
                               int* uma_boundary_value) {
  if (process_type != switches::kNaClLoaderProcess)
    return false;
  *uma_name = "NaCl.Client.Helper.StateOnFork";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
  return status_ == kNaClHelperSuccess;
}

pid_t NaClForkDelegate::Fork(const std::vector<int>& fds) {
  VLOG(1) << "NaClForkDelegate::Fork";

  DCHECK(fds.size() == kNaClParentFDIndex + 1);

  // First, send a remote fork request.
  Pickle write_pickle;
  write_pickle.WriteInt(kNaClForkRequest);

  char reply_buf[kNaClMaxIPCMessageLength];
  ssize_t reply_size = 0;
  bool got_reply =
      SendIPCRequestAndReadReply(fd_, fds, write_pickle,
                                 reply_buf, sizeof(reply_buf), &reply_size);
  if (!got_reply) {
    LOG(ERROR) << "Could not perform remote fork.";
    return -1;
  }

  // Now see if the other end managed to fork.
  Pickle reply_pickle(reply_buf, reply_size);
  PickleIterator iter(reply_pickle);
  pid_t nacl_child;
  if (!iter.ReadInt(&nacl_child)) {
    LOG(ERROR) << "NaClForkDelegate::Fork: pickle failed";
    return -1;
  }
  VLOG(1) << "nacl_child is " << nacl_child;
  return nacl_child;
}

bool NaClForkDelegate::AckChild(const int fd,
                                const std::string& channel_switch) {
  int nwritten = HANDLE_EINTR(write(fd, channel_switch.c_str(),
                                    channel_switch.length()));
  if (nwritten != static_cast<int>(channel_switch.length())) {
    return false;
  }
  return true;
}

bool NaClForkDelegate::GetTerminationStatus(pid_t pid, bool known_dead,
                                            base::TerminationStatus* status,
                                            int* exit_code) {
  VLOG(1) << "NaClForkDelegate::GetTerminationStatus";
  DCHECK(status);
  DCHECK(exit_code);

  Pickle write_pickle;
  write_pickle.WriteInt(kNaClGetTerminationStatusRequest);
  write_pickle.WriteInt(pid);
  write_pickle.WriteBool(known_dead);

  const std::vector<int> empty_fds;
  char reply_buf[kNaClMaxIPCMessageLength];
  ssize_t reply_size = 0;
  bool got_reply =
      SendIPCRequestAndReadReply(fd_, empty_fds, write_pickle,
                                 reply_buf, sizeof(reply_buf), &reply_size);
  if (!got_reply) {
    LOG(ERROR) << "Could not perform remote GetTerminationStatus.";
    return false;
  }

  Pickle reply_pickle(reply_buf, reply_size);
  PickleIterator iter(reply_pickle);
  int termination_status;
  if (!iter.ReadInt(&termination_status) ||
      termination_status < 0 ||
      termination_status >= base::TERMINATION_STATUS_MAX_ENUM) {
    LOG(ERROR) << "GetTerminationStatus: pickle failed";
    return false;
  }

  int remote_exit_code;
  if (!iter.ReadInt(&remote_exit_code)) {
    LOG(ERROR) << "GetTerminationStatus: pickle failed";
    return false;
  }

  *status = static_cast<base::TerminationStatus>(termination_status);
  *exit_code = remote_exit_code;
  return true;
}
