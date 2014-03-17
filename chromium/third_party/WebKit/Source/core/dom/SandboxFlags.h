/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SandboxFlags_h
#define SandboxFlags_h

#include "wtf/Forward.h"

namespace WebCore {

enum SandboxFlag {
    // See http://www.whatwg.org/specs/web-apps/current-work/#attr-iframe-sandbox for a list of the sandbox flags.
    SandboxNone = 0,
    SandboxNavigation = 1,
    SandboxPlugins = 1 << 1,
    SandboxOrigin = 1 << 2,
    SandboxForms = 1 << 3,
    SandboxScripts = 1 << 4,
    SandboxTopNavigation = 1 << 5,
    SandboxPopups = 1 << 6, // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=12393
    SandboxAutomaticFeatures = 1 << 7,
    SandboxSeamlessIframes = 1 << 8,
    SandboxPointerLock = 1 << 9,
    SandboxDocumentDomain = 1 << 10,
    SandboxAll = -1 // Mask with all bits set to 1.
};

typedef int SandboxFlags;

SandboxFlags parseSandboxPolicy(const String& policy, String& invalidTokensErrorMessage);

}

#endif
