// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/ssl_server_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/test/net_test_suite.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#include "net/android/net_jni_registrar.h"
#endif

#if !defined(OS_IOS)
#include "net/proxy/proxy_resolver_v8.h"
#endif

using net::internal::ClientSocketPoolBaseHelper;
using net::SpdySession;

int main(int argc, char** argv) {
  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder::Initialize();

#if defined(OS_ANDROID)
  // Register JNI bindings for android. Doing it early as the test suite setup
  // may initiate a call to Java.
  net::android::RegisterJni(base::android::AttachCurrentThread());
  file_util::RegisterContentUriTestUtils(base::android::AttachCurrentThread());
#endif

  NetTestSuite test_suite(argc, argv);
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);

#if defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  // Enable support for SSL server sockets, which must be done while
  // single-threaded.
  net::EnableSSLServerSockets();

#if !defined(OS_IOS)
  // This has to be done on the main thread.
  net::ProxyResolverV8::RememberDefaultIsolate();
#endif

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&NetTestSuite::Run,
                             base::Unretained(&test_suite)));
}
