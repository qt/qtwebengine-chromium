// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <csignal>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cast/receiver/channel/static_credentials.h"
#include "cast/standalone_receiver/cast_service.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "third_party/getopt/getopt.h"
#include "util/chrono_helpers.h"
#include "util/string_util.h"
#include "util/stringprintf.h"
#include "util/trace_logging.h"
#include "util/uuid.h"

namespace openscreen::cast {
namespace {

void LogUsage(const char* argv0) {
  static constexpr char kTemplate[] = R"(
usage: {} <options> <interface>

    interface
        Specifies the network interface to bind to. The interface is
        looked up from the system interface registry.
        Mandatory, as it must be known for publishing discovery.

options:
    -d, --developer-certificate=path-to-cert: Path to PEM file containing a
                           developer generated server root TLS certificate.
                           If a root server certificate is not provided, one
                           will be generated using a randomly generated
                           private key. Note that if a certificate path is
                           passed, the private key path is a mandatory field.

    -f, --friendly-name: Friendly name to be used for receiver discovery.

    -g, --generate-credentials: Instructs the binary to generate a private key
                                and self-signed root certificate with the CA
                                bit set to true, and then exit. The resulting
                                private key and certificate can then be used as
                                values for the -p and -s flags.

    -h, --help: Show this help message.

    -m, --model-name: Model name to be used for receiver discovery.

    -p, --private-key=path-to-key: Path to OpenSSL-generated private key to be
                    used for TLS authentication. If a private key is not
                    provided, a randomly generated one will be used for this
                    session.

    -q, --disable-dscp: Disable DSCP packet prioritization, used for QoS over
                        the UDP socket connection.

    -t, --tracing: Enable performance tracing logging.

    -v, --verbose: Enable verbose logging.

    -x, --disable-discovery: Disable discovery.

)";

  std::cerr << StringFormat(kTemplate, argv0);
}

InterfaceInfo GetInterfaceInfoFromName(const char* name) {
  OSP_CHECK(name) << "Missing mandatory argument: <interface>";
  InterfaceInfo interface_info;
  std::vector<InterfaceInfo> network_interfaces = GetNetworkInterfaces();
  for (auto& interface : network_interfaces) {
    if (interface.name == name) {
      interface_info = std::move(interface);
      break;
    }
  }
  OSP_CHECK(!interface_info.name.empty())
      << "Invalid interface " << name << " specified.";
  return interface_info;
}

void RunCastService(TaskRunnerImpl* runner, CastService::Configuration config) {
  std::unique_ptr<CastService> service;
  runner->PostTask(
      [&] { service = std::make_unique<CastService>(std::move(config)); });

  OSP_LOG_INFO << "CastService is running. CTRL-C (SIGINT), or send a "
                  "SIGTERM to exit.";
  runner->RunUntilSignaled();

  // Spin the TaskRunner to execute destruction/shutdown tasks.
  OSP_LOG_INFO << "Shutting down...";
  runner->PostTask([&] {
    service.reset();
    runner->RequestStopSoon();
  });
  runner->RunUntilStopped();
  OSP_LOG_INFO << "Bye!";
}

struct Arguments {
  // Required positional arguments
  const char* interface_name = nullptr;

  // Optional arguments
  std::string developer_certificate_path;
  bool enable_discovery = true;
  bool enable_dscp = true;
  std::string friendly_name = "Cast Standalone Receiver";
  bool should_generate_credentials = false;
  std::string model_name = "cast_standalone_receiver";
  std::string private_key_path;
  std::unique_ptr<TextTraceLoggingPlatform> trace_logger;
  bool is_verbose = false;
};

std::optional<Arguments> ParseArgs(int argc, char* argv[]) {
  // A note about modifying command line arguments: consider uniformity
  // between all Open Screen executables. If it is a platform feature
  // being exposed, consider if it applies to the standalone receiver,
  // standalone sender, osp demo, and test_main argument options.
  const get_opt::option kArgumentOptions[] = {
      {"developer-certificate", required_argument, nullptr, 'd'},
      {"disable-discovery", no_argument, nullptr, 'x'},
      {"disable-dscp", no_argument, nullptr, 'q'},
      {"friendly-name", required_argument, nullptr, 'f'},
      {"generate-credentials", no_argument, nullptr, 'g'},
      {"help", no_argument, nullptr, 'h'},
      {"model-name", required_argument, nullptr, 'm'},
      {"private-key", required_argument, nullptr, 'p'},
      {"tracing", no_argument, nullptr, 't'},
      {"verbose", no_argument, nullptr, 'v'},
      {nullptr, 0, nullptr, 0}};

  Arguments args;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "d:f:ghm:p:qtvx", kArgumentOptions,
                           nullptr)) != -1) {
    switch (ch) {
      case 'd':
        args.developer_certificate_path = get_opt::optarg;
        break;
      case 'f':
        args.friendly_name = get_opt::optarg;
        break;
      case 'g':
        args.should_generate_credentials = true;
        break;
      case 'h':
        return {};
      case 'm':
        args.model_name = get_opt::optarg;
        break;
      case 'p':
        args.private_key_path = get_opt::optarg;
        break;
      case 'q':
        args.enable_dscp = false;
        break;
      case 't':
        args.trace_logger = std::make_unique<TextTraceLoggingPlatform>();
        break;
      case 'v':
        args.is_verbose = true;
        break;
      case 'x':
        args.enable_discovery = false;
        break;
    }
  }

  args.interface_name = argv[get_opt::optind];
  if (!args.should_generate_credentials) {
    OSP_CHECK(args.interface_name && strlen(args.interface_name) > 0)
        << "No interface name provided.";
  }
  return args;
}

int RunStandaloneReceiver(int argc, char* argv[]) {
#if !defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  OSP_LOG_INFO
      << "Note: compiled without external libs. The dummy player will "
         "be linked and no video decoding will occur. If this is not desired, "
         "install the required external libraries. For more information, see: "
         "[external_libraries.md](../streaming/external_libraries.md).";
#endif
  const std::optional<Arguments> args = ParseArgs(argc, argv);
  if (!args) {
    LogUsage(argv[0]);
    return 1;
  }
  SetLogLevel(args->is_verbose ? LogLevel::kVerbose : LogLevel::kInfo);

  // Either -g is required, or both -p and -d.
  if (args->should_generate_credentials) {
    GenerateDeveloperCredentialsToFile();
    return 0;
  }
  if (args->private_key_path.empty() ||
      args->developer_certificate_path.empty()) {
    OSP_LOG_FATAL << "You must either invoke with -g to generate credentials, "
                     "or provide both a private key path and root certificate "
                     "using -p and -d";
    return 1;
  }

  const std::string receiver_id =
      string_util::StrCat({"Standalone Receiver on ", args->interface_name});
  ErrorOr<GeneratedCredentials> creds = GenerateCredentials(
      receiver_id, args->private_key_path, args->developer_certificate_path);
  OSP_CHECK(creds.is_value()) << creds.error();

  const InterfaceInfo interface =
      GetInterfaceInfoFromName(args->interface_name);
  OSP_CHECK(interface.GetIpAddressV4() || interface.GetIpAddressV6());

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(milliseconds(50),
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  RunCastService(
      task_runner,
      CastService::Configuration{
          *task_runner, interface, std::move(creds.value()),
          Uuid::GenerateRandomV4().AsLowercaseString(), args->friendly_name,
          args->model_name, args->enable_discovery});
  PlatformClientPosix::ShutDown();

  return 0;
}

}  // namespace
}  // namespace openscreen::cast

int main(int argc, char* argv[]) {
  // Ignore SIGPIPE events at the application level -- tearing down the network
  // interface will close a TLS or UDP socket connection, which will result
  // in a more graceful exit than terminating on the SIGPIPE call.
  std::signal(SIGPIPE, SIG_IGN);

  return openscreen::cast::RunStandaloneReceiver(argc, argv);
}
