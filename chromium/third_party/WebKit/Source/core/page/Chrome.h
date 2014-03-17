/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef Chrome_h
#define Chrome_h

#include "core/loader/NavigationPolicy.h"
#include "core/page/FocusDirection.h"
#include "platform/Cursor.h"
#include "platform/HostWindow.h"
#include "wtf/Forward.h"

namespace WebCore {

class ChromeClient;
class ColorChooser;
class ColorChooserClient;
class DateTimeChooser;
class DateTimeChooserClient;
class FileChooser;
class FloatRect;
class Frame;
class Geolocation;
class HTMLInputElement;
class HitTestResult;
class IntRect;
class Node;
class Page;
class PopupMenu;
class PopupMenuClient;
class PopupOpeningObserver;
class SearchPopupMenu;

struct DateTimeChooserParameters;
struct ViewportDescription;
struct WindowFeatures;

class Chrome : public HostWindow {
public:
    ~Chrome();

    static PassOwnPtr<Chrome> create(Page*, ChromeClient*);

    ChromeClient& client() { return *m_client; }

    // HostWindow methods.
    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE;
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE;
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) OVERRIDE;
    virtual IntPoint screenToRootView(const IntPoint&) const OVERRIDE;
    virtual IntRect rootViewToScreen(const IntRect&) const OVERRIDE;
    virtual blink::WebScreenInfo screenInfo() const OVERRIDE;

    virtual void scheduleAnimation() OVERRIDE;

    void contentsSizeChanged(Frame*, const IntSize&) const;
    void layoutUpdated(Frame*) const;

    void setCursor(const Cursor&);

    void setWindowRect(const FloatRect&) const;
    FloatRect windowRect() const;

    FloatRect pageRect() const;

    void focus() const;
    void unfocus() const;

    bool canTakeFocus(FocusDirection) const;
    void takeFocus(FocusDirection) const;

    void focusedNodeChanged(Node*) const;

    void show(NavigationPolicy = NavigationPolicyIgnore) const;

    bool canRunModal() const;
    bool canRunModalNow() const;
    void runModal() const;

    void setWindowFeatures(const WindowFeatures&) const;

    bool toolbarsVisible() const;
    bool statusbarVisible() const;
    bool scrollbarsVisible() const;
    bool menubarVisible() const;

    bool canRunBeforeUnloadConfirmPanel();
    bool runBeforeUnloadConfirmPanel(const String& message, Frame*);

    void closeWindowSoon();

    void runJavaScriptAlert(Frame*, const String&);
    bool runJavaScriptConfirm(Frame*, const String&);
    bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);
    void setStatusbarText(Frame*, const String&);

    IntRect windowResizerRect() const;

    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

    void setToolTip(const HitTestResult&);

    void print(Frame*);

    PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color& initialColor);
    PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&);
    void openTextDataListChooser(HTMLInputElement&);

    void runOpenPanel(Frame*, PassRefPtr<FileChooser>);
    void enumerateChosenDirectory(FileChooser*);

    void dispatchViewportPropertiesDidChange(const ViewportDescription&) const;

    bool hasOpenedPopup() const;
    PassRefPtr<PopupMenu> createPopupMenu(Frame&, PopupMenuClient*) const;

    void registerPopupOpeningObserver(PopupOpeningObserver*);
    void unregisterPopupOpeningObserver(PopupOpeningObserver*);

private:
    Chrome(Page*, ChromeClient*);
    void notifyPopupOpeningObservers() const;

    Page* m_page;
    ChromeClient* m_client;
    Vector<PopupOpeningObserver*> m_popupOpeningObservers;
};

}

#endif // Chrome_h
