// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/device_motion_message_filter.h"

#include "content/browser/device_orientation/device_inertial_sensor_service.h"
#include "content/common/device_orientation/device_motion_messages.h"

namespace content {

DeviceMotionMessageFilter::DeviceMotionMessageFilter()
    : is_started_(false) {
}

DeviceMotionMessageFilter::~DeviceMotionMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (is_started_)
    DeviceInertialSensorService::GetInstance()->RemoveConsumer(
        CONSUMER_TYPE_MOTION);
}

bool DeviceMotionMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DeviceMotionMessageFilter,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StartPolling,
                        OnDeviceMotionStartPolling)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StopPolling,
                        OnDeviceMotionStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DeviceMotionMessageFilter::OnDeviceMotionStartPolling() {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceInertialSensorService::GetInstance()->AddConsumer(
      CONSUMER_TYPE_MOTION);
  DidStartDeviceMotionPolling();
}

void DeviceMotionMessageFilter::OnDeviceMotionStopPolling() {
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceInertialSensorService::GetInstance()->RemoveConsumer(
      CONSUMER_TYPE_MOTION);
}

void DeviceMotionMessageFilter::DidStartDeviceMotionPolling() {
  Send(new DeviceMotionMsg_DidStartPolling(
      DeviceInertialSensorService::GetInstance()->
          GetSharedMemoryHandleForProcess(
              CONSUMER_TYPE_MOTION, PeerHandle())));
}

}  // namespace content
