// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PLUGIN_LIST_H_
#define CONTENT_COMMON_PLUGIN_LIST_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/public/common/webplugininfo.h"

class GURL;

namespace content {

// The PluginList is responsible for loading our NPAPI based plugins. It does
// so in whatever manner is appropriate for the platform. On Windows, it loads
// plugins from a known directory by looking for DLLs which start with "NP",
// and checking to see if they are valid NPAPI libraries. On the Mac, it walks
// the machine-wide and user plugin directories and loads anything that has
// the correct types. On Linux, it walks the plugin directories as well
// (e.g. /usr/lib/browser-plugins/).
// This object is thread safe.
class CONTENT_EXPORT PluginList {
 public:

  // Gets the one instance of the PluginList.
  static PluginList* Singleton();

  // Returns true if we're in debug-plugin-loading mode. This is controlled
  // by a command line switch.
  static bool DebugPluginLoading();

  // Returns true if the plugin supports |mime_type|. |mime_type| should be all
  // lower case.
  static bool SupportsType(const WebPluginInfo& plugin,
                           const std::string& mime_type,
                           bool allow_wildcard);

  // Disables discovery of third_party plugins in standard places next time
  // plugins are loaded.
  void DisablePluginsDiscovery();

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  void RefreshPlugins();

  // Add/Remove an extra plugin to load when we actually do the loading.  Must
  // be called before the plugins have been loaded.
  void AddExtraPluginPath(const base::FilePath& plugin_path);
  void RemoveExtraPluginPath(const base::FilePath& plugin_path);

  // Same as above, but specifies a directory in which to search for plugins.
  void AddExtraPluginDir(const base::FilePath& plugin_dir);

  // Get the ordered list of directories from which to load plugins
  void GetPluginDirectories(std::vector<base::FilePath>* plugin_dirs);

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // If |add_at_beginning| is true the plugin will be added earlier in
  // the list so that it can override the MIME types of older registrations.
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning);

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  void UnregisterInternalPlugin(const base::FilePath& path);

  // Gets a list of all the registered internal plugins.
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".
  // Returns false if the library couldn't be found, or if it's not a plugin.
  bool ReadPluginInfo(const base::FilePath& filename,
                      WebPluginInfo* info);

  // In Windows plugins, the mime types are passed as a specially formatted list
  // of strings. This function parses those strings into a WebPluginMimeType
  // vector.
  // TODO(evan): move this code into plugin_list_win.
  static bool ParseMimeTypes(
      const std::string& mime_types,
      const std::string& file_extensions,
      const base::string16& mime_type_descriptions,
      std::vector<WebPluginMimeType>* parsed_mime_types);

  // Get all the plugins synchronously, loading them if necessary.
  void GetPlugins(std::vector<WebPluginInfo>* plugins,
                  bool include_npapi);

  // Copies the list of plug-ins into |plugins| without loading them.
  // Returns true if the list of plugins is up-to-date.
  bool GetPluginsNoRefresh(std::vector<WebPluginInfo>* plugins);

  // Returns a list in |info| containing plugins that are found for
  // the given url and mime type (including disabled plugins, for
  // which |info->enabled| is false).  The mime type which corresponds
  // to the URL is optionally returned back in |actual_mime_types| (if
  // it is non-NULL), one for each of the plugin info objects found.
  // The |allow_wildcard| parameter controls whether this function
  // returns plugins which support wildcard mime types (* as the mime
  // type).  The |info| parameter is required to be non-NULL.  The
  // list is in order of "most desirable" to "least desirable".
  // If |use_stale| is NULL, this will load the plug-in list if necessary.
  // If it is not NULL, the plug-in list will not be loaded, and |*use_stale|
  // will be true iff the plug-in list was stale.
  void GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          bool* use_stale,
                          bool include_npapi,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types);

  // Load a specific plugin with full path. Return true iff loading the plug-in
  // was successful.
  bool LoadPluginIntoPluginList(const base::FilePath& filename,
                                std::vector<WebPluginInfo>* plugins,
                                WebPluginInfo* plugin_info);

  // The following functions are used to support probing for WebPluginInfo
  // using a different instance of this class.

  // Computes a list of all plugins to potentially load from all sources.
  void GetPluginPathsToLoad(std::vector<base::FilePath>* plugin_paths,
                            bool include_npapi);

  // Clears the internal list of Plugins and copies them from the vector.
  void SetPlugins(const std::vector<WebPluginInfo>& plugins);

  void set_will_load_plugins_callback(const base::Closure& callback);

  virtual ~PluginList();

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".
  // Returns false if the library couldn't be found, or if it's not a plugin.
  static bool ReadWebPluginInfo(const base::FilePath& filename,
                                WebPluginInfo* info);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Parse the result of an NP_GetMIMEDescription() call.
  // This API is only used on Unixes, and is exposed here for testing.
  static void ParseMIMEDescription(const std::string& description,
      std::vector<WebPluginMimeType>* mime_types);

  // Extract a version number from a description string.
  // This API is only used on Unixes, and is exposed here for testing.
  static void ExtractVersionString(const std::string& version,
                                   WebPluginInfo* info);
#endif

 private:
  enum LoadingState {
    LOADING_STATE_NEEDS_REFRESH,
    LOADING_STATE_REFRESHING,
    LOADING_STATE_UP_TO_DATE,
  };

  friend class PluginListTest;
  friend struct base::DefaultLazyInstanceTraits<PluginList>;

  PluginList();

  // Load all plugins from the default plugins directory.
  void LoadPlugins(bool include_npapi);

  // Walks a directory and produces a list of all the plugins to potentially
  // load in that directory.
  void GetPluginsInDir(const base::FilePath& path,
                       std::vector<base::FilePath>* plugins);

  // Returns true if we should load the given plugin, or false otherwise.
  // |plugins| is the list of plugins we have crawled in the current plugin
  // loading run.
  bool ShouldLoadPluginUsingPluginList(const WebPluginInfo& info,
                                       std::vector<WebPluginInfo>* plugins);

  // Returns true if the given plugin supports a given file extension.
  // |extension| should be all lower case. If |mime_type| is not NULL, it will
  // be set to the MIME type if found. The MIME type which corresponds to the
  // extension is optionally returned back.
  bool SupportsExtension(const WebPluginInfo& plugin,
                         const std::string& extension,
                         std::string* actual_mime_type);

  // Removes |plugin_path| from the list of extra plugin paths. Should only be
  // called while holding |lock_|.
  void RemoveExtraPluginPathLocked(const base::FilePath& plugin_path);

  //
  // Command-line switches
  //

#if defined(OS_WIN)
  // Gets plugin paths registered under HKCU\Software\MozillaPlugins and
  // HKLM\Software\MozillaPlugins.
  void GetPluginPathsFromRegistry(std::vector<base::FilePath>* plugins);
#endif

  //
  // Internals
  //

  // States whether we will load the plug-in list the next time we try to access
  // it, whether we are currently in the process of loading it, or whether we
  // consider it up-to-date.
  LoadingState loading_state_;

  // Extra plugin paths that we want to search when loading.
  std::vector<base::FilePath> extra_plugin_paths_;

  // Extra plugin directories that we want to search when loading.
  std::vector<base::FilePath> extra_plugin_dirs_;

  // Holds information about internal plugins.
  std::vector<WebPluginInfo> internal_plugins_;

  // A list holding all plug-ins.
  std::vector<WebPluginInfo> plugins_list_;

  // Callback that is invoked whenever the PluginList will reload the plugins.
  base::Closure will_load_plugins_callback_;

  // Need synchronization for the above members since this object can be
  // accessed on multiple threads.
  base::Lock lock_;

  // Flag indicating whether third_party plugins will be searched for
  // in common places.
  bool plugins_discovery_disabled_;

  DISALLOW_COPY_AND_ASSIGN(PluginList);
};

}  // namespace content

#endif  // CONTENT_COMMON_PLUGIN_LIST_H_
