// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_speech_recognition_manager_delegate.h"

namespace content {

class ShellBrowserContext;
class ShellBrowserMainParts;
class ShellResourceDispatcherHostDelegate;

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  static void SetSwapProcessesForRedirect(bool swap);

  ShellContentBrowserClient();
  virtual ~ShellContentBrowserClient();

  // ContentBrowserClient overrides.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) OVERRIDE;
  virtual void RenderProcessHostCreated(RenderProcessHost* host) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      BrowserContext* browser_context,
      ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   const GURL& url,
                                   WebPreferences* prefs) OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;
  virtual bool SupportsBrowserPlugin(content::BrowserContext* browser_context,
                                     const GURL& url) OVERRIDE;
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) OVERRIDE;
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() OVERRIDE;
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual bool ShouldSwapProcessesForRedirect(ResourceContext* resource_context,
                                              const GURL& current_url,
                                              const GURL& new_url) OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const CommandLine& command_line,
      int child_process_id,
      std::vector<content::FileDescriptorInfo>* mappings) OVERRIDE;
#endif

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ShellResourceDispatcherHostDelegate* resource_dispatcher_host_delegate() {
    return resource_dispatcher_host_delegate_.get();
  }
  ShellBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

 private:
  ShellBrowserContext* ShellBrowserContextForBrowserContext(
      BrowserContext* content_browser_context);

  scoped_ptr<ShellResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  base::FilePath webkit_source_dir_;

  ShellBrowserMainParts* shell_browser_main_parts_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
