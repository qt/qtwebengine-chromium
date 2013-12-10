// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_content_decryptor_private.idl,
//   modified Tue Sep 17 11:31:05 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void KeyAdded(PP_Instance instance,
              struct PP_Var key_system,
              struct PP_Var session_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::KeyAdded()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->KeyAdded(instance, key_system, session_id);
}

void KeyMessage(PP_Instance instance,
                struct PP_Var key_system,
                struct PP_Var session_id,
                struct PP_Var message,
                struct PP_Var default_url) {
  VLOG(4) << "PPB_ContentDecryptor_Private::KeyMessage()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->KeyMessage(instance,
                                key_system,
                                session_id,
                                message,
                                default_url);
}

void KeyError(PP_Instance instance,
              struct PP_Var key_system,
              struct PP_Var session_id,
              int32_t media_error,
              int32_t system_code) {
  VLOG(4) << "PPB_ContentDecryptor_Private::KeyError()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->KeyError(instance,
                              key_system,
                              session_id,
                              media_error,
                              system_code);
}

void DeliverBlock(PP_Instance instance,
                  PP_Resource decrypted_block,
                  const struct PP_DecryptedBlockInfo* decrypted_block_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverBlock()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverBlock(instance,
                                  decrypted_block,
                                  decrypted_block_info);
}

void DecoderInitializeDone(PP_Instance instance,
                           PP_DecryptorStreamType decoder_type,
                           uint32_t request_id,
                           PP_Bool success) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderInitializeDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderInitializeDone(instance,
                                           decoder_type,
                                           request_id,
                                           success);
}

void DecoderDeinitializeDone(PP_Instance instance,
                             PP_DecryptorStreamType decoder_type,
                             uint32_t request_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderDeinitializeDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderDeinitializeDone(instance,
                                             decoder_type,
                                             request_id);
}

void DecoderResetDone(PP_Instance instance,
                      PP_DecryptorStreamType decoder_type,
                      uint32_t request_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderResetDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderResetDone(instance, decoder_type, request_id);
}

void DeliverFrame(PP_Instance instance,
                  PP_Resource decrypted_frame,
                  const struct PP_DecryptedFrameInfo* decrypted_frame_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverFrame()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverFrame(instance,
                                  decrypted_frame,
                                  decrypted_frame_info);
}

void DeliverSamples(
    PP_Instance instance,
    PP_Resource audio_frames,
    const struct PP_DecryptedBlockInfo* decrypted_block_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverSamples()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverSamples(instance,
                                    audio_frames,
                                    decrypted_block_info);
}

const PPB_ContentDecryptor_Private_0_7
    g_ppb_contentdecryptor_private_thunk_0_7 = {
  &KeyAdded,
  &KeyMessage,
  &KeyError,
  &DeliverBlock,
  &DecoderInitializeDone,
  &DecoderDeinitializeDone,
  &DecoderResetDone,
  &DeliverFrame,
  &DeliverSamples
};

}  // namespace

const PPB_ContentDecryptor_Private_0_7*
    GetPPB_ContentDecryptor_Private_0_7_Thunk() {
  return &g_ppb_contentdecryptor_private_thunk_0_7;
}

}  // namespace thunk
}  // namespace ppapi
