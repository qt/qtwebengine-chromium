// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <csignal>

#include "platform/impl/logging.h"

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "cast/common/public/trust_store.h"
#include "cast/standalone_sender/constants.h"
#include "cast/standalone_sender/looping_file_cast_agent.h"
#include "cast/standalone_sender/receiver_chooser.h"
#include "cast/streaming/public/constants.h"
#include "platform/api/network_interface.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "third_party/getopt/getopt.h"
#include "util/chrono_helpers.h"
#include "util/string_parse.h"
#include "util/string_util.h"
#include "util/stringprintf.h"

namespace openscreen::cast {
namespace {

void LogUsage(const char* argv0) {
  static constexpr char kTemplate[] = R"(
usage: {} <options> network_interface media_file

or

usage: {} <options> addr[:port] media_file

   The first form runs this application in discovery+interactive mode. It will
   scan for Cast Receivers on the LAN reachable from the given network
   interface, and then the user will choose one interactively via a menu on the
   console.

   The second form runs this application in direct mode. It will not attempt to
   discover Cast Receivers, and instead connect directly to the Cast Receiver at
   addr:[port] (e.g., 192.168.1.22, 192.168.1.22:%d or [::1]:%d).

options:
    -a, --android-hack:
          Use the wrong RTP payload types, for compatibility with older Android
          TV receivers. See https://crbug.com/631828.

    -c, --codec: Specifies the video codec to be used. Can be one of:
                 vp8, vp9, av1. Defaults to vp8 if not specified.

    -d, --developer-certificate=path-to-cert
          Specifies the path to a self-signed developer certificate that will
          be permitted for use as a root CA certificate for receivers that
          this sender instance will connect to. If omitted, only connections to
          receivers using an official Google-signed cast certificate chain will
          be permitted.

    -h, --help: Show this help message.

    -m, --max-bitrate=N
          Specifies the maximum bits per second for the media streams.
          Default if not set: %d

    -n, --no-looping
          Disable looping the passed in video after it finishes playing.

    -q, --disable-dscp: Disable DSCP packet prioritization, used for QoS over
                        the UDP socket connection.

    -r, --remoting: Enable remoting content instead of mirroring.

    -t, --tracing: Enable performance tracing logging.

    -v, --verbose: Enable verbose logging.

)";

  std::cerr << StringFormat(kTemplate, argv0, argv0, kDefaultCastPort,
                            kDefaultCastPort, kDefaultMaxBitrate);
}

// Attempts to parse `string_form` into an IPEndpoint. The format is a
// standard-format IPv4 or IPv6 address followed by an optional colon and port.
// If the port is not provided, kDefaultCastPort is assumed.
//
// If the parse fails, a zero-port IPEndpoint is returned.
IPEndpoint ParseAsEndpoint(const char* string_form) {
  IPEndpoint result{};
  const ErrorOr<IPEndpoint> parsed_endpoint = IPEndpoint::Parse(string_form);
  if (parsed_endpoint.is_value()) {
    result = parsed_endpoint.value();
  } else {
    const ErrorOr<IPAddress> parsed_address = IPAddress::Parse(string_form);
    if (parsed_address.is_value()) {
      result = {parsed_address.value(), kDefaultCastPort};
    }
  }
  return result;
}

std::optional<VideoCodec> ParseCodec(std::string_view arg) {
  // We can only support codecs that have a corresponding encoder library.
  static constexpr std::array<VideoCodec, 3> kSupportedCodecs = {
      {VideoCodec::kVp8, VideoCodec::kVp9, VideoCodec::kAv1}};

  const auto parsed = StringToVideoCodec(arg);
  if (!parsed || std::ranges::find(kSupportedCodecs, parsed.value()) ==
                     std::ranges::end(kSupportedCodecs)) {
    OSP_LOG_ERROR << "Invalid --codec specified: " << arg << " is not one of: "
                  << string_util::Join(kSupportedCodecs, " ");
    return std::nullopt;
  }
  return parsed.value();
}

struct Arguments {
  // Required positional arguments
  const char* iface_or_endpoint = nullptr;
  const char* file_path = nullptr;

  // Optional arguments
  int max_bitrate = kDefaultMaxBitrate;
  bool should_loop_video = true;
  std::string developer_certificate_path;
  bool use_android_rtp_hack = false;
  bool use_remoting = false;
  bool is_verbose = false;
  VideoCodec codec = VideoCodec::kVp8;
  std::unique_ptr<TextTraceLoggingPlatform> trace_logger;
  bool enable_dscp = true;
};

std::optional<Arguments> ParseArgs(int argc, char* argv[]) {
  // A note about modifying command line arguments: consider uniformity
  // between all Open Screen executables. If it is a platform feature
  // being exposed, consider if it applies to the standalone receiver,
  // standalone sender, osp demo, and test_main argument options.
  const get_opt::option kArgumentOptions[] = {
      {"android-hack", no_argument, nullptr, 'a'},
      {"codec", required_argument, nullptr, 'c'},
      {"developer-certificate", required_argument, nullptr, 'd'},
      {"help", no_argument, nullptr, 'h'},
      {"max-bitrate", required_argument, nullptr, 'm'},
      {"no-looping", no_argument, nullptr, 'n'},
      {"disable-dscp", no_argument, nullptr, 'q'},
      {"remoting", no_argument, nullptr, 'r'},
      {"tracing", no_argument, nullptr, 't'},
      {"verbose", no_argument, nullptr, 'v'},
      {nullptr, 0, nullptr, 0}};

  Arguments args;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "ac:d:hm:nqrtv", kArgumentOptions,
                           nullptr)) != -1) {
    switch (ch) {
      case 'a':
        args.use_android_rtp_hack = true;
        break;
      case 'm':
        if (!string_parse::ParseAsciiNumber(get_opt::optarg,
                                            args.max_bitrate) ||
            args.max_bitrate < kMinRequiredBitrate) {
          OSP_LOG_ERROR << "Invalid --max-bitrate specified: "
                        << get_opt::optarg << " is less than "
                        << kMinRequiredBitrate;
          return std::nullopt;
        }
        break;
      case 'n':
        args.should_loop_video = false;
        break;
      case 'd':
        args.developer_certificate_path = get_opt::optarg;
        break;
      case 'q':
        args.enable_dscp = false;
        break;
      case 'r':
        args.use_remoting = true;
        break;
      case 't':
        args.trace_logger = std::make_unique<TextTraceLoggingPlatform>();
        break;
      case 'v':
        args.is_verbose = true;
        break;
      case 'h':
        return std::nullopt;
      case 'c':
        if (const auto parsed = ParseCodec(get_opt::optarg)) {
          args.codec = *parsed;
        } else {
          return std::nullopt;
        }
        break;
    }
  }

  // The second to last command line argument must be one of: 1) the network
  // interface name or 2) a specific IP address (port is optional). The last
  // argument must be the path to the file.
  if (get_opt::optind != (argc - 2)) {
    return std::nullopt;
  }
  args.iface_or_endpoint = argv[get_opt::optind++];
  args.file_path = argv[get_opt::optind];
  return args;
}

int StandaloneSenderMain(int argc, char* argv[]) {
  const std::optional<Arguments> args = ParseArgs(argc, argv);
  if (!args) {
    LogUsage(argv[0]);
    return 1;
  }

  openscreen::SetLogLevel(args->is_verbose ? openscreen::LogLevel::kVerbose
                                           : openscreen::LogLevel::kInfo);
  std::unique_ptr<TrustStore> cast_trust_store;
  if (!args->developer_certificate_path.empty()) {
    cast_trust_store =
        TrustStore::CreateInstanceFromPemFile(args->developer_certificate_path);
    OSP_LOG_INFO << "using cast trust store generated from: "
                 << args->developer_certificate_path;
  }
  if (!cast_trust_store) {
    cast_trust_store = CastTrustStore::Create();
  }

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(milliseconds(50),
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  IPEndpoint remote_endpoint = ParseAsEndpoint(args->iface_or_endpoint);
  if (!remote_endpoint.port) {
    for (const InterfaceInfo& interface : GetNetworkInterfaces()) {
      if (interface.name == args->iface_or_endpoint) {
        ReceiverChooser chooser(interface, *task_runner,
                                [&](IPEndpoint endpoint) {
                                  remote_endpoint = endpoint;
                                  task_runner->RequestStopSoon();
                                });
        task_runner->RunUntilSignaled();
        break;
      }
    }

    if (!remote_endpoint.port) {
      OSP_LOG_ERROR << "No Cast Receiver chosen, or bad command-line argument. "
                       "Cannot continue.";
      LogUsage(argv[0]);
      return 2;
    }
  }

  // `cast_agent` must be constructed and destroyed from a Task run by the
  // TaskRunner.
  std::unique_ptr<LoopingFileCastAgent> cast_agent;
  task_runner->PostTask([&] {
    cast_agent = std::make_unique<LoopingFileCastAgent>(
        *task_runner, std::move(cast_trust_store),
        [&] { task_runner->RequestStopSoon(); });

    cast_agent->Connect({.receiver_endpoint = remote_endpoint,
                         .path_to_file = args->file_path,
                         .max_bitrate = args->max_bitrate,
                         .should_include_video = true,
                         .use_android_rtp_hack = args->use_android_rtp_hack,
                         .use_remoting = args->use_remoting,
                         .should_loop_video = args->should_loop_video,
                         .codec = args->codec,
                         .enable_dscp = args->enable_dscp});
  });

  // Run the event loop until SIGINT (e.g., CTRL-C at the console) or
  // SIGTERM are signaled.
  task_runner->RunUntilSignaled();

  // Spin the TaskRunner to destroy the `cast_agent` and execute any lingering
  // destruction/shutdown tasks.
  OSP_LOG_INFO << "Shutting down...";
  task_runner->PostTask([&] {
    cast_agent.reset();
    task_runner->RequestStopSoon();
  });
  task_runner->RunUntilStopped();
  OSP_LOG_INFO << "Bye!";

  PlatformClientPosix::ShutDown();
  return 0;
}

}  // namespace
}  // namespace openscreen::cast
#endif

int main(int argc, char* argv[]) {
  // Ignore SIGPIPE events at the application level -- tearing down the network
  // interface will close a TLS or UDP socket connection, which will result
  // in a more graceful exit than terminating on the SIGPIPE call.
  std::signal(SIGPIPE, SIG_IGN);

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
  return openscreen::cast::StandaloneSenderMain(argc, argv);
#else
  OSP_LOG_ERROR
      << "It compiled! However, you need to configure the build to point to "
         "external libraries in order to build a useful app. For more "
         "information, please check the docs for external_libraries.md.";
  return 1;
#endif
}
