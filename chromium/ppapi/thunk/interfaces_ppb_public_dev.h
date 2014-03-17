// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

// Map the old dev console interface to the stable one (which is the same) to
// keep Flash, etc. working.
PROXIED_IFACE(PPB_Instance, "PPB_Console(Dev);0.1", PPB_Console_1_0)
PROXIED_IFACE(NoAPIName, PPB_ALARMS_DEV_INTERFACE_0_1, PPB_Alarms_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_CURSOR_CONTROL_DEV_INTERFACE_0_4,
              PPB_CursorControl_Dev_0_4)
PROXIED_IFACE(NoAPIName, PPB_EXT_SOCKET_DEV_INTERFACE_0_1,
              PPB_Ext_Socket_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_EXT_SOCKET_DEV_INTERFACE_0_2,
              PPB_Ext_Socket_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_FILECHOOSER_DEV_INTERFACE_0_5,
              PPB_FileChooser_Dev_0_5)
PROXIED_IFACE(NoAPIName, PPB_FILECHOOSER_DEV_INTERFACE_0_6,
              PPB_FileChooser_Dev_0_6)
UNPROXIED_IFACE(PPB_Find, PPB_FIND_DEV_INTERFACE_0_3, PPB_Find_Dev_0_3)
PROXIED_IFACE(NoAPIName, PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_2,
              PPB_IMEInputEvent_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_KEYBOARD_INPUT_EVENT_DEV_INTERFACE_0_2,
              PPB_KeyboardInputEvent_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_MEMORY_DEV_INTERFACE_0_1, PPB_Memory_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_PRINTING_DEV_INTERFACE_0_7,
              PPB_Printing_Dev_0_7)
PROXIED_IFACE(NoAPIName, PPB_RESOURCEARRAY_DEV_INTERFACE_0_1,
              PPB_ResourceArray_Dev_0_1)
PROXIED_IFACE(PPB_Instance, PPB_TEXTINPUT_DEV_INTERFACE_0_2,
              PPB_TextInput_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_TRUETYPEFONT_DEV_INTERFACE_0_1,
              PPB_TrueTypeFont_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_VAR_RESOURCE_DEV_INTERFACE_0_1,
              PPB_VarResource_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_VIEW_DEV_INTERFACE_0_1,
              PPB_View_Dev_0_1)
UNPROXIED_IFACE(PPB_Instance, PPB_ZOOM_DEV_INTERFACE_0_2, PPB_Zoom_Dev_0_2)
PROXIED_IFACE(PPB_Instance, PPB_TRACE_EVENT_DEV_INTERFACE_0_1,
              PPB_Trace_Event_Dev_0_1)
PROXIED_IFACE(PPB_Instance, PPB_TRACE_EVENT_DEV_INTERFACE_0_2,
              PPB_Trace_Event_Dev_0_2)

#if !defined(OS_NACL)
PROXIED_API(PPB_Buffer)
UNPROXIED_API(PPB_Scrollbar)
PROXIED_API(PPB_VideoDecoder)
UNPROXIED_API(PPB_Widget)

PROXIED_IFACE(NoAPIName, PPB_AUDIO_INPUT_DEV_INTERFACE_0_2,
              PPB_AudioInput_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_AUDIO_INPUT_DEV_INTERFACE_0_3,
              PPB_AudioInput_Dev_0_3)
PROXIED_IFACE(NoAPIName, PPB_AUDIO_INPUT_DEV_INTERFACE_0_4,
              PPB_AudioInput_Dev_0_4)
PROXIED_IFACE(NoAPIName, PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1,
              PPB_IMEInputEvent_Dev_0_1)
PROXIED_IFACE(PPB_Buffer, PPB_BUFFER_DEV_INTERFACE_0_4, PPB_Buffer_Dev_0_4)
PROXIED_IFACE(PPB_Graphics3D,
              PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE_0_1,
              PPB_GLESChromiumTextureMapping_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_CRYPTO_DEV_INTERFACE_0_1, PPB_Crypto_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_DEVICEREF_DEV_INTERFACE_0_1, PPB_DeviceRef_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_GRAPHICS2D_DEV_INTERFACE_0_1,
              PPB_Graphics2D_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_GRAPHICS2D_DEV_INTERFACE_0_2,
              PPB_Graphics2D_Dev_0_2)
PROXIED_IFACE(PPB_Instance, PPB_CHAR_SET_DEV_INTERFACE_0_4, PPB_CharSet_Dev_0_4)
PROXIED_IFACE(PPB_Instance, PPB_URLUTIL_DEV_INTERFACE_0_6, PPB_URLUtil_Dev_0_6)
PROXIED_IFACE(PPB_Instance, PPB_URLUTIL_DEV_INTERFACE_0_7, PPB_URLUtil_Dev_0_7)
UNPROXIED_IFACE(PPB_Scrollbar, PPB_SCROLLBAR_DEV_INTERFACE_0_5,
                PPB_Scrollbar_Dev_0_5)
PROXIED_IFACE(PPB_Instance, PPB_TEXTINPUT_DEV_INTERFACE_0_1,
              PPB_TextInput_Dev_0_1)
PROXIED_IFACE(NoAPIName, PPB_VIDEOCAPTURE_DEV_INTERFACE_0_2,
              PPB_VideoCapture_Dev_0_2)
PROXIED_IFACE(NoAPIName, PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3,
              PPB_VideoCapture_Dev_0_3)
PROXIED_IFACE(PPB_VideoDecoder, PPB_VIDEODECODER_DEV_INTERFACE_0_16,
              PPB_VideoDecoder_Dev_0_16)
UNPROXIED_IFACE(PPB_Widget, PPB_WIDGET_DEV_INTERFACE_0_3, PPB_Widget_Dev_0_3)
UNPROXIED_IFACE(PPB_Widget, PPB_WIDGET_DEV_INTERFACE_0_4, PPB_Widget_Dev_0_4)
#endif  // !defined(OS_NACL)

#include "ppapi/thunk/interfaces_postamble.h"
