/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/frame/Settings.h"

#include "core/inspector/InspectorInstrumentation.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace WebCore {

// NOTEs
//  1) EditingMacBehavior comprises builds on Mac;
//  2) EditingWindowsBehavior comprises builds on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS/Android (and then abusing the terminology);
//  4) EditingAndroidBehavior comprises Android builds.
// 99) MacEditingBehavior is used a fallback.
static EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if OS(MACOSX)
    EditingMacBehavior
#elif OS(WIN)
    EditingWindowsBehavior
#elif OS(ANDROID)
    EditingAndroidBehavior
#else // Rest of the UNIX-like systems
    EditingUnixBehavior
#endif
    ;
}

static const bool defaultUnifiedTextCheckerEnabled = false;
#if OS(MACOSX)
static const bool defaultSmartInsertDeleteEnabled = true;
#else
static const bool defaultSmartInsertDeleteEnabled = false;
#endif
#if OS(WIN)
static const bool defaultSelectTrailingWhitespaceEnabled = true;
#else
static const bool defaultSelectTrailingWhitespaceEnabled = false;
#endif

Settings::Settings()
    : m_deviceScaleAdjustment(1.0f)
#if HACK_FORCE_TEXT_AUTOSIZING_ON_DESKTOP
    , m_textAutosizingWindowSizeOverride(320, 480)
    , m_textAutosizingEnabled(true)
#else
    , m_textAutosizingEnabled(false)
#endif
    SETTINGS_INITIALIZER_LIST
    , m_isScriptEnabled(false)
    , m_openGLMultisamplingEnabled(false)
{
}

PassOwnPtr<Settings> Settings::create()
{
    return adoptPtr(new Settings);
}

SETTINGS_SETTER_BODIES

void Settings::setDelegate(SettingsDelegate* delegate)
{
    m_delegate = delegate;
}

void Settings::invalidate(SettingsDelegate::ChangeType changeType)
{
    if (m_delegate)
        m_delegate->settingsChanged(changeType);
}

// This is a total hack and should be removed.
Page* Settings::pageOfShame() const
{
    if (!m_delegate)
        return 0;
    return m_delegate->page();
}

void Settings::setTextAutosizingEnabled(bool textAutosizingEnabled)
{
    if (m_textAutosizingEnabled == textAutosizingEnabled)
        return;

    m_textAutosizingEnabled = textAutosizingEnabled;
    invalidate(SettingsDelegate::StyleChange);
}

bool Settings::textAutosizingEnabled() const
{
    return InspectorInstrumentation::overrideTextAutosizing(pageOfShame(), m_textAutosizingEnabled);
}

// FIXME: Move to Settings.in once make_settings can understand IntSize.
void Settings::setTextAutosizingWindowSizeOverride(const IntSize& textAutosizingWindowSizeOverride)
{
    if (m_textAutosizingWindowSizeOverride == textAutosizingWindowSizeOverride)
        return;

    m_textAutosizingWindowSizeOverride = textAutosizingWindowSizeOverride;
    invalidate(SettingsDelegate::StyleChange);
}

void Settings::setDeviceScaleAdjustment(float deviceScaleAdjustment)
{
    m_deviceScaleAdjustment = deviceScaleAdjustment;
    invalidate(SettingsDelegate::TextAutosizingChange);
}

float Settings::deviceScaleAdjustment() const
{
    return InspectorInstrumentation::overrideFontScaleFactor(pageOfShame(), m_deviceScaleAdjustment);
}

void Settings::setScriptEnabled(bool isScriptEnabled)
{
    m_isScriptEnabled = isScriptEnabled;
    InspectorInstrumentation::scriptsEnabled(pageOfShame(), m_isScriptEnabled);
}

void Settings::setMockScrollbarsEnabled(bool flag)
{
    ScrollbarTheme::setMockScrollbarsEnabled(flag);
}

bool Settings::mockScrollbarsEnabled()
{
    return ScrollbarTheme::mockScrollbarsEnabled();
}

void Settings::setOpenGLMultisamplingEnabled(bool flag)
{
    if (m_openGLMultisamplingEnabled == flag)
        return;

    m_openGLMultisamplingEnabled = flag;
    invalidate(SettingsDelegate::MultisamplingChange);
}

} // namespace WebCore
