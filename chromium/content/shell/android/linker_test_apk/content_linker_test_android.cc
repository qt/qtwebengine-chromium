// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"
#include "content/shell/android/linker_test_apk/content_linker_test_linker_tests.h"
#include "content/shell/android/shell_jni_registrar.h"
#include "content/shell/app/shell_main_delegate.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!content::RegisterLibraryLoaderEntryHook(env))
    return -1;

  // To be called only from the UI thread.  If loading the library is done on
  // a separate thread, this should be moved elsewhere.
  if (!content::android::RegisterShellJni(env))
    return -1;

  if (!content::RegisterLinkerTestsJni(env))
    return -1;

  content::Compositor::Initialize();
  content::SetContentMainDelegate(new content::ShellMainDelegate());
  return JNI_VERSION_1_4;
}
