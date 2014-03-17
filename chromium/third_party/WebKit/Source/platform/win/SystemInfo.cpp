/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/win/SystemInfo.h"

#if _WIN32_WINNT_WINBLUE
#include <versionhelpers.h>
#endif

#include <windows.h>

namespace WebCore {

#ifndef _WIN32_WINNT_WINBLUE
static bool IsWindowsVistaOrGreater()
{
    OSVERSIONINFOEXW osvi = { };
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_VISTA);
    osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_VISTA);
    DWORDLONG conditoin = 0;
    VER_SET_CONDITION(conditoin, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditoin, VER_MINORVERSION, VER_GREATER_EQUAL);
    return !!::VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditoin);
}
#endif

bool isWindowsVistaOrGreater()
{
    static bool initialized = false;
    static bool cachedIsWindowsVistaOrGreater;

    if (!initialized) {
        initialized = true;
        cachedIsWindowsVistaOrGreater = IsWindowsVistaOrGreater();
    }
    return cachedIsWindowsVistaOrGreater;
}

} // namespace WebCore
