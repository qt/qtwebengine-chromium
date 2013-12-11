/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NOTE: this is auto-generated from IDL */
#include "ppapi/generators/pnacl_shim.h"

#include "ppapi/c/ppb.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_graphics_2d_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_keyboard_input_event_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/dev/ppp_network_state_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/extensions/dev/ppb_ext_alarms_dev.h"
#include "ppapi/c/extensions/dev/ppb_ext_events_dev.h"
#include "ppapi/c/extensions/dev/ppb_ext_socket_dev.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_device_id.h"
#include "ppapi/c/private/ppb_flash_drm.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_message_loop.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/c/private/ppb_platform_verification_private.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/private/ppb_video_destination_private.h"
#include "ppapi/c/private/ppb_video_source_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"

/* Use local strcmp to avoid dependency on libc. */
static int mystrcmp(const char* s1, const char *s2) {
  while (1) {
    if (*s1 == 0) break;
    if (*s2 == 0) break;
    if (*s1 != *s2) break;
    ++s1;
    ++s2;
  }
  return (int)(*s1) - (int)(*s2);
}

/* BEGIN Declarations for all Wrapper Infos */

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Console_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Core_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileSystem_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics3D_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MessageLoop_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseLock_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkList_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkProxy_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TextInputController_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLLoader_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArray_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarDictionary_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WebSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Messaging_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Font_Dev_0_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Printing_Dev_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_9;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_91;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_92;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Selection_Dev_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_5;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_13_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_DeviceID_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_DRM_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Menu_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Instance_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NaCl_Private_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Talk_Private_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Talk_Private_2_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UMA_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Instance_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Events_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2;
/* END Declarations for all Wrapper Infos. */

/* Not generating wrapper methods for PPB_Audio_1_0 */

/* Not generating wrapper methods for PPB_Audio_1_1 */

/* Not generating wrapper methods for PPB_AudioConfig_1_0 */

/* Not generating wrapper methods for PPB_AudioConfig_1_1 */

/* Begin wrapper methods for PPB_Console_1_0 */

static void Pnacl_M25_PPB_Console_Log(PP_Instance instance, PP_LogLevel level, struct PP_Var* value) {
  const struct PPB_Console_1_0 *iface = Pnacl_WrapperInfo_PPB_Console_1_0.real_iface;
  iface->Log(instance, level, *value);
}

static void Pnacl_M25_PPB_Console_LogWithSource(PP_Instance instance, PP_LogLevel level, struct PP_Var* source, struct PP_Var* value) {
  const struct PPB_Console_1_0 *iface = Pnacl_WrapperInfo_PPB_Console_1_0.real_iface;
  iface->LogWithSource(instance, level, *source, *value);
}

/* End wrapper methods for PPB_Console_1_0 */

/* Begin wrapper methods for PPB_Core_1_0 */

static void Pnacl_M14_PPB_Core_AddRefResource(PP_Resource resource) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->AddRefResource(resource);
}

static void Pnacl_M14_PPB_Core_ReleaseResource(PP_Resource resource) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->ReleaseResource(resource);
}

static PP_Time Pnacl_M14_PPB_Core_GetTime(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->GetTime();
}

static PP_TimeTicks Pnacl_M14_PPB_Core_GetTimeTicks(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->GetTimeTicks();
}

static void Pnacl_M14_PPB_Core_CallOnMainThread(int32_t delay_in_milliseconds, struct PP_CompletionCallback* callback, int32_t result) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->CallOnMainThread(delay_in_milliseconds, *callback, result);
}

static PP_Bool Pnacl_M14_PPB_Core_IsMainThread(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->IsMainThread();
}

/* End wrapper methods for PPB_Core_1_0 */

/* Begin wrapper methods for PPB_FileIO_1_0 */

static PP_Resource Pnacl_M14_PPB_FileIO_Create(PP_Instance instance) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_FileIO_IsFileIO(PP_Resource resource) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->IsFileIO(resource);
}

static int32_t Pnacl_M14_PPB_FileIO_Open(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Open(file_io, file_ref, open_flags, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Query(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Query(file_io, info, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Touch(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Touch(file_io, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Read(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Read(file_io, offset, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Write(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Write(file_io, offset, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_SetLength(PP_Resource file_io, int64_t length, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->SetLength(file_io, length, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Flush(PP_Resource file_io, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Flush(file_io, *callback);
}

static void Pnacl_M14_PPB_FileIO_Close(PP_Resource file_io) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  iface->Close(file_io);
}

/* End wrapper methods for PPB_FileIO_1_0 */

/* Begin wrapper methods for PPB_FileIO_1_1 */

static PP_Resource Pnacl_M25_PPB_FileIO_Create(PP_Instance instance) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_FileIO_IsFileIO(PP_Resource resource) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->IsFileIO(resource);
}

static int32_t Pnacl_M25_PPB_FileIO_Open(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Open(file_io, file_ref, open_flags, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Query(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Query(file_io, info, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Touch(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Touch(file_io, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Read(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Read(file_io, offset, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Write(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Write(file_io, offset, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_SetLength(PP_Resource file_io, int64_t length, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->SetLength(file_io, length, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Flush(PP_Resource file_io, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Flush(file_io, *callback);
}

static void Pnacl_M25_PPB_FileIO_Close(PP_Resource file_io) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  iface->Close(file_io);
}

static int32_t Pnacl_M25_PPB_FileIO_ReadToArray(PP_Resource file_io, int64_t offset, int32_t max_read_length, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->ReadToArray(file_io, offset, max_read_length, output, *callback);
}

/* End wrapper methods for PPB_FileIO_1_1 */

/* Begin wrapper methods for PPB_FileRef_1_0 */

static PP_Resource Pnacl_M14_PPB_FileRef_Create(PP_Resource file_system, const char* path) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Create(file_system, path);
}

static PP_Bool Pnacl_M14_PPB_FileRef_IsFileRef(PP_Resource resource) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->IsFileRef(resource);
}

static PP_FileSystemType Pnacl_M14_PPB_FileRef_GetFileSystemType(PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->GetFileSystemType(file_ref);
}

static void Pnacl_M14_PPB_FileRef_GetName(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  *_struct_result = iface->GetName(file_ref);
}

static void Pnacl_M14_PPB_FileRef_GetPath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  *_struct_result = iface->GetPath(file_ref);
}

static PP_Resource Pnacl_M14_PPB_FileRef_GetParent(PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->GetParent(file_ref);
}

static int32_t Pnacl_M14_PPB_FileRef_MakeDirectory(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->MakeDirectory(directory_ref, make_ancestors, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Touch(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Touch(file_ref, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Delete(PP_Resource file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Delete(file_ref, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Rename(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Rename(file_ref, new_file_ref, *callback);
}

/* End wrapper methods for PPB_FileRef_1_0 */

/* Begin wrapper methods for PPB_FileRef_1_1 */

static PP_Resource Pnacl_M28_PPB_FileRef_Create(PP_Resource file_system, const char* path) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Create(file_system, path);
}

static PP_Bool Pnacl_M28_PPB_FileRef_IsFileRef(PP_Resource resource) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->IsFileRef(resource);
}

static PP_FileSystemType Pnacl_M28_PPB_FileRef_GetFileSystemType(PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->GetFileSystemType(file_ref);
}

static void Pnacl_M28_PPB_FileRef_GetName(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  *_struct_result = iface->GetName(file_ref);
}

static void Pnacl_M28_PPB_FileRef_GetPath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  *_struct_result = iface->GetPath(file_ref);
}

static PP_Resource Pnacl_M28_PPB_FileRef_GetParent(PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->GetParent(file_ref);
}

static int32_t Pnacl_M28_PPB_FileRef_MakeDirectory(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->MakeDirectory(directory_ref, make_ancestors, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Touch(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Touch(file_ref, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Delete(PP_Resource file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Delete(file_ref, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Rename(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Rename(file_ref, new_file_ref, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Query(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Query(file_ref, info, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_ReadDirectoryEntries(PP_Resource file_ref, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->ReadDirectoryEntries(file_ref, *output, *callback);
}

/* End wrapper methods for PPB_FileRef_1_1 */

/* Begin wrapper methods for PPB_FileSystem_1_0 */

static PP_Resource Pnacl_M14_PPB_FileSystem_Create(PP_Instance instance, PP_FileSystemType type) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->Create(instance, type);
}

static PP_Bool Pnacl_M14_PPB_FileSystem_IsFileSystem(PP_Resource resource) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->IsFileSystem(resource);
}

static int32_t Pnacl_M14_PPB_FileSystem_Open(PP_Resource file_system, int64_t expected_size, struct PP_CompletionCallback* callback) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->Open(file_system, expected_size, *callback);
}

static PP_FileSystemType Pnacl_M14_PPB_FileSystem_GetType(PP_Resource file_system) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->GetType(file_system);
}

/* End wrapper methods for PPB_FileSystem_1_0 */

/* Not generating wrapper methods for PPB_Fullscreen_1_0 */

/* Not generating wrapper methods for PPB_Gamepad_1_0 */

/* Begin wrapper methods for PPB_Graphics2D_1_0 */

static PP_Resource Pnacl_M14_PPB_Graphics2D_Create(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Create(instance, size, is_always_opaque);
}

static PP_Bool Pnacl_M14_PPB_Graphics2D_IsGraphics2D(PP_Resource resource) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->IsGraphics2D(resource);
}

static PP_Bool Pnacl_M14_PPB_Graphics2D_Describe(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Describe(graphics_2d, size, is_always_opaque);
}

static void Pnacl_M14_PPB_Graphics2D_PaintImageData(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->PaintImageData(graphics_2d, image_data, top_left, src_rect);
}

static void Pnacl_M14_PPB_Graphics2D_Scroll(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->Scroll(graphics_2d, clip_rect, amount);
}

static void Pnacl_M14_PPB_Graphics2D_ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->ReplaceContents(graphics_2d, image_data);
}

static int32_t Pnacl_M14_PPB_Graphics2D_Flush(PP_Resource graphics_2d, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Flush(graphics_2d, *callback);
}

/* End wrapper methods for PPB_Graphics2D_1_0 */

/* Begin wrapper methods for PPB_Graphics2D_1_1 */

static PP_Resource Pnacl_M27_PPB_Graphics2D_Create(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Create(instance, size, is_always_opaque);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_IsGraphics2D(PP_Resource resource) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->IsGraphics2D(resource);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_Describe(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Describe(graphics_2d, size, is_always_opaque);
}

static void Pnacl_M27_PPB_Graphics2D_PaintImageData(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->PaintImageData(graphics_2d, image_data, top_left, src_rect);
}

static void Pnacl_M27_PPB_Graphics2D_Scroll(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->Scroll(graphics_2d, clip_rect, amount);
}

static void Pnacl_M27_PPB_Graphics2D_ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->ReplaceContents(graphics_2d, image_data);
}

static int32_t Pnacl_M27_PPB_Graphics2D_Flush(PP_Resource graphics_2d, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Flush(graphics_2d, *callback);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_SetScale(PP_Resource resource, float scale) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->SetScale(resource, scale);
}

static float Pnacl_M27_PPB_Graphics2D_GetScale(PP_Resource resource) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->GetScale(resource);
}

/* End wrapper methods for PPB_Graphics2D_1_1 */

/* Begin wrapper methods for PPB_Graphics3D_1_0 */

static int32_t Pnacl_M15_PPB_Graphics3D_GetAttribMaxValue(PP_Resource instance, int32_t attribute, int32_t* value) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetAttribMaxValue(instance, attribute, value);
}

static PP_Resource Pnacl_M15_PPB_Graphics3D_Create(PP_Instance instance, PP_Resource share_context, const int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->Create(instance, share_context, attrib_list);
}

static PP_Bool Pnacl_M15_PPB_Graphics3D_IsGraphics3D(PP_Resource resource) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->IsGraphics3D(resource);
}

static int32_t Pnacl_M15_PPB_Graphics3D_GetAttribs(PP_Resource context, int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetAttribs(context, attrib_list);
}

static int32_t Pnacl_M15_PPB_Graphics3D_SetAttribs(PP_Resource context, const int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->SetAttribs(context, attrib_list);
}

static int32_t Pnacl_M15_PPB_Graphics3D_GetError(PP_Resource context) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetError(context);
}

static int32_t Pnacl_M15_PPB_Graphics3D_ResizeBuffers(PP_Resource context, int32_t width, int32_t height) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->ResizeBuffers(context, width, height);
}

static int32_t Pnacl_M15_PPB_Graphics3D_SwapBuffers(PP_Resource context, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->SwapBuffers(context, *callback);
}

/* End wrapper methods for PPB_Graphics3D_1_0 */

/* Begin wrapper methods for PPB_HostResolver_1_0 */

static PP_Resource Pnacl_M29_PPB_HostResolver_Create(PP_Instance instance) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_HostResolver_IsHostResolver(PP_Resource resource) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->IsHostResolver(resource);
}

static int32_t Pnacl_M29_PPB_HostResolver_Resolve(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Hint* hint, struct PP_CompletionCallback* callback) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->Resolve(host_resolver, host, port, hint, *callback);
}

static void Pnacl_M29_PPB_HostResolver_GetCanonicalName(struct PP_Var* _struct_result, PP_Resource host_resolver) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  *_struct_result = iface->GetCanonicalName(host_resolver);
}

static uint32_t Pnacl_M29_PPB_HostResolver_GetNetAddressCount(PP_Resource host_resolver) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->GetNetAddressCount(host_resolver);
}

static PP_Resource Pnacl_M29_PPB_HostResolver_GetNetAddress(PP_Resource host_resolver, uint32_t index) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->GetNetAddress(host_resolver, index);
}

/* End wrapper methods for PPB_HostResolver_1_0 */

/* Not generating wrapper methods for PPB_ImageData_1_0 */

/* Not generating wrapper methods for PPB_InputEvent_1_0 */

/* Begin wrapper methods for PPB_MouseInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_MouseInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, mouse_button, mouse_position, click_count);
}

static PP_Bool Pnacl_M13_PPB_MouseInputEvent_IsMouseInputEvent(PP_Resource resource) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->IsMouseInputEvent(resource);
}

static PP_InputEvent_MouseButton Pnacl_M13_PPB_MouseInputEvent_GetButton(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->GetButton(mouse_event);
}

static void Pnacl_M13_PPB_MouseInputEvent_GetPosition(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  *_struct_result = iface->GetPosition(mouse_event);
}

static int32_t Pnacl_M13_PPB_MouseInputEvent_GetClickCount(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->GetClickCount(mouse_event);
}

/* End wrapper methods for PPB_MouseInputEvent_1_0 */

/* Begin wrapper methods for PPB_MouseInputEvent_1_1 */

static PP_Resource Pnacl_M14_PPB_MouseInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count, const struct PP_Point* mouse_movement) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, mouse_button, mouse_position, click_count, mouse_movement);
}

static PP_Bool Pnacl_M14_PPB_MouseInputEvent_IsMouseInputEvent(PP_Resource resource) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->IsMouseInputEvent(resource);
}

static PP_InputEvent_MouseButton Pnacl_M14_PPB_MouseInputEvent_GetButton(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->GetButton(mouse_event);
}

static void Pnacl_M14_PPB_MouseInputEvent_GetPosition(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  *_struct_result = iface->GetPosition(mouse_event);
}

static int32_t Pnacl_M14_PPB_MouseInputEvent_GetClickCount(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->GetClickCount(mouse_event);
}

static void Pnacl_M14_PPB_MouseInputEvent_GetMovement(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  *_struct_result = iface->GetMovement(mouse_event);
}

/* End wrapper methods for PPB_MouseInputEvent_1_1 */

/* Begin wrapper methods for PPB_WheelInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_WheelInputEvent_Create(PP_Instance instance, PP_TimeTicks time_stamp, uint32_t modifiers, const struct PP_FloatPoint* wheel_delta, const struct PP_FloatPoint* wheel_ticks, PP_Bool scroll_by_page) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->Create(instance, time_stamp, modifiers, wheel_delta, wheel_ticks, scroll_by_page);
}

static PP_Bool Pnacl_M13_PPB_WheelInputEvent_IsWheelInputEvent(PP_Resource resource) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->IsWheelInputEvent(resource);
}

static void Pnacl_M13_PPB_WheelInputEvent_GetDelta(struct PP_FloatPoint* _struct_result, PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  *_struct_result = iface->GetDelta(wheel_event);
}

static void Pnacl_M13_PPB_WheelInputEvent_GetTicks(struct PP_FloatPoint* _struct_result, PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTicks(wheel_event);
}

static PP_Bool Pnacl_M13_PPB_WheelInputEvent_GetScrollByPage(PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->GetScrollByPage(wheel_event);
}

/* End wrapper methods for PPB_WheelInputEvent_1_0 */

/* Begin wrapper methods for PPB_KeyboardInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_KeyboardInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var* character_text) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, key_code, *character_text);
}

static PP_Bool Pnacl_M13_PPB_KeyboardInputEvent_IsKeyboardInputEvent(PP_Resource resource) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->IsKeyboardInputEvent(resource);
}

static uint32_t Pnacl_M13_PPB_KeyboardInputEvent_GetKeyCode(PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->GetKeyCode(key_event);
}

static void Pnacl_M13_PPB_KeyboardInputEvent_GetCharacterText(struct PP_Var* _struct_result, PP_Resource character_event) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  *_struct_result = iface->GetCharacterText(character_event);
}

/* End wrapper methods for PPB_KeyboardInputEvent_1_0 */

/* Begin wrapper methods for PPB_TouchInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_TouchInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers);
}

static void Pnacl_M13_PPB_TouchInputEvent_AddTouchPoint(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  iface->AddTouchPoint(touch_event, list, point);
}

static PP_Bool Pnacl_M13_PPB_TouchInputEvent_IsTouchInputEvent(PP_Resource resource) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->IsTouchInputEvent(resource);
}

static uint32_t Pnacl_M13_PPB_TouchInputEvent_GetTouchCount(PP_Resource resource, PP_TouchListType list) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->GetTouchCount(resource, list);
}

static void Pnacl_M13_PPB_TouchInputEvent_GetTouchByIndex(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t index) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTouchByIndex(resource, list, index);
}

static void Pnacl_M13_PPB_TouchInputEvent_GetTouchById(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t touch_id) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTouchById(resource, list, touch_id);
}

/* End wrapper methods for PPB_TouchInputEvent_1_0 */

/* Begin wrapper methods for PPB_IMEInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_IMEInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var* text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, *text, segment_number, segment_offsets, target_segment, selection_start, selection_end);
}

static PP_Bool Pnacl_M13_PPB_IMEInputEvent_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M13_PPB_IMEInputEvent_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M13_PPB_IMEInputEvent_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M13_PPB_IMEInputEvent_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M13_PPB_IMEInputEvent_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M13_PPB_IMEInputEvent_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_1_0 */

/* Not generating wrapper methods for PPB_Instance_1_0 */

/* Begin wrapper methods for PPB_MessageLoop_1_0 */

static PP_Resource Pnacl_M25_PPB_MessageLoop_Create(PP_Instance instance) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Resource Pnacl_M25_PPB_MessageLoop_GetForMainThread(void) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->GetForMainThread();
}

static PP_Resource Pnacl_M25_PPB_MessageLoop_GetCurrent(void) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->GetCurrent();
}

static int32_t Pnacl_M25_PPB_MessageLoop_AttachToCurrentThread(PP_Resource message_loop) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->AttachToCurrentThread(message_loop);
}

static int32_t Pnacl_M25_PPB_MessageLoop_Run(PP_Resource message_loop) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->Run(message_loop);
}

static int32_t Pnacl_M25_PPB_MessageLoop_PostWork(PP_Resource message_loop, struct PP_CompletionCallback* callback, int64_t delay_ms) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->PostWork(message_loop, *callback, delay_ms);
}

static int32_t Pnacl_M25_PPB_MessageLoop_PostQuit(PP_Resource message_loop, PP_Bool should_destroy) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->PostQuit(message_loop, should_destroy);
}

/* End wrapper methods for PPB_MessageLoop_1_0 */

/* Begin wrapper methods for PPB_Messaging_1_0 */

static void Pnacl_M14_PPB_Messaging_PostMessage(PP_Instance instance, struct PP_Var* message) {
  const struct PPB_Messaging_1_0 *iface = Pnacl_WrapperInfo_PPB_Messaging_1_0.real_iface;
  iface->PostMessage(instance, *message);
}

/* End wrapper methods for PPB_Messaging_1_0 */

/* Not generating wrapper methods for PPB_MouseCursor_1_0 */

/* Begin wrapper methods for PPB_MouseLock_1_0 */

static int32_t Pnacl_M16_PPB_MouseLock_LockMouse(PP_Instance instance, struct PP_CompletionCallback* callback) {
  const struct PPB_MouseLock_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseLock_1_0.real_iface;
  return iface->LockMouse(instance, *callback);
}

static void Pnacl_M16_PPB_MouseLock_UnlockMouse(PP_Instance instance) {
  const struct PPB_MouseLock_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseLock_1_0.real_iface;
  iface->UnlockMouse(instance);
}

/* End wrapper methods for PPB_MouseLock_1_0 */

/* Begin wrapper methods for PPB_NetAddress_1_0 */

static PP_Resource Pnacl_M29_PPB_NetAddress_CreateFromIPv4Address(PP_Instance instance, const struct PP_NetAddress_IPv4* ipv4_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->CreateFromIPv4Address(instance, ipv4_addr);
}

static PP_Resource Pnacl_M29_PPB_NetAddress_CreateFromIPv6Address(PP_Instance instance, const struct PP_NetAddress_IPv6* ipv6_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->CreateFromIPv6Address(instance, ipv6_addr);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_IsNetAddress(PP_Resource resource) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->IsNetAddress(resource);
}

static PP_NetAddress_Family Pnacl_M29_PPB_NetAddress_GetFamily(PP_Resource addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->GetFamily(addr);
}

static void Pnacl_M29_PPB_NetAddress_DescribeAsString(struct PP_Var* _struct_result, PP_Resource addr, PP_Bool include_port) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  *_struct_result = iface->DescribeAsString(addr, include_port);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_DescribeAsIPv4Address(PP_Resource addr, struct PP_NetAddress_IPv4* ipv4_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->DescribeAsIPv4Address(addr, ipv4_addr);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_DescribeAsIPv6Address(PP_Resource addr, struct PP_NetAddress_IPv6* ipv6_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->DescribeAsIPv6Address(addr, ipv6_addr);
}

/* End wrapper methods for PPB_NetAddress_1_0 */

/* Begin wrapper methods for PPB_NetworkList_1_0 */

static PP_Bool Pnacl_M31_PPB_NetworkList_IsNetworkList(PP_Resource resource) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->IsNetworkList(resource);
}

static uint32_t Pnacl_M31_PPB_NetworkList_GetCount(PP_Resource resource) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetCount(resource);
}

static void Pnacl_M31_PPB_NetworkList_GetName(struct PP_Var* _struct_result, PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  *_struct_result = iface->GetName(resource, index);
}

static PP_NetworkList_Type Pnacl_M31_PPB_NetworkList_GetType(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetType(resource, index);
}

static PP_NetworkList_State Pnacl_M31_PPB_NetworkList_GetState(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetState(resource, index);
}

static int32_t Pnacl_M31_PPB_NetworkList_GetIpAddresses(PP_Resource resource, uint32_t index, struct PP_ArrayOutput* output) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetIpAddresses(resource, index, *output);
}

static void Pnacl_M31_PPB_NetworkList_GetDisplayName(struct PP_Var* _struct_result, PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  *_struct_result = iface->GetDisplayName(resource, index);
}

static uint32_t Pnacl_M31_PPB_NetworkList_GetMTU(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetMTU(resource, index);
}

/* End wrapper methods for PPB_NetworkList_1_0 */

/* Begin wrapper methods for PPB_NetworkMonitor_1_0 */

static PP_Resource Pnacl_M31_PPB_NetworkMonitor_Create(PP_Instance instance) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M31_PPB_NetworkMonitor_UpdateNetworkList(PP_Resource network_monitor, PP_Resource* network_list, struct PP_CompletionCallback* callback) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->UpdateNetworkList(network_monitor, network_list, *callback);
}

static PP_Bool Pnacl_M31_PPB_NetworkMonitor_IsNetworkMonitor(PP_Resource resource) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->IsNetworkMonitor(resource);
}

/* End wrapper methods for PPB_NetworkMonitor_1_0 */

/* Begin wrapper methods for PPB_NetworkProxy_1_0 */

static int32_t Pnacl_M29_PPB_NetworkProxy_GetProxyForURL(PP_Instance instance, struct PP_Var* url, struct PP_Var* proxy_string, struct PP_CompletionCallback* callback) {
  const struct PPB_NetworkProxy_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkProxy_1_0.real_iface;
  return iface->GetProxyForURL(instance, *url, proxy_string, *callback);
}

/* End wrapper methods for PPB_NetworkProxy_1_0 */

/* Begin wrapper methods for PPB_TCPSocket_1_0 */

static PP_Resource Pnacl_M29_PPB_TCPSocket_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_TCPSocket_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Connect(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Connect(tcp_socket, addr, *callback);
}

static PP_Resource Pnacl_M29_PPB_TCPSocket_GetLocalAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->GetLocalAddress(tcp_socket);
}

static PP_Resource Pnacl_M29_PPB_TCPSocket_GetRemoteAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->GetRemoteAddress(tcp_socket);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M29_PPB_TCPSocket_Close(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  iface->Close(tcp_socket);
}

static int32_t Pnacl_M29_PPB_TCPSocket_SetOption(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_1_0 */

/* Begin wrapper methods for PPB_TCPSocket_1_1 */

static PP_Resource Pnacl_M31_PPB_TCPSocket_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M31_PPB_TCPSocket_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Bind(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Bind(tcp_socket, addr, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Connect(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Connect(tcp_socket, addr, *callback);
}

static PP_Resource Pnacl_M31_PPB_TCPSocket_GetLocalAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->GetLocalAddress(tcp_socket);
}

static PP_Resource Pnacl_M31_PPB_TCPSocket_GetRemoteAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->GetRemoteAddress(tcp_socket);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Listen(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Listen(tcp_socket, backlog, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Accept(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Accept(tcp_socket, accepted_tcp_socket, *callback);
}

static void Pnacl_M31_PPB_TCPSocket_Close(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  iface->Close(tcp_socket);
}

static int32_t Pnacl_M31_PPB_TCPSocket_SetOption(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_1_1 */

/* Begin wrapper methods for PPB_TextInputController_1_0 */

static void Pnacl_M30_PPB_TextInputController_SetTextInputType(PP_Instance instance, PP_TextInput_Type type) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->SetTextInputType(instance, type);
}

static void Pnacl_M30_PPB_TextInputController_UpdateCaretPosition(PP_Instance instance, const struct PP_Rect* caret) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->UpdateCaretPosition(instance, caret);
}

static void Pnacl_M30_PPB_TextInputController_CancelCompositionText(PP_Instance instance) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->CancelCompositionText(instance);
}

static void Pnacl_M30_PPB_TextInputController_UpdateSurroundingText(PP_Instance instance, struct PP_Var* text, uint32_t caret, uint32_t anchor) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->UpdateSurroundingText(instance, *text, caret, anchor);
}

/* End wrapper methods for PPB_TextInputController_1_0 */

/* Begin wrapper methods for PPB_UDPSocket_1_0 */

static PP_Resource Pnacl_M29_PPB_UDPSocket_Create(PP_Instance instance) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_UDPSocket_IsUDPSocket(PP_Resource resource) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->IsUDPSocket(resource);
}

static int32_t Pnacl_M29_PPB_UDPSocket_Bind(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Resource Pnacl_M29_PPB_UDPSocket_GetBoundAddress(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->GetBoundAddress(udp_socket);
}

static int32_t Pnacl_M29_PPB_UDPSocket_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, addr, *callback);
}

static int32_t Pnacl_M29_PPB_UDPSocket_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M29_PPB_UDPSocket_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  iface->Close(udp_socket);
}

static int32_t Pnacl_M29_PPB_UDPSocket_SetOption(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->SetOption(udp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_UDPSocket_1_0 */

/* Begin wrapper methods for PPB_URLLoader_1_0 */

static PP_Resource Pnacl_M14_PPB_URLLoader_Create(PP_Instance instance) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_IsURLLoader(PP_Resource resource) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->IsURLLoader(resource);
}

static int32_t Pnacl_M14_PPB_URLLoader_Open(PP_Resource loader, PP_Resource request_info, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->Open(loader, request_info, *callback);
}

static int32_t Pnacl_M14_PPB_URLLoader_FollowRedirect(PP_Resource loader, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->FollowRedirect(loader, *callback);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_GetUploadProgress(PP_Resource loader, int64_t* bytes_sent, int64_t* total_bytes_to_be_sent) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetUploadProgress(loader, bytes_sent, total_bytes_to_be_sent);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_GetDownloadProgress(PP_Resource loader, int64_t* bytes_received, int64_t* total_bytes_to_be_received) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetDownloadProgress(loader, bytes_received, total_bytes_to_be_received);
}

static PP_Resource Pnacl_M14_PPB_URLLoader_GetResponseInfo(PP_Resource loader) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetResponseInfo(loader);
}

static int32_t Pnacl_M14_PPB_URLLoader_ReadResponseBody(PP_Resource loader, void* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->ReadResponseBody(loader, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M14_PPB_URLLoader_FinishStreamingToFile(PP_Resource loader, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->FinishStreamingToFile(loader, *callback);
}

static void Pnacl_M14_PPB_URLLoader_Close(PP_Resource loader) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  iface->Close(loader);
}

/* End wrapper methods for PPB_URLLoader_1_0 */

/* Begin wrapper methods for PPB_URLRequestInfo_1_0 */

static PP_Resource Pnacl_M14_PPB_URLRequestInfo_Create(PP_Instance instance) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_IsURLRequestInfo(PP_Resource resource) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->IsURLRequestInfo(resource);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_SetProperty(PP_Resource request, PP_URLRequestProperty property, struct PP_Var* value) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->SetProperty(request, property, *value);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_AppendDataToBody(PP_Resource request, const void* data, uint32_t len) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->AppendDataToBody(request, data, len);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_AppendFileToBody(PP_Resource request, PP_Resource file_ref, int64_t start_offset, int64_t number_of_bytes, PP_Time expected_last_modified_time) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->AppendFileToBody(request, file_ref, start_offset, number_of_bytes, expected_last_modified_time);
}

/* End wrapper methods for PPB_URLRequestInfo_1_0 */

/* Begin wrapper methods for PPB_URLResponseInfo_1_0 */

static PP_Bool Pnacl_M14_PPB_URLResponseInfo_IsURLResponseInfo(PP_Resource resource) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  return iface->IsURLResponseInfo(resource);
}

static void Pnacl_M14_PPB_URLResponseInfo_GetProperty(struct PP_Var* _struct_result, PP_Resource response, PP_URLResponseProperty property) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  *_struct_result = iface->GetProperty(response, property);
}

static PP_Resource Pnacl_M14_PPB_URLResponseInfo_GetBodyAsFileRef(PP_Resource response) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  return iface->GetBodyAsFileRef(response);
}

/* End wrapper methods for PPB_URLResponseInfo_1_0 */

/* Begin wrapper methods for PPB_Var_1_0 */

static void Pnacl_M14_PPB_Var_AddRef(struct PP_Var* var) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  iface->AddRef(*var);
}

static void Pnacl_M14_PPB_Var_Release(struct PP_Var* var) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  iface->Release(*var);
}

static void Pnacl_M14_PPB_Var_VarFromUtf8(struct PP_Var* _struct_result, PP_Module module, const char* data, uint32_t len) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  *_struct_result = iface->VarFromUtf8(module, data, len);
}

static const char* Pnacl_M14_PPB_Var_VarToUtf8(struct PP_Var* var, uint32_t* len) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  return iface->VarToUtf8(*var, len);
}

/* End wrapper methods for PPB_Var_1_0 */

/* Begin wrapper methods for PPB_Var_1_1 */

static void Pnacl_M18_PPB_Var_AddRef(struct PP_Var* var) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  iface->AddRef(*var);
}

static void Pnacl_M18_PPB_Var_Release(struct PP_Var* var) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  iface->Release(*var);
}

static void Pnacl_M18_PPB_Var_VarFromUtf8(struct PP_Var* _struct_result, const char* data, uint32_t len) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  *_struct_result = iface->VarFromUtf8(data, len);
}

static const char* Pnacl_M18_PPB_Var_VarToUtf8(struct PP_Var* var, uint32_t* len) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  return iface->VarToUtf8(*var, len);
}

/* End wrapper methods for PPB_Var_1_1 */

/* Begin wrapper methods for PPB_VarArray_1_0 */

static void Pnacl_M29_PPB_VarArray_Create(struct PP_Var* _struct_result) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  *_struct_result = iface->Create();
}

static void Pnacl_M29_PPB_VarArray_Get(struct PP_Var* _struct_result, struct PP_Var* array, uint32_t index) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  *_struct_result = iface->Get(*array, index);
}

static PP_Bool Pnacl_M29_PPB_VarArray_Set(struct PP_Var* array, uint32_t index, struct PP_Var* value) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->Set(*array, index, *value);
}

static uint32_t Pnacl_M29_PPB_VarArray_GetLength(struct PP_Var* array) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->GetLength(*array);
}

static PP_Bool Pnacl_M29_PPB_VarArray_SetLength(struct PP_Var* array, uint32_t length) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->SetLength(*array, length);
}

/* End wrapper methods for PPB_VarArray_1_0 */

/* Begin wrapper methods for PPB_VarArrayBuffer_1_0 */

static void Pnacl_M18_PPB_VarArrayBuffer_Create(struct PP_Var* _struct_result, uint32_t size_in_bytes) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  *_struct_result = iface->Create(size_in_bytes);
}

static PP_Bool Pnacl_M18_PPB_VarArrayBuffer_ByteLength(struct PP_Var* array, uint32_t* byte_length) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  return iface->ByteLength(*array, byte_length);
}

static void* Pnacl_M18_PPB_VarArrayBuffer_Map(struct PP_Var* array) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  return iface->Map(*array);
}

static void Pnacl_M18_PPB_VarArrayBuffer_Unmap(struct PP_Var* array) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  iface->Unmap(*array);
}

/* End wrapper methods for PPB_VarArrayBuffer_1_0 */

/* Begin wrapper methods for PPB_VarDictionary_1_0 */

static void Pnacl_M29_PPB_VarDictionary_Create(struct PP_Var* _struct_result) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->Create();
}

static void Pnacl_M29_PPB_VarDictionary_Get(struct PP_Var* _struct_result, struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->Get(*dict, *key);
}

static PP_Bool Pnacl_M29_PPB_VarDictionary_Set(struct PP_Var* dict, struct PP_Var* key, struct PP_Var* value) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  return iface->Set(*dict, *key, *value);
}

static void Pnacl_M29_PPB_VarDictionary_Delete(struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  iface->Delete(*dict, *key);
}

static PP_Bool Pnacl_M29_PPB_VarDictionary_HasKey(struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  return iface->HasKey(*dict, *key);
}

static void Pnacl_M29_PPB_VarDictionary_GetKeys(struct PP_Var* _struct_result, struct PP_Var* dict) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->GetKeys(*dict);
}

/* End wrapper methods for PPB_VarDictionary_1_0 */

/* Not generating wrapper methods for PPB_View_1_0 */

/* Not generating wrapper methods for PPB_View_1_1 */

/* Begin wrapper methods for PPB_WebSocket_1_0 */

static PP_Resource Pnacl_M18_PPB_WebSocket_Create(PP_Instance instance) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M18_PPB_WebSocket_IsWebSocket(PP_Resource resource) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->IsWebSocket(resource);
}

static int32_t Pnacl_M18_PPB_WebSocket_Connect(PP_Resource web_socket, struct PP_Var* url, const struct PP_Var protocols[], uint32_t protocol_count, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Connect(web_socket, *url, protocols, protocol_count, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_Close(PP_Resource web_socket, uint16_t code, struct PP_Var* reason, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Close(web_socket, code, *reason, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_ReceiveMessage(PP_Resource web_socket, struct PP_Var* message, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->ReceiveMessage(web_socket, message, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_SendMessage(PP_Resource web_socket, struct PP_Var* message) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->SendMessage(web_socket, *message);
}

static uint64_t Pnacl_M18_PPB_WebSocket_GetBufferedAmount(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetBufferedAmount(web_socket);
}

static uint16_t Pnacl_M18_PPB_WebSocket_GetCloseCode(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetCloseCode(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetCloseReason(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetCloseReason(web_socket);
}

static PP_Bool Pnacl_M18_PPB_WebSocket_GetCloseWasClean(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetCloseWasClean(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetExtensions(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetExtensions(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetProtocol(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetProtocol(web_socket);
}

static PP_WebSocketReadyState Pnacl_M18_PPB_WebSocket_GetReadyState(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetReadyState(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetURL(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetURL(web_socket);
}

/* End wrapper methods for PPB_WebSocket_1_0 */

/* Not generating wrapper methods for PPP_Graphics3D_1_0 */

/* Not generating wrapper methods for PPP_InputEvent_0_1 */

/* Not generating wrapper methods for PPP_Instance_1_0 */

/* Not generating wrapper methods for PPP_Instance_1_1 */

/* Begin wrapper methods for PPP_Messaging_1_0 */

static void Pnacl_M14_PPP_Messaging_HandleMessage(PP_Instance instance, struct PP_Var message) {
  const struct PPP_Messaging_1_0 *iface = Pnacl_WrapperInfo_PPP_Messaging_1_0.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* message) =
    ((void (*)(PP_Instance instance, struct PP_Var* message))iface->HandleMessage);
  temp_fp(instance, &message);
}

/* End wrapper methods for PPP_Messaging_1_0 */

/* Not generating wrapper methods for PPP_MouseLock_1_0 */

/* Not generating wrapper methods for PPB_BrokerTrusted_0_2 */

/* Not generating wrapper methods for PPB_BrokerTrusted_0_3 */

/* Not generating wrapper methods for PPB_BrowserFont_Trusted_1_0 */

/* Not generating wrapper methods for PPB_CharSet_Trusted_1_0 */

/* Not generating wrapper methods for PPB_FileChooserTrusted_0_5 */

/* Not generating wrapper methods for PPB_FileChooserTrusted_0_6 */

/* Not generating wrapper methods for PPB_FileIOTrusted_0_4 */

/* Not generating wrapper methods for PPB_URLLoaderTrusted_0_3 */

/* Begin wrapper methods for PPB_AudioInput_Dev_0_2 */

static PP_Resource Pnacl_M19_PPB_AudioInput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_AudioInput_Dev_IsAudioInput(PP_Resource resource) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->IsAudioInput(resource);
}

static int32_t Pnacl_M19_PPB_AudioInput_Dev_EnumerateDevices(PP_Resource audio_input, PP_Resource* devices, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->EnumerateDevices(audio_input, devices, *callback);
}

static int32_t Pnacl_M19_PPB_AudioInput_Dev_Open(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_2 audio_input_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->Open(audio_input, device_ref, config, audio_input_callback, user_data, *callback);
}

static PP_Resource Pnacl_M19_PPB_AudioInput_Dev_GetCurrentConfig(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->GetCurrentConfig(audio_input);
}

static PP_Bool Pnacl_M19_PPB_AudioInput_Dev_StartCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->StartCapture(audio_input);
}

static PP_Bool Pnacl_M19_PPB_AudioInput_Dev_StopCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  return iface->StopCapture(audio_input);
}

static void Pnacl_M19_PPB_AudioInput_Dev_Close(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2.real_iface;
  iface->Close(audio_input);
}

/* End wrapper methods for PPB_AudioInput_Dev_0_2 */

/* Begin wrapper methods for PPB_AudioInput_Dev_0_3 */

static PP_Resource Pnacl_M25_PPB_AudioInput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_IsAudioInput(PP_Resource resource) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->IsAudioInput(resource);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_EnumerateDevices(PP_Resource audio_input, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->EnumerateDevices(audio_input, *output, *callback);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_MonitorDeviceChange(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->MonitorDeviceChange(audio_input, callback, user_data);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_Open(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_2 audio_input_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->Open(audio_input, device_ref, config, audio_input_callback, user_data, *callback);
}

static PP_Resource Pnacl_M25_PPB_AudioInput_Dev_GetCurrentConfig(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->GetCurrentConfig(audio_input);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_StartCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->StartCapture(audio_input);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_StopCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->StopCapture(audio_input);
}

static void Pnacl_M25_PPB_AudioInput_Dev_Close(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  iface->Close(audio_input);
}

/* End wrapper methods for PPB_AudioInput_Dev_0_3 */

/* Begin wrapper methods for PPB_AudioInput_Dev_0_4 */

static PP_Resource Pnacl_M30_PPB_AudioInput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_IsAudioInput(PP_Resource resource) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->IsAudioInput(resource);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_EnumerateDevices(PP_Resource audio_input, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->EnumerateDevices(audio_input, *output, *callback);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_MonitorDeviceChange(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->MonitorDeviceChange(audio_input, callback, user_data);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_Open(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback audio_input_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->Open(audio_input, device_ref, config, audio_input_callback, user_data, *callback);
}

static PP_Resource Pnacl_M30_PPB_AudioInput_Dev_GetCurrentConfig(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->GetCurrentConfig(audio_input);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_StartCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->StartCapture(audio_input);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_StopCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->StopCapture(audio_input);
}

static void Pnacl_M30_PPB_AudioInput_Dev_Close(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  iface->Close(audio_input);
}

/* End wrapper methods for PPB_AudioInput_Dev_0_4 */

/* Not generating wrapper methods for PPB_Buffer_Dev_0_4 */

/* Not generating wrapper methods for PPB_Crypto_Dev_0_1 */

/* Not generating wrapper methods for PPB_CursorControl_Dev_0_4 */

/* Begin wrapper methods for PPB_DeviceRef_Dev_0_1 */

static PP_Bool Pnacl_M18_PPB_DeviceRef_Dev_IsDeviceRef(PP_Resource resource) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  return iface->IsDeviceRef(resource);
}

static PP_DeviceType_Dev Pnacl_M18_PPB_DeviceRef_Dev_GetType(PP_Resource device_ref) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  return iface->GetType(device_ref);
}

static void Pnacl_M18_PPB_DeviceRef_Dev_GetName(struct PP_Var* _struct_result, PP_Resource device_ref) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  *_struct_result = iface->GetName(device_ref);
}

/* End wrapper methods for PPB_DeviceRef_Dev_0_1 */

/* Begin wrapper methods for PPB_FileChooser_Dev_0_5 */

static PP_Resource Pnacl_M16_PPB_FileChooser_Dev_Create(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var* accept_types) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->Create(instance, mode, *accept_types);
}

static PP_Bool Pnacl_M16_PPB_FileChooser_Dev_IsFileChooser(PP_Resource resource) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->IsFileChooser(resource);
}

static int32_t Pnacl_M16_PPB_FileChooser_Dev_Show(PP_Resource chooser, struct PP_CompletionCallback* callback) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->Show(chooser, *callback);
}

static PP_Resource Pnacl_M16_PPB_FileChooser_Dev_GetNextChosenFile(PP_Resource chooser) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->GetNextChosenFile(chooser);
}

/* End wrapper methods for PPB_FileChooser_Dev_0_5 */

/* Begin wrapper methods for PPB_FileChooser_Dev_0_6 */

static PP_Resource Pnacl_M19_PPB_FileChooser_Dev_Create(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var* accept_types) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->Create(instance, mode, *accept_types);
}

static PP_Bool Pnacl_M19_PPB_FileChooser_Dev_IsFileChooser(PP_Resource resource) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->IsFileChooser(resource);
}

static int32_t Pnacl_M19_PPB_FileChooser_Dev_Show(PP_Resource chooser, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->Show(chooser, *output, *callback);
}

/* End wrapper methods for PPB_FileChooser_Dev_0_6 */

/* Not generating wrapper methods for PPB_Find_Dev_0_3 */

/* Begin wrapper methods for PPB_Font_Dev_0_6 */

static void Pnacl_M14_PPB_Font_Dev_GetFontFamilies(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  *_struct_result = iface->GetFontFamilies(instance);
}

static PP_Resource Pnacl_M14_PPB_Font_Dev_Create(PP_Instance instance, const struct PP_FontDescription_Dev* description) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->Create(instance, description);
}

static PP_Bool Pnacl_M14_PPB_Font_Dev_IsFont(PP_Resource resource) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->IsFont(resource);
}

static PP_Bool Pnacl_M14_PPB_Font_Dev_Describe(PP_Resource font, struct PP_FontDescription_Dev* description, struct PP_FontMetrics_Dev* metrics) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->Describe(font, description, metrics);
}

static PP_Bool Pnacl_M14_PPB_Font_Dev_DrawTextAt(PP_Resource font, PP_Resource image_data, const struct PP_TextRun_Dev* text, const struct PP_Point* position, uint32_t color, const struct PP_Rect* clip, PP_Bool image_data_is_opaque) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->DrawTextAt(font, image_data, text, position, color, clip, image_data_is_opaque);
}

static int32_t Pnacl_M14_PPB_Font_Dev_MeasureText(PP_Resource font, const struct PP_TextRun_Dev* text) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->MeasureText(font, text);
}

static uint32_t Pnacl_M14_PPB_Font_Dev_CharacterOffsetForPixel(PP_Resource font, const struct PP_TextRun_Dev* text, int32_t pixel_position) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->CharacterOffsetForPixel(font, text, pixel_position);
}

static int32_t Pnacl_M14_PPB_Font_Dev_PixelOffsetForCharacter(PP_Resource font, const struct PP_TextRun_Dev* text, uint32_t char_offset) {
  const struct PPB_Font_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_Font_Dev_0_6.real_iface;
  return iface->PixelOffsetForCharacter(font, text, char_offset);
}

/* End wrapper methods for PPB_Font_Dev_0_6 */

/* Not generating wrapper methods for PPB_Graphics2D_Dev_0_1 */

/* Begin wrapper methods for PPB_IMEInputEvent_Dev_0_1 */

static PP_Bool Pnacl_M16_PPB_IMEInputEvent_Dev_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M16_PPB_IMEInputEvent_Dev_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M16_PPB_IMEInputEvent_Dev_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_Dev_0_1 */

/* Begin wrapper methods for PPB_IMEInputEvent_Dev_0_2 */

static PP_Resource Pnacl_M21_PPB_IMEInputEvent_Dev_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var* text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->Create(instance, type, time_stamp, *text, segment_number, segment_offsets, target_segment, selection_start, selection_end);
}

static PP_Bool Pnacl_M21_PPB_IMEInputEvent_Dev_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M21_PPB_IMEInputEvent_Dev_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M21_PPB_IMEInputEvent_Dev_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_Dev_0_2 */

/* Begin wrapper methods for PPB_KeyboardInputEvent_Dev_0_2 */

static PP_Bool Pnacl_M31_PPB_KeyboardInputEvent_Dev_SetUsbKeyCode(PP_Resource key_event, uint32_t usb_key_code) {
  const struct PPB_KeyboardInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2.real_iface;
  return iface->SetUsbKeyCode(key_event, usb_key_code);
}

static uint32_t Pnacl_M31_PPB_KeyboardInputEvent_Dev_GetUsbKeyCode(PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2.real_iface;
  return iface->GetUsbKeyCode(key_event);
}

static void Pnacl_M31_PPB_KeyboardInputEvent_Dev_GetCode(struct PP_Var* _struct_result, PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2.real_iface;
  *_struct_result = iface->GetCode(key_event);
}

/* End wrapper methods for PPB_KeyboardInputEvent_Dev_0_2 */

/* Not generating wrapper methods for PPB_Memory_Dev_0_1 */

/* Begin wrapper methods for PPB_Printing_Dev_0_7 */

static PP_Resource Pnacl_M23_PPB_Printing_Dev_Create(PP_Instance instance) {
  const struct PPB_Printing_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_Printing_Dev_0_7.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M23_PPB_Printing_Dev_GetDefaultPrintSettings(PP_Resource resource, struct PP_PrintSettings_Dev* print_settings, struct PP_CompletionCallback* callback) {
  const struct PPB_Printing_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_Printing_Dev_0_7.real_iface;
  return iface->GetDefaultPrintSettings(resource, print_settings, *callback);
}

/* End wrapper methods for PPB_Printing_Dev_0_7 */

/* Not generating wrapper methods for PPB_ResourceArray_Dev_0_1 */

/* Not generating wrapper methods for PPB_Scrollbar_Dev_0_5 */

/* Not generating wrapper methods for PPB_Testing_Dev_0_7 */

/* Not generating wrapper methods for PPB_Testing_Dev_0_8 */

/* Begin wrapper methods for PPB_Testing_Dev_0_9 */

static PP_Bool Pnacl_M17_PPB_Testing_Dev_ReadImageData(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  return iface->ReadImageData(device_context_2d, image, top_left);
}

static void Pnacl_M17_PPB_Testing_Dev_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M17_PPB_Testing_Dev_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  iface->QuitMessageLoop(instance);
}

static uint32_t Pnacl_M17_PPB_Testing_Dev_GetLiveObjectsForInstance(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  return iface->GetLiveObjectsForInstance(instance);
}

static PP_Bool Pnacl_M17_PPB_Testing_Dev_IsOutOfProcess(void) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  return iface->IsOutOfProcess();
}

static void Pnacl_M17_PPB_Testing_Dev_SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  iface->SimulateInputEvent(instance, input_event);
}

static void Pnacl_M17_PPB_Testing_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_Testing_Dev_0_9 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_9.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

/* End wrapper methods for PPB_Testing_Dev_0_9 */

/* Begin wrapper methods for PPB_Testing_Dev_0_91 */

static PP_Bool Pnacl_M18_PPB_Testing_Dev_ReadImageData(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  return iface->ReadImageData(device_context_2d, image, top_left);
}

static void Pnacl_M18_PPB_Testing_Dev_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M18_PPB_Testing_Dev_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  iface->QuitMessageLoop(instance);
}

static uint32_t Pnacl_M18_PPB_Testing_Dev_GetLiveObjectsForInstance(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  return iface->GetLiveObjectsForInstance(instance);
}

static PP_Bool Pnacl_M18_PPB_Testing_Dev_IsOutOfProcess(void) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  return iface->IsOutOfProcess();
}

static void Pnacl_M18_PPB_Testing_Dev_SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  iface->SimulateInputEvent(instance, input_event);
}

static void Pnacl_M18_PPB_Testing_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static uint32_t Pnacl_M18_PPB_Testing_Dev_GetLiveVars(struct PP_Var live_vars[], uint32_t array_size) {
  const struct PPB_Testing_Dev_0_91 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_91.real_iface;
  return iface->GetLiveVars(live_vars, array_size);
}

/* End wrapper methods for PPB_Testing_Dev_0_91 */

/* Begin wrapper methods for PPB_Testing_Dev_0_92 */

static PP_Bool Pnacl_M28_PPB_Testing_Dev_ReadImageData(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  return iface->ReadImageData(device_context_2d, image, top_left);
}

static void Pnacl_M28_PPB_Testing_Dev_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M28_PPB_Testing_Dev_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  iface->QuitMessageLoop(instance);
}

static uint32_t Pnacl_M28_PPB_Testing_Dev_GetLiveObjectsForInstance(PP_Instance instance) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  return iface->GetLiveObjectsForInstance(instance);
}

static PP_Bool Pnacl_M28_PPB_Testing_Dev_IsOutOfProcess(void) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  return iface->IsOutOfProcess();
}

static void Pnacl_M28_PPB_Testing_Dev_SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  iface->SimulateInputEvent(instance, input_event);
}

static void Pnacl_M28_PPB_Testing_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static uint32_t Pnacl_M28_PPB_Testing_Dev_GetLiveVars(struct PP_Var live_vars[], uint32_t array_size) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  return iface->GetLiveVars(live_vars, array_size);
}

static void Pnacl_M28_PPB_Testing_Dev_SetMinimumArrayBufferSizeForShmem(PP_Instance instance, uint32_t threshold) {
  const struct PPB_Testing_Dev_0_92 *iface = Pnacl_WrapperInfo_PPB_Testing_Dev_0_92.real_iface;
  iface->SetMinimumArrayBufferSizeForShmem(instance, threshold);
}

/* End wrapper methods for PPB_Testing_Dev_0_92 */

/* Not generating wrapper methods for PPB_TextInput_Dev_0_1 */

/* Not generating wrapper methods for PPB_TextInput_Dev_0_2 */

/* Not generating wrapper methods for PPB_Trace_Event_Dev_0_1 */

/* Not generating wrapper methods for PPB_Trace_Event_Dev_0_2 */

/* Begin wrapper methods for PPB_TrueTypeFont_Dev_0_1 */

static int32_t Pnacl_M26_PPB_TrueTypeFont_Dev_GetFontFamilies(PP_Instance instance, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->GetFontFamilies(instance, *output, *callback);
}

static int32_t Pnacl_M26_PPB_TrueTypeFont_Dev_GetFontsInFamily(PP_Instance instance, struct PP_Var* family, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->GetFontsInFamily(instance, *family, *output, *callback);
}

static PP_Resource Pnacl_M26_PPB_TrueTypeFont_Dev_Create(PP_Instance instance, const struct PP_TrueTypeFontDesc_Dev* desc) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->Create(instance, desc);
}

static PP_Bool Pnacl_M26_PPB_TrueTypeFont_Dev_IsTrueTypeFont(PP_Resource resource) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->IsTrueTypeFont(resource);
}

static int32_t Pnacl_M26_PPB_TrueTypeFont_Dev_Describe(PP_Resource font, struct PP_TrueTypeFontDesc_Dev* desc, struct PP_CompletionCallback* callback) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->Describe(font, desc, *callback);
}

static int32_t Pnacl_M26_PPB_TrueTypeFont_Dev_GetTableTags(PP_Resource font, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->GetTableTags(font, *output, *callback);
}

static int32_t Pnacl_M26_PPB_TrueTypeFont_Dev_GetTable(PP_Resource font, uint32_t table, int32_t offset, int32_t max_data_length, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_TrueTypeFont_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1.real_iface;
  return iface->GetTable(font, table, offset, max_data_length, *output, *callback);
}

/* End wrapper methods for PPB_TrueTypeFont_Dev_0_1 */

/* Begin wrapper methods for PPB_URLUtil_Dev_0_6 */

static void Pnacl_M17_PPB_URLUtil_Dev_Canonicalize(struct PP_Var* _struct_result, struct PP_Var* url, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->Canonicalize(*url, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToURL(struct PP_Var* _struct_result, struct PP_Var* base_url, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->ResolveRelativeToURL(*base_url, *relative_string, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToDocument(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->ResolveRelativeToDocument(instance, *relative_string, components);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_IsSameSecurityOrigin(struct PP_Var* url_a, struct PP_Var* url_b) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->IsSameSecurityOrigin(*url_a, *url_b);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_DocumentCanRequest(PP_Instance instance, struct PP_Var* url) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->DocumentCanRequest(instance, *url);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->DocumentCanAccessDocument(active, target);
}

static void Pnacl_M17_PPB_URLUtil_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_GetPluginInstanceURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->GetPluginInstanceURL(instance, components);
}

/* End wrapper methods for PPB_URLUtil_Dev_0_6 */

/* Begin wrapper methods for PPB_URLUtil_Dev_0_7 */

static void Pnacl_M31_PPB_URLUtil_Dev_Canonicalize(struct PP_Var* _struct_result, struct PP_Var* url, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->Canonicalize(*url, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToURL(struct PP_Var* _struct_result, struct PP_Var* base_url, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->ResolveRelativeToURL(*base_url, *relative_string, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToDocument(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->ResolveRelativeToDocument(instance, *relative_string, components);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_IsSameSecurityOrigin(struct PP_Var* url_a, struct PP_Var* url_b) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->IsSameSecurityOrigin(*url_a, *url_b);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_DocumentCanRequest(PP_Instance instance, struct PP_Var* url) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->DocumentCanRequest(instance, *url);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->DocumentCanAccessDocument(active, target);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetPluginInstanceURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetPluginInstanceURL(instance, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetPluginReferrerURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetPluginReferrerURL(instance, components);
}

/* End wrapper methods for PPB_URLUtil_Dev_0_7 */

/* Begin wrapper methods for PPB_VideoCapture_Dev_0_2 */

static PP_Resource Pnacl_M19_PPB_VideoCapture_Dev_Create(PP_Instance instance) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_VideoCapture_Dev_IsVideoCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->IsVideoCapture(video_capture);
}

static int32_t Pnacl_M19_PPB_VideoCapture_Dev_EnumerateDevices(PP_Resource video_capture, PP_Resource* devices, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->EnumerateDevices(video_capture, devices, *callback);
}

static int32_t Pnacl_M19_PPB_VideoCapture_Dev_Open(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->Open(video_capture, device_ref, requested_info, buffer_count, *callback);
}

static int32_t Pnacl_M19_PPB_VideoCapture_Dev_StartCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->StartCapture(video_capture);
}

static int32_t Pnacl_M19_PPB_VideoCapture_Dev_ReuseBuffer(PP_Resource video_capture, uint32_t buffer) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->ReuseBuffer(video_capture, buffer);
}

static int32_t Pnacl_M19_PPB_VideoCapture_Dev_StopCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  return iface->StopCapture(video_capture);
}

static void Pnacl_M19_PPB_VideoCapture_Dev_Close(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2.real_iface;
  iface->Close(video_capture);
}

/* End wrapper methods for PPB_VideoCapture_Dev_0_2 */

/* Begin wrapper methods for PPB_VideoCapture_Dev_0_3 */

static PP_Resource Pnacl_M25_PPB_VideoCapture_Dev_Create(PP_Instance instance) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_VideoCapture_Dev_IsVideoCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->IsVideoCapture(video_capture);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_EnumerateDevices(PP_Resource video_capture, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->EnumerateDevices(video_capture, *output, *callback);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_MonitorDeviceChange(PP_Resource video_capture, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->MonitorDeviceChange(video_capture, callback, user_data);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_Open(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->Open(video_capture, device_ref, requested_info, buffer_count, *callback);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_StartCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->StartCapture(video_capture);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_ReuseBuffer(PP_Resource video_capture, uint32_t buffer) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->ReuseBuffer(video_capture, buffer);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_StopCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->StopCapture(video_capture);
}

static void Pnacl_M25_PPB_VideoCapture_Dev_Close(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  iface->Close(video_capture);
}

/* End wrapper methods for PPB_VideoCapture_Dev_0_3 */

/* Begin wrapper methods for PPB_VideoDecoder_Dev_0_16 */

static PP_Resource Pnacl_M14_PPB_VideoDecoder_Dev_Create(PP_Instance instance, PP_Resource context, PP_VideoDecoder_Profile profile) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Create(instance, context, profile);
}

static PP_Bool Pnacl_M14_PPB_VideoDecoder_Dev_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Decode(PP_Resource video_decoder, const struct PP_VideoBitstreamBuffer_Dev* bitstream_buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Decode(video_decoder, bitstream_buffer, *callback);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_AssignPictureBuffers(PP_Resource video_decoder, uint32_t no_of_buffers, const struct PP_PictureBuffer_Dev buffers[]) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->AssignPictureBuffers(video_decoder, no_of_buffers, buffers);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->ReusePictureBuffer(video_decoder, picture_buffer_id);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Reset(video_decoder, *callback);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_Destroy(PP_Resource video_decoder) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->Destroy(video_decoder);
}

/* End wrapper methods for PPB_VideoDecoder_Dev_0_16 */

/* Not generating wrapper methods for PPB_View_Dev_0_1 */

/* Not generating wrapper methods for PPB_Widget_Dev_0_3 */

/* Not generating wrapper methods for PPB_Widget_Dev_0_4 */

/* Not generating wrapper methods for PPB_Zoom_Dev_0_2 */

/* Not generating wrapper methods for PPP_NetworkState_Dev_0_1 */

/* Not generating wrapper methods for PPP_Printing_Dev_0_6 */

/* Not generating wrapper methods for PPP_Scrollbar_Dev_0_2 */

/* Not generating wrapper methods for PPP_Scrollbar_Dev_0_3 */

/* Begin wrapper methods for PPP_Selection_Dev_0_3 */

static struct PP_Var Pnacl_M13_PPP_Selection_Dev_GetSelectedText(PP_Instance instance, PP_Bool html) {
  const struct PPP_Selection_Dev_0_3 *iface = Pnacl_WrapperInfo_PPP_Selection_Dev_0_3.real_iface;
  void (*temp_fp)(struct PP_Var* _struct_result, PP_Instance instance, PP_Bool html) =
    ((void (*)(struct PP_Var* _struct_result, PP_Instance instance, PP_Bool html))iface->GetSelectedText);
  struct PP_Var _struct_result;
  temp_fp(&_struct_result, instance, html);
  return _struct_result;
}

/* End wrapper methods for PPP_Selection_Dev_0_3 */

/* Not generating wrapper methods for PPP_TextInput_Dev_0_1 */

/* Not generating wrapper methods for PPP_VideoCapture_Dev_0_1 */

/* Not generating wrapper methods for PPP_VideoDecoder_Dev_0_9 */

/* Not generating wrapper methods for PPP_VideoDecoder_Dev_0_10 */

/* Not generating wrapper methods for PPP_VideoDecoder_Dev_0_11 */

/* Not generating wrapper methods for PPP_Widget_Dev_0_2 */

/* Not generating wrapper methods for PPP_Zoom_Dev_0_3 */

/* Begin wrapper methods for PPB_ContentDecryptor_Private_0_7 */

static void Pnacl_M31_PPB_ContentDecryptor_Private_KeyAdded(PP_Instance instance, struct PP_Var* key_system, struct PP_Var* session_id) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->KeyAdded(instance, *key_system, *session_id);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_KeyMessage(PP_Instance instance, struct PP_Var* key_system, struct PP_Var* session_id, struct PP_Var* message, struct PP_Var* default_url) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->KeyMessage(instance, *key_system, *session_id, *message, *default_url);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_KeyError(PP_Instance instance, struct PP_Var* key_system, struct PP_Var* session_id, int32_t media_error, int32_t system_code) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->KeyError(instance, *key_system, *session_id, media_error, system_code);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DeliverBlock(PP_Instance instance, PP_Resource decrypted_block, const struct PP_DecryptedBlockInfo* decrypted_block_info) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DeliverBlock(instance, decrypted_block, decrypted_block_info);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DecoderInitializeDone(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id, PP_Bool success) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DecoderInitializeDone(instance, decoder_type, request_id, success);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DecoderDeinitializeDone(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DecoderDeinitializeDone(instance, decoder_type, request_id);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DecoderResetDone(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DecoderResetDone(instance, decoder_type, request_id);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DeliverFrame(PP_Instance instance, PP_Resource decrypted_frame, const struct PP_DecryptedFrameInfo* decrypted_frame_info) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DeliverFrame(instance, decrypted_frame, decrypted_frame_info);
}

static void Pnacl_M31_PPB_ContentDecryptor_Private_DeliverSamples(PP_Instance instance, PP_Resource audio_frames, const struct PP_DecryptedBlockInfo* decrypted_block_info) {
  const struct PPB_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7.real_iface;
  iface->DeliverSamples(instance, audio_frames, decrypted_block_info);
}

/* End wrapper methods for PPB_ContentDecryptor_Private_0_7 */

/* Begin wrapper methods for PPB_Ext_CrxFileSystem_Private_0_1 */

static int32_t Pnacl_M28_PPB_Ext_CrxFileSystem_Private_Open(PP_Instance instance, PP_Resource* file_system, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_CrxFileSystem_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1.real_iface;
  return iface->Open(instance, file_system, *callback);
}

/* End wrapper methods for PPB_Ext_CrxFileSystem_Private_0_1 */

/* Begin wrapper methods for PPB_FileIO_Private_0_1 */

static int32_t Pnacl_M28_PPB_FileIO_Private_RequestOSFileHandle(PP_Resource file_io, PP_FileHandle* handle, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_Private_0_1.real_iface;
  return iface->RequestOSFileHandle(file_io, handle, *callback);
}

/* End wrapper methods for PPB_FileIO_Private_0_1 */

/* Begin wrapper methods for PPB_FileRefPrivate_0_1 */

static void Pnacl_M15_PPB_FileRefPrivate_GetAbsolutePath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRefPrivate_0_1 *iface = Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1.real_iface;
  *_struct_result = iface->GetAbsolutePath(file_ref);
}

/* End wrapper methods for PPB_FileRefPrivate_0_1 */

/* Begin wrapper methods for PPB_Flash_12_4 */

static void Pnacl_M21_PPB_Flash_SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  iface->SetInstanceAlwaysOnTop(instance, on_top);
}

static PP_Bool Pnacl_M21_PPB_Flash_DrawGlyphs(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->DrawGlyphs(instance, pp_image_data, font_desc, color, position, clip, transformation, allow_subpixel_aa, glyph_count, glyph_indices, glyph_advances);
}

static void Pnacl_M21_PPB_Flash_GetProxyForURL(struct PP_Var* _struct_result, PP_Instance instance, const char* url) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  *_struct_result = iface->GetProxyForURL(instance, url);
}

static int32_t Pnacl_M21_PPB_Flash_Navigate(PP_Resource request_info, const char* target, PP_Bool from_user_action) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->Navigate(request_info, target, from_user_action);
}

static void Pnacl_M21_PPB_Flash_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M21_PPB_Flash_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  iface->QuitMessageLoop(instance);
}

static double Pnacl_M21_PPB_Flash_GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->GetLocalTimeZoneOffset(instance, t);
}

static void Pnacl_M21_PPB_Flash_GetCommandLineArgs(struct PP_Var* _struct_result, PP_Module module) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  *_struct_result = iface->GetCommandLineArgs(module);
}

static void Pnacl_M21_PPB_Flash_PreloadFontWin(const void* logfontw) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  iface->PreloadFontWin(logfontw);
}

static PP_Bool Pnacl_M21_PPB_Flash_IsRectTopmost(PP_Instance instance, const struct PP_Rect* rect) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->IsRectTopmost(instance, rect);
}

static int32_t Pnacl_M21_PPB_Flash_InvokePrinting(PP_Instance instance) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->InvokePrinting(instance);
}

static void Pnacl_M21_PPB_Flash_UpdateActivity(PP_Instance instance) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  iface->UpdateActivity(instance);
}

static void Pnacl_M21_PPB_Flash_GetDeviceID(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  *_struct_result = iface->GetDeviceID(instance);
}

static int32_t Pnacl_M21_PPB_Flash_GetSettingInt(PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  return iface->GetSettingInt(instance, setting);
}

static void Pnacl_M21_PPB_Flash_GetSetting(struct PP_Var* _struct_result, PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_4 *iface = Pnacl_WrapperInfo_PPB_Flash_12_4.real_iface;
  *_struct_result = iface->GetSetting(instance, setting);
}

/* End wrapper methods for PPB_Flash_12_4 */

/* Begin wrapper methods for PPB_Flash_12_5 */

static void Pnacl_M22_PPB_Flash_SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  iface->SetInstanceAlwaysOnTop(instance, on_top);
}

static PP_Bool Pnacl_M22_PPB_Flash_DrawGlyphs(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->DrawGlyphs(instance, pp_image_data, font_desc, color, position, clip, transformation, allow_subpixel_aa, glyph_count, glyph_indices, glyph_advances);
}

static void Pnacl_M22_PPB_Flash_GetProxyForURL(struct PP_Var* _struct_result, PP_Instance instance, const char* url) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  *_struct_result = iface->GetProxyForURL(instance, url);
}

static int32_t Pnacl_M22_PPB_Flash_Navigate(PP_Resource request_info, const char* target, PP_Bool from_user_action) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->Navigate(request_info, target, from_user_action);
}

static void Pnacl_M22_PPB_Flash_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M22_PPB_Flash_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  iface->QuitMessageLoop(instance);
}

static double Pnacl_M22_PPB_Flash_GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->GetLocalTimeZoneOffset(instance, t);
}

static void Pnacl_M22_PPB_Flash_GetCommandLineArgs(struct PP_Var* _struct_result, PP_Module module) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  *_struct_result = iface->GetCommandLineArgs(module);
}

static void Pnacl_M22_PPB_Flash_PreloadFontWin(const void* logfontw) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  iface->PreloadFontWin(logfontw);
}

static PP_Bool Pnacl_M22_PPB_Flash_IsRectTopmost(PP_Instance instance, const struct PP_Rect* rect) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->IsRectTopmost(instance, rect);
}

static int32_t Pnacl_M22_PPB_Flash_InvokePrinting(PP_Instance instance) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->InvokePrinting(instance);
}

static void Pnacl_M22_PPB_Flash_UpdateActivity(PP_Instance instance) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  iface->UpdateActivity(instance);
}

static void Pnacl_M22_PPB_Flash_GetDeviceID(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  *_struct_result = iface->GetDeviceID(instance);
}

static int32_t Pnacl_M22_PPB_Flash_GetSettingInt(PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->GetSettingInt(instance, setting);
}

static void Pnacl_M22_PPB_Flash_GetSetting(struct PP_Var* _struct_result, PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  *_struct_result = iface->GetSetting(instance, setting);
}

static PP_Bool Pnacl_M22_PPB_Flash_SetCrashData(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var* value) {
  const struct PPB_Flash_12_5 *iface = Pnacl_WrapperInfo_PPB_Flash_12_5.real_iface;
  return iface->SetCrashData(instance, key, *value);
}

/* End wrapper methods for PPB_Flash_12_5 */

/* Begin wrapper methods for PPB_Flash_12_6 */

static void Pnacl_M24_0_PPB_Flash_SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  iface->SetInstanceAlwaysOnTop(instance, on_top);
}

static PP_Bool Pnacl_M24_0_PPB_Flash_DrawGlyphs(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->DrawGlyphs(instance, pp_image_data, font_desc, color, position, clip, transformation, allow_subpixel_aa, glyph_count, glyph_indices, glyph_advances);
}

static void Pnacl_M24_0_PPB_Flash_GetProxyForURL(struct PP_Var* _struct_result, PP_Instance instance, const char* url) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  *_struct_result = iface->GetProxyForURL(instance, url);
}

static int32_t Pnacl_M24_0_PPB_Flash_Navigate(PP_Resource request_info, const char* target, PP_Bool from_user_action) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->Navigate(request_info, target, from_user_action);
}

static void Pnacl_M24_0_PPB_Flash_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M24_0_PPB_Flash_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  iface->QuitMessageLoop(instance);
}

static double Pnacl_M24_0_PPB_Flash_GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->GetLocalTimeZoneOffset(instance, t);
}

static void Pnacl_M24_0_PPB_Flash_GetCommandLineArgs(struct PP_Var* _struct_result, PP_Module module) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  *_struct_result = iface->GetCommandLineArgs(module);
}

static void Pnacl_M24_0_PPB_Flash_PreloadFontWin(const void* logfontw) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  iface->PreloadFontWin(logfontw);
}

static PP_Bool Pnacl_M24_0_PPB_Flash_IsRectTopmost(PP_Instance instance, const struct PP_Rect* rect) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->IsRectTopmost(instance, rect);
}

static int32_t Pnacl_M24_0_PPB_Flash_InvokePrinting(PP_Instance instance) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->InvokePrinting(instance);
}

static void Pnacl_M24_0_PPB_Flash_UpdateActivity(PP_Instance instance) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  iface->UpdateActivity(instance);
}

static void Pnacl_M24_0_PPB_Flash_GetDeviceID(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  *_struct_result = iface->GetDeviceID(instance);
}

static int32_t Pnacl_M24_0_PPB_Flash_GetSettingInt(PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->GetSettingInt(instance, setting);
}

static void Pnacl_M24_0_PPB_Flash_GetSetting(struct PP_Var* _struct_result, PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  *_struct_result = iface->GetSetting(instance, setting);
}

static PP_Bool Pnacl_M24_0_PPB_Flash_SetCrashData(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var* value) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->SetCrashData(instance, key, *value);
}

static int32_t Pnacl_M24_0_PPB_Flash_EnumerateVideoCaptureDevices(PP_Instance instance, PP_Resource video_capture, struct PP_ArrayOutput* devices) {
  const struct PPB_Flash_12_6 *iface = Pnacl_WrapperInfo_PPB_Flash_12_6.real_iface;
  return iface->EnumerateVideoCaptureDevices(instance, video_capture, *devices);
}

/* End wrapper methods for PPB_Flash_12_6 */

/* Begin wrapper methods for PPB_Flash_13_0 */

static void Pnacl_M24_1_PPB_Flash_SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  iface->SetInstanceAlwaysOnTop(instance, on_top);
}

static PP_Bool Pnacl_M24_1_PPB_Flash_DrawGlyphs(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->DrawGlyphs(instance, pp_image_data, font_desc, color, position, clip, transformation, allow_subpixel_aa, glyph_count, glyph_indices, glyph_advances);
}

static void Pnacl_M24_1_PPB_Flash_GetProxyForURL(struct PP_Var* _struct_result, PP_Instance instance, const char* url) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  *_struct_result = iface->GetProxyForURL(instance, url);
}

static int32_t Pnacl_M24_1_PPB_Flash_Navigate(PP_Resource request_info, const char* target, PP_Bool from_user_action) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->Navigate(request_info, target, from_user_action);
}

static double Pnacl_M24_1_PPB_Flash_GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->GetLocalTimeZoneOffset(instance, t);
}

static void Pnacl_M24_1_PPB_Flash_GetCommandLineArgs(struct PP_Var* _struct_result, PP_Module module) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  *_struct_result = iface->GetCommandLineArgs(module);
}

static void Pnacl_M24_1_PPB_Flash_PreloadFontWin(const void* logfontw) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  iface->PreloadFontWin(logfontw);
}

static PP_Bool Pnacl_M24_1_PPB_Flash_IsRectTopmost(PP_Instance instance, const struct PP_Rect* rect) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->IsRectTopmost(instance, rect);
}

static void Pnacl_M24_1_PPB_Flash_UpdateActivity(PP_Instance instance) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  iface->UpdateActivity(instance);
}

static void Pnacl_M24_1_PPB_Flash_GetSetting(struct PP_Var* _struct_result, PP_Instance instance, PP_FlashSetting setting) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  *_struct_result = iface->GetSetting(instance, setting);
}

static PP_Bool Pnacl_M24_1_PPB_Flash_SetCrashData(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var* value) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->SetCrashData(instance, key, *value);
}

static int32_t Pnacl_M24_1_PPB_Flash_EnumerateVideoCaptureDevices(PP_Instance instance, PP_Resource video_capture, struct PP_ArrayOutput* devices) {
  const struct PPB_Flash_13_0 *iface = Pnacl_WrapperInfo_PPB_Flash_13_0.real_iface;
  return iface->EnumerateVideoCaptureDevices(instance, video_capture, *devices);
}

/* End wrapper methods for PPB_Flash_13_0 */

/* Begin wrapper methods for PPB_Flash_Clipboard_4_0 */

static PP_Bool Pnacl_M19_PPB_Flash_Clipboard_IsFormatAvailable(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, PP_Flash_Clipboard_Format format) {
  const struct PPB_Flash_Clipboard_4_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0.real_iface;
  return iface->IsFormatAvailable(instance_id, clipboard_type, format);
}

static void Pnacl_M19_PPB_Flash_Clipboard_ReadData(struct PP_Var* _struct_result, PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, PP_Flash_Clipboard_Format format) {
  const struct PPB_Flash_Clipboard_4_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0.real_iface;
  *_struct_result = iface->ReadData(instance_id, clipboard_type, format);
}

static int32_t Pnacl_M19_PPB_Flash_Clipboard_WriteData(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t data_item_count, const PP_Flash_Clipboard_Format formats[], const struct PP_Var data_items[]) {
  const struct PPB_Flash_Clipboard_4_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0.real_iface;
  return iface->WriteData(instance_id, clipboard_type, data_item_count, formats, data_items);
}

/* End wrapper methods for PPB_Flash_Clipboard_4_0 */

/* Begin wrapper methods for PPB_Flash_Clipboard_5_0 */

static uint32_t Pnacl_M24_PPB_Flash_Clipboard_RegisterCustomFormat(PP_Instance instance_id, const char* format_name) {
  const struct PPB_Flash_Clipboard_5_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0.real_iface;
  return iface->RegisterCustomFormat(instance_id, format_name);
}

static PP_Bool Pnacl_M24_PPB_Flash_Clipboard_IsFormatAvailable(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t format) {
  const struct PPB_Flash_Clipboard_5_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0.real_iface;
  return iface->IsFormatAvailable(instance_id, clipboard_type, format);
}

static void Pnacl_M24_PPB_Flash_Clipboard_ReadData(struct PP_Var* _struct_result, PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t format) {
  const struct PPB_Flash_Clipboard_5_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0.real_iface;
  *_struct_result = iface->ReadData(instance_id, clipboard_type, format);
}

static int32_t Pnacl_M24_PPB_Flash_Clipboard_WriteData(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t data_item_count, const uint32_t formats[], const struct PP_Var data_items[]) {
  const struct PPB_Flash_Clipboard_5_0 *iface = Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0.real_iface;
  return iface->WriteData(instance_id, clipboard_type, data_item_count, formats, data_items);
}

/* End wrapper methods for PPB_Flash_Clipboard_5_0 */

/* Begin wrapper methods for PPB_Flash_DeviceID_1_0 */

static PP_Resource Pnacl_M21_PPB_Flash_DeviceID_Create(PP_Instance instance) {
  const struct PPB_Flash_DeviceID_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DeviceID_1_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M21_PPB_Flash_DeviceID_GetDeviceID(PP_Resource device_id, struct PP_Var* id, struct PP_CompletionCallback* callback) {
  const struct PPB_Flash_DeviceID_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DeviceID_1_0.real_iface;
  return iface->GetDeviceID(device_id, id, *callback);
}

/* End wrapper methods for PPB_Flash_DeviceID_1_0 */

/* Begin wrapper methods for PPB_Flash_DRM_1_0 */

static PP_Resource Pnacl_M29_PPB_Flash_DRM_Create(PP_Instance instance) {
  const struct PPB_Flash_DRM_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DRM_1_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M29_PPB_Flash_DRM_GetDeviceID(PP_Resource drm, struct PP_Var* id, struct PP_CompletionCallback* callback) {
  const struct PPB_Flash_DRM_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DRM_1_0.real_iface;
  return iface->GetDeviceID(drm, id, *callback);
}

static PP_Bool Pnacl_M29_PPB_Flash_DRM_GetHmonitor(PP_Resource drm, int64_t* hmonitor) {
  const struct PPB_Flash_DRM_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DRM_1_0.real_iface;
  return iface->GetHmonitor(drm, hmonitor);
}

static int32_t Pnacl_M29_PPB_Flash_DRM_GetVoucherFile(PP_Resource drm, PP_Resource* file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_Flash_DRM_1_0 *iface = Pnacl_WrapperInfo_PPB_Flash_DRM_1_0.real_iface;
  return iface->GetVoucherFile(drm, file_ref, *callback);
}

/* End wrapper methods for PPB_Flash_DRM_1_0 */

/* Not generating wrapper methods for PPB_Flash_FontFile_0_1 */

/* Not generating wrapper methods for PPB_FlashFullscreen_0_1 */

/* Not generating wrapper methods for PPB_FlashFullscreen_1_0 */

/* Begin wrapper methods for PPB_Flash_Menu_0_2 */

static PP_Resource Pnacl_M14_PPB_Flash_Menu_Create(PP_Instance instance_id, const struct PP_Flash_Menu* menu_data) {
  const struct PPB_Flash_Menu_0_2 *iface = Pnacl_WrapperInfo_PPB_Flash_Menu_0_2.real_iface;
  return iface->Create(instance_id, menu_data);
}

static PP_Bool Pnacl_M14_PPB_Flash_Menu_IsFlashMenu(PP_Resource resource_id) {
  const struct PPB_Flash_Menu_0_2 *iface = Pnacl_WrapperInfo_PPB_Flash_Menu_0_2.real_iface;
  return iface->IsFlashMenu(resource_id);
}

static int32_t Pnacl_M14_PPB_Flash_Menu_Show(PP_Resource menu_id, const struct PP_Point* location, int32_t* selected_id, struct PP_CompletionCallback* callback) {
  const struct PPB_Flash_Menu_0_2 *iface = Pnacl_WrapperInfo_PPB_Flash_Menu_0_2.real_iface;
  return iface->Show(menu_id, location, selected_id, *callback);
}

/* End wrapper methods for PPB_Flash_Menu_0_2 */

/* Not generating wrapper methods for PPB_Flash_MessageLoop_0_1 */

/* Not generating wrapper methods for PPB_Flash_Print_1_0 */

/* Begin wrapper methods for PPB_HostResolver_Private_0_1 */

static PP_Resource Pnacl_M19_PPB_HostResolver_Private_Create(PP_Instance instance) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_HostResolver_Private_IsHostResolver(PP_Resource resource) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->IsHostResolver(resource);
}

static int32_t Pnacl_M19_PPB_HostResolver_Private_Resolve(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Private_Hint* hint, struct PP_CompletionCallback* callback) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->Resolve(host_resolver, host, port, hint, *callback);
}

static void Pnacl_M19_PPB_HostResolver_Private_GetCanonicalName(struct PP_Var* _struct_result, PP_Resource host_resolver) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  *_struct_result = iface->GetCanonicalName(host_resolver);
}

static uint32_t Pnacl_M19_PPB_HostResolver_Private_GetSize(PP_Resource host_resolver) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->GetSize(host_resolver);
}

static PP_Bool Pnacl_M19_PPB_HostResolver_Private_GetNetAddress(PP_Resource host_resolver, uint32_t index, struct PP_NetAddress_Private* addr) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->GetNetAddress(host_resolver, index, addr);
}

/* End wrapper methods for PPB_HostResolver_Private_0_1 */

/* Begin wrapper methods for PPB_Instance_Private_0_1 */

static void Pnacl_M13_PPB_Instance_Private_GetWindowObject(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->GetWindowObject(instance);
}

static void Pnacl_M13_PPB_Instance_Private_GetOwnerElementObject(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->GetOwnerElementObject(instance);
}

static void Pnacl_M13_PPB_Instance_Private_ExecuteScript(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* script, struct PP_Var* exception) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->ExecuteScript(instance, *script, exception);
}

/* End wrapper methods for PPB_Instance_Private_0_1 */

/* Begin wrapper methods for PPB_NaCl_Private_1_0 */

static PP_ExternalPluginResult Pnacl_M25_PPB_NaCl_Private_LaunchSelLdr(PP_Instance instance, const char* alleged_url, PP_Bool uses_irt, PP_Bool uses_ppapi, PP_Bool enable_ppapi_dev, PP_Bool enable_dyncode_syscalls, PP_Bool enable_exception_handling, PP_Bool enable_crash_throttling, void* imc_handle, struct PP_Var* error_message) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->LaunchSelLdr(instance, alleged_url, uses_irt, uses_ppapi, enable_ppapi_dev, enable_dyncode_syscalls, enable_exception_handling, enable_crash_throttling, imc_handle, error_message);
}

static PP_ExternalPluginResult Pnacl_M25_PPB_NaCl_Private_StartPpapiProxy(PP_Instance instance) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->StartPpapiProxy(instance);
}

static int32_t Pnacl_M25_PPB_NaCl_Private_UrandomFD(void) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->UrandomFD();
}

static PP_Bool Pnacl_M25_PPB_NaCl_Private_Are3DInterfacesDisabled(void) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->Are3DInterfacesDisabled();
}

static int32_t Pnacl_M25_PPB_NaCl_Private_BrokerDuplicateHandle(PP_FileHandle source_handle, uint32_t process_id, PP_FileHandle* target_handle, uint32_t desired_access, uint32_t options) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->BrokerDuplicateHandle(source_handle, process_id, target_handle, desired_access, options);
}

static int32_t Pnacl_M25_PPB_NaCl_Private_EnsurePnaclInstalled(PP_Instance instance, struct PP_CompletionCallback* callback) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->EnsurePnaclInstalled(instance, *callback);
}

static PP_FileHandle Pnacl_M25_PPB_NaCl_Private_GetReadonlyPnaclFd(const char* filename) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->GetReadonlyPnaclFd(filename);
}

static PP_FileHandle Pnacl_M25_PPB_NaCl_Private_CreateTemporaryFile(PP_Instance instance) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->CreateTemporaryFile(instance);
}

static int32_t Pnacl_M25_PPB_NaCl_Private_GetNexeFd(PP_Instance instance, const char* pexe_url, uint32_t abi_version, uint32_t opt_level, const char* last_modified, const char* etag, PP_Bool has_no_store_header, PP_Bool* is_hit, PP_FileHandle* nexe_handle, struct PP_CompletionCallback* callback) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->GetNexeFd(instance, pexe_url, abi_version, opt_level, last_modified, etag, has_no_store_header, is_hit, nexe_handle, *callback);
}

static void Pnacl_M25_PPB_NaCl_Private_ReportTranslationFinished(PP_Instance instance, PP_Bool success) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  iface->ReportTranslationFinished(instance, success);
}

static PP_Bool Pnacl_M25_PPB_NaCl_Private_IsOffTheRecord(void) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->IsOffTheRecord();
}

static PP_Bool Pnacl_M25_PPB_NaCl_Private_IsPnaclEnabled(void) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->IsPnaclEnabled();
}

static PP_ExternalPluginResult Pnacl_M25_PPB_NaCl_Private_ReportNaClError(PP_Instance instance, PP_NaClError message_id) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->ReportNaClError(instance, message_id);
}

static PP_FileHandle Pnacl_M25_PPB_NaCl_Private_OpenNaClExecutable(PP_Instance instance, const char* file_url, uint64_t* file_token_lo, uint64_t* file_token_hi) {
  const struct PPB_NaCl_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NaCl_Private_1_0.real_iface;
  return iface->OpenNaClExecutable(instance, file_url, file_token_lo, file_token_hi);
}

/* End wrapper methods for PPB_NaCl_Private_1_0 */

/* Begin wrapper methods for PPB_NetAddress_Private_0_1 */

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M17_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M17_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

/* End wrapper methods for PPB_NetAddress_Private_0_1 */

/* Begin wrapper methods for PPB_NetAddress_Private_1_0 */

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M19_0_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M19_0_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

static PP_NetAddressFamily_Private Pnacl_M19_0_PPB_NetAddress_Private_GetFamily(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetFamily(addr);
}

static uint16_t Pnacl_M19_0_PPB_NetAddress_Private_GetPort(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetPort(addr);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_GetAddress(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetAddress(addr, address, address_size);
}

/* End wrapper methods for PPB_NetAddress_Private_1_0 */

/* Begin wrapper methods for PPB_NetAddress_Private_1_1 */

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

static PP_NetAddressFamily_Private Pnacl_M19_1_PPB_NetAddress_Private_GetFamily(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetFamily(addr);
}

static uint16_t Pnacl_M19_1_PPB_NetAddress_Private_GetPort(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetPort(addr);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_GetAddress(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetAddress(addr, address, address_size);
}

static uint32_t Pnacl_M19_1_PPB_NetAddress_Private_GetScopeID(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetScopeID(addr);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv4Address(const uint8_t ip[4], uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->CreateFromIPv4Address(ip, port, addr_out);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv6Address(const uint8_t ip[16], uint32_t scope_id, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->CreateFromIPv6Address(ip, scope_id, port, addr_out);
}

/* End wrapper methods for PPB_NetAddress_Private_1_1 */

/* Begin wrapper methods for PPB_OutputProtection_Private_0_1 */

static PP_Resource Pnacl_M31_PPB_OutputProtection_Private_Create(PP_Instance instance) {
  const struct PPB_OutputProtection_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M31_PPB_OutputProtection_Private_IsOutputProtection(PP_Resource resource) {
  const struct PPB_OutputProtection_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1.real_iface;
  return iface->IsOutputProtection(resource);
}

static int32_t Pnacl_M31_PPB_OutputProtection_Private_QueryStatus(PP_Resource resource, uint32_t* link_mask, uint32_t* protection_mask, struct PP_CompletionCallback* callback) {
  const struct PPB_OutputProtection_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1.real_iface;
  return iface->QueryStatus(resource, link_mask, protection_mask, *callback);
}

static int32_t Pnacl_M31_PPB_OutputProtection_Private_EnableProtection(PP_Resource resource, uint32_t desired_protection_mask, struct PP_CompletionCallback* callback) {
  const struct PPB_OutputProtection_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1.real_iface;
  return iface->EnableProtection(resource, desired_protection_mask, *callback);
}

/* End wrapper methods for PPB_OutputProtection_Private_0_1 */

/* Begin wrapper methods for PPB_PlatformVerification_Private_0_1 */

static PP_Resource Pnacl_M31_PPB_PlatformVerification_Private_Create(PP_Instance instance) {
  const struct PPB_PlatformVerification_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M31_PPB_PlatformVerification_Private_IsPlatformVerification(PP_Resource resource) {
  const struct PPB_PlatformVerification_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1.real_iface;
  return iface->IsPlatformVerification(resource);
}

static int32_t Pnacl_M31_PPB_PlatformVerification_Private_CanChallengePlatform(PP_Resource instance, PP_Bool* can_challenge_platform, struct PP_CompletionCallback* callback) {
  const struct PPB_PlatformVerification_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1.real_iface;
  return iface->CanChallengePlatform(instance, can_challenge_platform, *callback);
}

static int32_t Pnacl_M31_PPB_PlatformVerification_Private_ChallengePlatform(PP_Resource instance, struct PP_Var* service_id, struct PP_Var* challenge, struct PP_Var* signed_data, struct PP_Var* signed_data_signature, struct PP_Var* platform_key_certificate, struct PP_CompletionCallback* callback) {
  const struct PPB_PlatformVerification_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1.real_iface;
  return iface->ChallengePlatform(instance, *service_id, *challenge, signed_data, signed_data_signature, platform_key_certificate, *callback);
}

/* End wrapper methods for PPB_PlatformVerification_Private_0_1 */

/* Begin wrapper methods for PPB_Talk_Private_1_0 */

static PP_Resource Pnacl_M19_PPB_Talk_Private_Create(PP_Instance instance) {
  const struct PPB_Talk_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_1_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M19_PPB_Talk_Private_GetPermission(PP_Resource talk_resource, struct PP_CompletionCallback* callback) {
  const struct PPB_Talk_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_1_0.real_iface;
  return iface->GetPermission(talk_resource, *callback);
}

/* End wrapper methods for PPB_Talk_Private_1_0 */

/* Begin wrapper methods for PPB_Talk_Private_2_0 */

static PP_Resource Pnacl_M29_PPB_Talk_Private_Create(PP_Instance instance) {
  const struct PPB_Talk_Private_2_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_2_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M29_PPB_Talk_Private_RequestPermission(PP_Resource talk_resource, PP_TalkPermission permission, struct PP_CompletionCallback* callback) {
  const struct PPB_Talk_Private_2_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_2_0.real_iface;
  return iface->RequestPermission(talk_resource, permission, *callback);
}

static int32_t Pnacl_M29_PPB_Talk_Private_StartRemoting(PP_Resource talk_resource, PP_TalkEventCallback event_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_Talk_Private_2_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_2_0.real_iface;
  return iface->StartRemoting(talk_resource, event_callback, user_data, *callback);
}

static int32_t Pnacl_M29_PPB_Talk_Private_StopRemoting(PP_Resource talk_resource, struct PP_CompletionCallback* callback) {
  const struct PPB_Talk_Private_2_0 *iface = Pnacl_WrapperInfo_PPB_Talk_Private_2_0.real_iface;
  return iface->StopRemoting(talk_resource, *callback);
}

/* End wrapper methods for PPB_Talk_Private_2_0 */

/* Begin wrapper methods for PPB_TCPServerSocket_Private_0_1 */

static PP_Resource Pnacl_M18_PPB_TCPServerSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M18_PPB_TCPServerSocket_Private_IsTCPServerSocket(PP_Resource resource) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->IsTCPServerSocket(resource);
}

static int32_t Pnacl_M18_PPB_TCPServerSocket_Private_Listen(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Listen(tcp_server_socket, addr, backlog, *callback);
}

static int32_t Pnacl_M18_PPB_TCPServerSocket_Private_Accept(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Accept(tcp_server_socket, tcp_socket, *callback);
}

static void Pnacl_M18_PPB_TCPServerSocket_Private_StopListening(PP_Resource tcp_server_socket) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  iface->StopListening(tcp_server_socket);
}

/* End wrapper methods for PPB_TCPServerSocket_Private_0_1 */

/* Begin wrapper methods for PPB_TCPServerSocket_Private_0_2 */

static PP_Resource Pnacl_M28_PPB_TCPServerSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M28_PPB_TCPServerSocket_Private_IsTCPServerSocket(PP_Resource resource) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->IsTCPServerSocket(resource);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_Listen(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Listen(tcp_server_socket, addr, backlog, *callback);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_Accept(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Accept(tcp_server_socket, tcp_socket, *callback);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_GetLocalAddress(PP_Resource tcp_server_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->GetLocalAddress(tcp_server_socket, addr);
}

static void Pnacl_M28_PPB_TCPServerSocket_Private_StopListening(PP_Resource tcp_server_socket) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  iface->StopListening(tcp_server_socket);
}

/* End wrapper methods for PPB_TCPServerSocket_Private_0_2 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_3 */

static PP_Resource Pnacl_M17_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M17_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  iface->Disconnect(tcp_socket);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_3 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_4 */

static PP_Resource Pnacl_M20_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static PP_Resource Pnacl_M20_PPB_TCPSocket_Private_GetServerCertificate(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetServerCertificate(tcp_socket);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_AddChainBuildingCertificate(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->AddChainBuildingCertificate(tcp_socket, certificate, is_trusted);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M20_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  iface->Disconnect(tcp_socket);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_4 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_5 */

static PP_Resource Pnacl_M27_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static PP_Resource Pnacl_M27_PPB_TCPSocket_Private_GetServerCertificate(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetServerCertificate(tcp_socket);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_AddChainBuildingCertificate(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->AddChainBuildingCertificate(tcp_socket, certificate, is_trusted);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M27_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  iface->Disconnect(tcp_socket);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_SetOption(PP_Resource tcp_socket, PP_TCPSocketOption_Private name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_5 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_2 */

static PP_Resource Pnacl_M17_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M17_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M17_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M17_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_2 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_3 */

static PP_Resource Pnacl_M19_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_GetBoundAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->GetBoundAddress(udp_socket, addr);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M19_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_3 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_4 */

static PP_Resource Pnacl_M23_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_SetSocketFeature(PP_Resource udp_socket, PP_UDPSocketFeature_Private name, struct PP_Var* value) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->SetSocketFeature(udp_socket, name, *value);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_GetBoundAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->GetBoundAddress(udp_socket, addr);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M23_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_4 */

/* Begin wrapper methods for PPB_UMA_Private_0_1 */

static void Pnacl_M18_PPB_UMA_Private_HistogramCustomTimes(struct PP_Var* name, int64_t sample, int64_t min, int64_t max, uint32_t bucket_count) {
  const struct PPB_UMA_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_1.real_iface;
  iface->HistogramCustomTimes(*name, sample, min, max, bucket_count);
}

static void Pnacl_M18_PPB_UMA_Private_HistogramCustomCounts(struct PP_Var* name, int32_t sample, int32_t min, int32_t max, uint32_t bucket_count) {
  const struct PPB_UMA_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_1.real_iface;
  iface->HistogramCustomCounts(*name, sample, min, max, bucket_count);
}

static void Pnacl_M18_PPB_UMA_Private_HistogramEnumeration(struct PP_Var* name, int32_t sample, int32_t boundary_value) {
  const struct PPB_UMA_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_1.real_iface;
  iface->HistogramEnumeration(*name, sample, boundary_value);
}

/* End wrapper methods for PPB_UMA_Private_0_1 */

/* Begin wrapper methods for PPB_VideoDestination_Private_0_1 */

static PP_Resource Pnacl_M28_PPB_VideoDestination_Private_Create(PP_Instance instance) {
  const struct PPB_VideoDestination_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M28_PPB_VideoDestination_Private_IsVideoDestination(PP_Resource resource) {
  const struct PPB_VideoDestination_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1.real_iface;
  return iface->IsVideoDestination(resource);
}

static int32_t Pnacl_M28_PPB_VideoDestination_Private_Open(PP_Resource destination, struct PP_Var* stream_url, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDestination_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1.real_iface;
  return iface->Open(destination, *stream_url, *callback);
}

static int32_t Pnacl_M28_PPB_VideoDestination_Private_PutFrame(PP_Resource destination, const struct PP_VideoFrame_Private* frame) {
  const struct PPB_VideoDestination_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1.real_iface;
  return iface->PutFrame(destination, frame);
}

static void Pnacl_M28_PPB_VideoDestination_Private_Close(PP_Resource destination) {
  const struct PPB_VideoDestination_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1.real_iface;
  iface->Close(destination);
}

/* End wrapper methods for PPB_VideoDestination_Private_0_1 */

/* Begin wrapper methods for PPB_VideoSource_Private_0_1 */

static PP_Resource Pnacl_M28_PPB_VideoSource_Private_Create(PP_Instance instance) {
  const struct PPB_VideoSource_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M28_PPB_VideoSource_Private_IsVideoSource(PP_Resource resource) {
  const struct PPB_VideoSource_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1.real_iface;
  return iface->IsVideoSource(resource);
}

static int32_t Pnacl_M28_PPB_VideoSource_Private_Open(PP_Resource source, struct PP_Var* stream_url, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoSource_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1.real_iface;
  return iface->Open(source, *stream_url, *callback);
}

static int32_t Pnacl_M28_PPB_VideoSource_Private_GetFrame(PP_Resource source, struct PP_VideoFrame_Private* frame, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoSource_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1.real_iface;
  return iface->GetFrame(source, frame, *callback);
}

static void Pnacl_M28_PPB_VideoSource_Private_Close(PP_Resource source) {
  const struct PPB_VideoSource_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1.real_iface;
  iface->Close(source);
}

/* End wrapper methods for PPB_VideoSource_Private_0_1 */

/* Begin wrapper methods for PPB_X509Certificate_Private_0_1 */

static PP_Resource Pnacl_M19_PPB_X509Certificate_Private_Create(PP_Instance instance) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_X509Certificate_Private_IsX509CertificatePrivate(PP_Resource resource) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->IsX509CertificatePrivate(resource);
}

static PP_Bool Pnacl_M19_PPB_X509Certificate_Private_Initialize(PP_Resource resource, const char* bytes, uint32_t length) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->Initialize(resource, bytes, length);
}

static void Pnacl_M19_PPB_X509Certificate_Private_GetField(struct PP_Var* _struct_result, PP_Resource resource, PP_X509Certificate_Private_Field field) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  *_struct_result = iface->GetField(resource, field);
}

/* End wrapper methods for PPB_X509Certificate_Private_0_1 */

/* Begin wrapper methods for PPP_ContentDecryptor_Private_0_7 */

static void Pnacl_M31_PPP_ContentDecryptor_Private_Initialize(PP_Instance instance, struct PP_Var key_system, PP_Bool can_challenge_platform) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* key_system, PP_Bool can_challenge_platform) =
    ((void (*)(PP_Instance instance, struct PP_Var* key_system, PP_Bool can_challenge_platform))iface->Initialize);
  temp_fp(instance, &key_system, can_challenge_platform);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_GenerateKeyRequest(PP_Instance instance, struct PP_Var type, struct PP_Var init_data) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* type, struct PP_Var* init_data) =
    ((void (*)(PP_Instance instance, struct PP_Var* type, struct PP_Var* init_data))iface->GenerateKeyRequest);
  temp_fp(instance, &type, &init_data);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_AddKey(PP_Instance instance, struct PP_Var session_id, struct PP_Var key, struct PP_Var init_data) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* session_id, struct PP_Var* key, struct PP_Var* init_data) =
    ((void (*)(PP_Instance instance, struct PP_Var* session_id, struct PP_Var* key, struct PP_Var* init_data))iface->AddKey);
  temp_fp(instance, &session_id, &key, &init_data);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_CancelKeyRequest(PP_Instance instance, struct PP_Var session_id) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* session_id) =
    ((void (*)(PP_Instance instance, struct PP_Var* session_id))iface->CancelKeyRequest);
  temp_fp(instance, &session_id);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_Decrypt(PP_Instance instance, PP_Resource encrypted_block, const struct PP_EncryptedBlockInfo* encrypted_block_info) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, PP_Resource encrypted_block, const struct PP_EncryptedBlockInfo* encrypted_block_info) =
    ((void (*)(PP_Instance instance, PP_Resource encrypted_block, const struct PP_EncryptedBlockInfo* encrypted_block_info))iface->Decrypt);
  temp_fp(instance, encrypted_block, encrypted_block_info);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_InitializeAudioDecoder(PP_Instance instance, const struct PP_AudioDecoderConfig* decoder_config, PP_Resource codec_extra_data) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, const struct PP_AudioDecoderConfig* decoder_config, PP_Resource codec_extra_data) =
    ((void (*)(PP_Instance instance, const struct PP_AudioDecoderConfig* decoder_config, PP_Resource codec_extra_data))iface->InitializeAudioDecoder);
  temp_fp(instance, decoder_config, codec_extra_data);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_InitializeVideoDecoder(PP_Instance instance, const struct PP_VideoDecoderConfig* decoder_config, PP_Resource codec_extra_data) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, const struct PP_VideoDecoderConfig* decoder_config, PP_Resource codec_extra_data) =
    ((void (*)(PP_Instance instance, const struct PP_VideoDecoderConfig* decoder_config, PP_Resource codec_extra_data))iface->InitializeVideoDecoder);
  temp_fp(instance, decoder_config, codec_extra_data);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_DeinitializeDecoder(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) =
    ((void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id))iface->DeinitializeDecoder);
  temp_fp(instance, decoder_type, request_id);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_ResetDecoder(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id) =
    ((void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id))iface->ResetDecoder);
  temp_fp(instance, decoder_type, request_id);
}

static void Pnacl_M31_PPP_ContentDecryptor_Private_DecryptAndDecode(PP_Instance instance, PP_DecryptorStreamType decoder_type, PP_Resource encrypted_buffer, const struct PP_EncryptedBlockInfo* encrypted_block_info) {
  const struct PPP_ContentDecryptor_Private_0_7 *iface = Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7.real_iface;
  void (*temp_fp)(PP_Instance instance, PP_DecryptorStreamType decoder_type, PP_Resource encrypted_buffer, const struct PP_EncryptedBlockInfo* encrypted_block_info) =
    ((void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, PP_Resource encrypted_buffer, const struct PP_EncryptedBlockInfo* encrypted_block_info))iface->DecryptAndDecode);
  temp_fp(instance, decoder_type, encrypted_buffer, encrypted_block_info);
}

/* End wrapper methods for PPP_ContentDecryptor_Private_0_7 */

/* Not generating wrapper methods for PPP_Flash_BrowserOperations_1_0 */

/* Not generating wrapper methods for PPP_Flash_BrowserOperations_1_2 */

/* Not generating wrapper methods for PPP_Flash_BrowserOperations_1_3 */

/* Begin wrapper methods for PPP_Instance_Private_0_1 */

static struct PP_Var Pnacl_M18_PPP_Instance_Private_GetInstanceObject(PP_Instance instance) {
  const struct PPP_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPP_Instance_Private_0_1.real_iface;
  void (*temp_fp)(struct PP_Var* _struct_result, PP_Instance instance) =
    ((void (*)(struct PP_Var* _struct_result, PP_Instance instance))iface->GetInstanceObject);
  struct PP_Var _struct_result;
  temp_fp(&_struct_result, instance);
  return _struct_result;
}

/* End wrapper methods for PPP_Instance_Private_0_1 */

/* Begin wrapper methods for PPB_Ext_Alarms_Dev_0_1 */

static void Pnacl_M27_PPB_Ext_Alarms_Dev_Create(PP_Instance instance, struct PP_Var* name, PP_Ext_Alarms_AlarmCreateInfo_Dev alarm_info) {
  const struct PPB_Ext_Alarms_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1.real_iface;
  iface->Create(instance, *name, alarm_info);
}

static int32_t Pnacl_M27_PPB_Ext_Alarms_Dev_Get(PP_Instance instance, struct PP_Var* name, PP_Ext_Alarms_Alarm_Dev* alarm, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Alarms_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1.real_iface;
  return iface->Get(instance, *name, alarm, *callback);
}

static int32_t Pnacl_M27_PPB_Ext_Alarms_Dev_GetAll(PP_Instance instance, PP_Ext_Alarms_Alarm_Dev_Array* alarms, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Alarms_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1.real_iface;
  return iface->GetAll(instance, alarms, *callback);
}

static void Pnacl_M27_PPB_Ext_Alarms_Dev_Clear(PP_Instance instance, struct PP_Var* name) {
  const struct PPB_Ext_Alarms_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1.real_iface;
  iface->Clear(instance, *name);
}

static void Pnacl_M27_PPB_Ext_Alarms_Dev_ClearAll(PP_Instance instance) {
  const struct PPB_Ext_Alarms_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1.real_iface;
  iface->ClearAll(instance);
}

/* End wrapper methods for PPB_Ext_Alarms_Dev_0_1 */

/* Begin wrapper methods for PPB_Ext_Events_Dev_0_1 */

static uint32_t Pnacl_M27_PPB_Ext_Events_Dev_AddListener(PP_Instance instance, struct PP_Ext_EventListener* listener) {
  const struct PPB_Ext_Events_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Events_Dev_0_1.real_iface;
  return iface->AddListener(instance, *listener);
}

static void Pnacl_M27_PPB_Ext_Events_Dev_RemoveListener(PP_Instance instance, uint32_t listener_id) {
  const struct PPB_Ext_Events_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Events_Dev_0_1.real_iface;
  iface->RemoveListener(instance, listener_id);
}

/* End wrapper methods for PPB_Ext_Events_Dev_0_1 */

/* Begin wrapper methods for PPB_Ext_Socket_Dev_0_1 */

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Create(PP_Instance instance, PP_Ext_Socket_SocketType_Dev type, PP_Ext_Socket_CreateOptions_Dev options, PP_Ext_Socket_CreateInfo_Dev* create_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Create(instance, type, options, create_info, *callback);
}

static void Pnacl_M28_PPB_Ext_Socket_Dev_Destroy(PP_Instance instance, struct PP_Var* socket_id) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  iface->Destroy(instance, *socket_id);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Connect(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* hostname, struct PP_Var* port, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Connect(instance, *socket_id, *hostname, *port, result, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Bind(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* port, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Bind(instance, *socket_id, *address, *port, result, *callback);
}

static void Pnacl_M28_PPB_Ext_Socket_Dev_Disconnect(PP_Instance instance, struct PP_Var* socket_id) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  iface->Disconnect(instance, *socket_id);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Read(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* buffer_size, PP_Ext_Socket_ReadInfo_Dev* read_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Read(instance, *socket_id, *buffer_size, read_info, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Write(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* data, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Write(instance, *socket_id, *data, write_info, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_RecvFrom(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* buffer_size, PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->RecvFrom(instance, *socket_id, *buffer_size, recv_from_info, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_SendTo(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* data, struct PP_Var* address, struct PP_Var* port, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->SendTo(instance, *socket_id, *data, *address, *port, write_info, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Listen(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* port, struct PP_Var* backlog, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Listen(instance, *socket_id, *address, *port, *backlog, result, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_Accept(PP_Instance instance, struct PP_Var* socket_id, PP_Ext_Socket_AcceptInfo_Dev* accept_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->Accept(instance, *socket_id, accept_info, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_SetKeepAlive(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* enable, struct PP_Var* delay, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->SetKeepAlive(instance, *socket_id, *enable, *delay, result, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_SetNoDelay(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* no_delay, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->SetNoDelay(instance, *socket_id, *no_delay, result, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_GetInfo(PP_Instance instance, struct PP_Var* socket_id, PP_Ext_Socket_SocketInfo_Dev* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->GetInfo(instance, *socket_id, result, *callback);
}

static int32_t Pnacl_M28_PPB_Ext_Socket_Dev_GetNetworkList(PP_Instance instance, PP_Ext_Socket_NetworkInterface_Dev_Array* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1.real_iface;
  return iface->GetNetworkList(instance, result, *callback);
}

/* End wrapper methods for PPB_Ext_Socket_Dev_0_1 */

/* Begin wrapper methods for PPB_Ext_Socket_Dev_0_2 */

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Create(PP_Instance instance, PP_Ext_Socket_SocketType_Dev type, PP_Ext_Socket_CreateOptions_Dev options, PP_Ext_Socket_CreateInfo_Dev* create_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Create(instance, type, options, create_info, *callback);
}

static void Pnacl_M29_PPB_Ext_Socket_Dev_Destroy(PP_Instance instance, struct PP_Var* socket_id) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  iface->Destroy(instance, *socket_id);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Connect(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* hostname, struct PP_Var* port, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Connect(instance, *socket_id, *hostname, *port, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Bind(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* port, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Bind(instance, *socket_id, *address, *port, result, *callback);
}

static void Pnacl_M29_PPB_Ext_Socket_Dev_Disconnect(PP_Instance instance, struct PP_Var* socket_id) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  iface->Disconnect(instance, *socket_id);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Read(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* buffer_size, PP_Ext_Socket_ReadInfo_Dev* read_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Read(instance, *socket_id, *buffer_size, read_info, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Write(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* data, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Write(instance, *socket_id, *data, write_info, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_RecvFrom(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* buffer_size, PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->RecvFrom(instance, *socket_id, *buffer_size, recv_from_info, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_SendTo(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* data, struct PP_Var* address, struct PP_Var* port, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->SendTo(instance, *socket_id, *data, *address, *port, write_info, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Listen(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* port, struct PP_Var* backlog, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Listen(instance, *socket_id, *address, *port, *backlog, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_Accept(PP_Instance instance, struct PP_Var* socket_id, PP_Ext_Socket_AcceptInfo_Dev* accept_info, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->Accept(instance, *socket_id, accept_info, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_SetKeepAlive(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* enable, struct PP_Var* delay, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->SetKeepAlive(instance, *socket_id, *enable, *delay, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_SetNoDelay(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* no_delay, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->SetNoDelay(instance, *socket_id, *no_delay, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_GetInfo(PP_Instance instance, struct PP_Var* socket_id, PP_Ext_Socket_SocketInfo_Dev* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->GetInfo(instance, *socket_id, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_GetNetworkList(PP_Instance instance, PP_Ext_Socket_NetworkInterface_Dev_Array* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->GetNetworkList(instance, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_JoinGroup(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->JoinGroup(instance, *socket_id, *address, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_LeaveGroup(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* address, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->LeaveGroup(instance, *socket_id, *address, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_SetMulticastTimeToLive(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* ttl, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->SetMulticastTimeToLive(instance, *socket_id, *ttl, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_SetMulticastLoopbackMode(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* enabled, struct PP_Var* result, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->SetMulticastLoopbackMode(instance, *socket_id, *enabled, result, *callback);
}

static int32_t Pnacl_M29_PPB_Ext_Socket_Dev_GetJoinedGroups(PP_Instance instance, struct PP_Var* socket_id, struct PP_Var* groups, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_Socket_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2.real_iface;
  return iface->GetJoinedGroups(instance, *socket_id, groups, *callback);
}

/* End wrapper methods for PPB_Ext_Socket_Dev_0_2 */

/* Not generating wrapper interface for PPB_Audio_1_0 */

/* Not generating wrapper interface for PPB_Audio_1_1 */

/* Not generating wrapper interface for PPB_AudioConfig_1_0 */

/* Not generating wrapper interface for PPB_AudioConfig_1_1 */

struct PPB_Console_1_0 Pnacl_Wrappers_PPB_Console_1_0 = {
    .Log = (void (*)(PP_Instance instance, PP_LogLevel level, struct PP_Var value))&Pnacl_M25_PPB_Console_Log,
    .LogWithSource = (void (*)(PP_Instance instance, PP_LogLevel level, struct PP_Var source, struct PP_Var value))&Pnacl_M25_PPB_Console_LogWithSource
};

struct PPB_Core_1_0 Pnacl_Wrappers_PPB_Core_1_0 = {
    .AddRefResource = (void (*)(PP_Resource resource))&Pnacl_M14_PPB_Core_AddRefResource,
    .ReleaseResource = (void (*)(PP_Resource resource))&Pnacl_M14_PPB_Core_ReleaseResource,
    .GetTime = (PP_Time (*)(void))&Pnacl_M14_PPB_Core_GetTime,
    .GetTimeTicks = (PP_TimeTicks (*)(void))&Pnacl_M14_PPB_Core_GetTimeTicks,
    .CallOnMainThread = (void (*)(int32_t delay_in_milliseconds, struct PP_CompletionCallback callback, int32_t result))&Pnacl_M14_PPB_Core_CallOnMainThread,
    .IsMainThread = (PP_Bool (*)(void))&Pnacl_M14_PPB_Core_IsMainThread
};

struct PPB_FileIO_1_0 Pnacl_Wrappers_PPB_FileIO_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_FileIO_Create,
    .IsFileIO = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileIO_IsFileIO,
    .Open = (int32_t (*)(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Open,
    .Query = (int32_t (*)(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Query,
    .Touch = (int32_t (*)(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Touch,
    .Read = (int32_t (*)(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Read,
    .Write = (int32_t (*)(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Write,
    .SetLength = (int32_t (*)(PP_Resource file_io, int64_t length, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_SetLength,
    .Flush = (int32_t (*)(PP_Resource file_io, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Flush,
    .Close = (void (*)(PP_Resource file_io))&Pnacl_M14_PPB_FileIO_Close
};

struct PPB_FileIO_1_1 Pnacl_Wrappers_PPB_FileIO_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_FileIO_Create,
    .IsFileIO = (PP_Bool (*)(PP_Resource resource))&Pnacl_M25_PPB_FileIO_IsFileIO,
    .Open = (int32_t (*)(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Open,
    .Query = (int32_t (*)(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Query,
    .Touch = (int32_t (*)(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Touch,
    .Read = (int32_t (*)(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Read,
    .Write = (int32_t (*)(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Write,
    .SetLength = (int32_t (*)(PP_Resource file_io, int64_t length, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_SetLength,
    .Flush = (int32_t (*)(PP_Resource file_io, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Flush,
    .Close = (void (*)(PP_Resource file_io))&Pnacl_M25_PPB_FileIO_Close,
    .ReadToArray = (int32_t (*)(PP_Resource file_io, int64_t offset, int32_t max_read_length, struct PP_ArrayOutput* output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_ReadToArray
};

struct PPB_FileRef_1_0 Pnacl_Wrappers_PPB_FileRef_1_0 = {
    .Create = (PP_Resource (*)(PP_Resource file_system, const char* path))&Pnacl_M14_PPB_FileRef_Create,
    .IsFileRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileRef_IsFileRef,
    .GetFileSystemType = (PP_FileSystemType (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetFileSystemType,
    .GetName = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetName,
    .GetPath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetPath,
    .GetParent = (PP_Resource (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetParent,
    .MakeDirectory = (int32_t (*)(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_MakeDirectory,
    .Touch = (int32_t (*)(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Touch,
    .Delete = (int32_t (*)(PP_Resource file_ref, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Delete,
    .Rename = (int32_t (*)(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Rename
};

struct PPB_FileRef_1_1 Pnacl_Wrappers_PPB_FileRef_1_1 = {
    .Create = (PP_Resource (*)(PP_Resource file_system, const char* path))&Pnacl_M28_PPB_FileRef_Create,
    .IsFileRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_FileRef_IsFileRef,
    .GetFileSystemType = (PP_FileSystemType (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetFileSystemType,
    .GetName = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetName,
    .GetPath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetPath,
    .GetParent = (PP_Resource (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetParent,
    .MakeDirectory = (int32_t (*)(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_MakeDirectory,
    .Touch = (int32_t (*)(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Touch,
    .Delete = (int32_t (*)(PP_Resource file_ref, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Delete,
    .Rename = (int32_t (*)(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Rename,
    .Query = (int32_t (*)(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Query,
    .ReadDirectoryEntries = (int32_t (*)(PP_Resource file_ref, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_ReadDirectoryEntries
};

struct PPB_FileSystem_1_0 Pnacl_Wrappers_PPB_FileSystem_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileSystemType type))&Pnacl_M14_PPB_FileSystem_Create,
    .IsFileSystem = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileSystem_IsFileSystem,
    .Open = (int32_t (*)(PP_Resource file_system, int64_t expected_size, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileSystem_Open,
    .GetType = (PP_FileSystemType (*)(PP_Resource file_system))&Pnacl_M14_PPB_FileSystem_GetType
};

/* Not generating wrapper interface for PPB_Fullscreen_1_0 */

/* Not generating wrapper interface for PPB_Gamepad_1_0 */

struct PPB_Graphics2D_1_0 Pnacl_Wrappers_PPB_Graphics2D_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque))&Pnacl_M14_PPB_Graphics2D_Create,
    .IsGraphics2D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_Graphics2D_IsGraphics2D,
    .Describe = (PP_Bool (*)(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque))&Pnacl_M14_PPB_Graphics2D_Describe,
    .PaintImageData = (void (*)(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect))&Pnacl_M14_PPB_Graphics2D_PaintImageData,
    .Scroll = (void (*)(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount))&Pnacl_M14_PPB_Graphics2D_Scroll,
    .ReplaceContents = (void (*)(PP_Resource graphics_2d, PP_Resource image_data))&Pnacl_M14_PPB_Graphics2D_ReplaceContents,
    .Flush = (int32_t (*)(PP_Resource graphics_2d, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_Graphics2D_Flush
};

struct PPB_Graphics2D_1_1 Pnacl_Wrappers_PPB_Graphics2D_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque))&Pnacl_M27_PPB_Graphics2D_Create,
    .IsGraphics2D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M27_PPB_Graphics2D_IsGraphics2D,
    .Describe = (PP_Bool (*)(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque))&Pnacl_M27_PPB_Graphics2D_Describe,
    .PaintImageData = (void (*)(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect))&Pnacl_M27_PPB_Graphics2D_PaintImageData,
    .Scroll = (void (*)(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount))&Pnacl_M27_PPB_Graphics2D_Scroll,
    .ReplaceContents = (void (*)(PP_Resource graphics_2d, PP_Resource image_data))&Pnacl_M27_PPB_Graphics2D_ReplaceContents,
    .Flush = (int32_t (*)(PP_Resource graphics_2d, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_Graphics2D_Flush,
    .SetScale = (PP_Bool (*)(PP_Resource resource, float scale))&Pnacl_M27_PPB_Graphics2D_SetScale,
    .GetScale = (float (*)(PP_Resource resource))&Pnacl_M27_PPB_Graphics2D_GetScale
};

struct PPB_Graphics3D_1_0 Pnacl_Wrappers_PPB_Graphics3D_1_0 = {
    .GetAttribMaxValue = (int32_t (*)(PP_Resource instance, int32_t attribute, int32_t* value))&Pnacl_M15_PPB_Graphics3D_GetAttribMaxValue,
    .Create = (PP_Resource (*)(PP_Instance instance, PP_Resource share_context, const int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_Create,
    .IsGraphics3D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M15_PPB_Graphics3D_IsGraphics3D,
    .GetAttribs = (int32_t (*)(PP_Resource context, int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_GetAttribs,
    .SetAttribs = (int32_t (*)(PP_Resource context, const int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_SetAttribs,
    .GetError = (int32_t (*)(PP_Resource context))&Pnacl_M15_PPB_Graphics3D_GetError,
    .ResizeBuffers = (int32_t (*)(PP_Resource context, int32_t width, int32_t height))&Pnacl_M15_PPB_Graphics3D_ResizeBuffers,
    .SwapBuffers = (int32_t (*)(PP_Resource context, struct PP_CompletionCallback callback))&Pnacl_M15_PPB_Graphics3D_SwapBuffers
};

struct PPB_HostResolver_1_0 Pnacl_Wrappers_PPB_HostResolver_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_HostResolver_Create,
    .IsHostResolver = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_HostResolver_IsHostResolver,
    .Resolve = (int32_t (*)(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Hint* hint, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_HostResolver_Resolve,
    .GetCanonicalName = (struct PP_Var (*)(PP_Resource host_resolver))&Pnacl_M29_PPB_HostResolver_GetCanonicalName,
    .GetNetAddressCount = (uint32_t (*)(PP_Resource host_resolver))&Pnacl_M29_PPB_HostResolver_GetNetAddressCount,
    .GetNetAddress = (PP_Resource (*)(PP_Resource host_resolver, uint32_t index))&Pnacl_M29_PPB_HostResolver_GetNetAddress
};

/* Not generating wrapper interface for PPB_ImageData_1_0 */

/* Not generating wrapper interface for PPB_InputEvent_1_0 */

struct PPB_MouseInputEvent_1_0 Pnacl_Wrappers_PPB_MouseInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count))&Pnacl_M13_PPB_MouseInputEvent_Create,
    .IsMouseInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_MouseInputEvent_IsMouseInputEvent,
    .GetButton = (PP_InputEvent_MouseButton (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetButton,
    .GetPosition = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetPosition,
    .GetClickCount = (int32_t (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetClickCount
};

struct PPB_MouseInputEvent_1_1 Pnacl_Wrappers_PPB_MouseInputEvent_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count, const struct PP_Point* mouse_movement))&Pnacl_M14_PPB_MouseInputEvent_Create,
    .IsMouseInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_MouseInputEvent_IsMouseInputEvent,
    .GetButton = (PP_InputEvent_MouseButton (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetButton,
    .GetPosition = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetPosition,
    .GetClickCount = (int32_t (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetClickCount,
    .GetMovement = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetMovement
};

struct PPB_WheelInputEvent_1_0 Pnacl_Wrappers_PPB_WheelInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_TimeTicks time_stamp, uint32_t modifiers, const struct PP_FloatPoint* wheel_delta, const struct PP_FloatPoint* wheel_ticks, PP_Bool scroll_by_page))&Pnacl_M13_PPB_WheelInputEvent_Create,
    .IsWheelInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_WheelInputEvent_IsWheelInputEvent,
    .GetDelta = (struct PP_FloatPoint (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetDelta,
    .GetTicks = (struct PP_FloatPoint (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetTicks,
    .GetScrollByPage = (PP_Bool (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetScrollByPage
};

struct PPB_KeyboardInputEvent_1_0 Pnacl_Wrappers_PPB_KeyboardInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var character_text))&Pnacl_M13_PPB_KeyboardInputEvent_Create,
    .IsKeyboardInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_KeyboardInputEvent_IsKeyboardInputEvent,
    .GetKeyCode = (uint32_t (*)(PP_Resource key_event))&Pnacl_M13_PPB_KeyboardInputEvent_GetKeyCode,
    .GetCharacterText = (struct PP_Var (*)(PP_Resource character_event))&Pnacl_M13_PPB_KeyboardInputEvent_GetCharacterText
};

struct PPB_TouchInputEvent_1_0 Pnacl_Wrappers_PPB_TouchInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers))&Pnacl_M13_PPB_TouchInputEvent_Create,
    .AddTouchPoint = (void (*)(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point))&Pnacl_M13_PPB_TouchInputEvent_AddTouchPoint,
    .IsTouchInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_TouchInputEvent_IsTouchInputEvent,
    .GetTouchCount = (uint32_t (*)(PP_Resource resource, PP_TouchListType list))&Pnacl_M13_PPB_TouchInputEvent_GetTouchCount,
    .GetTouchByIndex = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t index))&Pnacl_M13_PPB_TouchInputEvent_GetTouchByIndex,
    .GetTouchById = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t touch_id))&Pnacl_M13_PPB_TouchInputEvent_GetTouchById
};

struct PPB_IMEInputEvent_1_0 Pnacl_Wrappers_PPB_IMEInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end))&Pnacl_M13_PPB_IMEInputEvent_Create,
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_IMEInputEvent_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M13_PPB_IMEInputEvent_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M13_PPB_IMEInputEvent_GetSelection
};

/* Not generating wrapper interface for PPB_Instance_1_0 */

struct PPB_MessageLoop_1_0 Pnacl_Wrappers_PPB_MessageLoop_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_MessageLoop_Create,
    .GetForMainThread = (PP_Resource (*)(void))&Pnacl_M25_PPB_MessageLoop_GetForMainThread,
    .GetCurrent = (PP_Resource (*)(void))&Pnacl_M25_PPB_MessageLoop_GetCurrent,
    .AttachToCurrentThread = (int32_t (*)(PP_Resource message_loop))&Pnacl_M25_PPB_MessageLoop_AttachToCurrentThread,
    .Run = (int32_t (*)(PP_Resource message_loop))&Pnacl_M25_PPB_MessageLoop_Run,
    .PostWork = (int32_t (*)(PP_Resource message_loop, struct PP_CompletionCallback callback, int64_t delay_ms))&Pnacl_M25_PPB_MessageLoop_PostWork,
    .PostQuit = (int32_t (*)(PP_Resource message_loop, PP_Bool should_destroy))&Pnacl_M25_PPB_MessageLoop_PostQuit
};

struct PPB_Messaging_1_0 Pnacl_Wrappers_PPB_Messaging_1_0 = {
    .PostMessage = (void (*)(PP_Instance instance, struct PP_Var message))&Pnacl_M14_PPB_Messaging_PostMessage
};

/* Not generating wrapper interface for PPB_MouseCursor_1_0 */

struct PPB_MouseLock_1_0 Pnacl_Wrappers_PPB_MouseLock_1_0 = {
    .LockMouse = (int32_t (*)(PP_Instance instance, struct PP_CompletionCallback callback))&Pnacl_M16_PPB_MouseLock_LockMouse,
    .UnlockMouse = (void (*)(PP_Instance instance))&Pnacl_M16_PPB_MouseLock_UnlockMouse
};

struct PPB_NetAddress_1_0 Pnacl_Wrappers_PPB_NetAddress_1_0 = {
    .CreateFromIPv4Address = (PP_Resource (*)(PP_Instance instance, const struct PP_NetAddress_IPv4* ipv4_addr))&Pnacl_M29_PPB_NetAddress_CreateFromIPv4Address,
    .CreateFromIPv6Address = (PP_Resource (*)(PP_Instance instance, const struct PP_NetAddress_IPv6* ipv6_addr))&Pnacl_M29_PPB_NetAddress_CreateFromIPv6Address,
    .IsNetAddress = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_NetAddress_IsNetAddress,
    .GetFamily = (PP_NetAddress_Family (*)(PP_Resource addr))&Pnacl_M29_PPB_NetAddress_GetFamily,
    .DescribeAsString = (struct PP_Var (*)(PP_Resource addr, PP_Bool include_port))&Pnacl_M29_PPB_NetAddress_DescribeAsString,
    .DescribeAsIPv4Address = (PP_Bool (*)(PP_Resource addr, struct PP_NetAddress_IPv4* ipv4_addr))&Pnacl_M29_PPB_NetAddress_DescribeAsIPv4Address,
    .DescribeAsIPv6Address = (PP_Bool (*)(PP_Resource addr, struct PP_NetAddress_IPv6* ipv6_addr))&Pnacl_M29_PPB_NetAddress_DescribeAsIPv6Address
};

struct PPB_NetworkList_1_0 Pnacl_Wrappers_PPB_NetworkList_1_0 = {
    .IsNetworkList = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkList_IsNetworkList,
    .GetCount = (uint32_t (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkList_GetCount,
    .GetName = (struct PP_Var (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetName,
    .GetType = (PP_NetworkList_Type (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetType,
    .GetState = (PP_NetworkList_State (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetState,
    .GetIpAddresses = (int32_t (*)(PP_Resource resource, uint32_t index, struct PP_ArrayOutput output))&Pnacl_M31_PPB_NetworkList_GetIpAddresses,
    .GetDisplayName = (struct PP_Var (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetDisplayName,
    .GetMTU = (uint32_t (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetMTU
};

struct PPB_NetworkMonitor_1_0 Pnacl_Wrappers_PPB_NetworkMonitor_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_NetworkMonitor_Create,
    .UpdateNetworkList = (int32_t (*)(PP_Resource network_monitor, PP_Resource* network_list, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_NetworkMonitor_UpdateNetworkList,
    .IsNetworkMonitor = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkMonitor_IsNetworkMonitor
};

struct PPB_NetworkProxy_1_0 Pnacl_Wrappers_PPB_NetworkProxy_1_0 = {
    .GetProxyForURL = (int32_t (*)(PP_Instance instance, struct PP_Var url, struct PP_Var* proxy_string, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_NetworkProxy_GetProxyForURL
};

struct PPB_TCPSocket_1_0 Pnacl_Wrappers_PPB_TCPSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_TCPSocket_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_TCPSocket_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Connect,
    .GetLocalAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_GetLocalAddress,
    .GetRemoteAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_GetRemoteAddress,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Write,
    .Close = (void (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_SetOption
};

struct PPB_TCPSocket_1_1 Pnacl_Wrappers_PPB_TCPSocket_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_TCPSocket_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_TCPSocket_IsTCPSocket,
    .Bind = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Bind,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Connect,
    .GetLocalAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_GetLocalAddress,
    .GetRemoteAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_GetRemoteAddress,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Write,
    .Listen = (int32_t (*)(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Accept,
    .Close = (void (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_SetOption
};

struct PPB_TextInputController_1_0 Pnacl_Wrappers_PPB_TextInputController_1_0 = {
    .SetTextInputType = (void (*)(PP_Instance instance, PP_TextInput_Type type))&Pnacl_M30_PPB_TextInputController_SetTextInputType,
    .UpdateCaretPosition = (void (*)(PP_Instance instance, const struct PP_Rect* caret))&Pnacl_M30_PPB_TextInputController_UpdateCaretPosition,
    .CancelCompositionText = (void (*)(PP_Instance instance))&Pnacl_M30_PPB_TextInputController_CancelCompositionText,
    .UpdateSurroundingText = (void (*)(PP_Instance instance, struct PP_Var text, uint32_t caret, uint32_t anchor))&Pnacl_M30_PPB_TextInputController_UpdateSurroundingText
};

struct PPB_UDPSocket_1_0 Pnacl_Wrappers_PPB_UDPSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_UDPSocket_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_UDPSocket_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_Bind,
    .GetBoundAddress = (PP_Resource (*)(PP_Resource udp_socket))&Pnacl_M29_PPB_UDPSocket_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_RecvFrom,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M29_PPB_UDPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_SetOption
};

struct PPB_URLLoader_1_0 Pnacl_Wrappers_PPB_URLLoader_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_URLLoader_Create,
    .IsURLLoader = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLLoader_IsURLLoader,
    .Open = (int32_t (*)(PP_Resource loader, PP_Resource request_info, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_Open,
    .FollowRedirect = (int32_t (*)(PP_Resource loader, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_FollowRedirect,
    .GetUploadProgress = (PP_Bool (*)(PP_Resource loader, int64_t* bytes_sent, int64_t* total_bytes_to_be_sent))&Pnacl_M14_PPB_URLLoader_GetUploadProgress,
    .GetDownloadProgress = (PP_Bool (*)(PP_Resource loader, int64_t* bytes_received, int64_t* total_bytes_to_be_received))&Pnacl_M14_PPB_URLLoader_GetDownloadProgress,
    .GetResponseInfo = (PP_Resource (*)(PP_Resource loader))&Pnacl_M14_PPB_URLLoader_GetResponseInfo,
    .ReadResponseBody = (int32_t (*)(PP_Resource loader, void* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_ReadResponseBody,
    .FinishStreamingToFile = (int32_t (*)(PP_Resource loader, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_FinishStreamingToFile,
    .Close = (void (*)(PP_Resource loader))&Pnacl_M14_PPB_URLLoader_Close
};

struct PPB_URLRequestInfo_1_0 Pnacl_Wrappers_PPB_URLRequestInfo_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_URLRequestInfo_Create,
    .IsURLRequestInfo = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLRequestInfo_IsURLRequestInfo,
    .SetProperty = (PP_Bool (*)(PP_Resource request, PP_URLRequestProperty property, struct PP_Var value))&Pnacl_M14_PPB_URLRequestInfo_SetProperty,
    .AppendDataToBody = (PP_Bool (*)(PP_Resource request, const void* data, uint32_t len))&Pnacl_M14_PPB_URLRequestInfo_AppendDataToBody,
    .AppendFileToBody = (PP_Bool (*)(PP_Resource request, PP_Resource file_ref, int64_t start_offset, int64_t number_of_bytes, PP_Time expected_last_modified_time))&Pnacl_M14_PPB_URLRequestInfo_AppendFileToBody
};

struct PPB_URLResponseInfo_1_0 Pnacl_Wrappers_PPB_URLResponseInfo_1_0 = {
    .IsURLResponseInfo = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLResponseInfo_IsURLResponseInfo,
    .GetProperty = (struct PP_Var (*)(PP_Resource response, PP_URLResponseProperty property))&Pnacl_M14_PPB_URLResponseInfo_GetProperty,
    .GetBodyAsFileRef = (PP_Resource (*)(PP_Resource response))&Pnacl_M14_PPB_URLResponseInfo_GetBodyAsFileRef
};

struct PPB_Var_1_0 Pnacl_Wrappers_PPB_Var_1_0 = {
    .AddRef = (void (*)(struct PP_Var var))&Pnacl_M14_PPB_Var_AddRef,
    .Release = (void (*)(struct PP_Var var))&Pnacl_M14_PPB_Var_Release,
    .VarFromUtf8 = (struct PP_Var (*)(PP_Module module, const char* data, uint32_t len))&Pnacl_M14_PPB_Var_VarFromUtf8,
    .VarToUtf8 = (const char* (*)(struct PP_Var var, uint32_t* len))&Pnacl_M14_PPB_Var_VarToUtf8
};

struct PPB_Var_1_1 Pnacl_Wrappers_PPB_Var_1_1 = {
    .AddRef = (void (*)(struct PP_Var var))&Pnacl_M18_PPB_Var_AddRef,
    .Release = (void (*)(struct PP_Var var))&Pnacl_M18_PPB_Var_Release,
    .VarFromUtf8 = (struct PP_Var (*)(const char* data, uint32_t len))&Pnacl_M18_PPB_Var_VarFromUtf8,
    .VarToUtf8 = (const char* (*)(struct PP_Var var, uint32_t* len))&Pnacl_M18_PPB_Var_VarToUtf8
};

struct PPB_VarArray_1_0 Pnacl_Wrappers_PPB_VarArray_1_0 = {
    .Create = (struct PP_Var (*)(void))&Pnacl_M29_PPB_VarArray_Create,
    .Get = (struct PP_Var (*)(struct PP_Var array, uint32_t index))&Pnacl_M29_PPB_VarArray_Get,
    .Set = (PP_Bool (*)(struct PP_Var array, uint32_t index, struct PP_Var value))&Pnacl_M29_PPB_VarArray_Set,
    .GetLength = (uint32_t (*)(struct PP_Var array))&Pnacl_M29_PPB_VarArray_GetLength,
    .SetLength = (PP_Bool (*)(struct PP_Var array, uint32_t length))&Pnacl_M29_PPB_VarArray_SetLength
};

struct PPB_VarArrayBuffer_1_0 Pnacl_Wrappers_PPB_VarArrayBuffer_1_0 = {
    .Create = (struct PP_Var (*)(uint32_t size_in_bytes))&Pnacl_M18_PPB_VarArrayBuffer_Create,
    .ByteLength = (PP_Bool (*)(struct PP_Var array, uint32_t* byte_length))&Pnacl_M18_PPB_VarArrayBuffer_ByteLength,
    .Map = (void* (*)(struct PP_Var array))&Pnacl_M18_PPB_VarArrayBuffer_Map,
    .Unmap = (void (*)(struct PP_Var array))&Pnacl_M18_PPB_VarArrayBuffer_Unmap
};

struct PPB_VarDictionary_1_0 Pnacl_Wrappers_PPB_VarDictionary_1_0 = {
    .Create = (struct PP_Var (*)(void))&Pnacl_M29_PPB_VarDictionary_Create,
    .Get = (struct PP_Var (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_Get,
    .Set = (PP_Bool (*)(struct PP_Var dict, struct PP_Var key, struct PP_Var value))&Pnacl_M29_PPB_VarDictionary_Set,
    .Delete = (void (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_Delete,
    .HasKey = (PP_Bool (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_HasKey,
    .GetKeys = (struct PP_Var (*)(struct PP_Var dict))&Pnacl_M29_PPB_VarDictionary_GetKeys
};

/* Not generating wrapper interface for PPB_View_1_0 */

/* Not generating wrapper interface for PPB_View_1_1 */

struct PPB_WebSocket_1_0 Pnacl_Wrappers_PPB_WebSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M18_PPB_WebSocket_Create,
    .IsWebSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_WebSocket_IsWebSocket,
    .Connect = (int32_t (*)(PP_Resource web_socket, struct PP_Var url, const struct PP_Var protocols[], uint32_t protocol_count, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_Connect,
    .Close = (int32_t (*)(PP_Resource web_socket, uint16_t code, struct PP_Var reason, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_Close,
    .ReceiveMessage = (int32_t (*)(PP_Resource web_socket, struct PP_Var* message, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_ReceiveMessage,
    .SendMessage = (int32_t (*)(PP_Resource web_socket, struct PP_Var message))&Pnacl_M18_PPB_WebSocket_SendMessage,
    .GetBufferedAmount = (uint64_t (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetBufferedAmount,
    .GetCloseCode = (uint16_t (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseCode,
    .GetCloseReason = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseReason,
    .GetCloseWasClean = (PP_Bool (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseWasClean,
    .GetExtensions = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetExtensions,
    .GetProtocol = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetProtocol,
    .GetReadyState = (PP_WebSocketReadyState (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetReadyState,
    .GetURL = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetURL
};

/* Not generating wrapper interface for PPP_Graphics3D_1_0 */

/* Not generating wrapper interface for PPP_InputEvent_0_1 */

/* Not generating wrapper interface for PPP_Instance_1_0 */

/* Not generating wrapper interface for PPP_Instance_1_1 */

struct PPP_Messaging_1_0 Pnacl_Wrappers_PPP_Messaging_1_0 = {
    .HandleMessage = &Pnacl_M14_PPP_Messaging_HandleMessage
};

/* Not generating wrapper interface for PPP_MouseLock_1_0 */

/* Not generating wrapper interface for PPB_BrokerTrusted_0_2 */

/* Not generating wrapper interface for PPB_BrokerTrusted_0_3 */

/* Not generating wrapper interface for PPB_BrowserFont_Trusted_1_0 */

/* Not generating wrapper interface for PPB_CharSet_Trusted_1_0 */

/* Not generating wrapper interface for PPB_FileChooserTrusted_0_5 */

/* Not generating wrapper interface for PPB_FileChooserTrusted_0_6 */

/* Not generating wrapper interface for PPB_FileIOTrusted_0_4 */

/* Not generating wrapper interface for PPB_URLLoaderTrusted_0_3 */

struct PPB_AudioInput_Dev_0_2 Pnacl_Wrappers_PPB_AudioInput_Dev_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_AudioInput_Dev_Create,
    .IsAudioInput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_AudioInput_Dev_IsAudioInput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_input, PP_Resource* devices, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_AudioInput_Dev_EnumerateDevices,
    .Open = (int32_t (*)(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_2 audio_input_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_AudioInput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_input))&Pnacl_M19_PPB_AudioInput_Dev_GetCurrentConfig,
    .StartCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M19_PPB_AudioInput_Dev_StartCapture,
    .StopCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M19_PPB_AudioInput_Dev_StopCapture,
    .Close = (void (*)(PP_Resource audio_input))&Pnacl_M19_PPB_AudioInput_Dev_Close
};

struct PPB_AudioInput_Dev_0_3 Pnacl_Wrappers_PPB_AudioInput_Dev_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_AudioInput_Dev_Create,
    .IsAudioInput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M25_PPB_AudioInput_Dev_IsAudioInput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_input, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_AudioInput_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M25_PPB_AudioInput_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_2 audio_input_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_AudioInput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_GetCurrentConfig,
    .StartCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_StartCapture,
    .StopCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_StopCapture,
    .Close = (void (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_Close
};

struct PPB_AudioInput_Dev_0_4 Pnacl_Wrappers_PPB_AudioInput_Dev_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M30_PPB_AudioInput_Dev_Create,
    .IsAudioInput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M30_PPB_AudioInput_Dev_IsAudioInput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_input, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M30_PPB_AudioInput_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M30_PPB_AudioInput_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback audio_input_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M30_PPB_AudioInput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_GetCurrentConfig,
    .StartCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_StartCapture,
    .StopCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_StopCapture,
    .Close = (void (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_Close
};

/* Not generating wrapper interface for PPB_Buffer_Dev_0_4 */

/* Not generating wrapper interface for PPB_Crypto_Dev_0_1 */

/* Not generating wrapper interface for PPB_CursorControl_Dev_0_4 */

struct PPB_DeviceRef_Dev_0_1 Pnacl_Wrappers_PPB_DeviceRef_Dev_0_1 = {
    .IsDeviceRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_DeviceRef_Dev_IsDeviceRef,
    .GetType = (PP_DeviceType_Dev (*)(PP_Resource device_ref))&Pnacl_M18_PPB_DeviceRef_Dev_GetType,
    .GetName = (struct PP_Var (*)(PP_Resource device_ref))&Pnacl_M18_PPB_DeviceRef_Dev_GetName
};

struct PPB_FileChooser_Dev_0_5 Pnacl_Wrappers_PPB_FileChooser_Dev_0_5 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var accept_types))&Pnacl_M16_PPB_FileChooser_Dev_Create,
    .IsFileChooser = (PP_Bool (*)(PP_Resource resource))&Pnacl_M16_PPB_FileChooser_Dev_IsFileChooser,
    .Show = (int32_t (*)(PP_Resource chooser, struct PP_CompletionCallback callback))&Pnacl_M16_PPB_FileChooser_Dev_Show,
    .GetNextChosenFile = (PP_Resource (*)(PP_Resource chooser))&Pnacl_M16_PPB_FileChooser_Dev_GetNextChosenFile
};

struct PPB_FileChooser_Dev_0_6 Pnacl_Wrappers_PPB_FileChooser_Dev_0_6 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var accept_types))&Pnacl_M19_PPB_FileChooser_Dev_Create,
    .IsFileChooser = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_FileChooser_Dev_IsFileChooser,
    .Show = (int32_t (*)(PP_Resource chooser, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_FileChooser_Dev_Show
};

/* Not generating wrapper interface for PPB_Find_Dev_0_3 */

struct PPB_Font_Dev_0_6 Pnacl_Wrappers_PPB_Font_Dev_0_6 = {
    .GetFontFamilies = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M14_PPB_Font_Dev_GetFontFamilies,
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_FontDescription_Dev* description))&Pnacl_M14_PPB_Font_Dev_Create,
    .IsFont = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_Font_Dev_IsFont,
    .Describe = (PP_Bool (*)(PP_Resource font, struct PP_FontDescription_Dev* description, struct PP_FontMetrics_Dev* metrics))&Pnacl_M14_PPB_Font_Dev_Describe,
    .DrawTextAt = (PP_Bool (*)(PP_Resource font, PP_Resource image_data, const struct PP_TextRun_Dev* text, const struct PP_Point* position, uint32_t color, const struct PP_Rect* clip, PP_Bool image_data_is_opaque))&Pnacl_M14_PPB_Font_Dev_DrawTextAt,
    .MeasureText = (int32_t (*)(PP_Resource font, const struct PP_TextRun_Dev* text))&Pnacl_M14_PPB_Font_Dev_MeasureText,
    .CharacterOffsetForPixel = (uint32_t (*)(PP_Resource font, const struct PP_TextRun_Dev* text, int32_t pixel_position))&Pnacl_M14_PPB_Font_Dev_CharacterOffsetForPixel,
    .PixelOffsetForCharacter = (int32_t (*)(PP_Resource font, const struct PP_TextRun_Dev* text, uint32_t char_offset))&Pnacl_M14_PPB_Font_Dev_PixelOffsetForCharacter
};

/* Not generating wrapper interface for PPB_Graphics2D_Dev_0_1 */

struct PPB_IMEInputEvent_Dev_0_1 Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_1 = {
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M16_PPB_IMEInputEvent_Dev_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSelection
};

struct PPB_IMEInputEvent_Dev_0_2 Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end))&Pnacl_M21_PPB_IMEInputEvent_Dev_Create,
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M21_PPB_IMEInputEvent_Dev_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSelection
};

struct PPB_KeyboardInputEvent_Dev_0_2 Pnacl_Wrappers_PPB_KeyboardInputEvent_Dev_0_2 = {
    .SetUsbKeyCode = (PP_Bool (*)(PP_Resource key_event, uint32_t usb_key_code))&Pnacl_M31_PPB_KeyboardInputEvent_Dev_SetUsbKeyCode,
    .GetUsbKeyCode = (uint32_t (*)(PP_Resource key_event))&Pnacl_M31_PPB_KeyboardInputEvent_Dev_GetUsbKeyCode,
    .GetCode = (struct PP_Var (*)(PP_Resource key_event))&Pnacl_M31_PPB_KeyboardInputEvent_Dev_GetCode
};

/* Not generating wrapper interface for PPB_Memory_Dev_0_1 */

struct PPB_Printing_Dev_0_7 Pnacl_Wrappers_PPB_Printing_Dev_0_7 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M23_PPB_Printing_Dev_Create,
    .GetDefaultPrintSettings = (int32_t (*)(PP_Resource resource, struct PP_PrintSettings_Dev* print_settings, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_Printing_Dev_GetDefaultPrintSettings
};

/* Not generating wrapper interface for PPB_ResourceArray_Dev_0_1 */

/* Not generating wrapper interface for PPB_Scrollbar_Dev_0_5 */

/* Not generating wrapper interface for PPB_Testing_Dev_0_7 */

/* Not generating wrapper interface for PPB_Testing_Dev_0_8 */

struct PPB_Testing_Dev_0_9 Pnacl_Wrappers_PPB_Testing_Dev_0_9 = {
    .ReadImageData = (PP_Bool (*)(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left))&Pnacl_M17_PPB_Testing_Dev_ReadImageData,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M17_PPB_Testing_Dev_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M17_PPB_Testing_Dev_QuitMessageLoop,
    .GetLiveObjectsForInstance = (uint32_t (*)(PP_Instance instance))&Pnacl_M17_PPB_Testing_Dev_GetLiveObjectsForInstance,
    .IsOutOfProcess = (PP_Bool (*)(void))&Pnacl_M17_PPB_Testing_Dev_IsOutOfProcess,
    .SimulateInputEvent = (void (*)(PP_Instance instance, PP_Resource input_event))&Pnacl_M17_PPB_Testing_Dev_SimulateInputEvent,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_Testing_Dev_GetDocumentURL
};

struct PPB_Testing_Dev_0_91 Pnacl_Wrappers_PPB_Testing_Dev_0_91 = {
    .ReadImageData = (PP_Bool (*)(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left))&Pnacl_M18_PPB_Testing_Dev_ReadImageData,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M18_PPB_Testing_Dev_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M18_PPB_Testing_Dev_QuitMessageLoop,
    .GetLiveObjectsForInstance = (uint32_t (*)(PP_Instance instance))&Pnacl_M18_PPB_Testing_Dev_GetLiveObjectsForInstance,
    .IsOutOfProcess = (PP_Bool (*)(void))&Pnacl_M18_PPB_Testing_Dev_IsOutOfProcess,
    .SimulateInputEvent = (void (*)(PP_Instance instance, PP_Resource input_event))&Pnacl_M18_PPB_Testing_Dev_SimulateInputEvent,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M18_PPB_Testing_Dev_GetDocumentURL,
    .GetLiveVars = (uint32_t (*)(struct PP_Var live_vars[], uint32_t array_size))&Pnacl_M18_PPB_Testing_Dev_GetLiveVars
};

struct PPB_Testing_Dev_0_92 Pnacl_Wrappers_PPB_Testing_Dev_0_92 = {
    .ReadImageData = (PP_Bool (*)(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left))&Pnacl_M28_PPB_Testing_Dev_ReadImageData,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M28_PPB_Testing_Dev_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M28_PPB_Testing_Dev_QuitMessageLoop,
    .GetLiveObjectsForInstance = (uint32_t (*)(PP_Instance instance))&Pnacl_M28_PPB_Testing_Dev_GetLiveObjectsForInstance,
    .IsOutOfProcess = (PP_Bool (*)(void))&Pnacl_M28_PPB_Testing_Dev_IsOutOfProcess,
    .SimulateInputEvent = (void (*)(PP_Instance instance, PP_Resource input_event))&Pnacl_M28_PPB_Testing_Dev_SimulateInputEvent,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M28_PPB_Testing_Dev_GetDocumentURL,
    .GetLiveVars = (uint32_t (*)(struct PP_Var live_vars[], uint32_t array_size))&Pnacl_M28_PPB_Testing_Dev_GetLiveVars,
    .SetMinimumArrayBufferSizeForShmem = (void (*)(PP_Instance instance, uint32_t threshold))&Pnacl_M28_PPB_Testing_Dev_SetMinimumArrayBufferSizeForShmem
};

/* Not generating wrapper interface for PPB_TextInput_Dev_0_1 */

/* Not generating wrapper interface for PPB_TextInput_Dev_0_2 */

/* Not generating wrapper interface for PPB_Trace_Event_Dev_0_1 */

/* Not generating wrapper interface for PPB_Trace_Event_Dev_0_2 */

struct PPB_TrueTypeFont_Dev_0_1 Pnacl_Wrappers_PPB_TrueTypeFont_Dev_0_1 = {
    .GetFontFamilies = (int32_t (*)(PP_Instance instance, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M26_PPB_TrueTypeFont_Dev_GetFontFamilies,
    .GetFontsInFamily = (int32_t (*)(PP_Instance instance, struct PP_Var family, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M26_PPB_TrueTypeFont_Dev_GetFontsInFamily,
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_TrueTypeFontDesc_Dev* desc))&Pnacl_M26_PPB_TrueTypeFont_Dev_Create,
    .IsTrueTypeFont = (PP_Bool (*)(PP_Resource resource))&Pnacl_M26_PPB_TrueTypeFont_Dev_IsTrueTypeFont,
    .Describe = (int32_t (*)(PP_Resource font, struct PP_TrueTypeFontDesc_Dev* desc, struct PP_CompletionCallback callback))&Pnacl_M26_PPB_TrueTypeFont_Dev_Describe,
    .GetTableTags = (int32_t (*)(PP_Resource font, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M26_PPB_TrueTypeFont_Dev_GetTableTags,
    .GetTable = (int32_t (*)(PP_Resource font, uint32_t table, int32_t offset, int32_t max_data_length, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M26_PPB_TrueTypeFont_Dev_GetTable
};

struct PPB_URLUtil_Dev_0_6 Pnacl_Wrappers_PPB_URLUtil_Dev_0_6 = {
    .Canonicalize = (struct PP_Var (*)(struct PP_Var url, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_Canonicalize,
    .ResolveRelativeToURL = (struct PP_Var (*)(struct PP_Var base_url, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToURL,
    .ResolveRelativeToDocument = (struct PP_Var (*)(PP_Instance instance, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToDocument,
    .IsSameSecurityOrigin = (PP_Bool (*)(struct PP_Var url_a, struct PP_Var url_b))&Pnacl_M17_PPB_URLUtil_Dev_IsSameSecurityOrigin,
    .DocumentCanRequest = (PP_Bool (*)(PP_Instance instance, struct PP_Var url))&Pnacl_M17_PPB_URLUtil_Dev_DocumentCanRequest,
    .DocumentCanAccessDocument = (PP_Bool (*)(PP_Instance active, PP_Instance target))&Pnacl_M17_PPB_URLUtil_Dev_DocumentCanAccessDocument,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_GetDocumentURL,
    .GetPluginInstanceURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_GetPluginInstanceURL
};

struct PPB_URLUtil_Dev_0_7 Pnacl_Wrappers_PPB_URLUtil_Dev_0_7 = {
    .Canonicalize = (struct PP_Var (*)(struct PP_Var url, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_Canonicalize,
    .ResolveRelativeToURL = (struct PP_Var (*)(struct PP_Var base_url, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToURL,
    .ResolveRelativeToDocument = (struct PP_Var (*)(PP_Instance instance, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToDocument,
    .IsSameSecurityOrigin = (PP_Bool (*)(struct PP_Var url_a, struct PP_Var url_b))&Pnacl_M31_PPB_URLUtil_Dev_IsSameSecurityOrigin,
    .DocumentCanRequest = (PP_Bool (*)(PP_Instance instance, struct PP_Var url))&Pnacl_M31_PPB_URLUtil_Dev_DocumentCanRequest,
    .DocumentCanAccessDocument = (PP_Bool (*)(PP_Instance active, PP_Instance target))&Pnacl_M31_PPB_URLUtil_Dev_DocumentCanAccessDocument,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetDocumentURL,
    .GetPluginInstanceURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetPluginInstanceURL,
    .GetPluginReferrerURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetPluginReferrerURL
};

struct PPB_VideoCapture_Dev_0_2 Pnacl_Wrappers_PPB_VideoCapture_Dev_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_VideoCapture_Dev_Create,
    .IsVideoCapture = (PP_Bool (*)(PP_Resource video_capture))&Pnacl_M19_PPB_VideoCapture_Dev_IsVideoCapture,
    .EnumerateDevices = (int32_t (*)(PP_Resource video_capture, PP_Resource* devices, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_VideoCapture_Dev_EnumerateDevices,
    .Open = (int32_t (*)(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_VideoCapture_Dev_Open,
    .StartCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M19_PPB_VideoCapture_Dev_StartCapture,
    .ReuseBuffer = (int32_t (*)(PP_Resource video_capture, uint32_t buffer))&Pnacl_M19_PPB_VideoCapture_Dev_ReuseBuffer,
    .StopCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M19_PPB_VideoCapture_Dev_StopCapture,
    .Close = (void (*)(PP_Resource video_capture))&Pnacl_M19_PPB_VideoCapture_Dev_Close
};

struct PPB_VideoCapture_Dev_0_3 Pnacl_Wrappers_PPB_VideoCapture_Dev_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_VideoCapture_Dev_Create,
    .IsVideoCapture = (PP_Bool (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_IsVideoCapture,
    .EnumerateDevices = (int32_t (*)(PP_Resource video_capture, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_VideoCapture_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource video_capture, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M25_PPB_VideoCapture_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_VideoCapture_Dev_Open,
    .StartCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_StartCapture,
    .ReuseBuffer = (int32_t (*)(PP_Resource video_capture, uint32_t buffer))&Pnacl_M25_PPB_VideoCapture_Dev_ReuseBuffer,
    .StopCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_StopCapture,
    .Close = (void (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_Close
};

struct PPB_VideoDecoder_Dev_0_16 Pnacl_Wrappers_PPB_VideoDecoder_Dev_0_16 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_Resource context, PP_VideoDecoder_Profile profile))&Pnacl_M14_PPB_VideoDecoder_Dev_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_VideoDecoder_Dev_IsVideoDecoder,
    .Decode = (int32_t (*)(PP_Resource video_decoder, const struct PP_VideoBitstreamBuffer_Dev* bitstream_buffer, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Decode,
    .AssignPictureBuffers = (void (*)(PP_Resource video_decoder, uint32_t no_of_buffers, const struct PP_PictureBuffer_Dev buffers[]))&Pnacl_M14_PPB_VideoDecoder_Dev_AssignPictureBuffers,
    .ReusePictureBuffer = (void (*)(PP_Resource video_decoder, int32_t picture_buffer_id))&Pnacl_M14_PPB_VideoDecoder_Dev_ReusePictureBuffer,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Reset,
    .Destroy = (void (*)(PP_Resource video_decoder))&Pnacl_M14_PPB_VideoDecoder_Dev_Destroy
};

/* Not generating wrapper interface for PPB_View_Dev_0_1 */

/* Not generating wrapper interface for PPB_Widget_Dev_0_3 */

/* Not generating wrapper interface for PPB_Widget_Dev_0_4 */

/* Not generating wrapper interface for PPB_Zoom_Dev_0_2 */

/* Not generating wrapper interface for PPP_NetworkState_Dev_0_1 */

/* Not generating wrapper interface for PPP_Printing_Dev_0_6 */

/* Not generating wrapper interface for PPP_Scrollbar_Dev_0_2 */

/* Not generating wrapper interface for PPP_Scrollbar_Dev_0_3 */

struct PPP_Selection_Dev_0_3 Pnacl_Wrappers_PPP_Selection_Dev_0_3 = {
    .GetSelectedText = &Pnacl_M13_PPP_Selection_Dev_GetSelectedText
};

/* Not generating wrapper interface for PPP_TextInput_Dev_0_1 */

/* Not generating wrapper interface for PPP_VideoCapture_Dev_0_1 */

/* Not generating wrapper interface for PPP_VideoDecoder_Dev_0_9 */

/* Not generating wrapper interface for PPP_VideoDecoder_Dev_0_10 */

/* Not generating wrapper interface for PPP_VideoDecoder_Dev_0_11 */

/* Not generating wrapper interface for PPP_Widget_Dev_0_2 */

/* Not generating wrapper interface for PPP_Zoom_Dev_0_3 */

struct PPB_ContentDecryptor_Private_0_7 Pnacl_Wrappers_PPB_ContentDecryptor_Private_0_7 = {
    .KeyAdded = (void (*)(PP_Instance instance, struct PP_Var key_system, struct PP_Var session_id))&Pnacl_M31_PPB_ContentDecryptor_Private_KeyAdded,
    .KeyMessage = (void (*)(PP_Instance instance, struct PP_Var key_system, struct PP_Var session_id, struct PP_Var message, struct PP_Var default_url))&Pnacl_M31_PPB_ContentDecryptor_Private_KeyMessage,
    .KeyError = (void (*)(PP_Instance instance, struct PP_Var key_system, struct PP_Var session_id, int32_t media_error, int32_t system_code))&Pnacl_M31_PPB_ContentDecryptor_Private_KeyError,
    .DeliverBlock = (void (*)(PP_Instance instance, PP_Resource decrypted_block, const struct PP_DecryptedBlockInfo* decrypted_block_info))&Pnacl_M31_PPB_ContentDecryptor_Private_DeliverBlock,
    .DecoderInitializeDone = (void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id, PP_Bool success))&Pnacl_M31_PPB_ContentDecryptor_Private_DecoderInitializeDone,
    .DecoderDeinitializeDone = (void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id))&Pnacl_M31_PPB_ContentDecryptor_Private_DecoderDeinitializeDone,
    .DecoderResetDone = (void (*)(PP_Instance instance, PP_DecryptorStreamType decoder_type, uint32_t request_id))&Pnacl_M31_PPB_ContentDecryptor_Private_DecoderResetDone,
    .DeliverFrame = (void (*)(PP_Instance instance, PP_Resource decrypted_frame, const struct PP_DecryptedFrameInfo* decrypted_frame_info))&Pnacl_M31_PPB_ContentDecryptor_Private_DeliverFrame,
    .DeliverSamples = (void (*)(PP_Instance instance, PP_Resource audio_frames, const struct PP_DecryptedBlockInfo* decrypted_block_info))&Pnacl_M31_PPB_ContentDecryptor_Private_DeliverSamples
};

struct PPB_Ext_CrxFileSystem_Private_0_1 Pnacl_Wrappers_PPB_Ext_CrxFileSystem_Private_0_1 = {
    .Open = (int32_t (*)(PP_Instance instance, PP_Resource* file_system, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_CrxFileSystem_Private_Open
};

struct PPB_FileIO_Private_0_1 Pnacl_Wrappers_PPB_FileIO_Private_0_1 = {
    .RequestOSFileHandle = (int32_t (*)(PP_Resource file_io, PP_FileHandle* handle, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileIO_Private_RequestOSFileHandle
};

struct PPB_FileRefPrivate_0_1 Pnacl_Wrappers_PPB_FileRefPrivate_0_1 = {
    .GetAbsolutePath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M15_PPB_FileRefPrivate_GetAbsolutePath
};

struct PPB_Flash_12_4 Pnacl_Wrappers_PPB_Flash_12_4 = {
    .SetInstanceAlwaysOnTop = (void (*)(PP_Instance instance, PP_Bool on_top))&Pnacl_M21_PPB_Flash_SetInstanceAlwaysOnTop,
    .DrawGlyphs = (PP_Bool (*)(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]))&Pnacl_M21_PPB_Flash_DrawGlyphs,
    .GetProxyForURL = (struct PP_Var (*)(PP_Instance instance, const char* url))&Pnacl_M21_PPB_Flash_GetProxyForURL,
    .Navigate = (int32_t (*)(PP_Resource request_info, const char* target, PP_Bool from_user_action))&Pnacl_M21_PPB_Flash_Navigate,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_QuitMessageLoop,
    .GetLocalTimeZoneOffset = (double (*)(PP_Instance instance, PP_Time t))&Pnacl_M21_PPB_Flash_GetLocalTimeZoneOffset,
    .GetCommandLineArgs = (struct PP_Var (*)(PP_Module module))&Pnacl_M21_PPB_Flash_GetCommandLineArgs,
    .PreloadFontWin = (void (*)(const void* logfontw))&Pnacl_M21_PPB_Flash_PreloadFontWin,
    .IsRectTopmost = (PP_Bool (*)(PP_Instance instance, const struct PP_Rect* rect))&Pnacl_M21_PPB_Flash_IsRectTopmost,
    .InvokePrinting = (int32_t (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_InvokePrinting,
    .UpdateActivity = (void (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_UpdateActivity,
    .GetDeviceID = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_GetDeviceID,
    .GetSettingInt = (int32_t (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M21_PPB_Flash_GetSettingInt,
    .GetSetting = (struct PP_Var (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M21_PPB_Flash_GetSetting
};

struct PPB_Flash_12_5 Pnacl_Wrappers_PPB_Flash_12_5 = {
    .SetInstanceAlwaysOnTop = (void (*)(PP_Instance instance, PP_Bool on_top))&Pnacl_M22_PPB_Flash_SetInstanceAlwaysOnTop,
    .DrawGlyphs = (PP_Bool (*)(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]))&Pnacl_M22_PPB_Flash_DrawGlyphs,
    .GetProxyForURL = (struct PP_Var (*)(PP_Instance instance, const char* url))&Pnacl_M22_PPB_Flash_GetProxyForURL,
    .Navigate = (int32_t (*)(PP_Resource request_info, const char* target, PP_Bool from_user_action))&Pnacl_M22_PPB_Flash_Navigate,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M22_PPB_Flash_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M22_PPB_Flash_QuitMessageLoop,
    .GetLocalTimeZoneOffset = (double (*)(PP_Instance instance, PP_Time t))&Pnacl_M22_PPB_Flash_GetLocalTimeZoneOffset,
    .GetCommandLineArgs = (struct PP_Var (*)(PP_Module module))&Pnacl_M22_PPB_Flash_GetCommandLineArgs,
    .PreloadFontWin = (void (*)(const void* logfontw))&Pnacl_M22_PPB_Flash_PreloadFontWin,
    .IsRectTopmost = (PP_Bool (*)(PP_Instance instance, const struct PP_Rect* rect))&Pnacl_M22_PPB_Flash_IsRectTopmost,
    .InvokePrinting = (int32_t (*)(PP_Instance instance))&Pnacl_M22_PPB_Flash_InvokePrinting,
    .UpdateActivity = (void (*)(PP_Instance instance))&Pnacl_M22_PPB_Flash_UpdateActivity,
    .GetDeviceID = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M22_PPB_Flash_GetDeviceID,
    .GetSettingInt = (int32_t (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M22_PPB_Flash_GetSettingInt,
    .GetSetting = (struct PP_Var (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M22_PPB_Flash_GetSetting,
    .SetCrashData = (PP_Bool (*)(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var value))&Pnacl_M22_PPB_Flash_SetCrashData
};

struct PPB_Flash_12_6 Pnacl_Wrappers_PPB_Flash_12_6 = {
    .SetInstanceAlwaysOnTop = (void (*)(PP_Instance instance, PP_Bool on_top))&Pnacl_M24_0_PPB_Flash_SetInstanceAlwaysOnTop,
    .DrawGlyphs = (PP_Bool (*)(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]))&Pnacl_M24_0_PPB_Flash_DrawGlyphs,
    .GetProxyForURL = (struct PP_Var (*)(PP_Instance instance, const char* url))&Pnacl_M24_0_PPB_Flash_GetProxyForURL,
    .Navigate = (int32_t (*)(PP_Resource request_info, const char* target, PP_Bool from_user_action))&Pnacl_M24_0_PPB_Flash_Navigate,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M24_0_PPB_Flash_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M24_0_PPB_Flash_QuitMessageLoop,
    .GetLocalTimeZoneOffset = (double (*)(PP_Instance instance, PP_Time t))&Pnacl_M24_0_PPB_Flash_GetLocalTimeZoneOffset,
    .GetCommandLineArgs = (struct PP_Var (*)(PP_Module module))&Pnacl_M24_0_PPB_Flash_GetCommandLineArgs,
    .PreloadFontWin = (void (*)(const void* logfontw))&Pnacl_M24_0_PPB_Flash_PreloadFontWin,
    .IsRectTopmost = (PP_Bool (*)(PP_Instance instance, const struct PP_Rect* rect))&Pnacl_M24_0_PPB_Flash_IsRectTopmost,
    .InvokePrinting = (int32_t (*)(PP_Instance instance))&Pnacl_M24_0_PPB_Flash_InvokePrinting,
    .UpdateActivity = (void (*)(PP_Instance instance))&Pnacl_M24_0_PPB_Flash_UpdateActivity,
    .GetDeviceID = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M24_0_PPB_Flash_GetDeviceID,
    .GetSettingInt = (int32_t (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M24_0_PPB_Flash_GetSettingInt,
    .GetSetting = (struct PP_Var (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M24_0_PPB_Flash_GetSetting,
    .SetCrashData = (PP_Bool (*)(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var value))&Pnacl_M24_0_PPB_Flash_SetCrashData,
    .EnumerateVideoCaptureDevices = (int32_t (*)(PP_Instance instance, PP_Resource video_capture, struct PP_ArrayOutput devices))&Pnacl_M24_0_PPB_Flash_EnumerateVideoCaptureDevices
};

struct PPB_Flash_13_0 Pnacl_Wrappers_PPB_Flash_13_0 = {
    .SetInstanceAlwaysOnTop = (void (*)(PP_Instance instance, PP_Bool on_top))&Pnacl_M24_1_PPB_Flash_SetInstanceAlwaysOnTop,
    .DrawGlyphs = (PP_Bool (*)(PP_Instance instance, PP_Resource pp_image_data, const struct PP_BrowserFont_Trusted_Description* font_desc, uint32_t color, const struct PP_Point* position, const struct PP_Rect* clip, const float transformation[3][3], PP_Bool allow_subpixel_aa, uint32_t glyph_count, const uint16_t glyph_indices[], const struct PP_Point glyph_advances[]))&Pnacl_M24_1_PPB_Flash_DrawGlyphs,
    .GetProxyForURL = (struct PP_Var (*)(PP_Instance instance, const char* url))&Pnacl_M24_1_PPB_Flash_GetProxyForURL,
    .Navigate = (int32_t (*)(PP_Resource request_info, const char* target, PP_Bool from_user_action))&Pnacl_M24_1_PPB_Flash_Navigate,
    .GetLocalTimeZoneOffset = (double (*)(PP_Instance instance, PP_Time t))&Pnacl_M24_1_PPB_Flash_GetLocalTimeZoneOffset,
    .GetCommandLineArgs = (struct PP_Var (*)(PP_Module module))&Pnacl_M24_1_PPB_Flash_GetCommandLineArgs,
    .PreloadFontWin = (void (*)(const void* logfontw))&Pnacl_M24_1_PPB_Flash_PreloadFontWin,
    .IsRectTopmost = (PP_Bool (*)(PP_Instance instance, const struct PP_Rect* rect))&Pnacl_M24_1_PPB_Flash_IsRectTopmost,
    .UpdateActivity = (void (*)(PP_Instance instance))&Pnacl_M24_1_PPB_Flash_UpdateActivity,
    .GetSetting = (struct PP_Var (*)(PP_Instance instance, PP_FlashSetting setting))&Pnacl_M24_1_PPB_Flash_GetSetting,
    .SetCrashData = (PP_Bool (*)(PP_Instance instance, PP_FlashCrashKey key, struct PP_Var value))&Pnacl_M24_1_PPB_Flash_SetCrashData,
    .EnumerateVideoCaptureDevices = (int32_t (*)(PP_Instance instance, PP_Resource video_capture, struct PP_ArrayOutput devices))&Pnacl_M24_1_PPB_Flash_EnumerateVideoCaptureDevices
};

struct PPB_Flash_Clipboard_4_0 Pnacl_Wrappers_PPB_Flash_Clipboard_4_0 = {
    .IsFormatAvailable = (PP_Bool (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, PP_Flash_Clipboard_Format format))&Pnacl_M19_PPB_Flash_Clipboard_IsFormatAvailable,
    .ReadData = (struct PP_Var (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, PP_Flash_Clipboard_Format format))&Pnacl_M19_PPB_Flash_Clipboard_ReadData,
    .WriteData = (int32_t (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t data_item_count, const PP_Flash_Clipboard_Format formats[], const struct PP_Var data_items[]))&Pnacl_M19_PPB_Flash_Clipboard_WriteData
};

struct PPB_Flash_Clipboard_5_0 Pnacl_Wrappers_PPB_Flash_Clipboard_5_0 = {
    .RegisterCustomFormat = (uint32_t (*)(PP_Instance instance_id, const char* format_name))&Pnacl_M24_PPB_Flash_Clipboard_RegisterCustomFormat,
    .IsFormatAvailable = (PP_Bool (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t format))&Pnacl_M24_PPB_Flash_Clipboard_IsFormatAvailable,
    .ReadData = (struct PP_Var (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t format))&Pnacl_M24_PPB_Flash_Clipboard_ReadData,
    .WriteData = (int32_t (*)(PP_Instance instance_id, PP_Flash_Clipboard_Type clipboard_type, uint32_t data_item_count, const uint32_t formats[], const struct PP_Var data_items[]))&Pnacl_M24_PPB_Flash_Clipboard_WriteData
};

struct PPB_Flash_DeviceID_1_0 Pnacl_Wrappers_PPB_Flash_DeviceID_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M21_PPB_Flash_DeviceID_Create,
    .GetDeviceID = (int32_t (*)(PP_Resource device_id, struct PP_Var* id, struct PP_CompletionCallback callback))&Pnacl_M21_PPB_Flash_DeviceID_GetDeviceID
};

struct PPB_Flash_DRM_1_0 Pnacl_Wrappers_PPB_Flash_DRM_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_Flash_DRM_Create,
    .GetDeviceID = (int32_t (*)(PP_Resource drm, struct PP_Var* id, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Flash_DRM_GetDeviceID,
    .GetHmonitor = (PP_Bool (*)(PP_Resource drm, int64_t* hmonitor))&Pnacl_M29_PPB_Flash_DRM_GetHmonitor,
    .GetVoucherFile = (int32_t (*)(PP_Resource drm, PP_Resource* file_ref, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Flash_DRM_GetVoucherFile
};

/* Not generating wrapper interface for PPB_Flash_FontFile_0_1 */

/* Not generating wrapper interface for PPB_FlashFullscreen_0_1 */

/* Not generating wrapper interface for PPB_FlashFullscreen_1_0 */

struct PPB_Flash_Menu_0_2 Pnacl_Wrappers_PPB_Flash_Menu_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id, const struct PP_Flash_Menu* menu_data))&Pnacl_M14_PPB_Flash_Menu_Create,
    .IsFlashMenu = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M14_PPB_Flash_Menu_IsFlashMenu,
    .Show = (int32_t (*)(PP_Resource menu_id, const struct PP_Point* location, int32_t* selected_id, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_Flash_Menu_Show
};

/* Not generating wrapper interface for PPB_Flash_MessageLoop_0_1 */

/* Not generating wrapper interface for PPB_Flash_Print_1_0 */

struct PPB_HostResolver_Private_0_1 Pnacl_Wrappers_PPB_HostResolver_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_HostResolver_Private_Create,
    .IsHostResolver = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_HostResolver_Private_IsHostResolver,
    .Resolve = (int32_t (*)(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Private_Hint* hint, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_HostResolver_Private_Resolve,
    .GetCanonicalName = (struct PP_Var (*)(PP_Resource host_resolver))&Pnacl_M19_PPB_HostResolver_Private_GetCanonicalName,
    .GetSize = (uint32_t (*)(PP_Resource host_resolver))&Pnacl_M19_PPB_HostResolver_Private_GetSize,
    .GetNetAddress = (PP_Bool (*)(PP_Resource host_resolver, uint32_t index, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_HostResolver_Private_GetNetAddress
};

struct PPB_Instance_Private_0_1 Pnacl_Wrappers_PPB_Instance_Private_0_1 = {
    .GetWindowObject = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M13_PPB_Instance_Private_GetWindowObject,
    .GetOwnerElementObject = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M13_PPB_Instance_Private_GetOwnerElementObject,
    .ExecuteScript = (struct PP_Var (*)(PP_Instance instance, struct PP_Var script, struct PP_Var* exception))&Pnacl_M13_PPB_Instance_Private_ExecuteScript
};

struct PPB_NaCl_Private_1_0 Pnacl_Wrappers_PPB_NaCl_Private_1_0 = {
    .LaunchSelLdr = (PP_ExternalPluginResult (*)(PP_Instance instance, const char* alleged_url, PP_Bool uses_irt, PP_Bool uses_ppapi, PP_Bool enable_ppapi_dev, PP_Bool enable_dyncode_syscalls, PP_Bool enable_exception_handling, PP_Bool enable_crash_throttling, void* imc_handle, struct PP_Var* error_message))&Pnacl_M25_PPB_NaCl_Private_LaunchSelLdr,
    .StartPpapiProxy = (PP_ExternalPluginResult (*)(PP_Instance instance))&Pnacl_M25_PPB_NaCl_Private_StartPpapiProxy,
    .UrandomFD = (int32_t (*)(void))&Pnacl_M25_PPB_NaCl_Private_UrandomFD,
    .Are3DInterfacesDisabled = (PP_Bool (*)(void))&Pnacl_M25_PPB_NaCl_Private_Are3DInterfacesDisabled,
    .BrokerDuplicateHandle = (int32_t (*)(PP_FileHandle source_handle, uint32_t process_id, PP_FileHandle* target_handle, uint32_t desired_access, uint32_t options))&Pnacl_M25_PPB_NaCl_Private_BrokerDuplicateHandle,
    .EnsurePnaclInstalled = (int32_t (*)(PP_Instance instance, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_NaCl_Private_EnsurePnaclInstalled,
    .GetReadonlyPnaclFd = (PP_FileHandle (*)(const char* filename))&Pnacl_M25_PPB_NaCl_Private_GetReadonlyPnaclFd,
    .CreateTemporaryFile = (PP_FileHandle (*)(PP_Instance instance))&Pnacl_M25_PPB_NaCl_Private_CreateTemporaryFile,
    .GetNexeFd = (int32_t (*)(PP_Instance instance, const char* pexe_url, uint32_t abi_version, uint32_t opt_level, const char* last_modified, const char* etag, PP_Bool has_no_store_header, PP_Bool* is_hit, PP_FileHandle* nexe_handle, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_NaCl_Private_GetNexeFd,
    .ReportTranslationFinished = (void (*)(PP_Instance instance, PP_Bool success))&Pnacl_M25_PPB_NaCl_Private_ReportTranslationFinished,
    .IsOffTheRecord = (PP_Bool (*)(void))&Pnacl_M25_PPB_NaCl_Private_IsOffTheRecord,
    .IsPnaclEnabled = (PP_Bool (*)(void))&Pnacl_M25_PPB_NaCl_Private_IsPnaclEnabled,
    .ReportNaClError = (PP_ExternalPluginResult (*)(PP_Instance instance, PP_NaClError message_id))&Pnacl_M25_PPB_NaCl_Private_ReportNaClError,
    .OpenNaClExecutable = (PP_FileHandle (*)(PP_Instance instance, const char* file_url, uint64_t* file_token_lo, uint64_t* file_token_hi))&Pnacl_M25_PPB_NaCl_Private_OpenNaClExecutable
};

struct PPB_NetAddress_Private_0_1 Pnacl_Wrappers_PPB_NetAddress_Private_0_1 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M17_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M17_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M17_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M17_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M17_PPB_NetAddress_Private_GetAnyAddress
};

struct PPB_NetAddress_Private_1_0 Pnacl_Wrappers_PPB_NetAddress_Private_1_0 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_0_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_0_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M19_0_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_0_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetAnyAddress,
    .GetFamily = (PP_NetAddressFamily_Private (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetFamily,
    .GetPort = (uint16_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetPort,
    .GetAddress = (PP_Bool (*)(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size))&Pnacl_M19_0_PPB_NetAddress_Private_GetAddress
};

struct PPB_NetAddress_Private_1_1 Pnacl_Wrappers_PPB_NetAddress_Private_1_1 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_1_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_1_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M19_1_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetAnyAddress,
    .GetFamily = (PP_NetAddressFamily_Private (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetFamily,
    .GetPort = (uint16_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetPort,
    .GetAddress = (PP_Bool (*)(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size))&Pnacl_M19_1_PPB_NetAddress_Private_GetAddress,
    .GetScopeID = (uint32_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetScopeID,
    .CreateFromIPv4Address = (void (*)(const uint8_t ip[4], uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv4Address,
    .CreateFromIPv6Address = (void (*)(const uint8_t ip[16], uint32_t scope_id, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv6Address
};

struct PPB_OutputProtection_Private_0_1 Pnacl_Wrappers_PPB_OutputProtection_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_OutputProtection_Private_Create,
    .IsOutputProtection = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_OutputProtection_Private_IsOutputProtection,
    .QueryStatus = (int32_t (*)(PP_Resource resource, uint32_t* link_mask, uint32_t* protection_mask, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_OutputProtection_Private_QueryStatus,
    .EnableProtection = (int32_t (*)(PP_Resource resource, uint32_t desired_protection_mask, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_OutputProtection_Private_EnableProtection
};

struct PPB_PlatformVerification_Private_0_1 Pnacl_Wrappers_PPB_PlatformVerification_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_PlatformVerification_Private_Create,
    .IsPlatformVerification = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_PlatformVerification_Private_IsPlatformVerification,
    .CanChallengePlatform = (int32_t (*)(PP_Resource instance, PP_Bool* can_challenge_platform, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_PlatformVerification_Private_CanChallengePlatform,
    .ChallengePlatform = (int32_t (*)(PP_Resource instance, struct PP_Var service_id, struct PP_Var challenge, struct PP_Var* signed_data, struct PP_Var* signed_data_signature, struct PP_Var* platform_key_certificate, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_PlatformVerification_Private_ChallengePlatform
};

struct PPB_Talk_Private_1_0 Pnacl_Wrappers_PPB_Talk_Private_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_Talk_Private_Create,
    .GetPermission = (int32_t (*)(PP_Resource talk_resource, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_Talk_Private_GetPermission
};

struct PPB_Talk_Private_2_0 Pnacl_Wrappers_PPB_Talk_Private_2_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_Talk_Private_Create,
    .RequestPermission = (int32_t (*)(PP_Resource talk_resource, PP_TalkPermission permission, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Talk_Private_RequestPermission,
    .StartRemoting = (int32_t (*)(PP_Resource talk_resource, PP_TalkEventCallback event_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Talk_Private_StartRemoting,
    .StopRemoting = (int32_t (*)(PP_Resource talk_resource, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Talk_Private_StopRemoting
};

struct PPB_TCPServerSocket_Private_0_1 Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M18_PPB_TCPServerSocket_Private_Create,
    .IsTCPServerSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_TCPServerSocket_Private_IsTCPServerSocket,
    .Listen = (int32_t (*)(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_TCPServerSocket_Private_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_TCPServerSocket_Private_Accept,
    .StopListening = (void (*)(PP_Resource tcp_server_socket))&Pnacl_M18_PPB_TCPServerSocket_Private_StopListening
};

struct PPB_TCPServerSocket_Private_0_2 Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M28_PPB_TCPServerSocket_Private_Create,
    .IsTCPServerSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_TCPServerSocket_Private_IsTCPServerSocket,
    .Listen = (int32_t (*)(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_TCPServerSocket_Private_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_TCPServerSocket_Private_Accept,
    .GetLocalAddress = (int32_t (*)(PP_Resource tcp_server_socket, struct PP_NetAddress_Private* addr))&Pnacl_M28_PPB_TCPServerSocket_Private_GetLocalAddress,
    .StopListening = (void (*)(PP_Resource tcp_server_socket))&Pnacl_M28_PPB_TCPServerSocket_Private_StopListening
};

struct PPB_TCPSocket_Private_0_3 Pnacl_Wrappers_PPB_TCPSocket_Private_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M17_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M17_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M17_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M17_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_SSLHandshake,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M17_PPB_TCPSocket_Private_Disconnect
};

struct PPB_TCPSocket_Private_0_4 Pnacl_Wrappers_PPB_TCPSocket_Private_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M20_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M20_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M20_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M20_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_SSLHandshake,
    .GetServerCertificate = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M20_PPB_TCPSocket_Private_GetServerCertificate,
    .AddChainBuildingCertificate = (PP_Bool (*)(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted))&Pnacl_M20_PPB_TCPSocket_Private_AddChainBuildingCertificate,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M20_PPB_TCPSocket_Private_Disconnect
};

struct PPB_TCPSocket_Private_0_5 Pnacl_Wrappers_PPB_TCPSocket_Private_0_5 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M27_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M27_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M27_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M27_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_SSLHandshake,
    .GetServerCertificate = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M27_PPB_TCPSocket_Private_GetServerCertificate,
    .AddChainBuildingCertificate = (PP_Bool (*)(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted))&Pnacl_M27_PPB_TCPSocket_Private_AddChainBuildingCertificate,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M27_PPB_TCPSocket_Private_Disconnect,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocketOption_Private name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_SetOption
};

struct PPB_UDPSocket_Private_0_2 Pnacl_Wrappers_PPB_UDPSocket_Private_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M17_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M17_PPB_UDPSocket_Private_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_Bind,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M17_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M17_PPB_UDPSocket_Private_Close
};

struct PPB_UDPSocket_Private_0_3 Pnacl_Wrappers_PPB_UDPSocket_Private_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M19_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M19_PPB_UDPSocket_Private_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_Bind,
    .GetBoundAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_UDPSocket_Private_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M19_PPB_UDPSocket_Private_Close
};

struct PPB_UDPSocket_Private_0_4 Pnacl_Wrappers_PPB_UDPSocket_Private_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M23_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M23_PPB_UDPSocket_Private_IsUDPSocket,
    .SetSocketFeature = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocketFeature_Private name, struct PP_Var value))&Pnacl_M23_PPB_UDPSocket_Private_SetSocketFeature,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_Bind,
    .GetBoundAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M23_PPB_UDPSocket_Private_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M23_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M23_PPB_UDPSocket_Private_Close
};

struct PPB_UMA_Private_0_1 Pnacl_Wrappers_PPB_UMA_Private_0_1 = {
    .HistogramCustomTimes = (void (*)(struct PP_Var name, int64_t sample, int64_t min, int64_t max, uint32_t bucket_count))&Pnacl_M18_PPB_UMA_Private_HistogramCustomTimes,
    .HistogramCustomCounts = (void (*)(struct PP_Var name, int32_t sample, int32_t min, int32_t max, uint32_t bucket_count))&Pnacl_M18_PPB_UMA_Private_HistogramCustomCounts,
    .HistogramEnumeration = (void (*)(struct PP_Var name, int32_t sample, int32_t boundary_value))&Pnacl_M18_PPB_UMA_Private_HistogramEnumeration
};

struct PPB_VideoDestination_Private_0_1 Pnacl_Wrappers_PPB_VideoDestination_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M28_PPB_VideoDestination_Private_Create,
    .IsVideoDestination = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_VideoDestination_Private_IsVideoDestination,
    .Open = (int32_t (*)(PP_Resource destination, struct PP_Var stream_url, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_VideoDestination_Private_Open,
    .PutFrame = (int32_t (*)(PP_Resource destination, const struct PP_VideoFrame_Private* frame))&Pnacl_M28_PPB_VideoDestination_Private_PutFrame,
    .Close = (void (*)(PP_Resource destination))&Pnacl_M28_PPB_VideoDestination_Private_Close
};

struct PPB_VideoSource_Private_0_1 Pnacl_Wrappers_PPB_VideoSource_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M28_PPB_VideoSource_Private_Create,
    .IsVideoSource = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_VideoSource_Private_IsVideoSource,
    .Open = (int32_t (*)(PP_Resource source, struct PP_Var stream_url, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_VideoSource_Private_Open,
    .GetFrame = (int32_t (*)(PP_Resource source, struct PP_VideoFrame_Private* frame, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_VideoSource_Private_GetFrame,
    .Close = (void (*)(PP_Resource source))&Pnacl_M28_PPB_VideoSource_Private_Close
};

struct PPB_X509Certificate_Private_0_1 Pnacl_Wrappers_PPB_X509Certificate_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_X509Certificate_Private_Create,
    .IsX509CertificatePrivate = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_X509Certificate_Private_IsX509CertificatePrivate,
    .Initialize = (PP_Bool (*)(PP_Resource resource, const char* bytes, uint32_t length))&Pnacl_M19_PPB_X509Certificate_Private_Initialize,
    .GetField = (struct PP_Var (*)(PP_Resource resource, PP_X509Certificate_Private_Field field))&Pnacl_M19_PPB_X509Certificate_Private_GetField
};

struct PPP_ContentDecryptor_Private_0_7 Pnacl_Wrappers_PPP_ContentDecryptor_Private_0_7 = {
    .Initialize = &Pnacl_M31_PPP_ContentDecryptor_Private_Initialize,
    .GenerateKeyRequest = &Pnacl_M31_PPP_ContentDecryptor_Private_GenerateKeyRequest,
    .AddKey = &Pnacl_M31_PPP_ContentDecryptor_Private_AddKey,
    .CancelKeyRequest = &Pnacl_M31_PPP_ContentDecryptor_Private_CancelKeyRequest,
    .Decrypt = &Pnacl_M31_PPP_ContentDecryptor_Private_Decrypt,
    .InitializeAudioDecoder = &Pnacl_M31_PPP_ContentDecryptor_Private_InitializeAudioDecoder,
    .InitializeVideoDecoder = &Pnacl_M31_PPP_ContentDecryptor_Private_InitializeVideoDecoder,
    .DeinitializeDecoder = &Pnacl_M31_PPP_ContentDecryptor_Private_DeinitializeDecoder,
    .ResetDecoder = &Pnacl_M31_PPP_ContentDecryptor_Private_ResetDecoder,
    .DecryptAndDecode = &Pnacl_M31_PPP_ContentDecryptor_Private_DecryptAndDecode
};

/* Not generating wrapper interface for PPP_Flash_BrowserOperations_1_0 */

/* Not generating wrapper interface for PPP_Flash_BrowserOperations_1_2 */

/* Not generating wrapper interface for PPP_Flash_BrowserOperations_1_3 */

struct PPP_Instance_Private_0_1 Pnacl_Wrappers_PPP_Instance_Private_0_1 = {
    .GetInstanceObject = &Pnacl_M18_PPP_Instance_Private_GetInstanceObject
};

struct PPB_Ext_Alarms_Dev_0_1 Pnacl_Wrappers_PPB_Ext_Alarms_Dev_0_1 = {
    .Create = (void (*)(PP_Instance instance, struct PP_Var name, PP_Ext_Alarms_AlarmCreateInfo_Dev alarm_info))&Pnacl_M27_PPB_Ext_Alarms_Dev_Create,
    .Get = (int32_t (*)(PP_Instance instance, struct PP_Var name, PP_Ext_Alarms_Alarm_Dev* alarm, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_Ext_Alarms_Dev_Get,
    .GetAll = (int32_t (*)(PP_Instance instance, PP_Ext_Alarms_Alarm_Dev_Array* alarms, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_Ext_Alarms_Dev_GetAll,
    .Clear = (void (*)(PP_Instance instance, struct PP_Var name))&Pnacl_M27_PPB_Ext_Alarms_Dev_Clear,
    .ClearAll = (void (*)(PP_Instance instance))&Pnacl_M27_PPB_Ext_Alarms_Dev_ClearAll
};

struct PPB_Ext_Events_Dev_0_1 Pnacl_Wrappers_PPB_Ext_Events_Dev_0_1 = {
    .AddListener = (uint32_t (*)(PP_Instance instance, struct PP_Ext_EventListener listener))&Pnacl_M27_PPB_Ext_Events_Dev_AddListener,
    .RemoveListener = (void (*)(PP_Instance instance, uint32_t listener_id))&Pnacl_M27_PPB_Ext_Events_Dev_RemoveListener
};

struct PPB_Ext_Socket_Dev_0_1 Pnacl_Wrappers_PPB_Ext_Socket_Dev_0_1 = {
    .Create = (int32_t (*)(PP_Instance instance, PP_Ext_Socket_SocketType_Dev type, PP_Ext_Socket_CreateOptions_Dev options, PP_Ext_Socket_CreateInfo_Dev* create_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Create,
    .Destroy = (void (*)(PP_Instance instance, struct PP_Var socket_id))&Pnacl_M28_PPB_Ext_Socket_Dev_Destroy,
    .Connect = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var hostname, struct PP_Var port, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Connect,
    .Bind = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var port, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Bind,
    .Disconnect = (void (*)(PP_Instance instance, struct PP_Var socket_id))&Pnacl_M28_PPB_Ext_Socket_Dev_Disconnect,
    .Read = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var buffer_size, PP_Ext_Socket_ReadInfo_Dev* read_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Read,
    .Write = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var data, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Write,
    .RecvFrom = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var buffer_size, PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_RecvFrom,
    .SendTo = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var data, struct PP_Var address, struct PP_Var port, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_SendTo,
    .Listen = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var port, struct PP_Var backlog, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Listen,
    .Accept = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, PP_Ext_Socket_AcceptInfo_Dev* accept_info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_Accept,
    .SetKeepAlive = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var enable, struct PP_Var delay, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_SetKeepAlive,
    .SetNoDelay = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var no_delay, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_SetNoDelay,
    .GetInfo = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, PP_Ext_Socket_SocketInfo_Dev* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_GetInfo,
    .GetNetworkList = (int32_t (*)(PP_Instance instance, PP_Ext_Socket_NetworkInterface_Dev_Array* result, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_Socket_Dev_GetNetworkList
};

struct PPB_Ext_Socket_Dev_0_2 Pnacl_Wrappers_PPB_Ext_Socket_Dev_0_2 = {
    .Create = (int32_t (*)(PP_Instance instance, PP_Ext_Socket_SocketType_Dev type, PP_Ext_Socket_CreateOptions_Dev options, PP_Ext_Socket_CreateInfo_Dev* create_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Create,
    .Destroy = (void (*)(PP_Instance instance, struct PP_Var socket_id))&Pnacl_M29_PPB_Ext_Socket_Dev_Destroy,
    .Connect = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var hostname, struct PP_Var port, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Connect,
    .Bind = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var port, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Bind,
    .Disconnect = (void (*)(PP_Instance instance, struct PP_Var socket_id))&Pnacl_M29_PPB_Ext_Socket_Dev_Disconnect,
    .Read = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var buffer_size, PP_Ext_Socket_ReadInfo_Dev* read_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Read,
    .Write = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var data, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Write,
    .RecvFrom = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var buffer_size, PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_RecvFrom,
    .SendTo = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var data, struct PP_Var address, struct PP_Var port, PP_Ext_Socket_WriteInfo_Dev* write_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_SendTo,
    .Listen = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var port, struct PP_Var backlog, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Listen,
    .Accept = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, PP_Ext_Socket_AcceptInfo_Dev* accept_info, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_Accept,
    .SetKeepAlive = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var enable, struct PP_Var delay, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_SetKeepAlive,
    .SetNoDelay = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var no_delay, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_SetNoDelay,
    .GetInfo = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, PP_Ext_Socket_SocketInfo_Dev* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_GetInfo,
    .GetNetworkList = (int32_t (*)(PP_Instance instance, PP_Ext_Socket_NetworkInterface_Dev_Array* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_GetNetworkList,
    .JoinGroup = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_JoinGroup,
    .LeaveGroup = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var address, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_LeaveGroup,
    .SetMulticastTimeToLive = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var ttl, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_SetMulticastTimeToLive,
    .SetMulticastLoopbackMode = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var enabled, struct PP_Var* result, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_SetMulticastLoopbackMode,
    .GetJoinedGroups = (int32_t (*)(PP_Instance instance, struct PP_Var socket_id, struct PP_Var* groups, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_Ext_Socket_Dev_GetJoinedGroups
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Console_1_0 = {
  .iface_macro = PPB_CONSOLE_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Console_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Core_1_0 = {
  .iface_macro = PPB_CORE_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Core_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_0 = {
  .iface_macro = PPB_FILEIO_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileIO_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_1 = {
  .iface_macro = PPB_FILEIO_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileIO_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_0 = {
  .iface_macro = PPB_FILEREF_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileRef_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_1 = {
  .iface_macro = PPB_FILEREF_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileRef_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileSystem_1_0 = {
  .iface_macro = PPB_FILESYSTEM_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileSystem_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_0 = {
  .iface_macro = PPB_GRAPHICS_2D_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Graphics2D_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_1 = {
  .iface_macro = PPB_GRAPHICS_2D_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Graphics2D_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics3D_1_0 = {
  .iface_macro = PPB_GRAPHICS_3D_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Graphics3D_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_1_0 = {
  .iface_macro = PPB_HOSTRESOLVER_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_HostResolver_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0 = {
  .iface_macro = PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_MouseInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1 = {
  .iface_macro = PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_MouseInputEvent_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0 = {
  .iface_macro = PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_WheelInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0 = {
  .iface_macro = PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_KeyboardInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0 = {
  .iface_macro = PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TouchInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0 = {
  .iface_macro = PPB_IME_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_IMEInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MessageLoop_1_0 = {
  .iface_macro = PPB_MESSAGELOOP_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_MessageLoop_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_0 = {
  .iface_macro = PPB_MESSAGING_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Messaging_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseLock_1_0 = {
  .iface_macro = PPB_MOUSELOCK_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_MouseLock_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_1_0 = {
  .iface_macro = PPB_NETADDRESS_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetAddress_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkList_1_0 = {
  .iface_macro = PPB_NETWORKLIST_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetworkList_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0 = {
  .iface_macro = PPB_NETWORKMONITOR_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetworkMonitor_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkProxy_1_0 = {
  .iface_macro = PPB_NETWORKPROXY_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetworkProxy_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_0 = {
  .iface_macro = PPB_TCPSOCKET_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_1 = {
  .iface_macro = PPB_TCPSOCKET_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPSocket_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TextInputController_1_0 = {
  .iface_macro = PPB_TEXTINPUTCONTROLLER_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TextInputController_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_0 = {
  .iface_macro = PPB_UDPSOCKET_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_UDPSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLLoader_1_0 = {
  .iface_macro = PPB_URLLOADER_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_URLLoader_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0 = {
  .iface_macro = PPB_URLREQUESTINFO_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_URLRequestInfo_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0 = {
  .iface_macro = PPB_URLRESPONSEINFO_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_URLResponseInfo_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_0 = {
  .iface_macro = PPB_VAR_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Var_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_1 = {
  .iface_macro = PPB_VAR_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Var_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArray_1_0 = {
  .iface_macro = PPB_VAR_ARRAY_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VarArray_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0 = {
  .iface_macro = PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VarArrayBuffer_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarDictionary_1_0 = {
  .iface_macro = PPB_VAR_DICTIONARY_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VarDictionary_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WebSocket_1_0 = {
  .iface_macro = PPB_WEBSOCKET_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_WebSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Messaging_1_0 = {
  .iface_macro = PPP_MESSAGING_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPP_Messaging_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2 = {
  .iface_macro = PPB_AUDIO_INPUT_DEV_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_AudioInput_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3 = {
  .iface_macro = PPB_AUDIO_INPUT_DEV_INTERFACE_0_3,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_AudioInput_Dev_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4 = {
  .iface_macro = PPB_AUDIO_INPUT_DEV_INTERFACE_0_4,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_AudioInput_Dev_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1 = {
  .iface_macro = PPB_DEVICEREF_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_DeviceRef_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5 = {
  .iface_macro = PPB_FILECHOOSER_DEV_INTERFACE_0_5,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileChooser_Dev_0_5,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6 = {
  .iface_macro = PPB_FILECHOOSER_DEV_INTERFACE_0_6,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileChooser_Dev_0_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Font_Dev_0_6 = {
  .iface_macro = PPB_FONT_DEV_INTERFACE_0_6,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Font_Dev_0_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1 = {
  .iface_macro = PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2 = {
  .iface_macro = PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2 = {
  .iface_macro = PPB_KEYBOARD_INPUT_EVENT_DEV_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_KeyboardInputEvent_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Printing_Dev_0_7 = {
  .iface_macro = PPB_PRINTING_DEV_INTERFACE_0_7,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Printing_Dev_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_9 = {
  .iface_macro = PPB_TESTING_DEV_INTERFACE_0_9,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Testing_Dev_0_9,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_91 = {
  .iface_macro = PPB_TESTING_DEV_INTERFACE_0_91,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Testing_Dev_0_91,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Dev_0_92 = {
  .iface_macro = PPB_TESTING_DEV_INTERFACE_0_92,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Testing_Dev_0_92,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1 = {
  .iface_macro = PPB_TRUETYPEFONT_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TrueTypeFont_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6 = {
  .iface_macro = PPB_URLUTIL_DEV_INTERFACE_0_6,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_URLUtil_Dev_0_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7 = {
  .iface_macro = PPB_URLUTIL_DEV_INTERFACE_0_7,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_URLUtil_Dev_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2 = {
  .iface_macro = PPB_VIDEOCAPTURE_DEV_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VideoCapture_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3 = {
  .iface_macro = PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VideoCapture_Dev_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16 = {
  .iface_macro = PPB_VIDEODECODER_DEV_INTERFACE_0_16,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VideoDecoder_Dev_0_16,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Selection_Dev_0_3 = {
  .iface_macro = PPP_SELECTION_DEV_INTERFACE_0_3,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPP_Selection_Dev_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7 = {
  .iface_macro = PPB_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_7,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_ContentDecryptor_Private_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1 = {
  .iface_macro = PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Ext_CrxFileSystem_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_Private_0_1 = {
  .iface_macro = PPB_FILEIO_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileIO_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1 = {
  .iface_macro = PPB_FILEREFPRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_FileRefPrivate_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_4 = {
  .iface_macro = PPB_FLASH_INTERFACE_12_4,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_12_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_5 = {
  .iface_macro = PPB_FLASH_INTERFACE_12_5,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_12_5,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_12_6 = {
  .iface_macro = PPB_FLASH_INTERFACE_12_6,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_12_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_13_0 = {
  .iface_macro = PPB_FLASH_INTERFACE_13_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_13_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0 = {
  .iface_macro = PPB_FLASH_CLIPBOARD_INTERFACE_4_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_Clipboard_4_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0 = {
  .iface_macro = PPB_FLASH_CLIPBOARD_INTERFACE_5_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_Clipboard_5_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_DeviceID_1_0 = {
  .iface_macro = PPB_FLASH_DEVICEID_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_DeviceID_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_DRM_1_0 = {
  .iface_macro = PPB_FLASH_DRM_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_DRM_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Flash_Menu_0_2 = {
  .iface_macro = PPB_FLASH_MENU_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Flash_Menu_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1 = {
  .iface_macro = PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_HostResolver_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Instance_Private_0_1 = {
  .iface_macro = PPB_INSTANCE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Instance_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NaCl_Private_1_0 = {
  .iface_macro = PPB_NACL_PRIVATE_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NaCl_Private_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetAddress_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetAddress_Private_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_1_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_NetAddress_Private_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1 = {
  .iface_macro = PPB_OUTPUTPROTECTION_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_OutputProtection_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1 = {
  .iface_macro = PPB_PLATFORMVERIFICATION_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_PlatformVerification_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Talk_Private_1_0 = {
  .iface_macro = PPB_TALK_PRIVATE_INTERFACE_1_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Talk_Private_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Talk_Private_2_0 = {
  .iface_macro = PPB_TALK_PRIVATE_INTERFACE_2_0,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Talk_Private_2_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1 = {
  .iface_macro = PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2 = {
  .iface_macro = PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_5,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_5,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UMA_Private_0_1 = {
  .iface_macro = PPB_UMA_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_UMA_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1 = {
  .iface_macro = PPB_VIDEODESTINATION_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VideoDestination_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1 = {
  .iface_macro = PPB_VIDEOSOURCE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_VideoSource_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1 = {
  .iface_macro = PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_X509Certificate_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7 = {
  .iface_macro = PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_7,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPP_ContentDecryptor_Private_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Instance_Private_0_1 = {
  .iface_macro = PPP_INSTANCE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPP_Instance_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1 = {
  .iface_macro = PPB_EXT_ALARMS_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Ext_Alarms_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Events_Dev_0_1 = {
  .iface_macro = PPB_EXT_EVENTS_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Ext_Events_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1 = {
  .iface_macro = PPB_EXT_SOCKET_DEV_INTERFACE_0_1,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Ext_Socket_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2 = {
  .iface_macro = PPB_EXT_SOCKET_DEV_INTERFACE_0_2,
  .wrapped_iface = (void *) &Pnacl_Wrappers_PPB_Ext_Socket_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo *s_ppb_wrappers[] = {
  &Pnacl_WrapperInfo_PPB_Console_1_0,
  &Pnacl_WrapperInfo_PPB_Core_1_0,
  &Pnacl_WrapperInfo_PPB_FileIO_1_0,
  &Pnacl_WrapperInfo_PPB_FileIO_1_1,
  &Pnacl_WrapperInfo_PPB_FileRef_1_0,
  &Pnacl_WrapperInfo_PPB_FileRef_1_1,
  &Pnacl_WrapperInfo_PPB_FileSystem_1_0,
  &Pnacl_WrapperInfo_PPB_Graphics2D_1_0,
  &Pnacl_WrapperInfo_PPB_Graphics2D_1_1,
  &Pnacl_WrapperInfo_PPB_Graphics3D_1_0,
  &Pnacl_WrapperInfo_PPB_HostResolver_1_0,
  &Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1,
  &Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_MessageLoop_1_0,
  &Pnacl_WrapperInfo_PPB_Messaging_1_0,
  &Pnacl_WrapperInfo_PPB_MouseLock_1_0,
  &Pnacl_WrapperInfo_PPB_NetAddress_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkList_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkProxy_1_0,
  &Pnacl_WrapperInfo_PPB_TCPSocket_1_0,
  &Pnacl_WrapperInfo_PPB_TCPSocket_1_1,
  &Pnacl_WrapperInfo_PPB_TextInputController_1_0,
  &Pnacl_WrapperInfo_PPB_UDPSocket_1_0,
  &Pnacl_WrapperInfo_PPB_URLLoader_1_0,
  &Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0,
  &Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0,
  &Pnacl_WrapperInfo_PPB_Var_1_0,
  &Pnacl_WrapperInfo_PPB_Var_1_1,
  &Pnacl_WrapperInfo_PPB_VarArray_1_0,
  &Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0,
  &Pnacl_WrapperInfo_PPB_VarDictionary_1_0,
  &Pnacl_WrapperInfo_PPB_WebSocket_1_0,
  &Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_2,
  &Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3,
  &Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4,
  &Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5,
  &Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6,
  &Pnacl_WrapperInfo_PPB_Font_Dev_0_6,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2,
  &Pnacl_WrapperInfo_PPB_KeyboardInputEvent_Dev_0_2,
  &Pnacl_WrapperInfo_PPB_Printing_Dev_0_7,
  &Pnacl_WrapperInfo_PPB_Testing_Dev_0_9,
  &Pnacl_WrapperInfo_PPB_Testing_Dev_0_91,
  &Pnacl_WrapperInfo_PPB_Testing_Dev_0_92,
  &Pnacl_WrapperInfo_PPB_TrueTypeFont_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6,
  &Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7,
  &Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_2,
  &Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16,
  &Pnacl_WrapperInfo_PPB_ContentDecryptor_Private_0_7,
  &Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1,
  &Pnacl_WrapperInfo_PPB_FileIO_Private_0_1,
  &Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1,
  &Pnacl_WrapperInfo_PPB_Flash_12_4,
  &Pnacl_WrapperInfo_PPB_Flash_12_5,
  &Pnacl_WrapperInfo_PPB_Flash_12_6,
  &Pnacl_WrapperInfo_PPB_Flash_13_0,
  &Pnacl_WrapperInfo_PPB_Flash_Clipboard_4_0,
  &Pnacl_WrapperInfo_PPB_Flash_Clipboard_5_0,
  &Pnacl_WrapperInfo_PPB_Flash_DeviceID_1_0,
  &Pnacl_WrapperInfo_PPB_Flash_DRM_1_0,
  &Pnacl_WrapperInfo_PPB_Flash_Menu_0_2,
  &Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1,
  &Pnacl_WrapperInfo_PPB_Instance_Private_0_1,
  &Pnacl_WrapperInfo_PPB_NaCl_Private_1_0,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1,
  &Pnacl_WrapperInfo_PPB_OutputProtection_Private_0_1,
  &Pnacl_WrapperInfo_PPB_PlatformVerification_Private_0_1,
  &Pnacl_WrapperInfo_PPB_Talk_Private_1_0,
  &Pnacl_WrapperInfo_PPB_Talk_Private_2_0,
  &Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1,
  &Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4,
  &Pnacl_WrapperInfo_PPB_UMA_Private_0_1,
  &Pnacl_WrapperInfo_PPB_VideoDestination_Private_0_1,
  &Pnacl_WrapperInfo_PPB_VideoSource_Private_0_1,
  &Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1,
  &Pnacl_WrapperInfo_PPB_Ext_Alarms_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_Ext_Events_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_Ext_Socket_Dev_0_2,
  NULL
};

static struct __PnaclWrapperInfo *s_ppp_wrappers[] = {
  &Pnacl_WrapperInfo_PPP_Messaging_1_0,
  &Pnacl_WrapperInfo_PPP_Selection_Dev_0_3,
  &Pnacl_WrapperInfo_PPP_ContentDecryptor_Private_0_7,
  &Pnacl_WrapperInfo_PPP_Instance_Private_0_1,
  NULL
};



static PPB_GetInterface __real_PPBGetInterface;
static PPP_GetInterface_Type __real_PPPGetInterface;

void __set_real_Pnacl_PPBGetInterface(PPB_GetInterface real) {
  __real_PPBGetInterface = real;
}

void __set_real_Pnacl_PPPGetInterface(PPP_GetInterface_Type real) {
  __real_PPPGetInterface = real;
}

/* Map interface string -> wrapper metadata */
static struct __PnaclWrapperInfo *PnaclPPBShimIface(
    const char *name) {
  struct __PnaclWrapperInfo **next = s_ppb_wrappers;
  while (*next != NULL) {
    if (mystrcmp(name, (*next)->iface_macro) == 0) return *next;
    ++next;
  }
  return NULL;
}

/* Map interface string -> wrapper metadata */
static struct __PnaclWrapperInfo *PnaclPPPShimIface(
    const char *name) {
  struct __PnaclWrapperInfo **next = s_ppp_wrappers;
  while (*next != NULL) {
    if (mystrcmp(name, (*next)->iface_macro) == 0) return *next;
    ++next;
  }
  return NULL;
}

const void *__Pnacl_PPBGetInterface(const char *name) {
  struct __PnaclWrapperInfo *wrapper = PnaclPPBShimIface(name);
  if (wrapper == NULL) {
    /* We did not generate a wrapper for this, so return the real interface. */
    return (*__real_PPBGetInterface)(name);
  }

  /* Initialize the real_iface if it hasn't been. The wrapper depends on it. */
  if (wrapper->real_iface == NULL) {
    const void *iface = (*__real_PPBGetInterface)(name);
    if (NULL == iface) return NULL;
    wrapper->real_iface = iface;
  }

  if (wrapper->wrapped_iface) {
    return wrapper->wrapped_iface;
  } else {
    return wrapper->real_iface;
  }
}

const void *__Pnacl_PPPGetInterface(const char *name) {
  struct __PnaclWrapperInfo *wrapper = PnaclPPPShimIface(name);
  if (wrapper == NULL) {
    /* We did not generate a wrapper for this, so return the real interface. */
    return (*__real_PPPGetInterface)(name);
  }

  /* Initialize the real_iface if it hasn't been. The wrapper depends on it. */
  if (wrapper->real_iface == NULL) {
    const void *iface = (*__real_PPPGetInterface)(name);
    if (NULL == iface) return NULL;
    wrapper->real_iface = iface;
  }

  if (wrapper->wrapped_iface) {
    return wrapper->wrapped_iface;
  } else {
    return wrapper->real_iface;
  }
}
