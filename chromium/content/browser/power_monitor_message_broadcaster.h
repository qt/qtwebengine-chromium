// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_MONITOR_MESSAGE_BROADCASTER_H_
#define CONTENT_BROWSER_POWER_MONITOR_MESSAGE_BROADCASTER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/power_monitor/power_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace IPC {
  class Sender;
}

namespace content {

// A class used to monitor the power state change and communicate it to child
// processes via IPC.
class CONTENT_EXPORT PowerMonitorMessageBroadcaster
    : public base::PowerObserver {
 public:
  explicit PowerMonitorMessageBroadcaster(IPC::Sender* sender);
  virtual ~PowerMonitorMessageBroadcaster();

  // Implement PowerObserver.
  virtual void OnPowerStateChange(bool on_battery_power) OVERRIDE;
  virtual void OnSuspend() OVERRIDE;
  virtual void OnResume() OVERRIDE;

 private:
  IPC::Sender* sender_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorMessageBroadcaster);
};

}  // namespace base

#endif  // CONTENT_BROWSER_POWER_MONITOR_MESSAGE_BROADCASTER_H_
