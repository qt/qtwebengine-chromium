// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return NULL;
}

std::string ContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

bool ContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  return false;
}

WebKit::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderView* render_view,
    const base::FilePath& plugin_path) {
  return NULL;
}

bool ContentRendererClient::HasErrorPage(int http_status_code,
                                         std::string* error_domain) {
  return false;
}

void ContentRendererClient::DeferMediaLoad(RenderView* render_view,
                                           const base::Closure& closure) {
  closure.Run();
}

WebKit::WebMediaStreamCenter*
ContentRendererClient::OverrideCreateWebMediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client) {
  return NULL;
}

WebKit::WebRTCPeerConnectionHandler*
ContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    WebKit::WebRTCPeerConnectionHandlerClient* client) {
  return NULL;
}

WebKit::WebMIDIAccessor*
ContentRendererClient::OverrideCreateMIDIAccessor(
    WebKit::WebMIDIAccessorClient* client) {
  return NULL;
}

WebKit::WebAudioDevice*
ContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  return NULL;
}

WebKit::WebClipboard* ContentRendererClient::OverrideWebClipboard() {
  return NULL;
}

WebKit::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return NULL;
}

WebKit::WebSpeechSynthesizer* ContentRendererClient::OverrideSpeechSynthesizer(
    WebKit::WebSpeechSynthesizerClient* client) {
  return NULL;
}

WebKit::WebCrypto* ContentRendererClient::OverrideWebCrypto() {
  return NULL;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

bool ContentRendererClient::HandleNavigation(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebKit::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}

bool ContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool is_server_redirect,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(
    WebKit::WebFrame* frame,
    PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  return false;
}

bool ContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

WebKit::WebPrescientNetworking*
ContentRendererClient::GetPrescientNetworking() {
  return NULL;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) {
  return false;
}

bool ContentRendererClient::HandleGetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  return false;
}

bool ContentRendererClient::HandleSetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  return false;
}

const void* ContentRendererClient::CreatePPAPIInterface(
    const std::string& interface_name) {
  return NULL;
}

bool ContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToCallRequestOSFileHandle(
    WebKit::WebPluginContainer* container) {
  return false;
}

bool ContentRendererClient::AllowBrowserPlugin(
    WebKit::WebPluginContainer* container) {
  return false;
}

bool ContentRendererClient::AllowPepperMediaStreamAPI(const GURL& url) {
  return false;
}

void ContentRendererClient::AddKeySystems(
    std::vector<KeySystemInfo>* key_systems) {
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
  return false;
}

bool ContentRendererClient::ShouldEnableSiteIsolationPolicy() const {
  return true;
}

}  // namespace content
