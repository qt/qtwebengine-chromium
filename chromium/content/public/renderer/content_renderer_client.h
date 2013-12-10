// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace base {
class FilePath;
class MessageLoop;
}

namespace WebKit {
class WebAudioDevice;
class WebClipboard;
class WebCrypto;
class WebFrame;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebPlugin;
class WebPluginContainer;
class WebPrescientNetworking;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebThemeEngine;
class WebURLRequest;
struct WebPluginParams;
struct WebURLError;
}

namespace content {

class RenderView;
class SynchronousCompositor;
struct KeySystemInfo;
struct WebPluginInfo;

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentRendererClient {
 public:
  virtual ~ContentRendererClient() {}

  // Notifies us that the RenderThread has been created.
  virtual void RenderThreadStarted() {}

  // Notifies that a new RenderView has been created.
  virtual void RenderViewCreated(RenderView* render_view) {}

  // Sets a number of views/tabs opened in this process.
  virtual void SetNumberOfViews(int number_of_views) {}

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap();

  // Returns the bitmap to show when a <webview> guest has crashed, or NULL for
  // none.
  virtual SkBitmap* GetSadWebViewBitmap();

  // Returns the default text encoding.
  virtual std::string GetDefaultEncoding();

  // Allows the embedder to override creating a plugin. If it returns true, then
  // |plugin| will contain the created plugin, although it could be NULL. If it
  // returns false, the content layer will create the plugin.
  virtual bool OverrideCreatePlugin(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      WebKit::WebPlugin** plugin);

  // Creates a replacement plug-in that is shown when the plug-in at |file_path|
  // couldn't be loaded. This allows the embedder to show a custom placeholder.
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      RenderView* render_view,
      const base::FilePath& plugin_path);

  // Returns true if the embedder has an error page to show for the given http
  // status code. If so |error_domain| should be set to according to WebURLError
  // and the embedder's GetNavigationErrorHtml will be called afterwards to get
  // the error html.
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain);

  // Returns the information to display when a navigation error occurs.
  // If |error_html| is not null then it may be set to a HTML page containing
  // the details of the error and maybe links to more info.
  // If |error_description| is not null it may be set to contain a brief
  // message describing the error that has occurred.
  // Either of the out parameters may be not written to in certain cases
  // (lack of information on the error code) so the caller should take care to
  // initialize the string values with safe defaults before the call.
  virtual void GetNavigationErrorStrings(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      std::string* error_html,
      string16* error_description) {}

  // Allows the embedder to control when media resources are loaded. Embedders
  // can run |closure| immediately if they don't wish to defer media resource
  // loading.
  virtual void DeferMediaLoad(RenderView* render_view,
                              const base::Closure& closure);

  // Allows the embedder to override creating a WebMediaStreamCenter. If it
  // returns NULL the content layer will create the stream center.
  virtual WebKit::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client);

  // Allows the embedder to override creating a WebRTCPeerConnectionHandler. If
  // it returns NULL the content layer will create the connection handler.
  virtual WebKit::WebRTCPeerConnectionHandler*
  OverrideCreateWebRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);

  // Allows the embedder to override creating a WebMIDIAccessor.  If it
  // returns NULL the content layer will create the MIDI accessor.
  virtual WebKit::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      WebKit::WebMIDIAccessorClient* client);

  // Allows the embedder to override creating a WebAudioDevice.  If it
  // returns NULL the content layer will create the audio device.
  virtual WebKit::WebAudioDevice* OverrideCreateAudioDevice(
      double sample_rate);

  // Allows the embedder to override the WebKit::WebClipboard used. If it
  // returns NULL the content layer will handle clipboard interactions.
  virtual WebKit::WebClipboard* OverrideWebClipboard();

  // Allows the embedder to override the WebThemeEngine used. If it returns NULL
  // the content layer will provide an engine.
  virtual WebKit::WebThemeEngine* OverrideThemeEngine();

  // Allows the embedder to override the WebSpeechSynthesizer used.
  // If it returns NULL the content layer will provide an engine.
  virtual WebKit::WebSpeechSynthesizer* OverrideSpeechSynthesizer(
      WebKit::WebSpeechSynthesizerClient* client);

  // Allows the embedder to override the WebCrypto used.
  // If it returns NULL the content layer will handle crypto.
  virtual WebKit::WebCrypto* OverrideWebCrypto();

  // Returns true if the renderer process should schedule the idle handler when
  // all widgets are hidden.
  virtual bool RunIdleHandlerWhenWidgetsHidden();

  // Returns true if a popup window should be allowed.
  virtual bool AllowPopup();

  // Returns true if the navigation was handled by the embedder and should be
  // ignored by WebKit. This method is used by CEF.
  virtual bool HandleNavigation(WebKit::WebFrame* frame,
                                const WebKit::WebURLRequest& request,
                                WebKit::WebNavigationType type,
                                WebKit::WebNavigationPolicy default_policy,
                                bool is_redirect);

  // Returns true if we should fork a new process for the given navigation.
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                          const std::string& http_method,
                          bool is_initial_navigation,
                          bool is_server_redirect,
                          bool* send_referrer);

  // Notifies the embedder that the given frame is requesting the resource at
  // |url|.  If the function returns true, the url is changed to |new_url|.
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               PageTransition transition_type,
                               const GURL& url,
                               const GURL& first_party_for_cookies,
                               GURL* new_url);

  // Whether to pump events when sending sync cookie messages.  Needed if the
  // embedder can potentiall put up a modal dialog on the UI thread as a result.
  virtual bool ShouldPumpEventsDuringCookieMessage();

  // See the corresponding functions in WebKit::WebFrameClient.
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) {}
  virtual void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context>,
                                        int world_id) {}

  // See WebKit::Platform.
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length);
  virtual bool IsLinkVisited(unsigned long long link_hash);
  virtual WebKit::WebPrescientNetworking* GetPrescientNetworking();
  virtual bool ShouldOverridePageVisibilityState(
      const RenderView* render_view,
      WebKit::WebPageVisibilityState* override_state);

  // Return true if the GetCookie request will be handled by the embedder.
  // Cookies are returned in the cookie parameter.
  virtual bool HandleGetCookieRequest(RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      std::string* cookies);

  // Return true if the SetCookie request will be handled by the embedder.
  // Cookies to be set are passed in the value parameter.
  virtual bool HandleSetCookieRequest(RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& value);

  // Allows an embedder to return custom PPAPI interfaces.
  virtual const void* CreatePPAPIInterface(
      const std::string& interface_name);

  // Returns true if the given Pepper plugin is external (requiring special
  // startup steps).
  virtual bool IsExternalPepperPlugin(const std::string& module_name);

  // Returns true if plugin living in the container can use
  // pp::FileIO::RequestOSFileHandle.
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      WebKit::WebPluginContainer* container);

  // Returns whether BrowserPlugin should be allowed within the |container|.
  virtual bool AllowBrowserPlugin(WebKit::WebPluginContainer* container);

  // Returns true if the page at |url| can use Pepper MediaStream APIs.
  virtual bool AllowPepperMediaStreamAPI(const GURL& url);

  // Gives the embedder a chance to register the key system(s) it supports by
  // populating |key_systems|.
  virtual void AddKeySystems(std::vector<KeySystemInfo>* key_systems);

  // Returns true if we should report a detailed message (including a stack
  // trace) for console [logs|errors|exceptions]. |source| is the WebKit-
  // reported source for the error; this can point to a page or a script,
  // and can be external or internal.
  virtual bool ShouldReportDetailedMessageForSource(
      const base::string16& source) const;

  // Returns true if we should apply the cross-site document blocking policy to
  // this renderer process. Currently, we apply the policy only to a renderer
  // process running on a normal page from the web.
  virtual bool ShouldEnableSiteIsolationPolicy() const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
