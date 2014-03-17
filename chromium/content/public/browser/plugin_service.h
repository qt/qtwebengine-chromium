// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

class BrowserContext;
class PluginProcessHost;
class PluginServiceFilter;
class ResourceContext;
struct PepperPluginInfo;
struct WebPluginInfo;

// This must be created on the main thread but it's only called on the IO/file
// thread. This is an asynchronous wrapper around the PluginList interface for
// querying plugin information. This must be used instead of that to avoid
// doing expensive disk operations on the IO/UI threads.
class PluginService {
 public:
  typedef base::Callback<void(const std::vector<WebPluginInfo>&)>
      GetPluginsCallback;

  // Returns the PluginService singleton.
  CONTENT_EXPORT static PluginService* GetInstance();

  // Tells all the renderer processes associated with the given browser context
  // to throw away their cache of the plugin list, and optionally also reload
  // all the pages with plugins. If |browser_context| is NULL, purges the cache
  // in all renderers.
  // NOTE: can only be called on the UI thread.
  CONTENT_EXPORT static void PurgePluginListCache(
      BrowserContext* browser_context,
      bool reload_pages);

  virtual ~PluginService() {}

  // Must be called on the instance to finish initialization.
  virtual void Init() = 0;

  // Starts watching for changes in the list of installed plug-ins.
  virtual void StartWatchingPlugins() = 0;

  // Gets the plugin in the list of plugins that matches the given url and mime
  // type. Returns true if the data is frome a stale plugin list, false if it
  // is up to date. This can be called from any thread.
  virtual bool GetPluginInfoArray(
      const GURL& url,
      const std::string& mime_type,
      bool allow_wildcard,
      std::vector<WebPluginInfo>* info,
      std::vector<std::string>* actual_mime_types) = 0;

  // Gets plugin info for an individual plugin and filters the plugins using
  // the |context| and renderer IDs. This will report whether the data is stale
  // via |is_stale| and returns whether or not the plugin can be found.
  virtual bool GetPluginInfo(int render_process_id,
                             int render_frame_id,
                             ResourceContext* context,
                             const GURL& url,
                             const GURL& page_url,
                             const std::string& mime_type,
                             bool allow_wildcard,
                             bool* is_stale,
                             WebPluginInfo* info,
                             std::string* actual_mime_type) = 0;

  // Get plugin info by plugin path (including disabled plugins). Returns true
  // if the plugin is found and WebPluginInfo has been filled in |info|. This
  // will use cached data in the plugin list.
  virtual bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                                   WebPluginInfo* info) = 0;

  // Returns the display name for the plugin identified by the given path. If
  // the path doesn't identify a plugin, or the plugin has no display name,
  // this will attempt to generate a display name from the path.
  virtual base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& plugin_path) = 0;

  // Asynchronously loads plugins if necessary and then calls back to the
  // provided function on the calling MessageLoop on completion.
  virtual void GetPlugins(const GetPluginsCallback& callback) = 0;

  // Returns information about a pepper plugin if it exists, otherwise NULL.
  // The caller does not own the pointer, and it's not guaranteed to live past
  // the call stack.
  virtual PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const base::FilePath& plugin_path) = 0;

  virtual void SetFilter(PluginServiceFilter* filter) = 0;
  virtual PluginServiceFilter* GetFilter() = 0;

  // If the plugin with the given path is running, cleanly shuts it down.
  virtual void ForcePluginShutdown(const base::FilePath& plugin_path) = 0;

  // Used to monitor plug-in stability. An unstable plug-in is one that has
  // crashed more than a set number of times in a set time period.
  virtual bool IsPluginUnstable(const base::FilePath& plugin_path) = 0;

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  virtual void RefreshPlugins() = 0;

  // Add/Remove an extra plugin to load when we actually do the loading.  Must
  // be called before the plugins have been loaded.
  virtual void AddExtraPluginPath(const base::FilePath& path) = 0;
  virtual void RemoveExtraPluginPath(const base::FilePath& path) = 0;

  // Same as above, but specifies a directory in which to search for plugins.
  virtual void AddExtraPluginDir(const base::FilePath& path) = 0;

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // If |add_at_beginning| is true the plugin will be added earlier in
  // the list so that it can override the MIME types of older registrations.
  virtual void RegisterInternalPlugin(const WebPluginInfo& info,
                                      bool add_at_beginning) = 0;

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  virtual void UnregisterInternalPlugin(const base::FilePath& path) = 0;

  // Gets a list of all the registered internal plugins.
  virtual void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) = 0;

  // Returns true iff NPAPI plugins are supported on the current platform.
  // This can be called from any thread.
  virtual bool NPAPIPluginsSupported() = 0;

  // This is equivalent to specifying kDisablePluginsDiscovery, but is useful
  // for unittests.
  virtual void DisablePluginsDiscoveryForTesting() = 0;

#if defined(OS_MACOSX)
  // Called when the application is made active so that modal plugin windows can
  // be made forward too.
  virtual void AppActivated() = 0;
#elif defined(OS_WIN)
  // Returns the name and version of a plugin HWND. If the HWND isn't a valid
  // plugin, returns false.
  // This can be called from any thread.
  virtual bool GetPluginInfoFromWindow(HWND window,
                                       base::string16* plugin_name,
                                       base::string16* plugin_version) = 0;
#endif

  // Returns true iff PPAPI "dev channel" methods are supported.
  virtual bool PpapiDevChannelSupported() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
