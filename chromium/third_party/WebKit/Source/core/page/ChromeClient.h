/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include "core/accessibility/AXObjectCache.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationPolicy.h"
#include "core/frame/ConsoleTypes.h"
#include "core/page/FocusDirection.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/HostWindow.h"
#include "platform/PopupMenu.h"
#include "platform/PopupMenuClient.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"


#ifndef __OBJC__
class NSMenu;
class NSResponder;
#endif

namespace WebCore {

class AXObject;
class ColorChooser;
class ColorChooserClient;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FileChooser;
class FloatRect;
class Frame;
class Geolocation;
class GraphicsContext3D;
class GraphicsLayer;
class GraphicsLayerFactory;
class HitTestResult;
class HTMLInputElement;
class IntRect;
class Node;
class Page;
class PagePopup;
class PagePopupClient;
class PagePopupDriver;
class PopupContainer;
class PopupMenuClient;
class SecurityOrigin;
class Widget;

struct DateTimeChooserParameters;
struct FrameLoadRequest;
struct GraphicsDeviceAdapter;
struct ViewportDescription;
struct WindowFeatures;

class ChromeClient {
public:
    virtual void chromeDestroyed() = 0;

    virtual void setWindowRect(const FloatRect&) = 0;
    virtual FloatRect windowRect() = 0;

    virtual FloatRect pageRect() = 0;

    virtual void focus() = 0;
    virtual void unfocus() = 0;

    virtual bool canTakeFocus(FocusDirection) = 0;
    virtual void takeFocus(FocusDirection) = 0;

    virtual void focusedNodeChanged(Node*) = 0;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page. Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    // The FrameLoadRequest parameter is only for ChromeClient to check if the
    // request could be fulfilled. The ChromeClient should not load the request.
    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy, ShouldSendReferrer) = 0;
    virtual void show(NavigationPolicy) = 0;

    virtual bool canRunModal() = 0;
    virtual void runModal() = 0;

    virtual void setToolbarsVisible(bool) = 0;
    virtual bool toolbarsVisible() = 0;

    virtual void setStatusbarVisible(bool) = 0;
    virtual bool statusbarVisible() = 0;

    virtual void setScrollbarsVisible(bool) = 0;
    virtual bool scrollbarsVisible() = 0;

    virtual void setMenubarVisible(bool) = 0;
    virtual bool menubarVisible() = 0;

    virtual void setResizable(bool) = 0;

    virtual bool shouldReportDetailedMessageForSource(const String& source) = 0;
    virtual void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID, const String& stackTrace) = 0;

    virtual bool canRunBeforeUnloadConfirmPanel() = 0;
    virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame*) = 0;

    virtual void closeWindowSoon() = 0;

    virtual void runJavaScriptAlert(Frame*, const String&) = 0;
    virtual bool runJavaScriptConfirm(Frame*, const String&) = 0;
    virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) = 0;
    virtual void setStatusbarText(const String&) = 0;
    virtual bool tabsToLinks() = 0;

    virtual void* webView() const = 0;

    virtual IntRect windowResizerRect() const = 0;

    // Methods used by HostWindow.
    virtual void invalidateContentsAndRootView(const IntRect&) = 0;
    virtual void invalidateContentsForSlowScroll(const IntRect&) = 0;
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) = 0;
    virtual IntPoint screenToRootView(const IntPoint&) const = 0;
    virtual IntRect rootViewToScreen(const IntRect&) const = 0;
    virtual blink::WebScreenInfo screenInfo() const = 0;
    virtual void setCursor(const Cursor&) = 0;
    virtual void scheduleAnimation() = 0;
    // End methods used by HostWindow.

    virtual bool isCompositorFramePending() const = 0;

    virtual void dispatchViewportPropertiesDidChange(const ViewportDescription&) const { }

    virtual void contentsSizeChanged(Frame*, const IntSize&) const = 0;
    virtual void deviceOrPageScaleFactorChanged() const { }
    virtual void layoutUpdated(Frame*) const { }

    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) = 0;

    virtual void setToolTip(const String&, TextDirection) = 0;

    virtual void print(Frame*) = 0;
    virtual bool shouldRubberBandInDirection(ScrollDirection) const = 0;

    virtual void annotatedRegionsChanged() = 0;

    virtual bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&) = 0;

    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) = 0;

    // This function is used for:
    //  - Mandatory date/time choosers if !ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    //  - Date/time choosers for types for which RenderTheme::supportsCalendarPicker
    //    returns true, if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    //  - <datalist> UI for date/time input types regardless of
    //    ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) = 0;

    virtual void openTextDataListChooser(HTMLInputElement&) = 0;

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) = 0;

    // Asychronous request to enumerate all files in a directory chosen by the user.
    virtual void enumerateChosenDirectory(FileChooser*) = 0;

    // Notification that the given form element has changed. This function
    // will be called frequently, so handling should be very fast.
    virtual void formStateDidChange(const Node*) = 0;

    // Allows ports to customize the type of graphics layers created by this page.
    virtual GraphicsLayerFactory* graphicsLayerFactory() const { return 0; }

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) = 0;
    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerFlush() = 0;
    // Returns whether or not the client can render the composited layer,
    // regardless of the settings.
    virtual bool allowsAcceleratedCompositing() const { return true; }

    enum CompositingTrigger {
        ThreeDTransformTrigger = 1 << 0,
        VideoTrigger = 1 << 1,
        PluginTrigger = 1 << 2,
        CanvasTrigger = 1 << 3,
        AnimationTrigger = 1 << 4,
        FilterTrigger = 1 << 5,
        ScrollableInnerFrameTrigger = 1 << 6,
        AllTriggers = 0xFFFFFFFF
    };
    typedef unsigned CompositingTriggerFlags;

    // Returns a bitfield indicating conditions that can trigger the compositor.
    virtual CompositingTriggerFlags allowedCompositingTriggers() const { return static_cast<CompositingTriggerFlags>(AllTriggers); }

    virtual void enterFullScreenForElement(Element*) { }
    virtual void exitFullScreenForElement(Element*) { }

    virtual void needTouchEvents(bool) = 0;

    virtual void setTouchAction(TouchAction) = 0;

    // Checks if there is an opened popup, called by RenderMenuList::showPopup().
    virtual bool hasOpenedPopup() const = 0;
    virtual PassRefPtr<PopupMenu> createPopupMenu(Frame&, PopupMenuClient*) const = 0;
    // Creates a PagePopup object, and shows it beside originBoundsInRootView.
    // The return value can be 0.
    virtual PagePopup* openPagePopup(PagePopupClient*, const IntRect& originBoundsInRootView) = 0;
    virtual void closePagePopup(PagePopup*) = 0;
    // For testing.
    virtual void setPagePopupDriver(PagePopupDriver*) = 0;
    virtual void resetPagePopupDriver() = 0;

    // FIXME: Should these be on a different client interface?
    virtual bool isPasswordGenerationEnabled() const { return false; }
    virtual void openPasswordGenerator(HTMLInputElement*) { }

    virtual void postAccessibilityNotification(AXObject*, AXObjectCache::AXNotification) { }
    virtual String acceptLanguages() = 0;

    enum DialogType {
        AlertDialog = 0,
        ConfirmDialog = 1,
        PromptDialog = 2,
        HTMLDialog = 3
    };
    virtual bool shouldRunModalDialogDuringPageDismissal(const DialogType&, const String&, Document::PageDismissalType) const { return true; }

    virtual void numWheelEventHandlersChanged(unsigned) = 0;

    virtual bool isSVGImageChromeClient() const { return false; }

    virtual bool requestPointerLock() { return false; }
    virtual void requestPointerUnlock() { }
    virtual bool isPointerLocked() { return false; }

    virtual FloatSize minimumWindowSize() const { return FloatSize(100, 100); };

    virtual bool isEmptyChromeClient() const { return false; }
    virtual bool isChromeClientImpl() const { return false; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element> >&) { };
    virtual void didChangeValueInTextField(HTMLInputElement&) { }
    virtual void didEndEditingOnTextField(HTMLInputElement&) { }
    virtual void handleKeyboardEventOnTextField(HTMLInputElement&, KeyboardEvent&) { }

    // Input mehtod editor related functions.
    virtual void didCancelCompositionOnSelectionChange() { }
    virtual void willSetInputMethodState() { }

    // Notifies the client of a new popup widget.  The client should place
    // and size the widget with the given bounds, relative to the screen.
    // If handleExternal is true, then drawing and input handling for the
    // popup will be handled by the external embedder.
    virtual void popupOpened(PopupContainer* popupContainer, const IntRect& bounds,
                             bool handleExternal) = 0;

    // Notifies the client a popup was closed.
    virtual void popupClosed(PopupContainer* popupContainer) = 0;

protected:
    virtual ~ChromeClient() { }
};

}
#endif // ChromeClient_h
