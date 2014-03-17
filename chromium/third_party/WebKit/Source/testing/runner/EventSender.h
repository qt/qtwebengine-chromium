/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

/*
  EventSender class:
  Bound to a JavaScript window.eventSender object using
  CppBoundClass::bindToJavascript(), this allows layout tests to fire DOM events.
*/

#ifndef EventSender_h
#define EventSender_h

#include "CppBoundClass.h"
#include "public/platform/WebPoint.h"
#include "public/testing/WebScopedPtr.h"
#include "public/testing/WebTask.h"
#include "public/web/WebDragOperation.h"
#include "public/web/WebInputEvent.h"

namespace blink {
class WebDragData;
class WebView;
struct WebContextMenuData;
}

namespace WebTestRunner {

class TestInterfaces;
class WebTestDelegate;

class EventSender : public CppBoundClass {
public:
    explicit EventSender(TestInterfaces*);
    ~EventSender();

    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }
    void setWebView(blink::WebView* webView) { m_webView = webView; }

    void setContextMenuData(const blink::WebContextMenuData&);

    // Resets some static variable state.
    void reset();

    // Simulate drag&drop system call.
    void doDragDrop(const blink::WebDragData&, blink::WebDragOperationsMask);

    // Test helper for dragging out images.
    void dumpFilenameBeingDragged(const CppArgumentList&, CppVariant*);

    // JS callback methods.
    void contextClick(const CppArgumentList&, CppVariant*);
    void mouseDown(const CppArgumentList&, CppVariant*);
    void mouseUp(const CppArgumentList&, CppVariant*);
    void mouseMoveTo(const CppArgumentList&, CppVariant*);
    void leapForward(const CppArgumentList&, CppVariant*);
    void keyDown(const CppArgumentList&, CppVariant*);
    void dispatchMessage(const CppArgumentList&, CppVariant*);
    // FIXME: These aren't really events. They should be moved to layout controller.
    void textZoomIn(const CppArgumentList&, CppVariant*);
    void textZoomOut(const CppArgumentList&, CppVariant*);
    void zoomPageIn(const CppArgumentList&, CppVariant*);
    void zoomPageOut(const CppArgumentList&, CppVariant*);
    void setPageScaleFactor(const CppArgumentList&, CppVariant*);

    void mouseDragBegin(const CppArgumentList&, CppVariant*);
    void mouseDragEnd(const CppArgumentList&, CppVariant*);
    void mouseMomentumBegin(const CppArgumentList&, CppVariant*);
    void mouseMomentumScrollBy(const CppArgumentList&, CppVariant*);
    void mouseMomentumEnd(const CppArgumentList&, CppVariant*);
    void mouseScrollBy(const CppArgumentList&, CppVariant*);
    void continuousMouseScrollBy(const CppArgumentList&, CppVariant*);
    void scheduleAsynchronousClick(const CppArgumentList&, CppVariant*);
    void scheduleAsynchronousKeyDown(const CppArgumentList&, CppVariant*);
    void beginDragWithFiles(const CppArgumentList&, CppVariant*);
    CppVariant dragMode;

    void addTouchPoint(const CppArgumentList&, CppVariant*);
    void cancelTouchPoint(const CppArgumentList&, CppVariant*);
    void clearTouchPoints(const CppArgumentList&, CppVariant*);
    void releaseTouchPoint(const CppArgumentList&, CppVariant*);
    void setTouchModifier(const CppArgumentList&, CppVariant*);
    void touchCancel(const CppArgumentList&, CppVariant*);
    void touchEnd(const CppArgumentList&, CppVariant*);
    void touchMove(const CppArgumentList&, CppVariant*);
    void touchStart(const CppArgumentList&, CppVariant*);
    void updateTouchPoint(const CppArgumentList&, CppVariant*);

    void gestureFlingCancel(const CppArgumentList&, CppVariant*);
    void gestureFlingStart(const CppArgumentList&, CppVariant*);
    void gestureScrollBegin(const CppArgumentList&, CppVariant*);
    void gestureScrollEnd(const CppArgumentList&, CppVariant*);
    void gestureScrollFirstPoint(const CppArgumentList&, CppVariant*);
    void gestureScrollUpdate(const CppArgumentList&, CppVariant*);
    void gestureScrollUpdateWithoutPropagation(const CppArgumentList&, CppVariant*);
    void gestureTap(const CppArgumentList&, CppVariant*);
    void gestureTapDown(const CppArgumentList&, CppVariant*);
    void gestureShowPress(const CppArgumentList&, CppVariant*);
    void gestureTapCancel(const CppArgumentList&, CppVariant*);
    void gestureLongPress(const CppArgumentList&, CppVariant*);
    void gestureLongTap(const CppArgumentList&, CppVariant*);
    void gestureTwoFingerTap(const CppArgumentList&, CppVariant*);
    void gestureEvent(blink::WebInputEvent::Type, const CppArgumentList&);

    // Setting this to false makes EventSender not force layout() calls.
    // This makes it possible to test the standard WebCore event dispatch.
    CppVariant forceLayoutOnEvents;

    // Unimplemented stubs
    void enableDOMUIEventLogging(const CppArgumentList&, CppVariant*);
    void fireKeyboardEventsToElement(const CppArgumentList&, CppVariant*);
    void clearKillRing(const CppArgumentList&, CppVariant*);

    // Properties used in layout tests.
#if defined(OS_WIN)
    CppVariant wmKeyDown;
    CppVariant wmKeyUp;
    CppVariant wmChar;
    CppVariant wmDeadChar;
    CppVariant wmSysKeyDown;
    CppVariant wmSysKeyUp;
    CppVariant wmSysChar;
    CppVariant wmSysDeadChar;
#endif

    WebTaskList* taskList() { return &m_taskList; }

private:
    blink::WebView* webview() { return m_webView; }

    // Returns true if dragMode is true.
    bool isDragMode() { return dragMode.isBool() && dragMode.toBoolean(); }

    bool shouldForceLayoutOnEvents() const { return forceLayoutOnEvents.isBool() && forceLayoutOnEvents.toBoolean(); }

    // Sometimes we queue up mouse move and mouse up events for drag drop
    // handling purposes. These methods dispatch the event.
    void doMouseMove(const blink::WebMouseEvent&);
    void doMouseUp(const blink::WebMouseEvent&);
    static void doLeapForward(int milliseconds);
    void replaySavedEvents();

    // Helper to return the button type given a button code
    static blink::WebMouseEvent::Button getButtonTypeFromButtonNumber(int);

    // Helper to extract the button number from the optional argument in
    // mouseDown and mouseUp
    static int getButtonNumberFromSingleArg(const CppArgumentList&);

    // Returns true if the specified key code passed in needs a shift key
    // modifier to be passed into the generated event.
    bool needsShiftModifier(int);

    void finishDragAndDrop(const blink::WebMouseEvent&, blink::WebDragOperation);
    void updateClickCountForButton(blink::WebMouseEvent::Button);

    // Compose a touch event from the current touch points and send it.
    void sendCurrentTouchEvent(const blink::WebInputEvent::Type);

    // Init a mouse wheel event from the given args.
    void initMouseWheelEvent(const CppArgumentList&, CppVariant*, bool continuous, blink::WebMouseWheelEvent*);

    WebTaskList m_taskList;

    TestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
    blink::WebView* m_webView;

    WebScopedPtr<blink::WebContextMenuData> m_lastContextMenuData;

    // Location of the touch point that initiated a gesture.
    blink::WebPoint m_currentGestureLocation;

    // Location of last mouseMoveTo event.
    static blink::WebPoint lastMousePos;

    // Currently pressed mouse button (Left/Right/Middle or None)
    static blink::WebMouseEvent::Button pressedButton;

    // The last button number passed to mouseDown and mouseUp.
    // Used to determine whether the click count continues to
    // increment or not.
    static blink::WebMouseEvent::Button lastButtonType;
};

}

#endif // EventSender_h
