// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_CREATION_IMPL_H_
#define CONTENT_RENDERER_PEPPER_RESOURCE_CREATION_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace content {

class PepperPluginInstanceImpl;

// This is an abstract class.  ResourceCreationAPI functions that implement
// "old-style" resources are handled here. See
// content/renderer/pepper/pepper_in_process_resource_creation.h for functions
// that implement "new-style" resources.
class ResourceCreationImpl : public ppapi::thunk::ResourceCreationAPI {
 public:
  explicit ResourceCreationImpl(PepperPluginInstanceImpl* instance);
  virtual ~ResourceCreationImpl();

  // ResourceCreationAPI implementation.
  virtual PP_Resource CreateAudio1_0(PP_Instance instance,
                                     PP_Resource config_id,
                                     PPB_Audio_Callback_1_0 audio_callback,
                                     void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) OVERRIDE;
  virtual PP_Resource CreateAudioInput(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateBroker(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateBuffer(PP_Instance instance,
                                   uint32_t size) OVERRIDE;
  virtual PP_Resource CreateFlashDRM(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateFlashFontFile(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset) OVERRIDE;
  virtual PP_Resource CreateFlashMenu(PP_Instance instance,
                                      const PP_Flash_Menu* menu_data) OVERRIDE;
  virtual PP_Resource CreateFlashMessageLoop(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateGraphics3D(PP_Instance instance,
                                       PP_Resource share_context,
                                       const int32_t* attrib_list) OVERRIDE;
  virtual PP_Resource CreateGraphics3DRaw(PP_Instance instance,
                                          PP_Resource share_context,
                                          const int32_t* attrib_list) OVERRIDE;
  virtual PP_Resource CreateHostResolver(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateHostResolverPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size* size,
                                      PP_Bool init_to_zero) OVERRIDE;
  virtual PP_Resource CreateImageDataSimple(PP_Instance instance,
                                            PP_ImageDataFormat format,
                                            const PP_Size* size,
                                            PP_Bool init_to_zero) OVERRIDE;
  virtual PP_Resource CreateIMEInputEvent(PP_Instance instance,
                                          PP_InputEvent_Type type,
                                          PP_TimeTicks time_stamp,
                                          struct PP_Var text,
                                          uint32_t segment_number,
                                          const uint32_t* segment_offsets,
                                          int32_t target_segment,
                                          uint32_t selection_start,
                                          uint32_t selection_end) OVERRIDE;
  virtual PP_Resource CreateKeyboardInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      PP_Var character_text) OVERRIDE;
  virtual PP_Resource CreateMouseInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement) OVERRIDE;
  virtual PP_Resource CreateNetAddressFromIPv4Address(
      PP_Instance instance,
      const PP_NetAddress_IPv4* ipv4_addr) OVERRIDE;
  virtual PP_Resource CreateNetAddressFromIPv6Address(
      PP_Instance instance,
      const PP_NetAddress_IPv6* ipv6_addr) OVERRIDE;
  virtual PP_Resource CreateNetAddressFromNetAddressPrivate(
      PP_Instance instance,
      const PP_NetAddress_Private& private_addr) OVERRIDE;
  virtual PP_Resource CreateTouchInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers) OVERRIDE;
  virtual PP_Resource CreateNetworkMonitor(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreatePlatformVerificationPrivate(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateResourceArray(PP_Instance instance,
                                          const PP_Resource elements[],
                                          uint32_t size) OVERRIDE;
  virtual PP_Resource CreateScrollbar(PP_Instance instance,
                                      PP_Bool vertical) OVERRIDE;
  virtual PP_Resource CreateOutputProtectionPrivate(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTalk(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTCPServerSocketPrivate(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTCPSocket1_0(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTCPSocket(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTCPSocketPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateUDPSocket(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateUDPSocketPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateVideoCapture(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateVideoDecoder(
      PP_Instance instance,
      PP_Resource graphics3d_id,
      PP_VideoDecoder_Profile profile) OVERRIDE;
  virtual PP_Resource CreateVideoDestination(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateVideoSource(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateWheelInputEvent(
      PP_Instance instance,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      const PP_FloatPoint* wheel_delta,
      const PP_FloatPoint* wheel_ticks,
      PP_Bool scroll_by_page) OVERRIDE;
  virtual PP_Resource CreateX509CertificatePrivate(
      PP_Instance instance) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCreationImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_CREATION_IMPL_H_
