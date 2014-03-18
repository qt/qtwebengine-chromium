// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/child/webkitplatformsupport_impl.h"
#include "content/common/content_export.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"

namespace base {
class MessageLoopProxy;
}

namespace cc {
class ContextProvider;
}

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGraphicsContext3DProvider;
}

namespace content {
class DeviceMotionEventPump;
class DeviceOrientationEventPump;
class QuotaMessageFilter;
class RendererClipboardClient;
class ThreadSafeSender;
class WebClipboardImpl;
class WebCryptoImpl;
class WebDatabaseObserverImpl;
class WebFileSystemImpl;

class CONTENT_EXPORT RendererWebKitPlatformSupportImpl
    : public WebKitPlatformSupportImpl {
 public:
  RendererWebKitPlatformSupportImpl();
  virtual ~RendererWebKitPlatformSupportImpl();

  void set_plugin_refresh_allowed(bool plugin_refresh_allowed) {
    plugin_refresh_allowed_ = plugin_refresh_allowed;
  }
  // Platform methods:
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebSandboxSupport* sandboxSupport();
  virtual blink::WebCookieJar* cookieJar();
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebSpeechSynthesizer* createSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client);
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual blink::WebMessagePortChannel* createMessagePortChannel();
  virtual blink::WebPrescientNetworking* prescientNetworking();
  virtual void cacheMetadata(
      const blink::WebURL&, double, const char*, size_t);
  virtual blink::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);
  virtual blink::WebStorageNamespace* createLocalStorageNamespace();
  virtual blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier);
  virtual blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const blink::WebString& challenge,
      const blink::WebURL& url);
  virtual void getPluginList(bool refresh,
                             blink::WebPluginListBuilder* builder);
  virtual blink::WebPublicSuffixList* publicSuffixList();
  virtual void screenColorProfile(blink::WebVector<char>* to_profile);
  virtual blink::WebIDBFactory* idbFactory();
  virtual blink::WebFileSystem* fileSystem();
  virtual bool canAccelerate2dCanvas();
  virtual bool isThreadedCompositingEnabled();
  virtual double audioHardwareSampleRate();
  virtual size_t audioHardwareBufferSize();
  virtual unsigned audioHardwareOutputChannels();
  virtual blink::WebDatabaseObserver* databaseObserver();

  // TODO(crogers): remove deprecated API as soon as WebKit calls new API.
  virtual blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned channels, double sample_rate,
      blink::WebAudioDevice::RenderCallback* callback);
  // TODO(crogers): remove deprecated API as soon as WebKit calls new API.
  virtual blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned input_channels, unsigned channels,
      double sample_rate, blink::WebAudioDevice::RenderCallback* callback);

  virtual blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned input_channels, unsigned channels,
      double sample_rate, blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id);

  virtual bool loadAudioResource(
      blink::WebAudioBus* destination_bus, const char* audio_file_data,
      size_t data_size, double sample_rate);

  virtual blink::WebContentDecryptionModule* createContentDecryptionModule(
      const blink::WebString& key_system);
  virtual blink::WebMIDIAccessor*
      createMIDIAccessor(blink::WebMIDIAccessorClient* client);

  virtual blink::WebBlobRegistry* blobRegistry();
  virtual void sampleGamepads(blink::WebGamepads&);
  virtual blink::WebString userAgent(const blink::WebURL& url);
  virtual blink::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);
  virtual blink::WebMediaStreamCenter* createMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);
  virtual bool processMemorySizesInBytes(
      size_t* private_bytes, size_t* shared_bytes);
  virtual blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes);
  virtual blink::WebGraphicsContext3DProvider*
      createSharedOffscreenGraphicsContext3DProvider();
  virtual blink::WebCompositorSupport* compositorSupport();
  virtual blink::WebString convertIDNToUnicode(
      const blink::WebString& host, const blink::WebString& languages);
  virtual void setDeviceMotionListener(
      blink::WebDeviceMotionListener* listener) OVERRIDE;
  virtual void setDeviceOrientationListener(
      blink::WebDeviceOrientationListener* listener) OVERRIDE;
  virtual blink::WebCrypto* crypto() OVERRIDE;
  virtual void queryStorageUsageAndQuota(
      const blink::WebURL& storage_partition,
      blink::WebStorageQuotaType,
      blink::WebStorageQuotaCallbacks*) OVERRIDE;
  virtual void vibrate(unsigned int milliseconds);
  virtual void cancelVibration();

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  // Set WebGamepads to return when sampleGamepads() is invoked.
  static void SetMockGamepadsForTesting(const blink::WebGamepads& pads);
  // Set WebDeviceMotionData to return when setDeviceMotionListener is invoked.
  static void SetMockDeviceMotionDataForTesting(
      const blink::WebDeviceMotionData& data);
  // Set WebDeviceOrientationData to return when setDeviceOrientationListener
  // is invoked.
  static void SetMockDeviceOrientationDataForTesting(
      const blink::WebDeviceOrientationData& data);

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  scoped_ptr<RendererClipboardClient> clipboard_client_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;

  class MimeRegistry;
  scoped_ptr<MimeRegistry> mime_registry_;

  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  scoped_ptr<blink::WebIDBFactory> web_idb_factory_;

  scoped_ptr<blink::WebBlobRegistry> blob_registry_;

  WebPublicSuffixListImpl public_suffix_list_;

  scoped_ptr<DeviceMotionEventPump> device_motion_event_pump_;
  scoped_ptr<DeviceOrientationEventPump> device_orientation_event_pump_;

  scoped_refptr<base::MessageLoopProxy> child_thread_loop_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  webkit::WebCompositorSupportImpl compositor_support_;

  scoped_ptr<WebCryptoImpl> web_crypto_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
