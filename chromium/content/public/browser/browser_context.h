// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace fileapi {
class ExternalMountPoints;
}

namespace net {
class URLRequestContextGetter;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace content {

class DownloadManager;
class DownloadManagerDelegate;
class GeolocationPermissionContext;
class IndexedDBContext;
class ResourceContext;
class SiteInstance;
class StoragePartition;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class CONTENT_EXPORT BrowserContext : public base::SupportsUserData {
 public:
  static DownloadManager* GetDownloadManager(BrowserContext* browser_context);

  // Returns BrowserContext specific external mount points. It may return NULL
  // if the context doesn't have any BrowserContext specific external mount
  // points. Currenty, non-NULL value is returned only on ChromeOS.
  static fileapi::ExternalMountPoints* GetMountPoints(BrowserContext* context);

  static content::StoragePartition* GetStoragePartition(
      BrowserContext* browser_context, SiteInstance* site_instance);
  static content::StoragePartition* GetStoragePartitionForSite(
      BrowserContext* browser_context, const GURL& site);
  typedef base::Callback<void(StoragePartition*)> StoragePartitionCallback;
  static void ForEachStoragePartition(
      BrowserContext* browser_context,
      const StoragePartitionCallback& callback);
  static void AsyncObliterateStoragePartition(
      BrowserContext* browser_context,
      const GURL& site,
      const base::Closure& on_gc_required);

  // This function clears the contents of |active_paths| but does not take
  // ownership of the pointer.
  static void GarbageCollectStoragePartitions(
      BrowserContext* browser_context,
      scoped_ptr<base::hash_set<base::FilePath> > active_paths,
      const base::Closure& done);

  // DON'T USE THIS. GetDefaultStoragePartition() is going away.
  // Use GetStoragePartition() instead. Ask ajwong@ if you have problems.
  static content::StoragePartition* GetDefaultStoragePartition(
      BrowserContext* browser_context);

  // Ensures that the corresponding ResourceContext is initialized. Normally the
  // BrowserContext initializs the corresponding getters when its objects are
  // created, but if the embedder wants to pass the ResourceContext to another
  // thread before they use BrowserContext, they should call this to make sure
  // that the ResourceContext is ready.
  static void EnsureResourceContextInitialized(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to persist their session state
  // across the next restart.
  static void SaveSessionState(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to purge any uneeded memory.
  static void PurgeMemory(BrowserContext* browser_context);

  virtual ~BrowserContext();

  // Returns the path of the directory where this context's data is stored.
  virtual base::FilePath GetPath() const = 0;

  // Return whether this context is incognito. Default is false.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the request context information associated with this context.  Call
  // this only on the UI thread, since it can send notifications that should
  // happen on the UI thread.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Returns the request context appropriate for the given renderer. If the
  // renderer process doesn't have an associated installed app, or if the
  // installed app doesn't have isolated storage, this is equivalent to calling
  // GetRequestContext().
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) = 0;

  // Returns the default request context for media resources associated with
  // this context.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetMediaRequestContext() = 0;

  // Returns the request context for media resources associated with this
  // context and renderer process.
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) = 0;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) = 0;

  typedef base::Callback<void(bool)> MIDISysExPermissionCallback;

  // Requests a permission to use system exclusive messages in MIDI events.
  // |callback| will be invoked when the request is resolved.
  virtual void RequestMIDISysExPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      const MIDISysExPermissionCallback& callback) = 0;

  // Cancels a pending MIDI permission request.
  virtual void CancelMIDISysExPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) = 0;

  // Returns the resource context.
  virtual ResourceContext* GetResourceContext() = 0;

  // Returns the DownloadManagerDelegate for this context. This will be called
  // once per context. The embedder owns the delegate and is responsible for
  // ensuring that it outlives DownloadManager. It's valid to return NULL.
  virtual DownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

  // Returns the geolocation permission context for this context. It's valid to
  // return NULL, in which case geolocation requests will always be allowed.
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() = 0;

  // Returns a special storage policy implementation, or NULL.
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() = 0;
};

}  // namespace content

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {

template<>
struct hash<content::BrowserContext*> {
  std::size_t operator()(content::BrowserContext* const& p) const {
    return reinterpret_cast<std::size_t>(p);
  }
};

}  // namespace BASE_HASH_NAMESPACE
#endif

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
