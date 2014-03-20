// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    uint32 session_id,
    media::MediaKeys* media_keys,
    Client* client,
    const SessionClosedCB& session_closed_cb)
    : media_keys_(media_keys),
      client_(client),
      session_closed_cb_(session_closed_cb),
      session_id_(session_id) {
  DCHECK(media_keys_);
}

WebContentDecryptionModuleSessionImpl::
~WebContentDecryptionModuleSessionImpl() {
}

blink::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return web_session_id_;
}

void WebContentDecryptionModuleSessionImpl::generateKeyRequest(
    const blink::WebString& mime_type,
    const uint8* init_data, size_t init_data_length) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!IsStringASCII(mime_type)) {
    NOTREACHED();
    OnSessionError(media::MediaKeys::kUnknownError, 0);
    return;
  }

  media_keys_->CreateSession(
      session_id_, UTF16ToASCII(mime_type), init_data, init_data_length);
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* response,
                                                   size_t response_length) {
  DCHECK(response);
  media_keys_->UpdateSession(session_id_, response, response_length);
}

void WebContentDecryptionModuleSessionImpl::close() {
  media_keys_->ReleaseSession(session_id_);
}

void WebContentDecryptionModuleSessionImpl::OnSessionCreated(
    const std::string& web_session_id) {
  // Due to heartbeat messages, OnSessionCreated() can get called multiple
  // times.
  // TODO(jrummell): Once all CDMs are updated to support reference ids,
  // OnSessionCreated() should only be called once, and the second check can be
  // removed.
  blink::WebString id = blink::WebString::fromUTF8(web_session_id);
  DCHECK(web_session_id_.isEmpty() || web_session_id_ == id)
      << "Session ID may not be changed once set.";
  web_session_id_ = id;
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  client_->keyMessage(message.empty() ? NULL : &message[0],
                      message.size(),
                      GURL(destination_url));
}

void WebContentDecryptionModuleSessionImpl::OnSessionReady() {
  // TODO(jrummell): Blink APIs need to be updated to the new EME API. For now,
  // convert the response to the old v0.1b API.
  client_->keyAdded();
}

void WebContentDecryptionModuleSessionImpl::OnSessionClosed() {
  if (!session_closed_cb_.is_null())
    base::ResetAndReturn(&session_closed_cb_).Run(session_id_);
}

void WebContentDecryptionModuleSessionImpl::OnSessionError(
    media::MediaKeys::KeyError error_code,
    int system_code) {
  client_->keyError(static_cast<Client::MediaKeyErrorCode>(error_code),
                    system_code);
}

}  // namespace content
