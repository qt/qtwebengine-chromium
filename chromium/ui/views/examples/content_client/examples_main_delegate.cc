// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/content_client/examples_main_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/examples/content_client/examples_content_browser_client.h"

#if defined(OS_WIN)
#include "base/logging_win.h"
#endif

namespace views {
namespace examples {
namespace {

#if defined(OS_WIN)
// {83FAC8EE-7A0E-4dbb-A3F6-6F500D7CAB1A}
const GUID kViewsExamplesProviderName =
    { 0x83fac8ee, 0x7a0e, 0x4dbb,
        { 0xa3, 0xf6, 0x6f, 0x50, 0xd, 0x7c, 0xab, 0x1a } };
#endif

}  // namespace

ExamplesMainDelegate::ExamplesMainDelegate() {
}

ExamplesMainDelegate::~ExamplesMainDelegate() {
}

bool ExamplesMainDelegate::BasicStartupComplete(int* exit_code) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  content::SetContentClient(&content_client_);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  bool success = logging::InitLogging(settings);
  CHECK(success);
#if defined(OS_WIN)
  logging::LogEventProvider::Initialize(kViewsExamplesProviderName);
#endif

  return false;
}

void ExamplesMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

content::ContentBrowserClient*
    ExamplesMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new ExamplesContentBrowserClient);
  return browser_client_.get();
}

void ExamplesMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);

  base::FilePath pak_file;
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("ui_test.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

}  // namespace examples
}  // namespace views
