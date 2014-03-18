/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NavigatorID.h"

#include "core/frame/NavigatorBase.h"

#if !defined(WEBCORE_NAVIGATOR_PLATFORM) && OS(POSIX) && !OS(MACOSX)
#include "wtf/StdLibExtras.h"
#include <sys/utsname.h>
#endif

#ifndef WEBCORE_NAVIGATOR_PRODUCT
#define WEBCORE_NAVIGATOR_PRODUCT "Gecko"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT

namespace WebCore {

String NavigatorID::appName(const NavigatorBase*)
{
    return "Netscape";
}

String NavigatorID::appVersion(const NavigatorBase* navigator)
{
    // Version is everything in the user agent string past the "Mozilla/" prefix.
    const String& agent = navigator->userAgent();
    return agent.substring(agent.find('/') + 1);
}

String NavigatorID::userAgent(const NavigatorBase* navigator)
{
    return navigator->userAgent();
}

String NavigatorID::platform(const NavigatorBase*)
{
#if defined(WEBCORE_NAVIGATOR_PLATFORM)
    return WEBCORE_NAVIGATOR_PLATFORM;
#elif OS(MACOSX)
    // Match Safari and Mozilla on Mac x86.
    return "MacIntel";
#elif OS(WIN)
    // Match Safari and Mozilla on Windows.
    return "Win32";
#else // Unix-like systems
    struct utsname osname;
    DEFINE_STATIC_LOCAL(String, platformName, (uname(&osname) >= 0 ? String(osname.sysname) + String(" ") + String(osname.machine) : emptyString()));
    return platformName;
#endif
}

String NavigatorID::appCodeName(const NavigatorBase*)
{
    return "Mozilla";
}

String NavigatorID::product(const NavigatorBase*)
{
    return WEBCORE_NAVIGATOR_PRODUCT;
}

} // namespace WebCore
