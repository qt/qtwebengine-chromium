// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include <algorithm>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {

namespace {

const struct {
  const char* name;
  GLImplementation implementation;
} kGLImplementationNamePairs[] = {
  { kGLImplementationDesktopName, kGLImplementationDesktopGL },
  { kGLImplementationOSMesaName, kGLImplementationOSMesaGL },
#if defined(OS_MACOSX)
  { kGLImplementationAppleName, kGLImplementationAppleGL },
#endif
  { kGLImplementationEGLName, kGLImplementationEGLGLES2 },
  { kGLImplementationMockName, kGLImplementationMockGL }
};

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementation g_gl_implementation = kGLImplementationNone;
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* unused) {
  if (g_libraries) {
    // We do not call base::UnloadNativeLibrary() for these libraries as
    // unloading libGL without closing X display is not allowed. See
    // crbug.com/250813 for details.
    delete g_libraries;
    g_libraries = NULL;
  }
}

bool ExportsCoreFunctionsFromGetProcAddress(GLImplementation implementation) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationOSMesaGL:
    case kGLImplementationAppleGL:
    case kGLImplementationMockGL:
      return true;
    case kGLImplementationEGLGLES2:
      return false;
    default:
      NOTREACHED();
      return true;
  }
}

}

base::ThreadLocalPointer<GLApi>* g_current_gl_context_tls = NULL;
OSMESAApi* g_current_osmesa_context;

#if defined(OS_WIN)

EGLApi* g_current_egl_context;
WGLApi* g_current_wgl_context;

#elif defined(USE_X11)

EGLApi* g_current_egl_context;
GLXApi* g_current_glx_context;

#elif defined(USE_OZONE)

EGLApi* g_current_egl_context;

#elif defined(OS_ANDROID)

EGLApi* g_current_egl_context;

#endif

GLImplementation GetNamedGLImplementation(const std::string& name) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGLImplementationNamePairs); ++i) {
    if (name == kGLImplementationNamePairs[i].name)
      return kGLImplementationNamePairs[i].implementation;
  }

  return kGLImplementationNone;
}

const char* GetGLImplementationName(GLImplementation implementation) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGLImplementationNamePairs); ++i) {
    if (implementation == kGLImplementationNamePairs[i].implementation)
      return kGLImplementationNamePairs[i].name;
  }

  return "unknown";
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = implementation;
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation;
}

bool HasDesktopGLFeatures() {
  return kGLImplementationDesktopGL == g_gl_implementation ||
         kGLImplementationOSMesaGL == g_gl_implementation ||
         kGLImplementationAppleGL == g_gl_implementation;
}

void AddGLNativeLibrary(base::NativeLibrary library) {
  DCHECK(library);

  if (!g_libraries) {
    g_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(CleanupNativeLibraries, NULL);
  }

  g_libraries->push_back(library);
}

void UnloadGLNativeLibraries() {
  CleanupNativeLibraries(NULL);
}

void SetGLGetProcAddressProc(GLGetProcAddressProc proc) {
  DCHECK(proc);
  g_get_proc_address = proc;
}

void* GetGLCoreProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

  if (g_libraries) {
    for (size_t i = 0; i < g_libraries->size(); ++i) {
      void* proc = base::GetFunctionPointerFromNativeLibrary((*g_libraries)[i],
                                                             name);
      if (proc)
        return proc;
    }
  }
  if (ExportsCoreFunctionsFromGetProcAddress(g_gl_implementation) &&
      g_get_proc_address) {
    void* proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  return NULL;
}

void* GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

  void* proc = GetGLCoreProcAddress(name);
  if (!proc && g_get_proc_address) {
    proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  return proc;
}

}  // namespace gfx
