// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_client.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "ui/gfx/image/image.h"
#include "webkit/common/user_agent/user_agent.h"

namespace content {

static ContentClient* g_client;

class InternalTestInitializer {
 public:
  static ContentBrowserClient* SetBrowser(ContentBrowserClient* b) {
    ContentBrowserClient* rv = g_client->browser_;
    g_client->browser_ = b;
    return rv;
  }

  static ContentRendererClient* SetRenderer(ContentRendererClient* r) {
    ContentRendererClient* rv = g_client->renderer_;
    g_client->renderer_ = r;
    return rv;
  }

  static ContentUtilityClient* SetUtility(ContentUtilityClient* u) {
    ContentUtilityClient* rv = g_client->utility_;
    g_client->utility_ = u;
    return rv;
  }
};

void SetContentClient(ContentClient* client) {
  g_client = client;

  // Set the default user agent as provided by the client. We need to make
  // sure this is done before webkit_glue::GetUserAgent() is called (so that
  // the UA doesn't change).
  if (client) {
    webkit_glue::SetUserAgent(client->GetUserAgent(), false);
  }
}

ContentClient* GetContentClient() {
  return g_client;
}

ContentBrowserClient* SetBrowserClientForTesting(ContentBrowserClient* b) {
  return InternalTestInitializer::SetBrowser(b);
}

ContentRendererClient* SetRendererClientForTesting(ContentRendererClient* r) {
  return InternalTestInitializer::SetRenderer(r);
}

ContentUtilityClient* SetUtilityClientForTesting(ContentUtilityClient* u) {
  return InternalTestInitializer::SetUtility(u);
}

const std::string& GetUserAgent(const GURL& url) {
  DCHECK(g_client);
  return webkit_glue::GetUserAgent(url);
}

ContentClient::ContentClient()
    : browser_(NULL), plugin_(NULL), renderer_(NULL), utility_(NULL) {
}

ContentClient::~ContentClient() {
}

bool ContentClient::CanSendWhileSwappedOut(const IPC::Message* message) {
  return false;
}

bool ContentClient::CanHandleWhileSwappedOut(const IPC::Message& message) {
  return false;
}

std::string ContentClient::GetProduct() const {
  return std::string();
}

std::string ContentClient::GetUserAgent() const {
  return std::string();
}

base::string16 ContentClient::GetLocalizedString(int message_id) const {
  return base::string16();
}

base::StringPiece ContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return base::StringPiece();
}

base::RefCountedStaticMemory* ContentClient::GetDataResourceBytes(
    int resource_id) const {
  return NULL;
}

gfx::Image& ContentClient::GetNativeImageNamed(int resource_id) const {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, kEmptyImage, ());
  return kEmptyImage;
}

std::string ContentClient::GetProcessTypeNameInEnglish(int type) {
  NOTIMPLEMENTED();
  return std::string();
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool ContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  return false;
}

std::string ContentClient::GetCarbonInterposePath() const {
  return std::string();
}
#endif

}  // namespace content
