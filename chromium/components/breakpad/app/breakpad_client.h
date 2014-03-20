// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREAKPAD_APP_BREAKPAD_CLIENT_H_
#define COMPONENTS_BREAKPAD_APP_BREAKPAD_CLIENT_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}

#if defined(OS_MACOSX)
// We don't want to directly include
// breakpad/src/client/mac/Framework/Breakpad.h here, so we repeat the
// definition of BreakpadRef.
//
// On Mac, when compiling without breakpad support, a stub implementation is
// compiled in. Not having any includes of the breakpad library allows for
// reusing this header for the stub.
typedef void* BreakpadRef;
#endif

namespace breakpad {

class BreakpadClient;

// Setter and getter for the client.  The client should be set early, before any
// breakpad code is called, and should stay alive throughout the entire runtime.
void SetBreakpadClient(BreakpadClient* client);

#if defined(BREAKPAD_IMPLEMENTATION)
// Breakpad's embedder API should only be used by breakpad.
BreakpadClient* GetBreakpadClient();
#endif

// Interface that the embedder implements.
class BreakpadClient {
 public:
  BreakpadClient();
  virtual ~BreakpadClient();

  // Sets the Breakpad client ID, which is a unique identifier for the client
  // that is sending crash reports. After it is set, it should not be changed.
  virtual void SetClientID(const std::string& client_id);

#if defined(OS_WIN)
  // Returns true if an alternative location to store the minidump files was
  // specified. Returns true if |crash_dir| was set.
  virtual bool GetAlternativeCrashDumpLocation(base::FilePath* crash_dir);

  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const base::FilePath& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build,
                                        base::string16* channel_name);

  // Returns true if a restart dialog should be displayed. In that case,
  // |message| and |title| are set to a message to display in a dialog box with
  // the given title before restarting, and |is_rtl_locale| indicates whether
  // to display the text as RTL.
  virtual bool ShouldShowRestartDialog(base::string16* title,
                                       base::string16* message,
                                       bool* is_rtl_locale);

  // Returns true if it is ok to restart the application. Invoked right before
  // restarting after a crash.
  virtual bool AboutToRestart();

  // Returns true if the crash report uploader supports deferred uploads.
  virtual bool GetDeferredUploadsSupported(bool is_per_user_install);

  // Returns true if the running binary is a per-user installation.
  virtual bool GetIsPerUserInstall(const base::FilePath& exe_path);

  // Returns true if larger crash dumps should be dumped.
  virtual bool GetShouldDumpLargerDumps(bool is_per_user_install);

  // Returns the result code to return when breakpad failed to respawn a
  // crashed process.
  virtual int GetResultCodeRespawnFailed();

  // Invoked when initializing breakpad in the browser process.
  virtual void InitBrowserCrashDumpsRegKey();

  // Invoked before attempting to write a minidump.
  virtual void RecordCrashDumpAttempt(bool is_real_crash);
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(std::string* product_name,
                                        std::string* version);

  virtual base::FilePath GetReporterLogFilename();
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);

#if defined(OS_POSIX)
  // Sets a function that'll be invoked to dump the current process when
  // without crashing.
  virtual void SetDumpWithoutCrashingFunction(void (*function)());
#endif

  // Register all of the potential crash keys that can be sent to the crash
  // reporting server. Returns the size of the union of all keys.
  virtual size_t RegisterCrashKeys();

  // Returns true if running in unattended mode (for automated testing).
  virtual bool IsRunningUnattended();

  // Returns true if the user has given consent to collect stats.
  virtual bool GetCollectStatsConsent();

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Returns true if breakpad is enforced via management policies. In that
  // case, |breakpad_enabled| is set to the value enforced by policies.
  virtual bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);
#endif

#if defined(OS_ANDROID)
  // Returns the descriptor key of the android minidump global descriptor.
  virtual int GetAndroidMinidumpDescriptor();
#endif

#if defined(OS_MACOSX)
  // Install additional breakpad filter callbacks.
  virtual void InstallAdditionalFilters(BreakpadRef breakpad);
#endif

  // Returns true if breakpad should run in the given process type.
  virtual bool EnableBreakpadForProcess(const std::string& process_type);
};

}  // namespace breakpad

#endif  // COMPONENTS_BREAKPAD_APP_BREAKPAD_CLIENT_H_
