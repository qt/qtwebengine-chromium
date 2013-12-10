// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/child/webkitplatformsupport_impl.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

namespace WebKit {
class WebFileUtilities;
}

namespace content {
class QuotaMessageFilter;
class ThreadSafeSender;
class WebFileSystemImpl;

class WorkerWebKitPlatformSupportImpl : public WebKitPlatformSupportImpl,
                                        public WebKit::WebMimeRegistry {
 public:
  WorkerWebKitPlatformSupportImpl(
      ThreadSafeSender* sender,
      IPC::SyncMessageFilter* sync_message_filter,
      QuotaMessageFilter* quota_message_filter);
  virtual ~WorkerWebKitPlatformSupportImpl();

  // WebKitPlatformSupport methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebFileUtilities* fileUtilities();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString& value);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url,
      const WebKit::WebURL& first_party_for_cookies);
  virtual WebKit::WebString defaultLocale();
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace();
  virtual void dispatchStorageEvent(
      const WebKit::WebString& key, const WebKit::WebString& old_value,
      const WebKit::WebString& new_value, const WebKit::WebString& origin,
      const WebKit::WebURL& url, bool is_local_storage);

  virtual WebKit::Platform::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier);

  virtual WebKit::WebBlobRegistry* blobRegistry();

  virtual WebKit::WebIDBFactory* idbFactory();

  // WebMimeRegistry methods:
  virtual WebKit::WebMimeRegistry::SupportsType supportsMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const WebKit::WebString&);
  // TODO(ddorwin): Remove after http://webk.it/82983 lands.
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&, const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&,
      const WebKit::WebString&,
      const WebKit::WebString&);
  virtual bool supportsMediaSourceMIMEType(
      const WebKit::WebString&,
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
  virtual WebKit::WebString wellKnownMimeTypeForExtension(
      const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
  virtual void queryStorageUsageAndQuota(
      const WebKit::WebURL& storage_partition,
      WebKit::WebStorageQuotaType,
      WebKit::WebStorageQuotaCallbacks*) OVERRIDE;

 private:

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;
  scoped_ptr<WebKit::WebBlobRegistry> blob_registry_;
  scoped_ptr<WebFileSystemImpl> web_file_system_;
  scoped_ptr<WebKit::WebIDBFactory> web_idb_factory_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::MessageLoopProxy> child_thread_loop_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;
};

}  // namespace content

#endif  // CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_
