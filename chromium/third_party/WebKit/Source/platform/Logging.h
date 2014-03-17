/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
 */

#ifndef Logging_h
#define Logging_h

#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"

#if !LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif

namespace WebCore {

PLATFORM_EXPORT extern WTFLogChannel LogNotYetImplemented;
PLATFORM_EXPORT extern WTFLogChannel LogFrames;
PLATFORM_EXPORT extern WTFLogChannel LogLoading;
PLATFORM_EXPORT extern WTFLogChannel LogPopupBlocking;
PLATFORM_EXPORT extern WTFLogChannel LogEvents;
PLATFORM_EXPORT extern WTFLogChannel LogEditing;
PLATFORM_EXPORT extern WTFLogChannel LogLiveConnect;
PLATFORM_EXPORT extern WTFLogChannel LogIconDatabase;
PLATFORM_EXPORT extern WTFLogChannel LogSQLDatabase;
PLATFORM_EXPORT extern WTFLogChannel LogSpellingAndGrammar;
PLATFORM_EXPORT extern WTFLogChannel LogBackForward;
PLATFORM_EXPORT extern WTFLogChannel LogHistory;
PLATFORM_EXPORT extern WTFLogChannel LogPlatformLeaks;
PLATFORM_EXPORT extern WTFLogChannel LogResourceLoading;
PLATFORM_EXPORT extern WTFLogChannel LogNetwork;
PLATFORM_EXPORT extern WTFLogChannel LogFTP;
PLATFORM_EXPORT extern WTFLogChannel LogThreading;
PLATFORM_EXPORT extern WTFLogChannel LogStorageAPI;
PLATFORM_EXPORT extern WTFLogChannel LogMedia;
PLATFORM_EXPORT extern WTFLogChannel LogPlugins;
PLATFORM_EXPORT extern WTFLogChannel LogArchives;
PLATFORM_EXPORT extern WTFLogChannel LogProgress;
PLATFORM_EXPORT extern WTFLogChannel LogFileAPI;
PLATFORM_EXPORT extern WTFLogChannel LogWebAudio;
PLATFORM_EXPORT extern WTFLogChannel LogCompositing;
PLATFORM_EXPORT extern WTFLogChannel LogGamepad;

PLATFORM_EXPORT WTFLogChannel* getChannelFromName(const String& channelName);

}

#endif // !LOG_DISABLED

#endif // Logging_h
