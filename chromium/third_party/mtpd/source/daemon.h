// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_DAEMON_H_
#define MTPD_DAEMON_H_

#include <base/basictypes.h>

#include "mtpd_server_impl.h"

namespace mtpd {

class Daemon {
 public:
  explicit Daemon(DBus::Connection* dbus_connection);
  ~Daemon();

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

 private:
  MtpdServer server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace mtpd

#endif  // MTPD_DAEMON_H_
