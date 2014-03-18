// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gamepad_shared_memory_reader.h"

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "content/common/gamepad_messages.h"
#include "content/common/gamepad_user_gesture.h"
#include "content/public/renderer/render_thread.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

GamepadSharedMemoryReader::GamepadSharedMemoryReader()
    : gamepad_hardware_buffer_(NULL),
      ever_interacted_with_(false) {
  CHECK(RenderThread::Get()->Send(new GamepadHostMsg_StartPolling(
      &renderer_shared_memory_handle_)));
  // If we don't get a valid handle from the browser, don't try to Map (we're
  // probably out of memory or file handles).
  bool valid_handle = base::SharedMemory::IsHandleValid(
      renderer_shared_memory_handle_);
  UMA_HISTOGRAM_BOOLEAN("Gamepad.ValidSharedMemoryHandle", valid_handle);
  if (!valid_handle)
    return;
  renderer_shared_memory_.reset(
      new base::SharedMemory(renderer_shared_memory_handle_, true));
  CHECK(renderer_shared_memory_->Map(sizeof(GamepadHardwareBuffer)));
  void *memory = renderer_shared_memory_->memory();
  CHECK(memory);
  gamepad_hardware_buffer_ =
      static_cast<GamepadHardwareBuffer*>(memory);
}

void GamepadSharedMemoryReader::SampleGamepads(blink::WebGamepads& gamepads) {
  // ==========
  //   DANGER
  // ==========
  //
  // This logic is duplicated in Pepper as well. If you change it, that also
  // needs to be in sync. See ppapi/proxy/gamepad_resource.cc.
  blink::WebGamepads read_into;
  TRACE_EVENT0("GAMEPAD", "SampleGamepads");

  if (!base::SharedMemory::IsHandleValid(renderer_shared_memory_handle_))
    return;

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer. TODO(scottmg) Tune this
  // number (as low as 1?) if histogram shows distribution as mostly
  // 0-and-maximum.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  do {
    version = gamepad_hardware_buffer_->sequence.ReadBegin();
    memcpy(&read_into, &gamepad_hardware_buffer_->buffer, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (gamepad_hardware_buffer_->sequence.ReadRetry(version));
  UMA_HISTOGRAM_COUNTS("Gamepad.ReadContentionCount", contention_count);

  if (contention_count >= kMaximumContentionCount) {
    // We failed to successfully read, presumably because the hardware
    // thread was taking unusually long. Don't copy the data to the output
    // buffer, and simply leave what was there before.
    return;
  }

  // New data was read successfully, copy it into the output buffer.
  memcpy(&gamepads, &read_into, sizeof(gamepads));

  if (!ever_interacted_with_) {
    if (GamepadsHaveUserGesture(gamepads)) {
      ever_interacted_with_ = true;
    } else {
      // Clear the connected flag if the user hasn't interacted with any of the
      // gamepads to prevent fingerprinting. The actual data is not cleared.
      // WebKit will only copy out data into the JS buffers for connected
      // gamepads so this is sufficient.
      for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; i++)
        gamepads.items[i].connected = false;
    }
  }
}

GamepadSharedMemoryReader::~GamepadSharedMemoryReader() {
  RenderThread::Get()->Send(new GamepadHostMsg_StopPolling());
}

} // namespace content
