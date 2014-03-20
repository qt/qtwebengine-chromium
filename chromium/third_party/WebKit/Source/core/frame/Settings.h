/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef Settings_h
#define Settings_h

#include "SettingsMacros.h"
#include "core/editing/EditingBehaviorTypes.h"
#include "core/frame/SettingsDelegate.h"
#include "platform/Timer.h"
#include "platform/fonts/GenericFontFamilySettings.h"
#include "platform/geometry/IntSize.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Page; // For inspector, remove after http://crbug.com/327476

enum EditableLinkBehavior {
    EditableLinkDefaultBehavior,
    EditableLinkAlwaysLive,
    EditableLinkOnlyLiveWithShiftKey,
    EditableLinkLiveWhenNotFocused,
    EditableLinkNeverLive
};

class Settings {
    WTF_MAKE_NONCOPYABLE(Settings); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<Settings> create();

    GenericFontFamilySettings& genericFontFamilySettings() { return m_genericFontFamilySettings; }

    void setTextAutosizingEnabled(bool);
    bool textAutosizingEnabled() const;

    // Compensates for poor text legibility on mobile devices. This value is
    // multiplied by the font scale factor when performing text autosizing of
    // websites that do not set an explicit viewport description.
    void setDeviceScaleAdjustment(float);
    float deviceScaleAdjustment() const;

    // Only set by Layout Tests, and only used if textAutosizingEnabled() returns true.
    void setTextAutosizingWindowSizeOverride(const IntSize&);
    const IntSize& textAutosizingWindowSizeOverride() const { return m_textAutosizingWindowSizeOverride; }

    // Clients that execute script should call ScriptController::canExecuteScripts()
    // instead of this function. ScriptController::canExecuteScripts() checks the
    // HTML sandbox, plug-in sandboxing, and other important details.
    bool isScriptEnabled() const { return m_isScriptEnabled; }
    void setScriptEnabled(bool);

    SETTINGS_GETTERS_AND_SETTERS

    // FIXME: This does not belong here.
    static void setMockScrollbarsEnabled(bool flag);
    static bool mockScrollbarsEnabled();

    // FIXME: naming_utilities.py isn't smart enough to handle OpenGL yet.
    // It could handle "GL", but that seems a bit overly broad.
    void setOpenGLMultisamplingEnabled(bool flag);
    bool openGLMultisamplingEnabled() { return m_openGLMultisamplingEnabled; }

    void setDelegate(SettingsDelegate*);

private:
    Settings();

    void invalidate(SettingsDelegate::ChangeType);

    // FIXME: pageOfShame() is a hack for the inspector code:
    // http://crbug.com/327476
    Page* pageOfShame() const;

    SettingsDelegate* m_delegate;

    GenericFontFamilySettings m_genericFontFamilySettings;
    float m_deviceScaleAdjustment;
    IntSize m_textAutosizingWindowSizeOverride;
    bool m_textAutosizingEnabled : 1;

    SETTINGS_MEMBER_VARIABLES

    bool m_isScriptEnabled : 1;
    bool m_openGLMultisamplingEnabled : 1;
};

} // namespace WebCore

#endif // Settings_h
