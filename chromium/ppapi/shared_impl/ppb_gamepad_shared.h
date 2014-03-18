// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_

#include "base/atomicops.h"
#include "base/strings/string16.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// TODO(brettw) when we remove the non-IPC-based gamepad implementation, this
// code should all move into the GamepadResource.

#pragma pack(push, 1)

// This must match the definition of blink::Gamepad. The GamepadHost unit test
// has some compile asserts to validate this.
struct WebKitGamepad {
    static const size_t kIdLengthCap = 128;
    static const size_t kAxesLengthCap = 16;
    static const size_t kButtonsLengthCap = 32;

    // Is there a gamepad connected at this index?
    bool connected;

    // Device identifier (based on manufacturer, model, etc.).
    char16 id[kIdLengthCap];

    // Monotonically increasing value referring to when the data were last
    // updated.
    unsigned long long timestamp;

    // Number of valid entries in the axes array.
    unsigned axes_length;

    // Normalized values representing axes, in the range [-1..1].
    float axes[kAxesLengthCap];

    // Number of valid entries in the buttons array.
    unsigned buttons_length;

    // Normalized values representing buttons, in the range [0..1].
    float buttons[kButtonsLengthCap];
};

// This must match the definition of blink::Gamepads. The GamepadHost unit
// test has some compile asserts to validate this.
struct WebKitGamepads {
    static const size_t kItemsLengthCap = 4;

    // Number of valid entries in the items array.
    unsigned length;

    // Gamepad data for N separate gamepad devices.
    WebKitGamepad items[kItemsLengthCap];
};

// This is the structure store in shared memory. It must match
// content/common/gamepad_hardware_buffer.h. The GamepadHost unit test has
// some compile asserts to validate this.
struct ContentGamepadHardwareBuffer {
  base::subtle::Atomic32 sequence;
  WebKitGamepads buffer;
};

#pragma pack(pop)

PPAPI_SHARED_EXPORT void ConvertWebKitGamepadData(
    const WebKitGamepads& webkit_data,
    PP_GamepadsSampleData* output_data);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
