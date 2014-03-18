/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TestPlugin_h
#define TestPlugin_h

#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebExternalTextureLayerClient.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "public/platform/WebNonCopyable.h"
#include "public/testing/WebScopedPtr.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginContainer.h"
#include <string>

namespace WebTestRunner {

class WebTestDelegate;

// A fake implemention of blink::WebPlugin for testing purposes.
//
// It uses WebGraphicsContext3D to paint a scene consisiting of a primitive
// over a background. The primitive and background can be customized using
// the following plugin parameters:
// primitive: none (default), triangle.
// background-color: black (default), red, green, blue.
// primitive-color: black (default), red, green, blue.
// opacity: [0.0 - 1.0]. Default is 1.0.
//
// Whether the plugin accepts touch events or not can be customized using the
// 'accepts-touch' plugin parameter (defaults to false).
class TestPlugin : public blink::WebPlugin, public blink::WebExternalTextureLayerClient, public blink::WebNonCopyable {
public:
    static TestPlugin* create(blink::WebFrame*, const blink::WebPluginParams&, WebTestDelegate*);
    virtual ~TestPlugin();

    static const blink::WebString& mimeType();

    // WebPlugin methods:
    virtual bool initialize(blink::WebPluginContainer*);
    virtual void destroy();
    virtual NPObject* scriptableObject() { return 0; }
    virtual bool canProcessDrag() const { return m_canProcessDrag; }
    virtual void paint(blink::WebCanvas*, const blink::WebRect&) { }
    virtual void updateGeometry(const blink::WebRect& frameRect, const blink::WebRect& clipRect, const blink::WebVector<blink::WebRect>& cutOutsRects, bool isVisible);
    virtual void updateFocus(bool) { }
    virtual void updateVisibility(bool) { }
    virtual bool acceptsInputEvents() { return true; }
    virtual bool handleInputEvent(const blink::WebInputEvent&, blink::WebCursorInfo&);
    virtual bool handleDragStatusUpdate(blink::WebDragStatus, const blink::WebDragData&, blink::WebDragOperationsMask, const blink::WebPoint& position, const blink::WebPoint& screenPosition);
    virtual void didReceiveResponse(const blink::WebURLResponse&) { }
    virtual void didReceiveData(const char* data, int dataLength) { }
    virtual void didFinishLoading() { }
    virtual void didFailLoading(const blink::WebURLError&) { }
    virtual void didFinishLoadingFrameRequest(const blink::WebURL&, void* notifyData) { }
    virtual void didFailLoadingFrameRequest(const blink::WebURL&, void* notifyData, const blink::WebURLError&) { }
    virtual bool isPlaceholder() { return false; }

    // WebExternalTextureLayerClient methods:
    virtual blink::WebGraphicsContext3D* context() { return 0; }
    virtual bool prepareMailbox(blink::WebExternalTextureMailbox*, blink::WebExternalBitmap*);
    virtual void mailboxReleased(const blink::WebExternalTextureMailbox&);

private:
    TestPlugin(blink::WebFrame*, const blink::WebPluginParams&, WebTestDelegate*);

    enum Primitive {
        PrimitiveNone,
        PrimitiveTriangle
    };

    struct Scene {
        Primitive primitive;
        unsigned backgroundColor[3];
        unsigned primitiveColor[3];
        float opacity;

        unsigned vbo;
        unsigned program;
        int colorLocation;
        int positionLocation;

        Scene()
            : primitive(PrimitiveNone)
            , opacity(1.0f) // Fully opaque.
            , vbo(0)
            , program(0)
            , colorLocation(-1)
            , positionLocation(-1)
        {
            backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 0;
            primitiveColor[0] = primitiveColor[1] = primitiveColor[2] = 0;
        }
    };

    // Functions for parsing plugin parameters.
    Primitive parsePrimitive(const blink::WebString&);
    void parseColor(const blink::WebString&, unsigned color[3]);
    float parseOpacity(const blink::WebString&);
    bool parseBoolean(const blink::WebString&);

    // Functions for loading and drawing scene.
    bool initScene();
    void drawScene();
    void destroyScene();
    bool initProgram();
    bool initPrimitive();
    void drawPrimitive();
    unsigned loadShader(unsigned type, const std::string& source);
    unsigned loadProgram(const std::string& vertexSource, const std::string& fragmentSource);

    blink::WebFrame* m_frame;
    WebTestDelegate* m_delegate;
    blink::WebPluginContainer* m_container;

    blink::WebRect m_rect;
    blink::WebGraphicsContext3D* m_context;
    unsigned m_colorTexture;
    blink::WebExternalTextureMailbox m_mailbox;
    bool m_mailboxChanged;
    unsigned m_framebuffer;
    Scene m_scene;
    WebScopedPtr<blink::WebExternalTextureLayer> m_layer;

    blink::WebPluginContainer::TouchEventRequestType m_touchEventRequest;
    // Requests touch events from the WebPluginContainerImpl multiple times to tickle webkit.org/b/108381
    bool m_reRequestTouchEvents;
    bool m_printEventDetails;
    bool m_printUserGestureStatus;
    bool m_canProcessDrag;
};

}

#endif // TestPlugin_h
