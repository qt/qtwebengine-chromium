// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_process_host.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/common/nacl_cmd_line.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_listen_socket.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_nacl_channel_args.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "components/nacl/browser/nacl_broker_service_win.h"
#include "components/nacl/common/nacl_debug_exception_handler_win.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#endif

using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;
using ppapi::proxy::SerializedHandle;

#if defined(OS_WIN)

namespace {

// Looks for the largest contiguous unallocated region of address
// space and returns it via |*out_addr| and |*out_size|.
void FindAddressSpace(base::ProcessHandle process,
                      char** out_addr, size_t* out_size) {
  *out_addr = NULL;
  *out_size = 0;
  char* addr = 0;
  while (true) {
    MEMORY_BASIC_INFORMATION info;
    size_t result = VirtualQueryEx(process, static_cast<void*>(addr),
                                   &info, sizeof(info));
    if (result < sizeof(info))
      break;
    if (info.State == MEM_FREE && info.RegionSize > *out_size) {
      *out_addr = addr;
      *out_size = info.RegionSize;
    }
    addr += info.RegionSize;
  }
}

}  // namespace

namespace nacl {

// Allocates |size| bytes of address space in the given process at a
// randomised address.
void* AllocateAddressSpaceASLR(base::ProcessHandle process, size_t size) {
  char* addr;
  size_t avail_size;
  FindAddressSpace(process, &addr, &avail_size);
  if (avail_size < size)
    return NULL;
  size_t offset = base::RandGenerator(avail_size - size);
  const int kPageSize = 0x10000;
  void* request_addr =
      reinterpret_cast<void*>(reinterpret_cast<uint64>(addr + offset)
                              & ~(kPageSize - 1));
  return VirtualAllocEx(process, request_addr, size,
                        MEM_RESERVE, PAGE_NOACCESS);
}

}  // namespace nacl

#endif  // defined(OS_WIN)

namespace {

#if defined(OS_WIN)
bool RunningOnWOW64() {
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
}

// NOTE: changes to this class need to be reviewed by the security team.
class NaClSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  NaClSandboxedProcessLauncherDelegate() {}
  virtual ~NaClSandboxedProcessLauncherDelegate() {}

  virtual void PostSpawnTarget(base::ProcessHandle process) {
    // For Native Client sel_ldr processes on 32-bit Windows, reserve 1 GB of
    // address space to prevent later failure due to address space fragmentation
    // from .dll loading. The NaCl process will attempt to locate this space by
    // scanning the address space using VirtualQuery.
    // TODO(bbudge) Handle the --no-sandbox case.
    // http://code.google.com/p/nativeclient/issues/detail?id=2131
    const SIZE_T kNaClSandboxSize = 1 << 30;
    if (!nacl::AllocateAddressSpaceASLR(process, kNaClSandboxSize)) {
      DLOG(WARNING) << "Failed to reserve address space for Native Client";
    }
  }
};

#endif  // OS_WIN

void SetCloseOnExec(NaClHandle fd) {
#if defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFD);
  CHECK_NE(flags, -1);
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  CHECK_EQ(rc, 0);
#endif
}

bool ShareHandleToSelLdr(
    base::ProcessHandle processh,
    NaClHandle sourceh,
    bool close_source,
    std::vector<nacl::FileDescriptor> *handles_for_sel_ldr) {
#if defined(OS_WIN)
  HANDLE channel;
  int flags = DUPLICATE_SAME_ACCESS;
  if (close_source)
    flags |= DUPLICATE_CLOSE_SOURCE;
  if (!DuplicateHandle(GetCurrentProcess(),
                       reinterpret_cast<HANDLE>(sourceh),
                       processh,
                       &channel,
                       0,  // Unused given DUPLICATE_SAME_ACCESS.
                       FALSE,
                       flags)) {
    LOG(ERROR) << "DuplicateHandle() failed";
    return false;
  }
  handles_for_sel_ldr->push_back(
      reinterpret_cast<nacl::FileDescriptor>(channel));
#else
  nacl::FileDescriptor channel;
  channel.fd = sourceh;
  channel.auto_close = close_source;
  handles_for_sel_ldr->push_back(channel);
#endif
  return true;
}

ppapi::PpapiPermissions GetNaClPermissions(uint32 permission_bits) {
  // Only allow NaCl plugins to request certain permissions. We don't want
  // a compromised renderer to be able to start a nacl plugin with e.g. Flash
  // permissions which may expand the surface area of the sandbox.
  uint32 masked_bits = permission_bits & ppapi::PERMISSION_DEV;
  return ppapi::PpapiPermissions::GetForCommandLine(masked_bits);
}

}  // namespace

namespace nacl {

struct NaClProcessHost::NaClInternal {
  NaClHandle socket_for_renderer;
  NaClHandle socket_for_sel_ldr;

  NaClInternal()
    : socket_for_renderer(NACL_INVALID_HANDLE),
      socket_for_sel_ldr(NACL_INVALID_HANDLE) { }
};

// -----------------------------------------------------------------------------

NaClProcessHost::PluginListener::PluginListener(NaClProcessHost* host)
    : host_(host) {
}

bool NaClProcessHost::PluginListener::OnMessageReceived(
    const IPC::Message& msg) {
  return host_->OnUntrustedMessageForwarded(msg);
}

NaClProcessHost::NaClProcessHost(const GURL& manifest_url,
                                 int render_view_id,
                                 uint32 permission_bits,
                                 bool uses_irt,
                                 bool enable_dyncode_syscalls,
                                 bool enable_exception_handling,
                                 bool enable_crash_throttling,
                                 bool off_the_record,
                                 const base::FilePath& profile_directory)
    : manifest_url_(manifest_url),
      permissions_(GetNaClPermissions(permission_bits)),
#if defined(OS_WIN)
      process_launched_by_broker_(false),
#endif
      reply_msg_(NULL),
#if defined(OS_WIN)
      debug_exception_handler_requested_(false),
#endif
      internal_(new NaClInternal()),
      weak_factory_(this),
      uses_irt_(uses_irt),
      enable_debug_stub_(false),
      enable_dyncode_syscalls_(enable_dyncode_syscalls),
      enable_exception_handling_(enable_exception_handling),
      enable_crash_throttling_(enable_crash_throttling),
      off_the_record_(off_the_record),
      profile_directory_(profile_directory),
      ipc_plugin_listener_(this),
      render_view_id_(render_view_id) {
  process_.reset(content::BrowserChildProcessHost::Create(
      PROCESS_TYPE_NACL_LOADER, this));

  // Set the display name so the user knows what plugin the process is running.
  // We aren't on the UI thread so getting the pref locale for language
  // formatting isn't possible, so IDN will be lost, but this is probably OK
  // for this use case.
  process_->SetName(net::FormatUrl(manifest_url_, std::string()));

  enable_debug_stub_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNaClDebug);
}

NaClProcessHost::~NaClProcessHost() {
  // Report exit status only if the process was successfully started.
  if (process_->GetData().handle != base::kNullProcessHandle) {
    int exit_code = 0;
    process_->GetTerminationStatus(false /* known_dead */, &exit_code);
    std::string message =
        base::StringPrintf("NaCl process exited with status %i (0x%x)",
                           exit_code, exit_code);
    if (exit_code == 0) {
      VLOG(1) << message;
    } else {
      LOG(ERROR) << message;
    }
  }

  if (internal_->socket_for_renderer != NACL_INVALID_HANDLE) {
    if (NaClClose(internal_->socket_for_renderer) != 0) {
      NOTREACHED() << "NaClClose() failed";
    }
  }

  if (internal_->socket_for_sel_ldr != NACL_INVALID_HANDLE) {
    if (NaClClose(internal_->socket_for_sel_ldr) != 0) {
      NOTREACHED() << "NaClClose() failed";
    }
  }

  if (reply_msg_) {
    // The process failed to launch for some reason.
    // Don't keep the renderer hanging.
    reply_msg_->set_reply_error();
    nacl_host_message_filter_->Send(reply_msg_);
  }
#if defined(OS_WIN)
  if (process_launched_by_broker_) {
    NaClBrokerService::GetInstance()->OnLoaderDied();
  }
#endif
}

void NaClProcessHost::OnProcessCrashed(int exit_status) {
  if (enable_crash_throttling_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePnaclCrashThrottling)) {
    NaClBrowser::GetInstance()->OnProcessCrashed();
  }
}

// This is called at browser startup.
// static
void NaClProcessHost::EarlyStartup() {
  NaClBrowser::GetInstance()->EarlyStartup();
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Open the IRT file early to make sure that it isn't replaced out from
  // under us by autoupdate.
  NaClBrowser::GetInstance()->EnsureIrtAvailable();
#endif
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb",
      !cmd->GetSwitchValuePath(switches::kNaClGdb).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb-script",
      !cmd->GetSwitchValuePath(switches::kNaClGdbScript).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.enable-nacl-debug",
      cmd->HasSwitch(switches::kEnableNaClDebug));
  NaClBrowser::GetDelegate()->SetDebugPatterns(
      cmd->GetSwitchValueASCII(switches::kNaClDebugMask));
}

void NaClProcessHost::Launch(
    NaClHostMessageFilter* nacl_host_message_filter,
    IPC::Message* reply_msg,
    const base::FilePath& manifest_path) {
  nacl_host_message_filter_ = nacl_host_message_filter;
  reply_msg_ = reply_msg;
  manifest_path_ = manifest_path;

  // Do not launch the requested NaCl module if NaCl is marked "unstable" due
  // to too many crashes within a given time period.
  if (enable_crash_throttling_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePnaclCrashThrottling) &&
      NaClBrowser::GetInstance()->IsThrottled()) {
    SendErrorToRenderer("Process creation was throttled due to excessive"
                        " crashes");
    delete this;
    return;
  }

  const CommandLine* cmd = CommandLine::ForCurrentProcess();
#if defined(OS_WIN)
  if (cmd->HasSwitch(switches::kEnableNaClDebug) &&
      !cmd->HasSwitch(switches::kNoSandbox)) {
    // We don't switch off sandbox automatically for security reasons.
    SendErrorToRenderer("NaCl's GDB debug stub requires --no-sandbox flag"
                        " on Windows. See crbug.com/265624.");
    delete this;
    return;
  }
#endif
  if (cmd->HasSwitch(switches::kNaClGdb) &&
      !cmd->HasSwitch(switches::kEnableNaClDebug)) {
    LOG(WARNING) << "--nacl-gdb flag requires --enable-nacl-debug flag";
  }

  // Start getting the IRT open asynchronously while we launch the NaCl process.
  // We'll make sure this actually finished in StartWithLaunchedProcess, below.
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  nacl_browser->EnsureAllResourcesAvailable();
  if (!nacl_browser->IsOk()) {
    SendErrorToRenderer("could not find all the resources needed"
                        " to launch the process");
    delete this;
    return;
  }

  // Rather than creating a socket pair in the renderer, and passing
  // one side through the browser to sel_ldr, socket pairs are created
  // in the browser and then passed to the renderer and sel_ldr.
  //
  // This is mainly for the benefit of Windows, where sockets cannot
  // be passed in messages, but are copied via DuplicateHandle().
  // This means the sandboxed renderer cannot send handles to the
  // browser process.

  NaClHandle pair[2];
  // Create a connected socket
  if (NaClSocketPair(pair) == -1) {
    SendErrorToRenderer("NaClSocketPair() failed");
    delete this;
    return;
  }
  internal_->socket_for_renderer = pair[0];
  internal_->socket_for_sel_ldr = pair[1];
  SetCloseOnExec(pair[0]);
  SetCloseOnExec(pair[1]);

  // Launch the process
  if (!LaunchSelLdr()) {
    delete this;
  }
}

void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
  if (!CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kNaClGdb).empty()) {
    LaunchNaClGdb();
  }
}

#if defined(OS_WIN)
void NaClProcessHost::OnProcessLaunchedByBroker(base::ProcessHandle handle) {
  process_launched_by_broker_ = true;
  process_->SetHandle(handle);
  if (!StartWithLaunchedProcess())
    delete this;
}

void NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker(bool success) {
  IPC::Message* reply = attach_debug_exception_handler_reply_msg_.release();
  NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply, success);
  Send(reply);
}
#endif

// Needed to handle sync messages in OnMessageRecieved.
bool NaClProcessHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

bool NaClProcessHost::LaunchNaClGdb() {
#if defined(OS_WIN)
  base::FilePath nacl_gdb =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(switches::kNaClGdb);
  CommandLine cmd_line(nacl_gdb);
#else
  CommandLine::StringType nacl_gdb =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kNaClGdb);
  CommandLine::StringVector argv;
  // We don't support spaces inside arguments in --nacl-gdb switch.
  base::SplitString(nacl_gdb, static_cast<CommandLine::CharType>(' '), &argv);
  CommandLine cmd_line(argv);
#endif
  cmd_line.AppendArg("--eval-command");
  base::FilePath::StringType irt_path(
      NaClBrowser::GetInstance()->GetIrtFilePath().value());
  // Avoid back slashes because nacl-gdb uses posix escaping rules on Windows.
  // See issue https://code.google.com/p/nativeclient/issues/detail?id=3482.
  std::replace(irt_path.begin(), irt_path.end(), '\\', '/');
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-irt \"") + irt_path +
                           FILE_PATH_LITERAL("\""));
  if (!manifest_path_.empty()) {
    cmd_line.AppendArg("--eval-command");
    base::FilePath::StringType manifest_path_value(manifest_path_.value());
    std::replace(manifest_path_value.begin(), manifest_path_value.end(),
                 '\\', '/');
    cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-manifest \"") +
                             manifest_path_value + FILE_PATH_LITERAL("\""));
  }
  cmd_line.AppendArg("--eval-command");
  cmd_line.AppendArg("target remote :4014");
  base::FilePath script = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kNaClGdbScript);
  if (!script.empty()) {
    cmd_line.AppendArg("--command");
    cmd_line.AppendArgNative(script.value());
  }
  return base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

bool NaClProcessHost::LaunchSelLdr() {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty()) {
    SendErrorToRenderer("CreateChannel() failed");
    return false;
  }

  CommandLine::StringType nacl_loader_prefix;
#if defined(OS_POSIX)
  nacl_loader_prefix = CommandLine::ForCurrentProcess()->GetSwitchValueNative(
      switches::kNaClLoaderCmdPrefix);
#endif  // defined(OS_POSIX)

  // Build command line for nacl.

#if defined(OS_MACOSX)
  // The Native Client process needs to be able to allocate a 1GB contiguous
  // region to use as the client environment's virtual address space. ASLR
  // (PIE) interferes with this by making it possible that no gap large enough
  // to accomodate this request will exist in the child process' address
  // space. Disable PIE for NaCl processes. See http://crbug.com/90221 and
  // http://code.google.com/p/nativeclient/issues/detail?id=2043.
  int flags = ChildProcessHost::CHILD_NO_PIE;
#elif defined(OS_LINUX)
  int flags = nacl_loader_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

#if defined(OS_WIN)
  // On Windows 64-bit NaCl loader is called nacl64.exe instead of chrome.exe
  if (RunningOnWOW64()) {
    if (!NaClBrowser::GetInstance()->GetNaCl64ExePath(&exe_path)) {
      SendErrorToRenderer("could not get path to nacl64.exe");
      return false;
    }
  }
#endif

  scoped_ptr<CommandLine> cmd_line(new CommandLine(exe_path));
  CopyNaClCommandLineArguments(cmd_line.get());

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClLoaderProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  if (NaClBrowser::GetDelegate()->DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  if (!nacl_loader_prefix.empty())
    cmd_line->PrependWrapper(nacl_loader_prefix);

  // On Windows we might need to start the broker process to launch a new loader
#if defined(OS_WIN)
  if (RunningOnWOW64()) {
    if (!NaClBrokerService::GetInstance()->LaunchLoader(
            weak_factory_.GetWeakPtr(), channel_id)) {
      SendErrorToRenderer("broker service did not launch process");
      return false;
    }
  } else {
    process_->Launch(new NaClSandboxedProcessLauncherDelegate,
                     cmd_line.release());
  }
#elif defined(OS_POSIX)
  process_->Launch(nacl_loader_prefix.empty(),  // use_zygote
                   base::EnvironmentMap(),
                   cmd_line.release());
#endif

  return true;
}

bool NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_QueryKnownToValidate,
                        OnQueryKnownToValidate)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_SetKnownToValidate,
                        OnSetKnownToValidate)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClProcessMsg_ResolveFileToken,
                                    OnResolveFileToken)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClProcessMsg_AttachDebugExceptionHandler,
                                    OnAttachDebugExceptionHandler)
#endif
    IPC_MESSAGE_HANDLER(NaClProcessHostMsg_PpapiChannelCreated,
                        OnPpapiChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClProcessHost::OnProcessLaunched() {
  if (!StartWithLaunchedProcess())
    delete this;
}

// Called when the NaClBrowser singleton has been fully initialized.
void NaClProcessHost::OnResourcesReady() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  if (!nacl_browser->IsReady()) {
    SendErrorToRenderer("could not acquire shared resources needed by NaCl");
    delete this;
  } else if (!SendStart()) {
    delete this;
  }
}

bool NaClProcessHost::ReplyToRenderer(
    const IPC::ChannelHandle& channel_handle) {
#if defined(OS_WIN)
  // If we are on 64-bit Windows, the NaCl process's sandbox is
  // managed by a different process from the renderer's sandbox.  We
  // need to inform the renderer's sandbox about the NaCl process so
  // that the renderer can send handles to the NaCl process using
  // BrokerDuplicateHandle().
  if (RunningOnWOW64()) {
    if (!content::BrokerAddTargetPeer(process_->GetData().handle)) {
      SendErrorToRenderer("BrokerAddTargetPeer() failed");
      return false;
    }
  }
#endif

  FileDescriptor handle_for_renderer;
#if defined(OS_WIN)
  // Copy the handle into the renderer process.
  HANDLE handle_in_renderer;
  if (!DuplicateHandle(base::GetCurrentProcessHandle(),
                       reinterpret_cast<HANDLE>(
                           internal_->socket_for_renderer),
                       nacl_host_message_filter_->PeerHandle(),
                       &handle_in_renderer,
                       0,  // Unused given DUPLICATE_SAME_ACCESS.
                       FALSE,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    SendErrorToRenderer("DuplicateHandle() failed");
    return false;
  }
  handle_for_renderer = reinterpret_cast<FileDescriptor>(
      handle_in_renderer);
#else
  // No need to dup the imc_handle - we don't pass it anywhere else so
  // it cannot be closed.
  FileDescriptor imc_handle;
  imc_handle.fd = internal_->socket_for_renderer;
  imc_handle.auto_close = true;
  handle_for_renderer = imc_handle;
#endif

  const ChildProcessData& data = process_->GetData();
  SendMessageToRenderer(
      NaClLaunchResult(handle_for_renderer,
                             channel_handle,
                             base::GetProcId(data.handle),
                             data.id),
      std::string() /* error_message */);
  internal_->socket_for_renderer = NACL_INVALID_HANDLE;
  return true;
}

void NaClProcessHost::SendErrorToRenderer(const std::string& error_message) {
  LOG(ERROR) << "NaCl process launch failed: " << error_message;
  SendMessageToRenderer(NaClLaunchResult(), error_message);
}

void NaClProcessHost::SendMessageToRenderer(
    const NaClLaunchResult& result,
    const std::string& error_message) {
  DCHECK(nacl_host_message_filter_);
  DCHECK(reply_msg_);
  if (nacl_host_message_filter_ != NULL && reply_msg_ != NULL) {
    NaClHostMsg_LaunchNaCl::WriteReplyParams(
        reply_msg_, result, error_message);
    nacl_host_message_filter_->Send(reply_msg_);
    nacl_host_message_filter_ = NULL;
    reply_msg_ = NULL;
  }
}

// TCP port we chose for NaCl debug stub. It can be any other number.
static const int kDebugStubPort = 4014;

#if defined(OS_POSIX)
net::SocketDescriptor NaClProcessHost::GetDebugStubSocketHandle() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  net::SocketDescriptor s = net::kInvalidSocket;
  // We allocate currently unused TCP port for debug stub tests. The port
  // number is passed to the test via debug stub port listener.
  if (nacl_browser->HasGdbDebugStubPortListener()) {
    int port;
    s = net::TCPListenSocket::CreateAndBindAnyPort("127.0.0.1", &port);
    if (s != net::kInvalidSocket) {
      nacl_browser->FireGdbDebugStubPortOpened(port);
    }
  } else {
    s = net::TCPListenSocket::CreateAndBind("127.0.0.1", kDebugStubPort);
  }
  if (s == net::kInvalidSocket) {
    LOG(ERROR) << "failed to open socket for debug stub";
    return net::kInvalidSocket;
  }
  if (listen(s, 1)) {
    LOG(ERROR) << "listen() failed on debug stub socket";
    if (IGNORE_EINTR(close(s)) < 0)
      PLOG(ERROR) << "failed to close debug stub socket";
    return net::kInvalidSocket;
  }
  return s;
}
#endif

bool NaClProcessHost::StartNaClExecution() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  NaClStartParams params;
  params.validation_cache_enabled = nacl_browser->ValidationCacheIsEnabled();
  params.validation_cache_key = nacl_browser->GetValidationCacheKey();
  params.version = NaClBrowser::GetDelegate()->GetVersionString();
  params.enable_exception_handling = enable_exception_handling_;
  params.enable_debug_stub = enable_debug_stub_ &&
      NaClBrowser::GetDelegate()->URLMatchesDebugPatterns(manifest_url_);
  // Enable PPAPI proxy channel creation only for renderer processes.
  params.enable_ipc_proxy = enable_ppapi_proxy();
  params.uses_irt = uses_irt_;
  params.enable_dyncode_syscalls = enable_dyncode_syscalls_;

  const ChildProcessData& data = process_->GetData();
  if (!ShareHandleToSelLdr(data.handle,
                           internal_->socket_for_sel_ldr, true,
                           &params.handles)) {
    return false;
  }

  if (params.uses_irt) {
    base::PlatformFile irt_file = nacl_browser->IrtFile();
    CHECK_NE(irt_file, base::kInvalidPlatformFileValue);
    // Send over the IRT file handle.  We don't close our own copy!
    if (!ShareHandleToSelLdr(data.handle, irt_file, false, &params.handles))
      return false;
  }

#if defined(OS_MACOSX)
  // For dynamic loading support, NaCl requires a file descriptor that
  // was created in /tmp, since those created with shm_open() are not
  // mappable with PROT_EXEC.  Rather than requiring an extra IPC
  // round trip out of the sandbox, we create an FD here.
  base::SharedMemory memory_buffer;
  base::SharedMemoryCreateOptions options;
  options.size = 1;
  options.executable = true;
  if (!memory_buffer.Create(options)) {
    DLOG(ERROR) << "Failed to allocate memory buffer";
    return false;
  }
  FileDescriptor memory_fd;
  memory_fd.fd = dup(memory_buffer.handle().fd);
  if (memory_fd.fd < 0) {
    DLOG(ERROR) << "Failed to dup() a file descriptor";
    return false;
  }
  memory_fd.auto_close = true;
  params.handles.push_back(memory_fd);
#endif

#if defined(OS_POSIX)
  if (params.enable_debug_stub) {
    net::SocketDescriptor server_bound_socket = GetDebugStubSocketHandle();
    if (server_bound_socket != net::kInvalidSocket) {
      params.debug_stub_server_bound_socket =
          FileDescriptor(server_bound_socket, true);
    }
  }
#endif

  process_->Send(new NaClProcessMsg_Start(params));

  internal_->socket_for_sel_ldr = NACL_INVALID_HANDLE;
  return true;
}

bool NaClProcessHost::SendStart() {
  if (!enable_ppapi_proxy()) {
    if (!ReplyToRenderer(IPC::ChannelHandle()))
      return false;
  }
  return StartNaClExecution();
}

// This method is called when NaClProcessHostMsg_PpapiChannelCreated is
// received or PpapiHostMsg_ChannelCreated is forwarded by our plugin
// listener.
void NaClProcessHost::OnPpapiChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  // Only renderer processes should create a channel.
  DCHECK(enable_ppapi_proxy());
  // If the proxy channel is null, this must be the initial NaCl-Browser IPC
  // channel.
  if (!ipc_proxy_channel_.get()) {
    DCHECK_EQ(PROCESS_TYPE_NACL_LOADER, process_->GetData().process_type);

    ipc_proxy_channel_.reset(
        new IPC::ChannelProxy(channel_handle,
                              IPC::Channel::MODE_CLIENT,
                              &ipc_plugin_listener_,
                              base::MessageLoopProxy::current().get()));
    // Create the browser ppapi host and enable PPAPI message dispatching to the
    // browser process.
    ppapi_host_.reset(content::BrowserPpapiHost::CreateExternalPluginProcess(
        ipc_proxy_channel_.get(),  // sender
        permissions_,
        process_->GetData().handle,
        ipc_proxy_channel_.get(),
        nacl_host_message_filter_->render_process_id(),
        render_view_id_,
        profile_directory_));

    ppapi::PpapiNaClChannelArgs args;
    args.off_the_record = nacl_host_message_filter_->off_the_record();
    args.permissions = permissions_;
    args.supports_dev_channel =
        content::PluginService::GetInstance()->PpapiDevChannelSupported();
    CommandLine* cmdline = CommandLine::ForCurrentProcess();
    DCHECK(cmdline);
    std::string flag_whitelist[] = {switches::kV, switches::kVModule};
    for (size_t i = 0; i < arraysize(flag_whitelist); ++i) {
      std::string value = cmdline->GetSwitchValueASCII(flag_whitelist[i]);
      if (!value.empty()) {
        args.switch_names.push_back(flag_whitelist[i]);
        args.switch_values.push_back(value);
      }
    }

    ppapi_host_->GetPpapiHost()->AddHostFactoryFilter(
        scoped_ptr<ppapi::host::HostFactory>(
            NaClBrowser::GetDelegate()->CreatePpapiHostFactory(
                ppapi_host_.get())));

    // Send a message to create the NaCl-Renderer channel. The handle is just
    // a place holder.
    ipc_proxy_channel_->Send(
        new PpapiMsg_CreateNaClChannel(
            nacl_host_message_filter_->render_process_id(),
            args,
            SerializedHandle(SerializedHandle::CHANNEL_HANDLE,
                             IPC::InvalidPlatformFileForTransit())));
  } else if (reply_msg_) {
    // Otherwise, this must be a renderer channel.
    ReplyToRenderer(channel_handle);
  } else {
    // Attempt to open more than 1 renderer channel is not supported.
    // Shut down the NaCl process.
    process_->GetHost()->ForceShutdown();
  }
}

bool NaClProcessHost::OnUntrustedMessageForwarded(const IPC::Message& msg) {
  // Handle messages that have been forwarded from our PluginListener.
  // These messages come from untrusted code so should be handled with care.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnPpapiChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool NaClProcessHost::StartWithLaunchedProcess() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  if (nacl_browser->IsReady()) {
    return SendStart();
  } else if (nacl_browser->IsOk()) {
    nacl_browser->WaitForResources(
        base::Bind(&NaClProcessHost::OnResourcesReady,
                   weak_factory_.GetWeakPtr()));
    return true;
  } else {
    SendErrorToRenderer("previously failed to acquire shared resources");
    return false;
  }
}

void NaClProcessHost::OnQueryKnownToValidate(const std::string& signature,
                                             bool* result) {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  *result = nacl_browser->QueryKnownToValidate(signature, off_the_record_);
}

void NaClProcessHost::OnSetKnownToValidate(const std::string& signature) {
  NaClBrowser::GetInstance()->SetKnownToValidate(
      signature, off_the_record_);
}

void NaClProcessHost::FileResolved(
    const base::FilePath& file_path,
    IPC::Message* reply_msg,
    const base::PlatformFile& file) {
  if (file != base::kInvalidPlatformFileValue) {
    IPC::PlatformFileForTransit handle = IPC::GetFileHandleForProcess(
        file,
        process_->GetData().handle,
        true /* close_source */);
    NaClProcessMsg_ResolveFileToken::WriteReplyParams(
        reply_msg,
        handle,
        file_path);
  } else {
    NaClProcessMsg_ResolveFileToken::WriteReplyParams(
        reply_msg,
        IPC::InvalidPlatformFileForTransit(),
        base::FilePath());
  }
  Send(reply_msg);
}

void NaClProcessHost::OnResolveFileToken(uint64 file_token_lo,
                                         uint64 file_token_hi,
                                         IPC::Message* reply_msg) {
  // Was the file registered?
  //
  // Note that the file path cache is of bounded size, and old entries can get
  // evicted.  If a large number of NaCl modules are being launched at once,
  // resolving the file_token may fail because the path cache was thrashed
  // while the file_token was in flight.  In this case the query fails, and we
  // need to fall back to the slower path.
  //
  // However: each NaCl process will consume 2-3 entries as it starts up, this
  // means that eviction will not happen unless you start up 33+ NaCl processes
  // at the same time, and this still requires worst-case timing.  As a
  // practical matter, no entries should be evicted prematurely.
  // The cache itself should take ~ (150 characters * 2 bytes/char + ~60 bytes
  // data structure overhead) * 100 = 35k when full, so making it bigger should
  // not be a problem, if needed.
  //
  // Each NaCl process will consume 2-3 entries because the manifest and main
  // nexe are currently not resolved.  Shared libraries will be resolved.  They
  // will be loaded sequentially, so they will only consume a single entry
  // while the load is in flight.
  //
  // TODO(ncbray): track behavior with UMA. If entries are getting evicted or
  // bogus keys are getting queried, this would be good to know.
  base::FilePath file_path;
  if (!NaClBrowser::GetInstance()->GetFilePath(
        file_token_lo, file_token_hi, &file_path)) {
    NaClProcessMsg_ResolveFileToken::WriteReplyParams(
        reply_msg,
        IPC::InvalidPlatformFileForTransit(),
        base::FilePath());
    Send(reply_msg);
    return;
  }

  // Open the file.
  if (!base::PostTaskAndReplyWithResult(
          content::BrowserThread::GetBlockingPool(),
          FROM_HERE,
          base::Bind(OpenNaClExecutableImpl, file_path),
          base::Bind(&NaClProcessHost::FileResolved,
                     weak_factory_.GetWeakPtr(),
                     file_path,
                     reply_msg))) {
     NaClProcessMsg_ResolveFileToken::WriteReplyParams(
         reply_msg,
         IPC::InvalidPlatformFileForTransit(),
         base::FilePath());
     Send(reply_msg);
  }
}

#if defined(OS_WIN)
void NaClProcessHost::OnAttachDebugExceptionHandler(const std::string& info,
                                                    IPC::Message* reply_msg) {
  if (!AttachDebugExceptionHandler(info, reply_msg)) {
    // Send failure message.
    NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply_msg,
                                                                 false);
    Send(reply_msg);
  }
}

bool NaClProcessHost::AttachDebugExceptionHandler(const std::string& info,
                                                  IPC::Message* reply_msg) {
  if (!enable_exception_handling_ && !enable_debug_stub_) {
    DLOG(ERROR) <<
        "Debug exception handler requested by NaCl process when not enabled";
    return false;
  }
  if (debug_exception_handler_requested_) {
    // The NaCl process should not request this multiple times.
    DLOG(ERROR) << "Multiple AttachDebugExceptionHandler requests received";
    return false;
  }
  debug_exception_handler_requested_ = true;

  base::ProcessId nacl_pid = base::GetProcId(process_->GetData().handle);
  base::ProcessHandle temp_handle;
  // We cannot use process_->GetData().handle because it does not have
  // the necessary access rights.  We open the new handle here rather
  // than in the NaCl broker process in case the NaCl loader process
  // dies before the NaCl broker process receives the message we send.
  // The debug exception handler uses DebugActiveProcess() to attach,
  // but this takes a PID.  We need to prevent the NaCl loader's PID
  // from being reused before DebugActiveProcess() is called, and
  // holding a process handle open achieves this.
  if (!base::OpenProcessHandleWithAccess(
           nacl_pid,
           base::kProcessAccessQueryInformation |
           base::kProcessAccessSuspendResume |
           base::kProcessAccessTerminate |
           base::kProcessAccessVMOperation |
           base::kProcessAccessVMRead |
           base::kProcessAccessVMWrite |
           base::kProcessAccessDuplicateHandle |
           base::kProcessAccessWaitForTermination,
           &temp_handle)) {
    LOG(ERROR) << "Failed to get process handle";
    return false;
  }
  base::win::ScopedHandle process_handle(temp_handle);

  attach_debug_exception_handler_reply_msg_.reset(reply_msg);
  // If the NaCl loader is 64-bit, the process running its debug
  // exception handler must be 64-bit too, so we use the 64-bit NaCl
  // broker process for this.  Otherwise, on a 32-bit system, we use
  // the 32-bit browser process to run the debug exception handler.
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchDebugExceptionHandler(
               weak_factory_.GetWeakPtr(), nacl_pid, process_handle, info);
  } else {
    NaClStartDebugExceptionHandlerThread(
        process_handle.Take(), info,
        base::MessageLoopProxy::current(),
        base::Bind(&NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker,
                   weak_factory_.GetWeakPtr()));
    return true;
  }
}
#endif

}  // namespace nacl
