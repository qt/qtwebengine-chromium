// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device motion.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DeviceMotionMsgStart

// Messages sent from the renderer to the browser.

// Asks the browser process to start polling, and return a shared memory
// handles that will hold the data from the hardware. See
// device_motion_hardware_buffer.h for a description of how synchronization is
// handled. The number of Starts should match the number of Stops.
IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StartPolling)
IPC_MESSAGE_CONTROL1(DeviceMotionMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StopPolling)
