// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_frontend.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_client.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/net_util.h"

namespace content {

// DevTools frontend path for inspector LayoutTests.
GURL GetDevToolsPathAsURL() {
  base::FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe)) {
    NOTREACHED();
    return GURL();
  }
#if defined(OS_MACOSX)
  // On Mac, the executable is in
  // out/Release/Content Shell.app/Contents/MacOS/Content Shell.
  // We need to go up 3 directories to get to out/Release.
  dir_exe = dir_exe.AppendASCII("../../..");
#endif
  base::FilePath dev_tools_path = dir_exe.AppendASCII(
      "resources/inspector/devtools.html");
  return net::FilePathToFileURL(dev_tools_path);
}

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents) {
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        MSG_ROUTING_NONE,
                                        gfx::Size());
  ShellDevToolsFrontend* devtools_frontend = new ShellDevToolsFrontend(
      shell,
      DevToolsAgentHost::GetOrCreateFor(inspected_contents->GetRenderViewHost())
          .get());

  ShellDevToolsDelegate* delegate = ShellContentBrowserClient::Get()->
      shell_browser_main_parts()->devtools_delegate();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    shell->LoadURL(GetDevToolsPathAsURL());
  else
    shell->LoadURL(delegate->devtools_http_handler()->GetFrontendURL());

  devtools_frontend->Activate();
  devtools_frontend->Focus();

  return devtools_frontend;
}

void ShellDevToolsFrontend::Activate() {
  frontend_shell_->ActivateContents(web_contents());
}

void ShellDevToolsFrontend::Focus() {
  web_contents()->GetView()->Focus();
}

void ShellDevToolsFrontend::Close() {
  frontend_shell_->Close();
}

ShellDevToolsFrontend::ShellDevToolsFrontend(Shell* frontend_shell,
                                             DevToolsAgentHost* agent_host)
    : WebContentsObserver(frontend_shell->web_contents()),
      frontend_shell_(frontend_shell),
      agent_host_(agent_host) {
  frontend_host_.reset(
      DevToolsClientHost::CreateDevToolsFrontendHost(web_contents(), this));
}

ShellDevToolsFrontend::~ShellDevToolsFrontend() {
}

void ShellDevToolsFrontend::RenderViewCreated(
    RenderViewHost* render_view_host) {
  DevToolsClientHost::SetupDevToolsFrontendClient(
      web_contents()->GetRenderViewHost());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->RegisterDevToolsClientHostFor(agent_host_.get(),
                                         frontend_host_.get());
}

void ShellDevToolsFrontend::DocumentOnLoadCompletedInMainFrame(int32 page_id) {
  web_contents()->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      base::string16(),
      ASCIIToUTF16("InspectorFrontendAPI.setUseSoftMenu(true);"));
}

void ShellDevToolsFrontend::WebContentsDestroyed(WebContents* web_contents) {
  DevToolsManager::GetInstance()->ClientHostClosing(frontend_host_.get());
  delete this;
}

void ShellDevToolsFrontend::InspectedContentsClosing() {
  frontend_shell_->Close();
}

}  // namespace content
