// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_interceptions.h"

#include <atomic>
#include <type_traits>

#include <ntstatus.h>

#include "sandbox/win/src/interception_agent.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

namespace {

// When an std::atomic is of an enum type, MSVC sneaks in a memcpy when reimplement_cast-ing
// it back to an integer. This causes a DEP crash when launching a sandboxed process,
// since memcpy is implemented in ntdll.dll, and the code is trying to call it while in
// the middle of having its system calls intercepted. To work around this, we make the atomic
// itself an int, which gets rid of the problematic cast.
std::atomic<std::underlying_type_t<SectionLoadState>> g_section_load_state {
    static_cast<std::underlying_type_t<SectionLoadState>>(SectionLoadState::kBeforeKernel32)};

void UpdateSectionLoadState(SectionLoadState new_state) {
  g_section_load_state = static_cast<std::underlying_type_t<SectionLoadState>>(new_state);
}

const char KERNEL32_DLL_NAME[] = "kernel32.dll";
}  // namespace

SectionLoadState GetSectionLoadState() {
  return static_cast<SectionLoadState>(g_section_load_state.load(std::memory_order_relaxed));
}

// Hooks NtMapViewOfSection to detect the load of DLLs. If hot patching is
// required for this dll, this functions patches it.
NTSTATUS WINAPI
TargetNtMapViewOfSection(NtMapViewOfSectionFunction orig_MapViewOfSection,
                         HANDLE section,
                         HANDLE process,
                         PVOID* base,
                         ULONG_PTR zero_bits,
                         SIZE_T commit_size,
                         PLARGE_INTEGER offset,
                         PSIZE_T view_size,
                         SECTION_INHERIT inherit,
                         ULONG allocation_type,
                         ULONG protect) {
  NTSTATUS ret = orig_MapViewOfSection(section, process, base, zero_bits,
                                       commit_size, offset, view_size, inherit,
                                       allocation_type, protect);

  do {
    if (!NT_SUCCESS(ret))
      break;

    if (!IsSameProcess(process))
      break;

    // Only check for verifier.dll or kernel32.dll loading if we haven't moved
    // past that state yet.
    if (GetSectionLoadState() == SectionLoadState::kBeforeKernel32) {
      const char* ansi_module_name =
          GetAnsiImageInfoFromModule(reinterpret_cast<HMODULE>(*base));

      // _strnicmp below may hit read access violations for some sections. We
      // find what looks like a valid export directory for a PE module but the
      // pointer to the module name will be pointing to invalid memory.
      __try {
        if (ansi_module_name &&
            (GetNtExports()->_strnicmp(ansi_module_name, KERNEL32_DLL_NAME,
                                       sizeof(KERNEL32_DLL_NAME)) == 0)) {
          UpdateSectionLoadState(SectionLoadState::kAfterKernel32);
        }
      } __except (EXCEPTION_EXECUTE_HANDLER) {
      }
    }

    // Assume the Windows heap isn't initialized until we've loaded kernel32. We
    // don't expect that we will need to patch any modules before kernel32.dll
    // is loaded as they should all depend on kernel32.dll, and Windows may load
    // sections before it's safe to call into the Windows heap (e.g. AppVerifier
    // or internal Windows feature key checking).
    if (GetSectionLoadState() == SectionLoadState::kBeforeKernel32) {
      break;
    }

    if (!InitHeap())
      break;

    if (!IsValidImageSection(section, base, offset, view_size))
      break;

    UINT image_flags;
    UNICODE_STRING* module_name =
        GetImageInfoFromModule(reinterpret_cast<HMODULE>(*base), &image_flags);
    UNICODE_STRING* file_name = GetBackingFilePath(*base);

    if ((!module_name) && (image_flags & MODULE_HAS_CODE)) {
      // If the module has no exports we retrieve the module name from the
      // full path of the mapped section.
      module_name = ExtractModuleName(file_name);
    }

    InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

    if (agent) {
      if (!agent->OnDllLoad(file_name, module_name, *base)) {
        // Interception agent is demanding to un-map the module.
        GetNtExports()->UnmapViewOfSection(process, *base);
        *base = nullptr;
        ret = STATUS_UNSUCCESSFUL;
      }
    }

    if (module_name)
      operator delete(module_name, NT_ALLOC);

    if (file_name)
      operator delete(file_name, NT_ALLOC);

  } while (false);

  return ret;
}

NTSTATUS WINAPI
TargetNtUnmapViewOfSection(NtUnmapViewOfSectionFunction orig_UnmapViewOfSection,
                           HANDLE process,
                           PVOID base) {
  NTSTATUS ret = orig_UnmapViewOfSection(process, base);

  if (!NT_SUCCESS(ret))
    return ret;

  if (!IsSameProcess(process))
    return ret;

  InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

  if (agent)
    agent->OnDllUnload(base);

  return ret;
}

}  // namespace sandbox
