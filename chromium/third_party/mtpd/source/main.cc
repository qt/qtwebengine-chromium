// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect and access PTP/MTP devices.

#include <glib.h>
#include <glib-object.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <dbus-c++/glib-integration.h>

#include <base/at_exit.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>

#include "build_config.h"
#include "daemon.h"
#include "service_constants.h"

#if defined(CROS_BUILD)
#include <chromeos/syslog_logging.h>
#endif

// TODO(thestig) Merge these once libchrome catches up to Chromium's base,
// or when mtpd moves into its own repo. http://crbug.com/221123
#if defined(CROS_BUILD)
#include <base/string_number_conversions.h>
#else
#include <base/strings/string_number_conversions.h>
#endif

using mtpd::Daemon;

// Messages logged at a level lower than this don't get logged anywhere.
static const char kMinLogLevelSwitch[] = "minloglevel";

void SetupLogging() {
#if defined(CROS_BUILD)
  chromeos::InitLog(chromeos::kLogToSyslog);
#endif

  std::string log_level_str =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kMinLogLevelSwitch);
  int log_level = 0;
  if (base::StringToInt(log_level_str, &log_level) && log_level >= 0)
    logging::SetMinLogLevel(log_level);
}

// This callback will be invoked once there is a new device event that
// should be processed by the Daemon::ProcessDeviceEvents().
gboolean DeviceEventCallback(GIOChannel* /* source */,
                             GIOCondition /* condition */,
                             gpointer data) {
  Daemon* daemon = reinterpret_cast<Daemon*>(data);
  daemon->ProcessDeviceEvents();
  // This function should always return true so that the main loop
  // continues to select on device event file descriptor.
  return true;
}

// This callback will be inovked when this process receives SIGINT or SIGTERM.
gboolean TerminationSignalCallback(GIOChannel* /* source */,
                                   GIOCondition /* condition */,
                                   gpointer data) {
  LOG(INFO) << "Received a signal to terminate the daemon";
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(data);
  g_main_loop_quit(loop);

  // This function can return false to remove this signal handler as we are
  // quitting the main loop anyway.
  return false;
}

int main(int argc, char** argv) {
  ::g_type_init();
  // g_thread_init() is deprecated since glib 2.32 and the symbol is no longer
  // exported since glib 2.34:
  //
  //   https://developer.gnome.org/glib/2.32/glib-Deprecated-Thread-APIs.html
  //
  // To be compatible with various versions of glib, only call g_thread_init()
  // when using glib older than 2.32.0.
#if !(GLIB_CHECK_VERSION(2, 32, 0))
  g_thread_init(NULL);
#endif

  // Needed by base::RandBytes() and other Chromium bits that expects
  // an AtExitManager to exist.
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  SetupLogging();

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create a GMainLoop";

  LOG(INFO) << "Creating the D-Bus dispatcher";
  DBus::Glib::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  LOG(INFO) << "Creating the mtpd server";
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  server_conn.request_name(mtpd::kMtpdServiceName);
  Daemon daemon(&server_conn);

  // Set up a monitor for handling device events.
  g_io_add_watch_full(g_io_channel_unix_new(daemon.GetDeviceEventDescriptor()),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL),
                      DeviceEventCallback,
                      &daemon,
                      NULL);

  // TODO(thestig) Switch back to g_unix_signal_add() once Chromium no longer
  // supports a Linux system with glib older than 2.30.
  //
  // Set up a signal socket and monitor it.
  sigset_t signal_set;
  CHECK_EQ(0, sigemptyset(&signal_set));
  CHECK_EQ(0, sigaddset(&signal_set, SIGINT));
  CHECK_EQ(0, sigaddset(&signal_set, SIGTERM));
  int signal_fd = signalfd(-1, &signal_set, 0);
  PCHECK(signal_fd >= 0);

  // Set up a monitor for |signal_fd|.
  g_io_add_watch_full(g_io_channel_unix_new(signal_fd),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL),
                      TerminationSignalCallback,
                      loop,
                      NULL);

  g_main_loop_run(loop);

  LOG(INFO) << "Cleaning up and exiting";
  g_main_loop_unref(loop);

  return 0;
}
