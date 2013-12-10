// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_creation_impl.h"

#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/ppb_audio_impl.h"
#include "content/renderer/pepper/ppb_broker_impl.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "content/renderer/pepper/ppb_flash_message_loop_impl.h"
#include "content/renderer/pepper/ppb_graphics_3d_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/ppb_scrollbar_impl.h"
#include "content/renderer/pepper/ppb_video_decoder_impl.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_resource_array_shared.h"
#include "ppapi/shared_impl/var.h"

using ppapi::InputEventData;
using ppapi::PPB_InputEvent_Shared;
using ppapi::PPB_ResourceArray_Shared;
using ppapi::StringVar;

namespace content {

ResourceCreationImpl::ResourceCreationImpl(PepperPluginInstanceImpl* instance) {
}

ResourceCreationImpl::~ResourceCreationImpl() {
}

PP_Resource ResourceCreationImpl::CreateAudio1_0(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback_1_0 audio_callback,
    void* user_data) {
  return PPB_Audio_Impl::Create(
      instance, config_id, ppapi::AudioCallbackCombined(audio_callback),
      user_data);
}

PP_Resource ResourceCreationImpl::CreateAudio(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  return PPB_Audio_Impl::Create(
      instance, config_id, ppapi::AudioCallbackCombined(audio_callback),
      user_data);
}

PP_Resource ResourceCreationImpl::CreateAudioConfig(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  return ppapi::PPB_AudioConfig_Shared::Create(
      ppapi::OBJECT_IS_IMPL, instance, sample_rate, sample_frame_count);
}

PP_Resource ResourceCreationImpl::CreateAudioTrusted(
    PP_Instance instance) {
  return (new PPB_Audio_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateAudioInput(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateBroker(PP_Instance instance) {
  return (new PPB_Broker_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateBuffer(PP_Instance instance,
                                               uint32_t size) {
  return PPB_Buffer_Impl::Create(instance, size);
}

PP_Resource ResourceCreationImpl::CreateFlashDRM(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateFlashFontFile(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateFlashMenu(
    PP_Instance instance,
    const PP_Flash_Menu* menu_data) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateFlashMessageLoop(PP_Instance instance) {
  return PPB_Flash_MessageLoop_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateGraphics3D(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Impl::Create(instance, share_context, attrib_list);
}

PP_Resource ResourceCreationImpl::CreateGraphics3DRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Impl::CreateRaw(instance, share_context, attrib_list);
}

PP_Resource ResourceCreationImpl::CreateHostResolver(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateHostResolverPrivate(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateImageData(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  return PPB_ImageData_Impl::Create(instance,
                                    ppapi::PPB_ImageData_Shared::PLATFORM,
                                    format, *size, init_to_zero);
}

PP_Resource ResourceCreationImpl::CreateImageDataSimple(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  return PPB_ImageData_Impl::Create(instance,
                                    ppapi::PPB_ImageData_Shared::SIMPLE,
                                    format, *size, init_to_zero);
}

PP_Resource ResourceCreationImpl::CreateIMEInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    struct PP_Var text,
    uint32_t segment_number,
    const uint32_t* segment_offsets,
    int32_t target_segment,
    uint32_t selection_start,
    uint32_t selection_end) {
  return PPB_InputEvent_Shared::CreateIMEInputEvent(
      ppapi::OBJECT_IS_IMPL, instance, type, time_stamp, text, segment_number,
      segment_offsets, target_segment, selection_start, selection_end);
}

PP_Resource ResourceCreationImpl::CreateKeyboardInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text) {
  return PPB_InputEvent_Shared::CreateKeyboardInputEvent(
      ppapi::OBJECT_IS_IMPL, instance, type, time_stamp, modifiers, key_code,
      character_text);
}

PP_Resource ResourceCreationImpl::CreateMouseInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    PP_InputEvent_MouseButton mouse_button,
    const PP_Point* mouse_position,
    int32_t click_count,
    const PP_Point* mouse_movement) {
  return PPB_InputEvent_Shared::CreateMouseInputEvent(
      ppapi::OBJECT_IS_IMPL, instance, type, time_stamp, modifiers,
      mouse_button, mouse_position, click_count, mouse_movement);
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromIPv4Address(
    PP_Instance instance,
    const PP_NetAddress_IPv4* ipv4_addr) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromIPv6Address(
    PP_Instance instance,
    const PP_NetAddress_IPv6* ipv6_addr) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromNetAddressPrivate(
    PP_Instance instance,
    const PP_NetAddress_Private& private_addr) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateTouchInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers) {
  return PPB_InputEvent_Shared::CreateTouchInputEvent(
      ppapi::OBJECT_IS_IMPL, instance, type, time_stamp, modifiers);
}

PP_Resource ResourceCreationImpl::CreateNetworkMonitor(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreatePlatformVerificationPrivate(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateScrollbar(PP_Instance instance,
                                                  PP_Bool vertical) {
  return PPB_Scrollbar_Impl::Create(instance, PP_ToBool(vertical));
}

PP_Resource ResourceCreationImpl::CreateTalk(PP_Instance /* instance */) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateResourceArray(
    PP_Instance instance,
    const PP_Resource elements[],
    uint32_t size) {
  PPB_ResourceArray_Shared* object = new PPB_ResourceArray_Shared(
      ppapi::OBJECT_IS_IMPL, instance, elements, size);
  return object->GetReference();
}

PP_Resource ResourceCreationImpl::CreateTCPServerSocketPrivate(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateTCPSocket1_0(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateTCPSocket(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateTCPSocketPrivate(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateUDPSocket(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateUDPSocketPrivate(PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateVideoCapture(PP_Instance instance) {
  return 0;  // VideoCapture is not supported in process now.
}

PP_Resource ResourceCreationImpl::CreateVideoDecoder(
    PP_Instance instance,
    PP_Resource graphics3d_id,
    PP_VideoDecoder_Profile profile) {
  return PPB_VideoDecoder_Impl::Create(instance, graphics3d_id, profile);
}

PP_Resource ResourceCreationImpl::CreateVideoDestination(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateVideoSource(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

PP_Resource ResourceCreationImpl::CreateWheelInputEvent(
    PP_Instance instance,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    const PP_FloatPoint* wheel_delta,
    const PP_FloatPoint* wheel_ticks,
    PP_Bool scroll_by_page) {
  return PPB_InputEvent_Shared::CreateWheelInputEvent(
      ppapi::OBJECT_IS_IMPL, instance, time_stamp, modifiers,
      wheel_delta, wheel_ticks, scroll_by_page);
}

PP_Resource ResourceCreationImpl::CreateX509CertificatePrivate(
    PP_Instance instance) {
  return 0;  // Not supported in-process.
}

}  // namespace content
