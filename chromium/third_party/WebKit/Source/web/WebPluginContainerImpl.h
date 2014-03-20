/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebPluginContainerImpl_h
#define WebPluginContainerImpl_h

#include "WebPluginContainer.h"
#include "core/plugins/PluginView.h"
#include "platform/Widget.h"

#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

struct NPObject;

namespace WebCore {
class GestureEvent;
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class MouseEvent;
class PlatformGestureEvent;
class ResourceError;
class ResourceResponse;
class TouchEvent;
class WheelEvent;
class Widget;
}

namespace blink {

struct WebPrintParams;

class ScrollbarGroup;
class WebPlugin;
class WebPluginLoadObserver;
class WebExternalTextureLayer;

class WebPluginContainerImpl : public WebCore::PluginView, public WebPluginContainer {
public:
    static PassRefPtr<WebPluginContainerImpl> create(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return adoptRef(new WebPluginContainerImpl(element, webPlugin));
    }

    // PluginView methods
    virtual WebLayer* platformLayer() const OVERRIDE;
    virtual NPObject* scriptableObject() OVERRIDE;
    virtual bool getFormValue(String&) OVERRIDE;
    virtual bool supportsKeyboardFocus() const OVERRIDE;
    virtual bool supportsInputMethod() const OVERRIDE;
    virtual bool canProcessDrag() const OVERRIDE;
    virtual bool wantsWheelEvents() OVERRIDE;

    // Widget methods
    virtual void setFrameRect(const WebCore::IntRect&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void invalidateRect(const WebCore::IntRect&);
    virtual void setFocus(bool);
    virtual void show();
    virtual void hide();
    virtual void handleEvent(WebCore::Event*);
    virtual void frameRectsChanged();
    virtual void setParentVisible(bool);
    virtual void setParent(WebCore::Widget*);
    virtual void widgetPositionsUpdated();
    virtual void clipRectChanged() OVERRIDE;
    virtual bool isPluginContainer() const { return true; }
    virtual void eventListenersRemoved() OVERRIDE;

    // WebPluginContainer methods
    virtual WebElement element();
    virtual void invalidate();
    virtual void invalidateRect(const WebRect&);
    virtual void scrollRect(int dx, int dy, const WebRect&);
    virtual void reportGeometry();
    virtual void allowScriptObjects();
    virtual void clearScriptObjects();
    virtual NPObject* scriptableObjectForElement();
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed);
    virtual void loadFrameRequest(const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData);
    virtual void zoomLevelChanged(double zoomLevel);
    virtual bool isRectTopmost(const WebRect&);
    virtual void requestTouchEventType(TouchEventRequestType);
    virtual void setWantsWheelEvents(bool);
    virtual WebPoint windowToLocalPoint(const WebPoint&);
    virtual WebPoint localToWindowPoint(const WebPoint&);

    // This cannot be null.
    WebPlugin* plugin() { return m_webPlugin; }
    void setPlugin(WebPlugin*);

    virtual float deviceScaleFactor();
    virtual float pageScaleFactor();
    virtual float pageZoomFactor();

    virtual void setWebLayer(WebLayer*);

    // Printing interface. The plugin can support custom printing
    // (which means it controls the layout, number of pages etc).
    // Whether the plugin supports its own paginated print. The other print
    // interface methods are called only if this method returns true.
    bool supportsPaginatedPrint() const;
    // If the plugin content should not be scaled to the printable area of
    // the page, then this method should return true.
    bool isPrintScalingDisabled() const;
    // Sets up printing at the specified WebPrintParams. Returns the number of pages to be printed at these settings.
    int printBegin(const WebPrintParams&) const;
    // Prints the page specified by pageNumber (0-based index) into the supplied canvas.
    bool printPage(int pageNumber, WebCore::GraphicsContext* gc);
    // Ends the print operation.
    void printEnd();

    // Copy the selected text.
    void copy();

    // Pass the edit command to the plugin.
    bool executeEditCommand(const WebString& name);
    bool executeEditCommand(const WebString& name, const WebString& value);

    // Resource load events for the plugin's source data:
    virtual void didReceiveResponse(const WebCore::ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(const char *data, int dataLength) OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFailLoading(const WebCore::ResourceError&) OVERRIDE;

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

    ScrollbarGroup* scrollbarGroup();

    void willStartLiveResize();
    void willEndLiveResize();

    bool paintCustomOverhangArea(WebCore::GraphicsContext*, const WebCore::IntRect&, const WebCore::IntRect&, const WebCore::IntRect&);

private:
    WebPluginContainerImpl(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin);
    ~WebPluginContainerImpl();

    void handleMouseEvent(WebCore::MouseEvent*);
    void handleDragEvent(WebCore::MouseEvent*);
    void handleWheelEvent(WebCore::WheelEvent*);
    void handleKeyboardEvent(WebCore::KeyboardEvent*);
    void handleTouchEvent(WebCore::TouchEvent*);
    void handleGestureEvent(WebCore::GestureEvent*);

    void synthesizeMouseEventIfPossible(WebCore::TouchEvent*);

    void focusPlugin();

    void calculateGeometry(const WebCore::IntRect& frameRect,
                           WebCore::IntRect& windowRect,
                           WebCore::IntRect& clipRect,
                           Vector<WebCore::IntRect>& cutOutRects);
    WebCore::IntRect windowClipRect() const;
    void windowCutOutRects(const WebCore::IntRect& frameRect,
                           Vector<WebCore::IntRect>& cutOutRects);

    WebCore::HTMLPlugInElement* m_element;
    WebPlugin* m_webPlugin;
    Vector<WebPluginLoadObserver*> m_pluginLoadObservers;

    WebLayer* m_webLayer;

    // The associated scrollbar group object, created lazily. Used for Pepper
    // scrollbars.
    OwnPtr<ScrollbarGroup> m_scrollbarGroup;

    TouchEventRequestType m_touchEventRequestType;
    bool m_wantsWheelEvents;
};

inline WebPluginContainerImpl* toPluginContainerImpl(WebCore::Widget* widget)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!widget || widget->isPluginContainer());
    // We need to ensure that the object is actually of type PluginContainer
    // as there are many subclasses of Widget.
    return static_cast<WebPluginContainerImpl*>(widget);
}

inline WebPluginContainerImpl* toPluginContainerImpl(WebPluginContainer* container)
{
    // Unlike Widget, we need not worry about object type for container.
    // WebPluginContainerImpl is the only subclass of WebPluginContainer.
    return static_cast<WebPluginContainerImpl*>(container);
}

// This will catch anyone doing an unnecessary cast.
void toPluginContainerImpl(const WebPluginContainerImpl*);

} // namespace blink

#endif
