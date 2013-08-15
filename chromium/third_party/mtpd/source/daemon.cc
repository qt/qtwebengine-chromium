// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "daemon.h"

namespace mtpd {

Daemon::Daemon(DBus::Connection* dbus_connection)
    : server_(*dbus_connection) {
}

Daemon::~Daemon() {
}

int Daemon::GetDeviceEventDescriptor() const {
  return server_.GetDeviceEventDescriptor();
}

void Daemon::ProcessDeviceEvents() {
  server_.ProcessDeviceEvents();
}

}  // namespace mtpd
