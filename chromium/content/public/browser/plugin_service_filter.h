// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_

class GURL;

namespace base {
class FilePath;
}

namespace content {
struct WebPluginInfo;

// Callback class to let the client filter the list of all installed plug-ins
// and block them from being loaded.
// This class is called on the FILE thread.
class PluginServiceFilter {
 public:
  virtual ~PluginServiceFilter() {}

  // Whether |plugin| is available. The client can return false to hide the
  // plugin, or return true and optionally change the passed in plugin.
  virtual bool IsPluginAvailable(int render_process_id,
                                 int render_frame_id,
                                 const void* context,
                                 const GURL& url,
                                 const GURL& policy_url,
                                 WebPluginInfo* plugin) = 0;

  // Whether the renderer has permission to load available |plugin|.
  virtual bool CanLoadPlugin(int render_process_id,
                             const base::FilePath& path) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_
