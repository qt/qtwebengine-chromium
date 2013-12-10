// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_INPUT_SCOPE_H_
#define UI_BASE_IME_WIN_TSF_INPUT_SCOPE_H_

#include <InputScope.h>
#include <Windows.h>
#include <vector>

#include "base/basictypes.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

namespace ui {
namespace tsf_inputscope {

// Returns InputScope list corresoponding to ui::TextInputType and
// ui::TextInputMode.
// This function is only used from following functions but declared for test.
UI_EXPORT std::vector<InputScope> GetInputScopes(TextInputType text_input_type,
                                                 TextInputMode text_input_mode);

// Returns an instance of ITfInputScope, which is the Windows-specific
// category representation corresponding to ui::TextInputType and
// ui::TextInputMode that we are using to specify the expected text type
// in the target field.
// The returned instance has 0 reference count. The caller must maintain its
// reference count.
UI_EXPORT ITfInputScope* CreateInputScope(TextInputType text_input_type,
                                          TextInputMode text_input_mode);

// A wrapper of the SetInputScopes API exported by msctf.dll.
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms629026.aspx
// Does nothing on Windows XP in case TSF is disabled.
// NOTE: For TSF-aware window, you should use ITfInputScope instead.
UI_EXPORT void SetInputScopeForTsfUnawareWindow(
    HWND window_handle,
    TextInputType text_input_type,
    TextInputMode text_input_mode);

}  // namespace tsf_inputscope
}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_INPUT_SCOPE_H_
