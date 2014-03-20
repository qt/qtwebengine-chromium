/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "core/html/canvas/WebGLRenderingContext.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/ANGLEInstancedArrays.h"
#include "core/html/canvas/EXTFragDepth.h"
#include "core/html/canvas/EXTTextureFilterAnisotropic.h"
#include "core/html/canvas/OESElementIndexUint.h"
#include "core/html/canvas/OESStandardDerivatives.h"
#include "core/html/canvas/OESTextureFloat.h"
#include "core/html/canvas/OESTextureFloatLinear.h"
#include "core/html/canvas/OESTextureHalfFloat.h"
#include "core/html/canvas/OESTextureHalfFloatLinear.h"
#include "core/html/canvas/OESVertexArrayObject.h"
#include "core/html/canvas/WebGLActiveInfo.h"
#include "core/html/canvas/WebGLBuffer.h"
#include "core/html/canvas/WebGLCompressedTextureATC.h"
#include "core/html/canvas/WebGLCompressedTexturePVRTC.h"
#include "core/html/canvas/WebGLCompressedTextureS3TC.h"
#include "core/html/canvas/WebGLContextAttributes.h"
#include "core/html/canvas/WebGLContextEvent.h"
#include "core/html/canvas/WebGLContextGroup.h"
#include "core/html/canvas/WebGLDebugRendererInfo.h"
#include "core/html/canvas/WebGLDebugShaders.h"
#include "core/html/canvas/WebGLDepthTexture.h"
#include "core/html/canvas/WebGLDrawBuffers.h"
#include "core/html/canvas/WebGLFramebuffer.h"
#include "core/html/canvas/WebGLLoseContext.h"
#include "core/html/canvas/WebGLProgram.h"
#include "core/html/canvas/WebGLRenderbuffer.h"
#include "core/html/canvas/WebGLShader.h"
#include "core/html/canvas/WebGLShaderPrecisionFormat.h"
#include "core/html/canvas/WebGLTexture.h"
#include "core/html/canvas/WebGLUniformLocation.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/Frame.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderBox.h"
#include "platform/CheckedInt.h"
#include "platform/NotImplemented.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Extensions3D.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBuffer.h"

#include "wtf/PassOwnPtr.h"
#include "wtf/Uint32Array.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

const double secondsBetweenRestoreAttempts = 1.0;
const int maxGLErrorsAllowedToConsole = 256;
const unsigned maxGLActiveContexts = 16;

Vector<WebGLRenderingContext*>& WebGLRenderingContext::activeContexts()
{
    DEFINE_STATIC_LOCAL(Vector<WebGLRenderingContext*>, activeContexts, ());
    return activeContexts;
}

Vector<WebGLRenderingContext*>& WebGLRenderingContext::forciblyEvictedContexts()
{
    DEFINE_STATIC_LOCAL(Vector<WebGLRenderingContext*>, forciblyEvictedContexts, ());
    return forciblyEvictedContexts;
}

void WebGLRenderingContext::forciblyLoseOldestContext(const String& reason)
{
    size_t candidateID = oldestContextIndex();
    if (candidateID >= activeContexts().size())
        return;

    WebGLRenderingContext* candidate = activeContexts()[candidateID];

    activeContexts().remove(candidateID);

    candidate->printWarningToConsole(reason);
    InspectorInstrumentation::didFireWebGLWarning(candidate->canvas());

    // This will call deactivateContext once the context has actually been lost.
    candidate->forceLostContext(WebGLRenderingContext::SyntheticLostContext);
}

size_t WebGLRenderingContext::oldestContextIndex()
{
    if (!activeContexts().size())
        return maxGLActiveContexts;

    WebGLRenderingContext* candidate = activeContexts().first();
    size_t candidateID = 0;
    for (size_t ii = 1; ii < activeContexts().size(); ++ii) {
        WebGLRenderingContext* context = activeContexts()[ii];
        if (context->graphicsContext3D() && candidate->graphicsContext3D() && context->graphicsContext3D()->lastFlushID() < candidate->graphicsContext3D()->lastFlushID()) {
            candidate = context;
            candidateID = ii;
        }
    }

    return candidateID;
}

IntSize WebGLRenderingContext::oldestContextSize()
{
    IntSize size;

    size_t candidateID = oldestContextIndex();
    if (candidateID < activeContexts().size()) {
        WebGLRenderingContext* candidate = activeContexts()[candidateID];
        size.setWidth(candidate->drawingBufferWidth());
        size.setHeight(candidate->drawingBufferHeight());
    }

    return size;
}

void WebGLRenderingContext::activateContext(WebGLRenderingContext* context)
{
    unsigned removedContexts = 0;
    while (activeContexts().size() >= maxGLActiveContexts && removedContexts < maxGLActiveContexts) {
        forciblyLoseOldestContext("WARNING: Too many active WebGL contexts. Oldest context will be lost.");
        removedContexts++;
    }

    if (!activeContexts().contains(context))
        activeContexts().append(context);
}

void WebGLRenderingContext::deactivateContext(WebGLRenderingContext* context, bool addToEvictedList)
{
    size_t position = activeContexts().find(context);
    if (position != WTF::kNotFound)
        activeContexts().remove(position);

    if (addToEvictedList && !forciblyEvictedContexts().contains(context))
        forciblyEvictedContexts().append(context);
}

void WebGLRenderingContext::willDestroyContext(WebGLRenderingContext* context)
{
    size_t position = forciblyEvictedContexts().find(context);
    if (position != WTF::kNotFound)
        forciblyEvictedContexts().remove(position);

    deactivateContext(context, false);

    // Try to re-enable the oldest inactive contexts.
    while(activeContexts().size() < maxGLActiveContexts && forciblyEvictedContexts().size()) {
        WebGLRenderingContext* evictedContext = forciblyEvictedContexts().first();
        if (!evictedContext->m_restoreAllowed) {
            forciblyEvictedContexts().remove(0);
            continue;
        }

        IntSize desiredSize = evictedContext->m_drawingBuffer->adjustSize(evictedContext->clampedCanvasSize());

        // If there's room in the pixel budget for this context, restore it.
        if (!desiredSize.isEmpty()) {
            forciblyEvictedContexts().remove(0);
            evictedContext->forceRestoreContext();
            activeContexts().append(evictedContext);
        }
        break;
    }
}

class WebGLRenderingContextEvictionManager : public ContextEvictionManager {
public:
    void forciblyLoseOldestContext(const String& reason) {
        WebGLRenderingContext::forciblyLoseOldestContext(reason);
    };
    IntSize oldestContextSize() {
        return WebGLRenderingContext::oldestContextSize();
    };
};

namespace {

    class ScopedDrawingBufferBinder {
    public:
        ScopedDrawingBufferBinder(DrawingBuffer* drawingBuffer, WebGLFramebuffer* framebufferBinding)
            : m_drawingBuffer(drawingBuffer)
            , m_framebufferBinding(framebufferBinding)
        {
            // Commit DrawingBuffer if needed (e.g., for multisampling)
            if (!m_framebufferBinding && m_drawingBuffer)
                m_drawingBuffer->commit();
        }

        ~ScopedDrawingBufferBinder()
        {
            // Restore DrawingBuffer if needed
            if (!m_framebufferBinding && m_drawingBuffer)
                m_drawingBuffer->bind();
        }

    private:
        DrawingBuffer* m_drawingBuffer;
        WebGLFramebuffer* m_framebufferBinding;
    };

    Platform3DObject objectOrZero(WebGLObject* object)
    {
        return object ? object->object() : 0;
    }

    GC3Dint clamp(GC3Dint value, GC3Dint min, GC3Dint max)
    {
        if (value < min)
            value = min;
        if (value > max)
            value = max;
        return value;
    }

    // Return true if a character belongs to the ASCII subset as defined in
    // GLSL ES 1.0 spec section 3.1.
    bool validateCharacter(unsigned char c)
    {
        // Printing characters are valid except " $ ` @ \ ' DEL.
        if (c >= 32 && c <= 126
            && c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
            return true;
        // Horizontal tab, line feed, vertical tab, form feed, carriage return
        // are also valid.
        if (c >= 9 && c <= 13)
            return true;
        return false;
    }

    bool isPrefixReserved(const String& name)
    {
        if (name.startsWith("gl_") || name.startsWith("webgl_") || name.startsWith("_webgl_"))
            return true;
        return false;
    }

    // Strips comments from shader text. This allows non-ASCII characters
    // to be used in comments without potentially breaking OpenGL
    // implementations not expecting characters outside the GLSL ES set.
    class StripComments {
    public:
        StripComments(const String& str)
            : m_parseState(BeginningOfLine)
            , m_sourceString(str)
            , m_length(str.length())
            , m_position(0)
        {
            parse();
        }

        String result()
        {
            return m_builder.toString();
        }

    private:
        bool hasMoreCharacters() const
        {
            return (m_position < m_length);
        }

        void parse()
        {
            while (hasMoreCharacters()) {
                process(current());
                // process() might advance the position.
                if (hasMoreCharacters())
                    advance();
            }
        }

        void process(UChar);

        bool peek(UChar& character) const
        {
            if (m_position + 1 >= m_length)
                return false;
            character = m_sourceString[m_position + 1];
            return true;
        }

        UChar current()
        {
            ASSERT_WITH_SECURITY_IMPLICATION(m_position < m_length);
            return m_sourceString[m_position];
        }

        void advance()
        {
            ++m_position;
        }

        static bool isNewline(UChar character)
        {
            // Don't attempt to canonicalize newline related characters.
            return (character == '\n' || character == '\r');
        }

        void emit(UChar character)
        {
            m_builder.append(character);
        }

        enum ParseState {
            // Have not seen an ASCII non-whitespace character yet on
            // this line. Possible that we might see a preprocessor
            // directive.
            BeginningOfLine,

            // Have seen at least one ASCII non-whitespace character
            // on this line.
            MiddleOfLine,

            // Handling a preprocessor directive. Passes through all
            // characters up to the end of the line. Disables comment
            // processing.
            InPreprocessorDirective,

            // Handling a single-line comment. The comment text is
            // replaced with a single space.
            InSingleLineComment,

            // Handling a multi-line comment. Newlines are passed
            // through to preserve line numbers.
            InMultiLineComment
        };

        ParseState m_parseState;
        String m_sourceString;
        unsigned m_length;
        unsigned m_position;
        StringBuilder m_builder;
    };

    void StripComments::process(UChar c)
    {
        if (isNewline(c)) {
            // No matter what state we are in, pass through newlines
            // so we preserve line numbers.
            emit(c);

            if (m_parseState != InMultiLineComment)
                m_parseState = BeginningOfLine;

            return;
        }

        UChar temp = 0;
        switch (m_parseState) {
        case BeginningOfLine:
            if (WTF::isASCIISpace(c)) {
                emit(c);
                break;
            }

            if (c == '#') {
                m_parseState = InPreprocessorDirective;
                emit(c);
                break;
            }

            // Transition to normal state and re-handle character.
            m_parseState = MiddleOfLine;
            process(c);
            break;

        case MiddleOfLine:
            if (c == '/' && peek(temp)) {
                if (temp == '/') {
                    m_parseState = InSingleLineComment;
                    emit(' ');
                    advance();
                    break;
                }

                if (temp == '*') {
                    m_parseState = InMultiLineComment;
                    // Emit the comment start in case the user has
                    // an unclosed comment and we want to later
                    // signal an error.
                    emit('/');
                    emit('*');
                    advance();
                    break;
                }
            }

            emit(c);
            break;

        case InPreprocessorDirective:
            // No matter what the character is, just pass it
            // through. Do not parse comments in this state. This
            // might not be the right thing to do long term, but it
            // should handle the #error preprocessor directive.
            emit(c);
            break;

        case InSingleLineComment:
            // The newline code at the top of this function takes care
            // of resetting our state when we get out of the
            // single-line comment. Swallow all other characters.
            break;

        case InMultiLineComment:
            if (c == '*' && peek(temp) && temp == '/') {
                emit('*');
                emit('/');
                m_parseState = MiddleOfLine;
                advance();
                break;
            }

            // Swallow all other characters. Unclear whether we may
            // want or need to just emit a space per character to try
            // to preserve column numbers for debugging purposes.
            break;
        }
    }

    GraphicsContext3D::Attributes adjustAttributes(const GraphicsContext3D::Attributes& attributes, Settings* settings)
    {
        GraphicsContext3D::Attributes adjustedAttributes = attributes;
        if (adjustedAttributes.antialias) {
            if (settings && !settings->openGLMultisamplingEnabled())
                adjustedAttributes.antialias = false;
        }

        return adjustedAttributes;
    }
} // namespace anonymous

class WebGLRenderingContextLostCallback : public GraphicsContext3D::ContextLostCallback {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebGLRenderingContextLostCallback(WebGLRenderingContext* cb) : m_context(cb) { }
    virtual void onContextLost() { m_context->forceLostContext(WebGLRenderingContext::RealLostContext); }
    virtual ~WebGLRenderingContextLostCallback() {}
private:
    WebGLRenderingContext* m_context;
};

class WebGLRenderingContextErrorMessageCallback : public GraphicsContext3D::ErrorMessageCallback {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebGLRenderingContextErrorMessageCallback(WebGLRenderingContext* cb) : m_context(cb) { }
    virtual void onErrorMessage(const String& message, GC3Dint)
    {
        if (m_context->m_synthesizedErrorsToConsole)
            m_context->printGLErrorToConsole(message);
        InspectorInstrumentation::didFireWebGLErrorOrWarning(m_context->canvas(), message);
    }
    virtual ~WebGLRenderingContextErrorMessageCallback() { }
private:
    WebGLRenderingContext* m_context;
};

PassOwnPtr<WebGLRenderingContext> WebGLRenderingContext::create(HTMLCanvasElement* canvas, WebGLContextAttributes* attrs)
{
    Document& document = canvas->document();
    Frame* frame = document.frame();
    if (!frame)
        return nullptr;
    Settings* settings = frame->settings();

    // The FrameLoaderClient might block creation of a new WebGL context despite the page settings; in
    // particular, if WebGL contexts were lost one or more times via the GL_ARB_robustness extension.
    if (!frame->loader().client()->allowWebGL(settings && settings->webGLEnabled())) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Web page was not allowed to create a WebGL context."));
        return nullptr;
    }

    GraphicsContext3D::Attributes requestedAttributes = attrs ? attrs->attributes() : GraphicsContext3D::Attributes();
    requestedAttributes.noExtensions = true;
    requestedAttributes.shareResources = true;
    requestedAttributes.preferDiscreteGPU = true;
    requestedAttributes.topDocumentURL = document.topDocument()->url();

    GraphicsContext3D::Attributes attributes = adjustAttributes(requestedAttributes, settings);

    RefPtr<GraphicsContext3D> context(GraphicsContext3D::create(attributes));

    if (!context || !context->makeContextCurrent()) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Could not create a WebGL context."));
        return nullptr;
    }

    Extensions3D* extensions = context->extensions();
    if (extensions->supports("GL_EXT_debug_marker"))
        extensions->pushGroupMarkerEXT("WebGLRenderingContext");

    OwnPtr<WebGLRenderingContext> renderingContext = adoptPtr(new WebGLRenderingContext(canvas, context, attributes, requestedAttributes));
    renderingContext->suspendIfNeeded();

    if (renderingContext->m_drawingBuffer->isZeroSized()) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Could not create a WebGL context."));
        return nullptr;
    }

    return renderingContext.release();
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassRefPtr<GraphicsContext3D> context, GraphicsContext3D::Attributes attributes, GraphicsContext3D::Attributes requestedAttributes)
    : CanvasRenderingContext(passedCanvas)
    , ActiveDOMObject(&passedCanvas->document())
    , m_context(context)
    , m_drawingBuffer(0)
    , m_dispatchContextLostEventTimer(this, &WebGLRenderingContext::dispatchContextLostEvent)
    , m_restoreAllowed(false)
    , m_restoreTimer(this, &WebGLRenderingContext::maybeRestoreContext)
    , m_generatedImageCache(4)
    , m_contextLost(false)
    , m_contextLostMode(SyntheticLostContext)
    , m_attributes(attributes)
    , m_requestedAttributes(requestedAttributes)
    , m_synthesizedErrorsToConsole(true)
    , m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole)
    , m_multisamplingAllowed(false)
    , m_multisamplingObserverRegistered(false)
    , m_onePlusMaxEnabledAttribIndex(0)
    , m_onePlusMaxNonDefaultTextureUnit(0)
{
    ASSERT(m_context);
    ScriptWrappable::init(this);

    m_contextGroup = WebGLContextGroup::create();
    m_contextGroup->addContext(this);

    m_maxViewportDims[0] = m_maxViewportDims[1] = 0;
    m_context->getIntegerv(GL_MAX_VIEWPORT_DIMS, m_maxViewportDims);

    RefPtr<WebGLRenderingContextEvictionManager> contextEvictionManager = adoptRef(new WebGLRenderingContextEvictionManager());

    // Create the DrawingBuffer and initialize the platform layer.
    DrawingBuffer::PreserveDrawingBuffer preserve = m_attributes.preserveDrawingBuffer ? DrawingBuffer::Preserve : DrawingBuffer::Discard;
    m_drawingBuffer = DrawingBuffer::create(m_context.get(), clampedCanvasSize(), preserve, contextEvictionManager.release());

    if (!m_drawingBuffer->isZeroSized()) {
        m_drawingBuffer->bind();
        setupFlags();
        initializeNewContext();
    }

    // Register extensions.
    static const char* const webkitPrefix[] = { "WEBKIT_", 0, };
    static const char* const bothPrefixes[] = { "", "WEBKIT_", 0, };

    registerExtension<ANGLEInstancedArrays>(m_angleInstancedArrays);
    registerExtension<EXTTextureFilterAnisotropic>(m_extTextureFilterAnisotropic, PrefixedExtension, webkitPrefix);
    registerExtension<OESElementIndexUint>(m_oesElementIndexUint);
    registerExtension<OESStandardDerivatives>(m_oesStandardDerivatives);
    registerExtension<OESTextureFloat>(m_oesTextureFloat);
    registerExtension<OESTextureFloatLinear>(m_oesTextureFloatLinear);
    registerExtension<OESTextureHalfFloat>(m_oesTextureHalfFloat);
    registerExtension<OESTextureHalfFloatLinear>(m_oesTextureHalfFloatLinear);
    registerExtension<OESVertexArrayObject>(m_oesVertexArrayObject);
    registerExtension<WebGLCompressedTextureATC>(m_webglCompressedTextureATC, PrefixedExtension, webkitPrefix);
    registerExtension<WebGLCompressedTexturePVRTC>(m_webglCompressedTexturePVRTC, PrefixedExtension, webkitPrefix);
    registerExtension<WebGLCompressedTextureS3TC>(m_webglCompressedTextureS3TC, PrefixedExtension, bothPrefixes);
    registerExtension<WebGLDepthTexture>(m_webglDepthTexture, PrefixedExtension, bothPrefixes);
    registerExtension<WebGLDrawBuffers>(m_webglDrawBuffers);
    registerExtension<WebGLLoseContext>(m_webglLoseContext, ApprovedExtension, bothPrefixes);

    // Register draft extensions.
    registerExtension<EXTFragDepth>(m_extFragDepth, DraftExtension);

    // Register privileged extensions.
    registerExtension<WebGLDebugRendererInfo>(m_webglDebugRendererInfo, WebGLDebugRendererInfoExtension);
    registerExtension<WebGLDebugShaders>(m_webglDebugShaders, PrivilegedExtension);
}

void WebGLRenderingContext::initializeNewContext()
{
    ASSERT(!isContextLost());
    m_needsUpdate = true;
    m_markedCanvasDirty = false;
    m_activeTextureUnit = 0;
    m_packAlignment = 4;
    m_unpackAlignment = 4;
    m_unpackFlipY = false;
    m_unpackPremultiplyAlpha = false;
    m_unpackColorspaceConversion = GC3D_BROWSER_DEFAULT_WEBGL;
    m_boundArrayBuffer = 0;
    m_currentProgram = 0;
    m_framebufferBinding = 0;
    m_renderbufferBinding = 0;
    m_depthMask = true;
    m_stencilEnabled = false;
    m_stencilMask = 0xFFFFFFFF;
    m_stencilMaskBack = 0xFFFFFFFF;
    m_stencilFuncRef = 0;
    m_stencilFuncRefBack = 0;
    m_stencilFuncMask = 0xFFFFFFFF;
    m_stencilFuncMaskBack = 0xFFFFFFFF;
    m_layerCleared = false;
    m_numGLErrorsToConsoleAllowed = maxGLErrorsAllowedToConsole;

    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0;
    m_scissorEnabled = false;
    m_clearDepth = 1;
    m_clearStencil = 0;
    m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = true;

    GC3Dint numCombinedTextureImageUnits = 0;
    m_context->getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numCombinedTextureImageUnits);
    m_textureUnits.clear();
    m_textureUnits.resize(numCombinedTextureImageUnits);

    GC3Dint numVertexAttribs = 0;
    m_context->getIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribs);
    m_maxVertexAttribs = numVertexAttribs;

    m_maxTextureSize = 0;
    m_context->getIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    m_maxTextureLevel = WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize);
    m_maxCubeMapTextureSize = 0;
    m_context->getIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &m_maxCubeMapTextureSize);
    m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(m_maxCubeMapTextureSize, m_maxCubeMapTextureSize);
    m_maxRenderbufferSize = 0;
    m_context->getIntegerv(GL_MAX_RENDERBUFFER_SIZE, &m_maxRenderbufferSize);

    // These two values from EXT_draw_buffers are lazily queried.
    m_maxDrawBuffers = 0;
    m_maxColorAttachments = 0;

    m_backDrawBuffer = GL_BACK;

    m_defaultVertexArrayObject = WebGLVertexArrayObjectOES::create(this, WebGLVertexArrayObjectOES::VaoTypeDefault);
    addContextObject(m_defaultVertexArrayObject.get());
    m_boundVertexArrayObject = m_defaultVertexArrayObject;

    m_vertexAttribValue.resize(m_maxVertexAttribs);

    createFallbackBlackTextures1x1();

    IntSize canvasSize = clampedCanvasSize();
    m_drawingBuffer->reset(canvasSize);

    m_context->viewport(0, 0, canvasSize.width(), canvasSize.height());
    m_context->scissor(0, 0, canvasSize.width(), canvasSize.height());

    m_context->setContextLostCallback(adoptPtr(new WebGLRenderingContextLostCallback(this)));
    m_context->setErrorMessageCallback(adoptPtr(new WebGLRenderingContextErrorMessageCallback(this)));

    // This ensures that the context has a valid "lastFlushID" and won't be mistakenly identified as the "least recently used" context.
    m_context->flush();

    activateContext(this);
}

void WebGLRenderingContext::setupFlags()
{
    ASSERT(m_context);
    if (Page* p = canvas()->document().page()) {
        m_synthesizedErrorsToConsole = p->settings().webGLErrorsToConsoleEnabled();

        if (!m_multisamplingObserverRegistered && m_requestedAttributes.antialias) {
            m_multisamplingAllowed = m_drawingBuffer->multisample();
            p->addMultisamplingChangedObserver(this);
            m_multisamplingObserverRegistered = true;
        }
    }

    m_isGLES2NPOTStrict = !m_context->extensions()->isEnabled("GL_OES_texture_npot");
    m_isDepthStencilSupported = m_context->extensions()->isEnabled("GL_OES_packed_depth_stencil");
}

bool WebGLRenderingContext::allowPrivilegedExtensions() const
{
    if (Page* p = canvas()->document().page())
        return p->settings().privilegedWebGLExtensionsEnabled();
    return false;
}

bool WebGLRenderingContext::allowWebGLDebugRendererInfo() const
{
    return true;
}

void WebGLRenderingContext::addCompressedTextureFormat(GC3Denum format)
{
    if (!m_compressedTextureFormats.contains(format))
        m_compressedTextureFormats.append(format);
}

void WebGLRenderingContext::removeAllCompressedTextureFormats()
{
    m_compressedTextureFormats.clear();
}

WebGLRenderingContext::~WebGLRenderingContext()
{
    // Remove all references to WebGLObjects so if they are the last reference
    // they will be freed before the last context is removed from the context group.
    m_boundArrayBuffer = 0;
    m_defaultVertexArrayObject = 0;
    m_boundVertexArrayObject = 0;
    m_vertexAttrib0Buffer = 0;
    m_currentProgram = 0;
    m_framebufferBinding = 0;
    m_renderbufferBinding = 0;

    for (size_t i = 0; i < m_textureUnits.size(); ++i) {
      m_textureUnits[i].m_texture2DBinding = 0;
      m_textureUnits[i].m_textureCubeMapBinding = 0;
    }

    m_blackTexture2D = 0;
    m_blackTextureCubeMap = 0;

    detachAndRemoveAllObjects();

    // release all extensions
    for (size_t i = 0; i < m_extensions.size(); ++i)
        delete m_extensions[i];

    // Context must be removed from the group prior to the destruction of the
    // GraphicsContext3D, otherwise shared objects may not be properly deleted.
    m_contextGroup->removeContext(this);

    destroyGraphicsContext3D();

    if (m_multisamplingObserverRegistered) {
        Page* page = canvas()->document().page();
        if (page)
            page->removeMultisamplingChangedObserver(this);
    }

    willDestroyContext(this);
}

void WebGLRenderingContext::destroyGraphicsContext3D()
{
    m_contextLost = true;

    // The drawing buffer holds a context reference. It must also be destroyed
    // in order for the context to be released.
    m_drawingBuffer->releaseResources();

    if (m_context) {
        m_context->setContextLostCallback(nullptr);
        m_context->setErrorMessageCallback(nullptr);
        m_context.clear();
    }
}

void WebGLRenderingContext::markContextChanged()
{
    if (m_framebufferBinding || isContextLost())
        return;

    m_context->markContextChanged();
    m_drawingBuffer->markContentsChanged();

    m_layerCleared = false;
    RenderBox* renderBox = canvas()->renderBox();
    if (renderBox && renderBox->hasAcceleratedCompositing()) {
        m_markedCanvasDirty = true;
        canvas()->clearCopiedImage();
        renderBox->contentChanged(CanvasChanged);
    } else {
        if (!m_markedCanvasDirty) {
            m_markedCanvasDirty = true;
            canvas()->didDraw(FloatRect(FloatPoint(0, 0), clampedCanvasSize()));
        }
    }
}

bool WebGLRenderingContext::clearIfComposited(GC3Dbitfield mask)
{
    if (isContextLost())
        return false;

    if (!m_context->layerComposited() || m_layerCleared
        || m_attributes.preserveDrawingBuffer || (mask && m_framebufferBinding))
        return false;

    RefPtr<WebGLContextAttributes> contextAttributes = getContextAttributes();

    // Determine if it's possible to combine the clear the user asked for and this clear.
    bool combinedClear = mask && !m_scissorEnabled;

    m_context->disable(GL_SCISSOR_TEST);
    if (combinedClear && (mask & GL_COLOR_BUFFER_BIT))
        m_context->clearColor(m_colorMask[0] ? m_clearColor[0] : 0,
                              m_colorMask[1] ? m_clearColor[1] : 0,
                              m_colorMask[2] ? m_clearColor[2] : 0,
                              m_colorMask[3] ? m_clearColor[3] : 0);
    else
        m_context->clearColor(0, 0, 0, 0);
    m_context->colorMask(true, true, true, true);
    GC3Dbitfield clearMask = GL_COLOR_BUFFER_BIT;
    if (contextAttributes->depth()) {
        if (!combinedClear || !m_depthMask || !(mask & GL_DEPTH_BUFFER_BIT))
            m_context->clearDepth(1.0f);
        clearMask |= GL_DEPTH_BUFFER_BIT;
        m_context->depthMask(true);
    }
    if (contextAttributes->stencil()) {
        if (combinedClear && (mask & GL_STENCIL_BUFFER_BIT))
            m_context->clearStencil(m_clearStencil & m_stencilMask);
        else
            m_context->clearStencil(0);
        clearMask |= GL_STENCIL_BUFFER_BIT;
        m_context->stencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
    }

    m_drawingBuffer->clearFramebuffers(clearMask);

    restoreStateAfterClear();
    if (m_framebufferBinding)
        m_context->bindFramebuffer(GL_FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
    m_layerCleared = true;

    return combinedClear;
}

void WebGLRenderingContext::restoreStateAfterClear()
{
    if (isContextLost())
        return;

    // Restore the state that the context set.
    if (m_scissorEnabled)
        m_context->enable(GL_SCISSOR_TEST);
    m_context->clearColor(m_clearColor[0], m_clearColor[1],
                          m_clearColor[2], m_clearColor[3]);
    m_context->colorMask(m_colorMask[0], m_colorMask[1],
                         m_colorMask[2], m_colorMask[3]);
    m_context->clearDepth(m_clearDepth);
    m_context->clearStencil(m_clearStencil);
    m_context->stencilMaskSeparate(GL_FRONT, m_stencilMask);
    m_context->depthMask(m_depthMask);
}

void WebGLRenderingContext::markLayerComposited()
{
    if (!isContextLost())
        m_context->markLayerComposited();
}

void WebGLRenderingContext::paintRenderingResultsToCanvas()
{
    if (isContextLost()) {
        canvas()->clearPresentationCopy();
        return;
    }

    if (canvas()->document().printing())
        canvas()->clearPresentationCopy();

    // Until the canvas is written to by the application, the clear that
    // happened after it was composited should be ignored by the compositor.
    if (m_context->layerComposited() && !m_attributes.preserveDrawingBuffer) {
        m_drawingBuffer->paintCompositedResultsToCanvas(canvas()->buffer());

        canvas()->makePresentationCopy();
    } else
        canvas()->clearPresentationCopy();
    clearIfComposited();

    if (!m_markedCanvasDirty && !m_layerCleared)
        return;

    canvas()->clearCopiedImage();
    m_markedCanvasDirty = false;

    m_drawingBuffer->commit();
    if (!(canvas()->buffer())->copyRenderingResultsFromDrawingBuffer(m_drawingBuffer.get())) {
        canvas()->ensureUnacceleratedImageBuffer();
        if (canvas()->hasImageBuffer())
            m_context->paintRenderingResultsToCanvas(canvas()->buffer(), m_drawingBuffer.get());
    }

    if (m_framebufferBinding)
        m_context->bindFramebuffer(GL_FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
    else
        m_drawingBuffer->bind();
}

PassRefPtr<ImageData> WebGLRenderingContext::paintRenderingResultsToImageData()
{
    if (isContextLost())
        return 0;

    clearIfComposited();
    m_drawingBuffer->commit();
    int width, height;
    RefPtr<Uint8ClampedArray> imageDataPixels = m_context->paintRenderingResultsToImageData(m_drawingBuffer.get(), width, height);
    if (!imageDataPixels)
        return 0;

    if (m_framebufferBinding)
        m_context->bindFramebuffer(GL_FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
    else
        m_drawingBuffer->bind();

    return ImageData::create(IntSize(width, height), imageDataPixels);
}

void WebGLRenderingContext::reshape(int width, int height)
{
    if (isContextLost())
        return;

    // This is an approximation because at WebGLRenderingContext level we don't
    // know if the underlying FBO uses textures or renderbuffers.
    GC3Dint maxSize = std::min(m_maxTextureSize, m_maxRenderbufferSize);
    // Limit drawing buffer size to 4k to avoid memory exhaustion.
    const int sizeUpperLimit = 4096;
    maxSize = std::min(maxSize, sizeUpperLimit);
    GC3Dint maxWidth = std::min(maxSize, m_maxViewportDims[0]);
    GC3Dint maxHeight = std::min(maxSize, m_maxViewportDims[1]);
    width = clamp(width, 1, maxWidth);
    height = clamp(height, 1, maxHeight);

    if (m_needsUpdate) {
        RenderBox* renderBox = canvas()->renderBox();
        if (renderBox && renderBox->hasAcceleratedCompositing())
            renderBox->contentChanged(CanvasChanged);
        m_needsUpdate = false;
    }

    // We don't have to mark the canvas as dirty, since the newly created image buffer will also start off
    // clear (and this matches what reshape will do).
    m_drawingBuffer->reset(IntSize(width, height));
    restoreStateAfterClear();

    m_context->bindTexture(GL_TEXTURE_2D, objectOrZero(m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get()));
    m_context->bindRenderbuffer(GL_RENDERBUFFER, objectOrZero(m_renderbufferBinding.get()));
    if (m_framebufferBinding)
        m_context->bindFramebuffer(GL_FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
}

int WebGLRenderingContext::drawingBufferWidth() const
{
    return m_drawingBuffer->size().width();
}

int WebGLRenderingContext::drawingBufferHeight() const
{
    return m_drawingBuffer->size().height();
}

unsigned int WebGLRenderingContext::sizeInBytes(GC3Denum type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(GC3Dbyte);
    case GL_UNSIGNED_BYTE:
        return sizeof(GC3Dubyte);
    case GL_SHORT:
        return sizeof(GC3Dshort);
    case GL_UNSIGNED_SHORT:
        return sizeof(GC3Dushort);
    case GL_INT:
        return sizeof(GC3Dint);
    case GL_UNSIGNED_INT:
        return sizeof(GC3Duint);
    case GL_FLOAT:
        return sizeof(GC3Dfloat);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebGLRenderingContext::activeTexture(GC3Denum texture)
{
    if (isContextLost())
        return;
    if (texture - GL_TEXTURE0 >= m_textureUnits.size()) {
        synthesizeGLError(GL_INVALID_ENUM, "activeTexture", "texture unit out of range");
        return;
    }
    m_activeTextureUnit = texture - GL_TEXTURE0;
    m_context->activeTexture(texture);

    m_drawingBuffer->setActiveTextureUnit(texture);

}

void WebGLRenderingContext::attachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (isContextLost() || !validateWebGLObject("attachShader", program) || !validateWebGLObject("attachShader", shader))
        return;
    if (!program->attachShader(shader)) {
        synthesizeGLError(GL_INVALID_OPERATION, "attachShader", "shader attachment already has shader");
        return;
    }
    m_context->attachShader(objectOrZero(program), objectOrZero(shader));
    shader->onAttached();
}

void WebGLRenderingContext::bindAttribLocation(WebGLProgram* program, GC3Duint index, const String& name)
{
    if (isContextLost() || !validateWebGLObject("bindAttribLocation", program))
        return;
    if (!validateLocationLength("bindAttribLocation", name))
        return;
    if (!validateString("bindAttribLocation", name))
        return;
    if (isPrefixReserved(name)) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindAttribLocation", "reserved prefix");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "bindAttribLocation", "index out of range");
        return;
    }
    m_context->bindAttribLocation(objectOrZero(program), index, name);
}

bool WebGLRenderingContext::checkObjectToBeBound(const char* functionName, WebGLObject* object, bool& deleted)
{
    deleted = false;
    if (isContextLost())
        return false;
    if (object) {
        if (!object->validate(contextGroup(), this)) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "object not from this context");
            return false;
        }
        deleted = !object->object();
    }
    return true;
}

void WebGLRenderingContext::bindBuffer(GC3Denum target, WebGLBuffer* buffer)
{
    bool deleted;
    if (!checkObjectToBeBound("bindBuffer", buffer, deleted))
        return;
    if (deleted)
        buffer = 0;
    if (buffer && buffer->getTarget() && buffer->getTarget() != target) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindBuffer", "buffers can not be used with multiple targets");
        return;
    }
    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        m_boundVertexArrayObject->setElementArrayBuffer(buffer);
    else {
        synthesizeGLError(GL_INVALID_ENUM, "bindBuffer", "invalid target");
        return;
    }

    m_context->bindBuffer(target, objectOrZero(buffer));
    if (buffer)
        buffer->setTarget(target);
}

void WebGLRenderingContext::bindFramebuffer(GC3Denum target, WebGLFramebuffer* buffer)
{
    bool deleted;
    if (!checkObjectToBeBound("bindFramebuffer", buffer, deleted))
        return;
    if (deleted)
        buffer = 0;
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "bindFramebuffer", "invalid target");
        return;
    }
    m_framebufferBinding = buffer;
    m_drawingBuffer->setFramebufferBinding(objectOrZero(m_framebufferBinding.get()));
    if (!m_framebufferBinding) {
        // Instead of binding fb 0, bind the drawing buffer.
        m_drawingBuffer->bind();
    } else
        m_context->bindFramebuffer(target, objectOrZero(buffer));
    if (buffer)
        buffer->setHasEverBeenBound();
    applyStencilTest();
}

void WebGLRenderingContext::bindRenderbuffer(GC3Denum target, WebGLRenderbuffer* renderBuffer)
{
    bool deleted;
    if (!checkObjectToBeBound("bindRenderbuffer", renderBuffer, deleted))
        return;
    if (deleted)
        renderBuffer = 0;
    if (target != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "bindRenderbuffer", "invalid target");
        return;
    }
    m_renderbufferBinding = renderBuffer;
    m_context->bindRenderbuffer(target, objectOrZero(renderBuffer));
    if (renderBuffer)
        renderBuffer->setHasEverBeenBound();
}

void WebGLRenderingContext::bindTexture(GC3Denum target, WebGLTexture* texture)
{
    bool deleted;
    if (!checkObjectToBeBound("bindTexture", texture, deleted))
        return;
    if (deleted)
        texture = 0;
    if (texture && texture->getTarget() && texture->getTarget() != target) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindTexture", "textures can not be used with multiple targets");
        return;
    }
    GC3Dint maxLevel = 0;
    if (target == GL_TEXTURE_2D) {
        m_textureUnits[m_activeTextureUnit].m_texture2DBinding = texture;
        maxLevel = m_maxTextureLevel;

        if (!m_activeTextureUnit)
            m_drawingBuffer->setTexture2DBinding(objectOrZero(texture));

    } else if (target == GL_TEXTURE_CUBE_MAP) {
        m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding = texture;
        maxLevel = m_maxCubeMapTextureLevel;
    } else {
        synthesizeGLError(GL_INVALID_ENUM, "bindTexture", "invalid target");
        return;
    }

    m_context->bindTexture(target, objectOrZero(texture));
    if (texture) {
        texture->setTarget(target, maxLevel);
        m_onePlusMaxNonDefaultTextureUnit = max(m_activeTextureUnit + 1, m_onePlusMaxNonDefaultTextureUnit);
    } else {
        // If the disabled index is the current maximum, trace backwards to find the new max enabled texture index
        if (m_onePlusMaxNonDefaultTextureUnit == m_activeTextureUnit + 1) {
            findNewMaxNonDefaultTextureUnit();
        }
    }

    // Note: previously we used to automatically set the TEXTURE_WRAP_R
    // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
    // ES 2.0 doesn't expose this flag (a bug in the specification) and
    // otherwise the application has no control over the seams in this
    // dimension. However, it appears that supporting this properly on all
    // platforms is fairly involved (will require a HashMap from texture ID
    // in all ports), and we have not had any complaints, so the logic has
    // been removed.

}

void WebGLRenderingContext::blendColor(GC3Dfloat red, GC3Dfloat green, GC3Dfloat blue, GC3Dfloat alpha)
{
    if (isContextLost())
        return;
    m_context->blendColor(red, green, blue, alpha);
}

void WebGLRenderingContext::blendEquation(GC3Denum mode)
{
    if (isContextLost() || !validateBlendEquation("blendEquation", mode))
        return;
    m_context->blendEquation(mode);
}

void WebGLRenderingContext::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    if (isContextLost() || !validateBlendEquation("blendEquationSeparate", modeRGB) || !validateBlendEquation("blendEquationSeparate", modeAlpha))
        return;
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
}


void WebGLRenderingContext::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    if (isContextLost() || !validateBlendFuncFactors("blendFunc", sfactor, dfactor))
        return;
    m_context->blendFunc(sfactor, dfactor);
}

void WebGLRenderingContext::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    // Note: Alpha does not have the same restrictions as RGB.
    if (isContextLost() || !validateBlendFuncFactors("blendFuncSeparate", srcRGB, dstRGB))
        return;
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void WebGLRenderingContext::bufferData(GC3Denum target, long long size, GC3Denum usage)
{
    if (isContextLost())
        return;
    WebGLBuffer* buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;
    if (size < 0) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferData", "size < 0");
        return;
    }
    if (!size) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferData", "size == 0");
        return;
    }

    m_context->bufferData(target, static_cast<GC3Dsizeiptr>(size), usage);
}

void WebGLRenderingContext::bufferData(GC3Denum target, ArrayBuffer* data, GC3Denum usage)
{
    if (isContextLost())
        return;
    WebGLBuffer* buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;
    if (!data) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
        return;
    }
    m_context->bufferData(target, data->byteLength(), data->data(), usage);
}

void WebGLRenderingContext::bufferData(GC3Denum target, ArrayBufferView* data, GC3Denum usage)
{
    if (isContextLost())
        return;
    WebGLBuffer* buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;
    if (!data) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
        return;
    }

    m_context->bufferData(target, data->byteLength(), data->baseAddress(), usage);
}

void WebGLRenderingContext::bufferSubData(GC3Denum target, long long offset, ArrayBuffer* data)
{
    if (isContextLost())
        return;
    WebGLBuffer* buffer = validateBufferDataParameters("bufferSubData", target, GL_STATIC_DRAW);
    if (!buffer)
        return;
    if (offset < 0) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "offset < 0");
        return;
    }
    if (!data)
        return;

    m_context->bufferSubData(target, static_cast<GC3Dintptr>(offset), data->byteLength(), data->data());
}

void WebGLRenderingContext::bufferSubData(GC3Denum target, long long offset, ArrayBufferView* data)
{
    if (isContextLost())
        return;
    WebGLBuffer* buffer = validateBufferDataParameters("bufferSubData", target, GL_STATIC_DRAW);
    if (!buffer)
        return;
    if (offset < 0) {
        synthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "offset < 0");
        return;
    }
    if (!data)
        return;

    m_context->bufferSubData(target, static_cast<GC3Dintptr>(offset), data->byteLength(), data->baseAddress());
}

GC3Denum WebGLRenderingContext::checkFramebufferStatus(GC3Denum target)
{
    if (isContextLost())
        return GL_FRAMEBUFFER_UNSUPPORTED;
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "checkFramebufferStatus", "invalid target");
        return 0;
    }
    if (!m_framebufferBinding || !m_framebufferBinding->object())
        return GL_FRAMEBUFFER_COMPLETE;
    const char* reason = "framebuffer incomplete";
    GC3Denum result = m_framebufferBinding->checkStatus(&reason);
    if (result != GL_FRAMEBUFFER_COMPLETE) {
        emitGLWarning("checkFramebufferStatus", reason);
        return result;
    }
    result = m_context->checkFramebufferStatus(target);
    return result;
}

void WebGLRenderingContext::clear(GC3Dbitfield mask)
{
    if (isContextLost())
        return;
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GL_INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (!clearIfComposited(mask))
        m_context->clear(mask);
    markContextChanged();
}

void WebGLRenderingContext::clearColor(GC3Dfloat r, GC3Dfloat g, GC3Dfloat b, GC3Dfloat a)
{
    if (isContextLost())
        return;
    if (std::isnan(r))
        r = 0;
    if (std::isnan(g))
        g = 0;
    if (std::isnan(b))
        b = 0;
    if (std::isnan(a))
        a = 1;
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
    m_context->clearColor(r, g, b, a);
}

void WebGLRenderingContext::clearDepth(GC3Dfloat depth)
{
    if (isContextLost())
        return;
    m_clearDepth = depth;
    m_context->clearDepth(depth);
}

void WebGLRenderingContext::clearStencil(GC3Dint s)
{
    if (isContextLost())
        return;
    m_clearStencil = s;
    m_context->clearStencil(s);
}

void WebGLRenderingContext::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    if (isContextLost())
        return;
    m_colorMask[0] = red;
    m_colorMask[1] = green;
    m_colorMask[2] = blue;
    m_colorMask[3] = alpha;
    m_context->colorMask(red, green, blue, alpha);
}

void WebGLRenderingContext::compileShader(WebGLShader* shader)
{
    if (isContextLost() || !validateWebGLObject("compileShader", shader))
        return;
    m_context->compileShader(objectOrZero(shader));
}

void WebGLRenderingContext::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
                                                 GC3Dsizei height, GC3Dint border, ArrayBufferView* data)
{
    if (isContextLost())
        return;
    if (!validateTexFuncLevel("compressedTexImage2D", target, level))
        return;

    if (!validateCompressedTexFormat(internalformat)) {
        synthesizeGLError(GL_INVALID_ENUM, "compressedTexImage2D", "invalid internalformat");
        return;
    }
    if (border) {
        synthesizeGLError(GL_INVALID_VALUE, "compressedTexImage2D", "border not 0");
        return;
    }
    if (!validateCompressedTexDimensions("compressedTexImage2D", NotTexSubImage2D, target, level, width, height, internalformat))
        return;
    if (!validateCompressedTexFuncData("compressedTexImage2D", width, height, internalformat, data))
        return;

    WebGLTexture* tex = validateTextureBinding("compressedTexImage2D", target, true);
    if (!tex)
        return;
    if (!isGLES2NPOTStrict()) {
        if (level && WebGLTexture::isNPOT(width, height)) {
            synthesizeGLError(GL_INVALID_VALUE, "compressedTexImage2D", "level > 0 not power of 2");
            return;
        }
    }
    graphicsContext3D()->compressedTexImage2D(target, level, internalformat, width, height,
                                              border, data->byteLength(), data->baseAddress());
    tex->setLevelInfo(target, level, internalformat, width, height, GL_UNSIGNED_BYTE);
}

void WebGLRenderingContext::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                                    GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView* data)
{
    if (isContextLost())
        return;
    if (!validateTexFuncLevel("compressedTexSubImage2D", target, level))
        return;
    if (!validateCompressedTexFormat(format)) {
        synthesizeGLError(GL_INVALID_ENUM, "compressedTexSubImage2D", "invalid format");
        return;
    }
    if (!validateCompressedTexFuncData("compressedTexSubImage2D", width, height, format, data))
        return;

    WebGLTexture* tex = validateTextureBinding("compressedTexSubImage2D", target, true);
    if (!tex)
        return;

    if (format != tex->getInternalFormat(target, level)) {
        synthesizeGLError(GL_INVALID_OPERATION, "compressedTexSubImage2D", "format does not match texture format");
        return;
    }

    if (!validateCompressedTexSubDimensions("compressedTexSubImage2D", target, level, xoffset, yoffset, width, height, format, tex))
        return;

    graphicsContext3D()->compressedTexSubImage2D(target, level, xoffset, yoffset,
                                                 width, height, format, data->byteLength(), data->baseAddress());
}

bool WebGLRenderingContext::validateSettableTexFormat(const char* functionName, GC3Denum format)
{
    if (GraphicsContext3D::getClearBitsByFormat(format) & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "format can not be set, only rendered to");
        return false;
    }
    return true;
}

void WebGLRenderingContext::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    if (isContextLost())
        return;
    if (!validateTexFuncParameters("copyTexImage2D", NotTexSubImage2D, target, level, internalformat, width, height, border, internalformat, GL_UNSIGNED_BYTE))
        return;
    if (!validateSettableTexFormat("copyTexImage2D", internalformat))
        return;
    WebGLTexture* tex = validateTextureBinding("copyTexImage2D", target, true);
    if (!tex)
        return;
    if (!isTexInternalFormatColorBufferCombinationValid(internalformat, boundFramebufferColorFormat())) {
        synthesizeGLError(GL_INVALID_OPERATION, "copyTexImage2D", "framebuffer is incompatible format");
        return;
    }
    if (!isGLES2NPOTStrict() && level && WebGLTexture::isNPOT(width, height)) {
        synthesizeGLError(GL_INVALID_VALUE, "copyTexImage2D", "level > 0 not power of 2");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "copyTexImage2D", reason);
        return;
    }
    clearIfComposited();
    ScopedDrawingBufferBinder binder(m_drawingBuffer.get(), m_framebufferBinding.get());
    m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    // FIXME: if the framebuffer is not complete, none of the below should be executed.
    tex->setLevelInfo(target, level, internalformat, width, height, GL_UNSIGNED_BYTE);
}

void WebGLRenderingContext::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLost())
        return;
    if (!validateTexFuncLevel("copyTexSubImage2D", target, level))
        return;
    WebGLTexture* tex = validateTextureBinding("copyTexSubImage2D", target, true);
    if (!tex)
        return;
    if (!validateSize("copyTexSubImage2D", xoffset, yoffset) || !validateSize("copyTexSubImage2D", width, height))
        return;
    // Before checking if it is in the range, check if overflow happens first.
    Checked<GC3Dint, RecordOverflow> maxX = xoffset;
    maxX += width;
    Checked<GC3Dint, RecordOverflow> maxY = yoffset;
    maxY += height;

    if (maxX.hasOverflowed() || maxY.hasOverflowed()) {
        synthesizeGLError(GL_INVALID_VALUE, "copyTexSubImage2D", "bad dimensions");
        return;
    }
    if (maxX.unsafeGet() > tex->getWidth(target, level) || maxY.unsafeGet() > tex->getHeight(target, level)) {
        synthesizeGLError(GL_INVALID_VALUE, "copyTexSubImage2D", "rectangle out of range");
        return;
    }
    GC3Denum internalformat = tex->getInternalFormat(target, level);
    if (!validateSettableTexFormat("copyTexSubImage2D", internalformat))
        return;
    if (!isTexInternalFormatColorBufferCombinationValid(internalformat, boundFramebufferColorFormat())) {
        synthesizeGLError(GL_INVALID_OPERATION, "copyTexSubImage2D", "framebuffer is incompatible format");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "copyTexSubImage2D", reason);
        return;
    }
    clearIfComposited();
    ScopedDrawingBufferBinder binder(m_drawingBuffer.get(), m_framebufferBinding.get());
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

PassRefPtr<WebGLBuffer> WebGLRenderingContext::createBuffer()
{
    if (isContextLost())
        return 0;
    RefPtr<WebGLBuffer> o = WebGLBuffer::create(this);
    addSharedObject(o.get());
    return o;
}

PassRefPtr<WebGLFramebuffer> WebGLRenderingContext::createFramebuffer()
{
    if (isContextLost())
        return 0;
    RefPtr<WebGLFramebuffer> o = WebGLFramebuffer::create(this);
    addContextObject(o.get());
    return o;
}

PassRefPtr<WebGLTexture> WebGLRenderingContext::createTexture()
{
    if (isContextLost())
        return 0;
    RefPtr<WebGLTexture> o = WebGLTexture::create(this);
    addSharedObject(o.get());
    return o;
}

PassRefPtr<WebGLProgram> WebGLRenderingContext::createProgram()
{
    if (isContextLost())
        return 0;
    RefPtr<WebGLProgram> o = WebGLProgram::create(this);
    addSharedObject(o.get());
    return o;
}

PassRefPtr<WebGLRenderbuffer> WebGLRenderingContext::createRenderbuffer()
{
    if (isContextLost())
        return 0;
    RefPtr<WebGLRenderbuffer> o = WebGLRenderbuffer::create(this);
    addSharedObject(o.get());
    return o;
}

WebGLRenderbuffer* WebGLRenderingContext::ensureEmulatedStencilBuffer(GC3Denum target, WebGLRenderbuffer* renderbuffer)
{
    if (isContextLost())
        return 0;
    if (!renderbuffer->emulatedStencilBuffer()) {
        renderbuffer->setEmulatedStencilBuffer(createRenderbuffer());
        m_context->bindRenderbuffer(target, objectOrZero(renderbuffer->emulatedStencilBuffer()));
        m_context->bindRenderbuffer(target, objectOrZero(m_renderbufferBinding.get()));
    }
    return renderbuffer->emulatedStencilBuffer();
}

PassRefPtr<WebGLShader> WebGLRenderingContext::createShader(GC3Denum type)
{
    if (isContextLost())
        return 0;
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) {
        synthesizeGLError(GL_INVALID_ENUM, "createShader", "invalid shader type");
        return 0;
    }

    RefPtr<WebGLShader> o = WebGLShader::create(this, type);
    addSharedObject(o.get());
    return o;
}

void WebGLRenderingContext::cullFace(GC3Denum mode)
{
    if (isContextLost())
        return;
    switch (mode) {
    case GL_FRONT_AND_BACK:
    case GL_FRONT:
    case GL_BACK:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "cullFace", "invalid mode");
        return;
    }
    m_context->cullFace(mode);
}

bool WebGLRenderingContext::deleteObject(WebGLObject* object)
{
    if (isContextLost() || !object)
        return false;
    if (!object->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_OPERATION, "delete", "object does not belong to this context");
        return false;
    }
    if (object->object())
        // We need to pass in context here because we want
        // things in this context unbound.
        object->deleteObject(graphicsContext3D());
    return true;
}

void WebGLRenderingContext::deleteBuffer(WebGLBuffer* buffer)
{
    if (!deleteObject(buffer))
        return;
    if (m_boundArrayBuffer == buffer)
        m_boundArrayBuffer = 0;

    m_boundVertexArrayObject->unbindBuffer(buffer);
}

void WebGLRenderingContext::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!deleteObject(framebuffer))
        return;
    if (framebuffer == m_framebufferBinding) {
        m_framebufferBinding = 0;
        m_drawingBuffer->setFramebufferBinding(0);
        // Have to call bindFramebuffer here to bind back to internal fbo.
        m_drawingBuffer->bind();
    }
}

void WebGLRenderingContext::deleteProgram(WebGLProgram* program)
{
    deleteObject(program);
    // We don't reset m_currentProgram to 0 here because the deletion of the
    // current program is delayed.
}

void WebGLRenderingContext::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!deleteObject(renderbuffer))
        return;
    if (renderbuffer == m_renderbufferBinding)
        m_renderbufferBinding = 0;
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(renderbuffer);
}

void WebGLRenderingContext::deleteShader(WebGLShader* shader)
{
    deleteObject(shader);
}

void WebGLRenderingContext::deleteTexture(WebGLTexture* texture)
{
    if (!deleteObject(texture))
        return;

    int maxBoundTextureIndex = -1;
    for (size_t i = 0; i < m_onePlusMaxNonDefaultTextureUnit; ++i) {
        if (texture == m_textureUnits[i].m_texture2DBinding) {
            m_textureUnits[i].m_texture2DBinding = 0;
            maxBoundTextureIndex = i;
            if (!i)
                m_drawingBuffer->setTexture2DBinding(0);
        }
        if (texture == m_textureUnits[i].m_textureCubeMapBinding) {
            m_textureUnits[i].m_textureCubeMapBinding = 0;
            maxBoundTextureIndex = i;
        }
    }
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(texture);

    // If the deleted was bound to the the current maximum index, trace backwards to find the new max texture index
    if (m_onePlusMaxNonDefaultTextureUnit == static_cast<unsigned long>(maxBoundTextureIndex + 1)) {
        findNewMaxNonDefaultTextureUnit();
    }
}

void WebGLRenderingContext::depthFunc(GC3Denum func)
{
    if (isContextLost())
        return;
    if (!validateStencilOrDepthFunc("depthFunc", func))
        return;
    m_context->depthFunc(func);
}

void WebGLRenderingContext::depthMask(GC3Dboolean flag)
{
    if (isContextLost())
        return;
    m_depthMask = flag;
    m_context->depthMask(flag);
}

void WebGLRenderingContext::depthRange(GC3Dfloat zNear, GC3Dfloat zFar)
{
    if (isContextLost())
        return;
    if (zNear > zFar) {
        synthesizeGLError(GL_INVALID_OPERATION, "depthRange", "zNear > zFar");
        return;
    }
    m_context->depthRange(zNear, zFar);
}

void WebGLRenderingContext::detachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (isContextLost() || !validateWebGLObject("detachShader", program) || !validateWebGLObject("detachShader", shader))
        return;
    if (!program->detachShader(shader)) {
        synthesizeGLError(GL_INVALID_OPERATION, "detachShader", "shader not attached");
        return;
    }
    m_context->detachShader(objectOrZero(program), objectOrZero(shader));
    shader->onDetached(graphicsContext3D());
}

void WebGLRenderingContext::disable(GC3Denum cap)
{
    if (isContextLost() || !validateCapability("disable", cap))
        return;
    if (cap == GL_STENCIL_TEST) {
        m_stencilEnabled = false;
        applyStencilTest();
        return;
    }
    if (cap == GL_SCISSOR_TEST) {
        m_scissorEnabled = false;
        m_drawingBuffer->setScissorEnabled(m_scissorEnabled);
    }
    m_context->disable(cap);
}

void WebGLRenderingContext::disableVertexAttribArray(GC3Duint index)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "disableVertexAttribArray", "index out of range");
        return;
    }

    WebGLVertexArrayObjectOES::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);
    state.enabled = false;

    // If the disabled index is the current maximum, trace backwards to find the new max enabled attrib index
    if (m_onePlusMaxEnabledAttribIndex == index + 1) {
        findNewMaxEnabledAttribIndex();
    }

    m_context->disableVertexAttribArray(index);
}

bool WebGLRenderingContext::validateRenderingState()
{
    if (!m_currentProgram)
        return false;

    // Look in each enabled vertex attrib and check if they've been bound to a buffer.
    for (unsigned i = 0; i < m_onePlusMaxEnabledAttribIndex; ++i) {
        const WebGLVertexArrayObjectOES::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(i);
        if (state.enabled
            && (!state.bufferBinding || !state.bufferBinding->object()))
            return false;
    }

    return true;
}

bool WebGLRenderingContext::validateWebGLObject(const char* functionName, WebGLObject* object)
{
    if (!object || !object->object()) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no object or object deleted");
        return false;
    }
    if (!object->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "object does not belong to this context");
        return false;
    }
    return true;
}

void WebGLRenderingContext::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    if (!validateDrawArrays("drawArrays", mode, first, count))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawArrays", true);
    m_context->drawArrays(mode, first, count);
    handleTextureCompleteness("drawArrays", false);
    markContextChanged();
}

void WebGLRenderingContext::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset)
{
    if (!validateDrawElements("drawElements", mode, count, type, offset))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawElements", true);
    m_context->drawElements(mode, count, type, static_cast<GC3Dintptr>(offset));
    handleTextureCompleteness("drawElements", false);
    markContextChanged();
}

void WebGLRenderingContext::drawArraysInstancedANGLE(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    if (!validateDrawArrays("drawArraysInstancedANGLE", mode, first, count))
        return;

    if (!validateDrawInstanced("drawArraysInstancedANGLE", primcount))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawArraysInstancedANGLE", true);
    m_context->extensions()->drawArraysInstancedANGLE(mode, first, count, primcount);
    handleTextureCompleteness("drawArraysInstancedANGLE", false);
    markContextChanged();
}

void WebGLRenderingContext::drawElementsInstancedANGLE(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount)
{
    if (!validateDrawElements("drawElementsInstancedANGLE", mode, count, type, offset))
        return;

    if (!validateDrawInstanced("drawElementsInstancedANGLE", primcount))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawElementsInstancedANGLE", true);
    m_context->extensions()->drawElementsInstancedANGLE(mode, count, type, static_cast<GC3Dintptr>(offset), primcount);
    handleTextureCompleteness("drawElementsInstancedANGLE", false);
    markContextChanged();
}

void WebGLRenderingContext::enable(GC3Denum cap)
{
    if (isContextLost() || !validateCapability("enable", cap))
        return;
    if (cap == GL_STENCIL_TEST) {
        m_stencilEnabled = true;
        applyStencilTest();
        return;
    }
    if (cap == GL_SCISSOR_TEST) {
        m_scissorEnabled = true;
        m_drawingBuffer->setScissorEnabled(m_scissorEnabled);
    }
    m_context->enable(cap);
}

void WebGLRenderingContext::enableVertexAttribArray(GC3Duint index)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "enableVertexAttribArray", "index out of range");
        return;
    }

    WebGLVertexArrayObjectOES::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);
    state.enabled = true;

    m_onePlusMaxEnabledAttribIndex = max(index + 1, m_onePlusMaxEnabledAttribIndex);

    m_context->enableVertexAttribArray(index);
}

void WebGLRenderingContext::finish()
{
    if (isContextLost())
        return;
    m_context->flush(); // Intentionally a flush, not a finish.
}

void WebGLRenderingContext::flush()
{
    if (isContextLost())
        return;
    m_context->flush();
}

void WebGLRenderingContext::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, WebGLRenderbuffer* buffer)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferRenderbuffer", target, attachment))
        return;
    if (renderbuffertarget != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "framebufferRenderbuffer", "invalid target");
        return;
    }
    if (buffer && !buffer->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer", "no buffer or buffer not from this context");
        return;
    }
    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer", "no framebuffer bound");
        return;
    }
    Platform3DObject bufferObject = objectOrZero(buffer);
    switch (attachment) {
    case GC3D_DEPTH_STENCIL_ATTACHMENT_WEBGL:
        if (isDepthStencilSupported() || !buffer) {
            m_context->framebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, bufferObject);
            m_context->framebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, bufferObject);
        } else {
            WebGLRenderbuffer* emulatedStencilBuffer = ensureEmulatedStencilBuffer(renderbuffertarget, buffer);
            if (!emulatedStencilBuffer) {
                synthesizeGLError(GL_OUT_OF_MEMORY, "framebufferRenderbuffer", "out of memory");
                return;
            }
            m_context->framebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, bufferObject);
            m_context->framebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, objectOrZero(emulatedStencilBuffer));
        }
        break;
    default:
        m_context->framebufferRenderbuffer(target, attachment, renderbuffertarget, bufferObject);
    }
    m_framebufferBinding->setAttachmentForBoundFramebuffer(attachment, buffer);
    applyStencilTest();
}

void WebGLRenderingContext::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, WebGLTexture* texture, GC3Dint level)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferTexture2D", target, attachment))
        return;
    if (level) {
        synthesizeGLError(GL_INVALID_VALUE, "framebufferTexture2D", "level not 0");
        return;
    }
    if (texture && !texture->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D", "no texture or texture not from this context");
        return;
    }
    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D", "no framebuffer bound");
        return;
    }
    Platform3DObject textureObject = objectOrZero(texture);
    switch (attachment) {
    case GC3D_DEPTH_STENCIL_ATTACHMENT_WEBGL:
        m_context->framebufferTexture2D(target, GL_DEPTH_ATTACHMENT, textarget, textureObject, level);
        m_context->framebufferTexture2D(target, GL_STENCIL_ATTACHMENT, textarget, textureObject, level);
        break;
    case GL_DEPTH_ATTACHMENT:
        m_context->framebufferTexture2D(target, attachment, textarget, textureObject, level);
        break;
    case GL_STENCIL_ATTACHMENT:
        m_context->framebufferTexture2D(target, attachment, textarget, textureObject, level);
        break;
    default:
        m_context->framebufferTexture2D(target, attachment, textarget, textureObject, level);
    }
    m_framebufferBinding->setAttachmentForBoundFramebuffer(attachment, textarget, texture, level);
    applyStencilTest();
}

void WebGLRenderingContext::frontFace(GC3Denum mode)
{
    if (isContextLost())
        return;
    switch (mode) {
    case GL_CW:
    case GL_CCW:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "frontFace", "invalid mode");
        return;
    }
    m_context->frontFace(mode);
}

void WebGLRenderingContext::generateMipmap(GC3Denum target)
{
    if (isContextLost())
        return;
    WebGLTexture* tex = validateTextureBinding("generateMipmap", target, false);
    if (!tex)
        return;
    if (!tex->canGenerateMipmaps()) {
        synthesizeGLError(GL_INVALID_OPERATION, "generateMipmap", "level 0 not power of 2 or not all the same size");
        return;
    }
    if (!validateSettableTexFormat("generateMipmap", tex->getInternalFormat(target, 0)))
        return;

    // generateMipmap won't work properly if minFilter is not NEAREST_MIPMAP_LINEAR
    // on Mac.  Remove the hack once this driver bug is fixed.
#if OS(MACOSX)
    bool needToResetMinFilter = false;
    if (tex->getMinFilter() != GL_NEAREST_MIPMAP_LINEAR) {
        m_context->texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        needToResetMinFilter = true;
    }
#endif
    m_context->generateMipmap(target);
#if OS(MACOSX)
    if (needToResetMinFilter)
        m_context->texParameteri(target, GL_TEXTURE_MIN_FILTER, tex->getMinFilter());
#endif
    tex->generateMipmapLevelInfo();
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveAttrib(WebGLProgram* program, GC3Duint index)
{
    if (isContextLost() || !validateWebGLObject("getActiveAttrib", program))
        return 0;
    ActiveInfo info;
    if (!m_context->getActiveAttrib(objectOrZero(program), index, info))
        return 0;
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveUniform(WebGLProgram* program, GC3Duint index)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniform", program))
        return 0;
    ActiveInfo info;
    if (!m_context->getActiveUniform(objectOrZero(program), index, info))
        return 0;
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

bool WebGLRenderingContext::getAttachedShaders(WebGLProgram* program, Vector<RefPtr<WebGLShader> >& shaderObjects)
{
    shaderObjects.clear();
    if (isContextLost() || !validateWebGLObject("getAttachedShaders", program))
        return false;

    const GC3Denum shaderType[] = {
        GL_VERTEX_SHADER,
        GL_FRAGMENT_SHADER
    };
    for (unsigned i = 0; i < sizeof(shaderType) / sizeof(GC3Denum); ++i) {
        WebGLShader* shader = program->getAttachedShader(shaderType[i]);
        if (shader)
            shaderObjects.append(shader);
    }
    return true;
}

GC3Dint WebGLRenderingContext::getAttribLocation(WebGLProgram* program, const String& name)
{
    if (isContextLost() || !validateWebGLObject("getAttribLocation", program))
        return -1;
    if (!validateLocationLength("getAttribLocation", name))
        return -1;
    if (!validateString("getAttribLocation", name))
        return -1;
    if (isPrefixReserved(name))
        return -1;
    if (!program->linkStatus()) {
        synthesizeGLError(GL_INVALID_OPERATION, "getAttribLocation", "program not linked");
        return 0;
    }
    return m_context->getAttribLocation(objectOrZero(program), name);
}

WebGLGetInfo WebGLRenderingContext::getBufferParameter(GC3Denum target, GC3Denum pname)
{
    if (isContextLost())
        return WebGLGetInfo();
    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "getBufferParameter", "invalid target");
        return WebGLGetInfo();
    }

    if (pname != GL_BUFFER_SIZE && pname != GL_BUFFER_USAGE) {
        synthesizeGLError(GL_INVALID_ENUM, "getBufferParameter", "invalid parameter name");
        return WebGLGetInfo();
    }

    GC3Dint value = 0;
    m_context->getBufferParameteriv(target, pname, &value);
    if (pname == GL_BUFFER_SIZE)
        return WebGLGetInfo(value);
    return WebGLGetInfo(static_cast<unsigned int>(value));
}

PassRefPtr<WebGLContextAttributes> WebGLRenderingContext::getContextAttributes()
{
    if (isContextLost())
        return 0;
    // We always need to return a new WebGLContextAttributes object to
    // prevent the user from mutating any cached version.

    // Also, we need to enforce requested values of "false" for depth
    // and stencil, regardless of the properties of the underlying
    // GraphicsContext3D or DrawingBuffer.
    RefPtr<WebGLContextAttributes> attributes = WebGLContextAttributes::create(m_context->getContextAttributes());
    if (!m_attributes.depth)
        attributes->setDepth(false);
    if (!m_attributes.stencil)
        attributes->setStencil(false);
    // The DrawingBuffer obtains its parameters from GraphicsContext3D::getContextAttributes(),
    // but it makes its own determination of whether multisampling is supported.
    attributes->setAntialias(m_drawingBuffer->multisample());
    return attributes.release();
}

GC3Denum WebGLRenderingContext::getError()
{
    if (lost_context_errors_.size()) {
        GC3Denum err = lost_context_errors_.first();
        lost_context_errors_.remove(0);
        return err;
    }

    if (isContextLost())
        return GL_NO_ERROR;

    return m_context->getError();
}

bool WebGLRenderingContext::ExtensionTracker::matchesNameWithPrefixes(const String& name) const
{
    static const char* const unprefixed[] = { "", 0, };

    const char* const* prefixes = m_prefixes ? m_prefixes : unprefixed;
    for (; *prefixes; ++prefixes) {
        String prefixedName = String(*prefixes) + extensionName();
        if (equalIgnoringCase(prefixedName, name)) {
            return true;
        }
    }
    return false;
}

PassRefPtr<WebGLExtension> WebGLRenderingContext::getExtension(const String& name)
{
    if (isContextLost())
        return 0;

    for (size_t i = 0; i < m_extensions.size(); ++i) {
        ExtensionTracker* tracker = m_extensions[i];
        if (tracker->matchesNameWithPrefixes(name)) {
            if (tracker->webglDebugRendererInfo() && !allowWebGLDebugRendererInfo())
                return 0;
            if (tracker->privileged() && !allowPrivilegedExtensions())
                return 0;
            if (tracker->draft() && !RuntimeEnabledFeatures::webGLDraftExtensionsEnabled())
                return 0;
            if (!tracker->supported(this))
                return 0;
            return tracker->getExtension(this);
        }
    }

    return 0;
}

WebGLGetInfo WebGLRenderingContext::getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname)
{
    if (isContextLost() || !validateFramebufferFuncParameters("getFramebufferAttachmentParameter", target, attachment))
        return WebGLGetInfo();

    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "getFramebufferAttachmentParameter", "no framebuffer bound");
        return WebGLGetInfo();
    }

    WebGLSharedObject* object = m_framebufferBinding->getAttachmentObject(attachment);
    if (!object) {
        if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return WebGLGetInfo(GL_NONE);
        // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
        // specifies INVALID_OPERATION.
        synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name");
        return WebGLGetInfo();
    }

    ASSERT(object->isTexture() || object->isRenderbuffer());
    if (object->isTexture()) {
        switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GL_TEXTURE);
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLTexture>(static_cast<WebGLTexture*>(object)));
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            {
                GC3Dint value = 0;
                m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
                return WebGLGetInfo(value);
            }
        default:
            synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for texture attachment");
            return WebGLGetInfo();
        }
    } else {
        switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GL_RENDERBUFFER);
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(static_cast<WebGLRenderbuffer*>(object)));
        default:
            synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
            return WebGLGetInfo();
        }
    }
}

WebGLGetInfo WebGLRenderingContext::getParameter(GC3Denum pname)
{
    if (isContextLost())
        return WebGLGetInfo();
    const int intZero = 0;
    switch (pname) {
    case GL_ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GL_ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GL_ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GL_ALPHA_BITS:
        return getIntParameter(pname);
    case GL_ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundArrayBuffer));
    case GL_BLEND:
        return getBooleanParameter(pname);
    case GL_BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GL_BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GL_BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GL_BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GL_BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GL_BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GL_BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GL_BLUE_BITS:
        return getIntParameter(pname);
    case GL_COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GL_COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GL_COMPRESSED_TEXTURE_FORMATS:
        return WebGLGetInfo(Uint32Array::create(m_compressedTextureFormats.data(), m_compressedTextureFormats.size()));
    case GL_CULL_FACE:
        return getBooleanParameter(pname);
    case GL_CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GL_CURRENT_PROGRAM:
        return WebGLGetInfo(PassRefPtr<WebGLProgram>(m_currentProgram));
    case GL_DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GL_DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GL_DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GL_DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GL_DEPTH_TEST:
        return getBooleanParameter(pname);
    case GL_DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GL_DITHER:
        return getBooleanParameter(pname);
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundVertexArrayObject->boundElementArrayBuffer()));
    case GL_FRAMEBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLFramebuffer>(m_framebufferBinding));
    case GL_FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GL_GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GL_GREEN_BITS:
        return getIntParameter(pname);
    case GL_LINE_WIDTH:
        return getFloatParameter(pname);
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GL_MAX_RENDERBUFFER_SIZE:
        return getIntParameter(pname);
    case GL_MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GL_MAX_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GL_MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GL_MAX_VERTEX_ATTRIBS:
        return getIntParameter(pname);
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GL_MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GL_NUM_SHADER_BINARY_FORMATS:
        // FIXME: should we always return 0 for this?
        return getIntParameter(pname);
    case GL_PACK_ALIGNMENT:
        return getIntParameter(pname);
    case GL_POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GL_POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GL_POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GL_RED_BITS:
        return getIntParameter(pname);
    case GL_RENDERBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(m_renderbufferBinding));
    case GL_RENDERER:
        return WebGLGetInfo(String("WebKit WebGL"));
    case GL_SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GL_SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GL_SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GL_SAMPLES:
        return getIntParameter(pname);
    case GL_SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GL_SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GL_SHADING_LANGUAGE_VERSION:
        return WebGLGetInfo("WebGL GLSL ES 1.0 (" + m_context->getString(GL_SHADING_LANGUAGE_VERSION) + ")");
    case GL_STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GL_STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GL_STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GL_STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_REF:
        return getIntParameter(pname);
    case GL_STENCIL_TEST:
        return getBooleanParameter(pname);
    case GL_STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GL_STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GL_SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GL_TEXTURE_BINDING_2D:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].m_texture2DBinding));
    case GL_TEXTURE_BINDING_CUBE_MAP:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding));
    case GL_UNPACK_ALIGNMENT:
        return getIntParameter(pname);
    case GC3D_UNPACK_FLIP_Y_WEBGL:
        return WebGLGetInfo(m_unpackFlipY);
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return WebGLGetInfo(m_unpackPremultiplyAlpha);
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return WebGLGetInfo(m_unpackColorspaceConversion);
    case GL_VENDOR:
        return WebGLGetInfo(String("WebKit"));
    case GL_VERSION:
        return WebGLGetInfo("WebGL 1.0 (" + m_context->getString(GL_VERSION) + ")");
    case GL_VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, OES_standard_derivatives not enabled");
        return WebGLGetInfo();
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GL_RENDERER));
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GL_VENDOR));
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case Extensions3D::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (!m_boundVertexArrayObject->isDefaultObject())
                return WebGLGetInfo(PassRefPtr<WebGLVertexArrayObjectOES>(m_boundVertexArrayObject));
            return WebGLGetInfo();
        }
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, OES_vertex_array_object not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers)
            return WebGLGetInfo(maxColorAttachments());
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers)
            return WebGLGetInfo(maxDrawBuffers());
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return WebGLGetInfo();
    default:
        if (m_webglDrawBuffers
            && pname >= Extensions3D::DRAW_BUFFER0_EXT
            && pname < static_cast<GC3Denum>(Extensions3D::DRAW_BUFFER0_EXT + maxDrawBuffers())) {
            GC3Dint value = GL_NONE;
            if (m_framebufferBinding)
                value = m_framebufferBinding->getDrawBuffer(pname);
            else // emulated backbuffer
                value = m_backDrawBuffer;
            return WebGLGetInfo(value);
        }
        synthesizeGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getProgramParameter(WebGLProgram* program, GC3Denum pname)
{
    if (isContextLost() || !validateWebGLObject("getProgramParameter", program))
        return WebGLGetInfo();

    GC3Dint value = 0;
    switch (pname) {
    case GL_DELETE_STATUS:
        return WebGLGetInfo(program->isDeleted());
    case GL_VALIDATE_STATUS:
        m_context->getProgramiv(objectOrZero(program), pname, &value);
        return WebGLGetInfo(static_cast<bool>(value));
    case GL_LINK_STATUS:
        return WebGLGetInfo(program->linkStatus());
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
        m_context->getProgramiv(objectOrZero(program), pname, &value);
        return WebGLGetInfo(value);
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getProgramParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

String WebGLRenderingContext::getProgramInfoLog(WebGLProgram* program)
{
    if (isContextLost())
        return String();
    if (!validateWebGLObject("getProgramInfoLog", program))
        return "";
    return ensureNotNull(m_context->getProgramInfoLog(objectOrZero(program)));
}

WebGLGetInfo WebGLRenderingContext::getRenderbufferParameter(GC3Denum target, GC3Denum pname)
{
    if (isContextLost())
        return WebGLGetInfo();
    if (target != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter", "invalid target");
        return WebGLGetInfo();
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "getRenderbufferParameter", "no renderbuffer bound");
        return WebGLGetInfo();
    }

    GC3Dint value = 0;
    switch (pname) {
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
        m_context->getRenderbufferParameteriv(target, pname, &value);
        return WebGLGetInfo(value);
    case GL_RENDERBUFFER_STENCIL_SIZE:
        if (m_renderbufferBinding->emulatedStencilBuffer()) {
            m_context->bindRenderbuffer(target, objectOrZero(m_renderbufferBinding->emulatedStencilBuffer()));
            m_context->getRenderbufferParameteriv(target, pname, &value);
            m_context->bindRenderbuffer(target, objectOrZero(m_renderbufferBinding.get()));
        } else {
            m_context->getRenderbufferParameteriv(target, pname, &value);
        }
        return WebGLGetInfo(value);
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
        return WebGLGetInfo(m_renderbufferBinding->internalFormat());
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getShaderParameter(WebGLShader* shader, GC3Denum pname)
{
    if (isContextLost() || !validateWebGLObject("getShaderParameter", shader))
        return WebGLGetInfo();
    GC3Dint value = 0;
    switch (pname) {
    case GL_DELETE_STATUS:
        return WebGLGetInfo(shader->isDeleted());
    case GL_COMPILE_STATUS:
        m_context->getShaderiv(objectOrZero(shader), pname, &value);
        return WebGLGetInfo(static_cast<bool>(value));
    case GL_SHADER_TYPE:
        m_context->getShaderiv(objectOrZero(shader), pname, &value);
        return WebGLGetInfo(static_cast<unsigned int>(value));
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getShaderParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

String WebGLRenderingContext::getShaderInfoLog(WebGLShader* shader)
{
    if (isContextLost())
        return String();
    if (!validateWebGLObject("getShaderInfoLog", shader))
        return "";
    return ensureNotNull(m_context->getShaderInfoLog(objectOrZero(shader)));
}

PassRefPtr<WebGLShaderPrecisionFormat> WebGLRenderingContext::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType)
{
    if (isContextLost())
        return 0;
    switch (shaderType) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat", "invalid shader type");
        return 0;
    }
    switch (precisionType) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat", "invalid precision type");
        return 0;
    }

    GC3Dint range[2] = {0, 0};
    GC3Dint precision = 0;
    m_context->getShaderPrecisionFormat(shaderType, precisionType, range, &precision);
    return WebGLShaderPrecisionFormat::create(range[0], range[1], precision);
}

String WebGLRenderingContext::getShaderSource(WebGLShader* shader)
{
    if (isContextLost())
        return String();
    if (!validateWebGLObject("getShaderSource", shader))
        return "";
    return ensureNotNull(shader->source());
}

Vector<String> WebGLRenderingContext::getSupportedExtensions()
{
    Vector<String> result;
    if (isContextLost())
        return result;

    for (size_t i = 0; i < m_extensions.size(); ++i) {
        ExtensionTracker* tracker = m_extensions[i];
        if (tracker->webglDebugRendererInfo() && !allowWebGLDebugRendererInfo())
            continue;
        if (tracker->privileged() && !allowPrivilegedExtensions())
            continue;
        if (tracker->draft() && !RuntimeEnabledFeatures::webGLDraftExtensionsEnabled())
            continue;
        if (tracker->supported(this))
            result.append(String(tracker->prefixed()  ? "WEBKIT_" : "") + tracker->extensionName());
    }

    return result;
}

WebGLGetInfo WebGLRenderingContext::getTexParameter(GC3Denum target, GC3Denum pname)
{
    if (isContextLost())
        return WebGLGetInfo();
    WebGLTexture* tex = validateTextureBinding("getTexParameter", target, false);
    if (!tex)
        return WebGLGetInfo();
    GC3Dint value = 0;
    switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        m_context->getTexParameteriv(target, pname, &value);
        return WebGLGetInfo(static_cast<unsigned int>(value));
    case Extensions3D::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic) {
            m_context->getTexParameteriv(target, pname, &value);
            return WebGLGetInfo(static_cast<unsigned int>(value));
        }
        synthesizeGLError(GL_INVALID_ENUM, "getTexParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return WebGLGetInfo();
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getTexParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getUniform(WebGLProgram* program, const WebGLUniformLocation* uniformLocation)
{
    if (isContextLost() || !validateWebGLObject("getUniform", program))
        return WebGLGetInfo();
    if (!uniformLocation || uniformLocation->program() != program) {
        synthesizeGLError(GL_INVALID_OPERATION, "getUniform", "no uniformlocation or not valid for this program");
        return WebGLGetInfo();
    }
    GC3Dint location = uniformLocation->location();

    // FIXME: make this more efficient using WebGLUniformLocation and caching types in it
    GC3Dint activeUniforms = 0;
    m_context->getProgramiv(objectOrZero(program), GL_ACTIVE_UNIFORMS, &activeUniforms);
    for (GC3Dint i = 0; i < activeUniforms; i++) {
        ActiveInfo info;
        if (!m_context->getActiveUniform(objectOrZero(program), i, info))
            return WebGLGetInfo();
        // Strip "[0]" from the name if it's an array.
        if (info.size > 1 && info.name.endsWith("[0]"))
            info.name = info.name.left(info.name.length() - 3);
        // If it's an array, we need to iterate through each element, appending "[index]" to the name.
        for (GC3Dint index = 0; index < info.size; ++index) {
            String name = info.name;
            if (info.size > 1 && index >= 1) {
                name.append('[');
                name.append(String::number(index));
                name.append(']');
            }
            // Now need to look this up by name again to find its location
            GC3Dint loc = m_context->getUniformLocation(objectOrZero(program), name);
            if (loc == location) {
                // Found it. Use the type in the ActiveInfo to determine the return type.
                GC3Denum baseType;
                unsigned int length;
                switch (info.type) {
                case GL_BOOL:
                    baseType = GL_BOOL;
                    length = 1;
                    break;
                case GL_BOOL_VEC2:
                    baseType = GL_BOOL;
                    length = 2;
                    break;
                case GL_BOOL_VEC3:
                    baseType = GL_BOOL;
                    length = 3;
                    break;
                case GL_BOOL_VEC4:
                    baseType = GL_BOOL;
                    length = 4;
                    break;
                case GL_INT:
                    baseType = GL_INT;
                    length = 1;
                    break;
                case GL_INT_VEC2:
                    baseType = GL_INT;
                    length = 2;
                    break;
                case GL_INT_VEC3:
                    baseType = GL_INT;
                    length = 3;
                    break;
                case GL_INT_VEC4:
                    baseType = GL_INT;
                    length = 4;
                    break;
                case GL_FLOAT:
                    baseType = GL_FLOAT;
                    length = 1;
                    break;
                case GL_FLOAT_VEC2:
                    baseType = GL_FLOAT;
                    length = 2;
                    break;
                case GL_FLOAT_VEC3:
                    baseType = GL_FLOAT;
                    length = 3;
                    break;
                case GL_FLOAT_VEC4:
                    baseType = GL_FLOAT;
                    length = 4;
                    break;
                case GL_FLOAT_MAT2:
                    baseType = GL_FLOAT;
                    length = 4;
                    break;
                case GL_FLOAT_MAT3:
                    baseType = GL_FLOAT;
                    length = 9;
                    break;
                case GL_FLOAT_MAT4:
                    baseType = GL_FLOAT;
                    length = 16;
                    break;
                case GL_SAMPLER_2D:
                case GL_SAMPLER_CUBE:
                    baseType = GL_INT;
                    length = 1;
                    break;
                default:
                    // Can't handle this type
                    synthesizeGLError(GL_INVALID_VALUE, "getUniform", "unhandled type");
                    return WebGLGetInfo();
                }
                switch (baseType) {
                case GL_FLOAT: {
                    GC3Dfloat value[16] = {0};
                    m_context->getUniformfv(objectOrZero(program), location, value);
                    if (length == 1)
                        return WebGLGetInfo(value[0]);
                    return WebGLGetInfo(Float32Array::create(value, length));
                }
                case GL_INT: {
                    GC3Dint value[4] = {0};
                    m_context->getUniformiv(objectOrZero(program), location, value);
                    if (length == 1)
                        return WebGLGetInfo(value[0]);
                    return WebGLGetInfo(Int32Array::create(value, length));
                }
                case GL_BOOL: {
                    GC3Dint value[4] = {0};
                    m_context->getUniformiv(objectOrZero(program), location, value);
                    if (length > 1) {
                        bool boolValue[16] = {0};
                        for (unsigned j = 0; j < length; j++)
                            boolValue[j] = static_cast<bool>(value[j]);
                        return WebGLGetInfo(boolValue, length);
                    }
                    return WebGLGetInfo(static_cast<bool>(value[0]));
                }
                default:
                    notImplemented();
                }
            }
        }
    }
    // If we get here, something went wrong in our unfortunately complex logic above
    synthesizeGLError(GL_INVALID_VALUE, "getUniform", "unknown error");
    return WebGLGetInfo();
}

PassRefPtr<WebGLUniformLocation> WebGLRenderingContext::getUniformLocation(WebGLProgram* program, const String& name)
{
    if (isContextLost() || !validateWebGLObject("getUniformLocation", program))
        return 0;
    if (!validateLocationLength("getUniformLocation", name))
        return 0;
    if (!validateString("getUniformLocation", name))
        return 0;
    if (isPrefixReserved(name))
        return 0;
    if (!program->linkStatus()) {
        synthesizeGLError(GL_INVALID_OPERATION, "getUniformLocation", "program not linked");
        return 0;
    }
    GC3Dint uniformLocation = m_context->getUniformLocation(objectOrZero(program), name);
    if (uniformLocation == -1)
        return 0;
    return WebGLUniformLocation::create(program, uniformLocation);
}

WebGLGetInfo WebGLRenderingContext::getVertexAttrib(GC3Duint index, GC3Denum pname)
{
    if (isContextLost())
        return WebGLGetInfo();
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "getVertexAttrib", "index out of range");
        return WebGLGetInfo();
    }
    const WebGLVertexArrayObjectOES::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);

    if (m_angleInstancedArrays && pname == Extensions3D::VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE)
        return WebGLGetInfo(state.divisor);

    switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        if (!state.bufferBinding || !state.bufferBinding->object())
            return WebGLGetInfo();
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(state.bufferBinding));
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        return WebGLGetInfo(state.enabled);
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        return WebGLGetInfo(state.normalized);
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
        return WebGLGetInfo(state.size);
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
        return WebGLGetInfo(state.originalStride);
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
        return WebGLGetInfo(state.type);
    case GL_CURRENT_VERTEX_ATTRIB:
        return WebGLGetInfo(Float32Array::create(m_vertexAttribValue[index].value, 4));
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getVertexAttrib", "invalid parameter name");
        return WebGLGetInfo();
    }
}

long long WebGLRenderingContext::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    if (isContextLost())
        return 0;
    if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER) {
        synthesizeGLError(GL_INVALID_ENUM, "getVertexAttribOffset", "invalid parameter name");
        return 0;
    }
    GC3Dsizeiptr result = m_context->getVertexAttribOffset(index, pname);
    return static_cast<long long>(result);
}

void WebGLRenderingContext::hint(GC3Denum target, GC3Denum mode)
{
    if (isContextLost())
        return;
    bool isValid = false;
    switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
        isValid = true;
        break;
    case Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GL_INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}

GC3Dboolean WebGLRenderingContext::isBuffer(WebGLBuffer* buffer)
{
    if (!buffer || isContextLost())
        return 0;

    if (!buffer->hasEverBeenBound())
        return 0;

    return m_context->isBuffer(buffer->object());
}

bool WebGLRenderingContext::isContextLost()
{
    return m_contextLost;
}

GC3Dboolean WebGLRenderingContext::isEnabled(GC3Denum cap)
{
    if (isContextLost() || !validateCapability("isEnabled", cap))
        return 0;
    if (cap == GL_STENCIL_TEST)
        return m_stencilEnabled;
    return m_context->isEnabled(cap);
}

GC3Dboolean WebGLRenderingContext::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer || isContextLost())
        return 0;

    if (!framebuffer->hasEverBeenBound())
        return 0;

    return m_context->isFramebuffer(framebuffer->object());
}

GC3Dboolean WebGLRenderingContext::isProgram(WebGLProgram* program)
{
    if (!program || isContextLost())
        return 0;

    return m_context->isProgram(program->object());
}

GC3Dboolean WebGLRenderingContext::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer || isContextLost())
        return 0;

    if (!renderbuffer->hasEverBeenBound())
        return 0;

    return m_context->isRenderbuffer(renderbuffer->object());
}

GC3Dboolean WebGLRenderingContext::isShader(WebGLShader* shader)
{
    if (!shader || isContextLost())
        return 0;

    return m_context->isShader(shader->object());
}

GC3Dboolean WebGLRenderingContext::isTexture(WebGLTexture* texture)
{
    if (!texture || isContextLost())
        return 0;

    if (!texture->hasEverBeenBound())
        return 0;

    return m_context->isTexture(texture->object());
}

void WebGLRenderingContext::lineWidth(GC3Dfloat width)
{
    if (isContextLost())
        return;
    m_context->lineWidth(width);
}

void WebGLRenderingContext::linkProgram(WebGLProgram* program)
{
    if (isContextLost() || !validateWebGLObject("linkProgram", program))
        return;

    m_context->linkProgram(objectOrZero(program));
    program->increaseLinkCount();
}

void WebGLRenderingContext::pixelStorei(GC3Denum pname, GC3Dint param)
{
    if (isContextLost())
        return;
    switch (pname) {
    case GC3D_UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipY = param;
        break;
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlpha = param;
        break;
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
        if (static_cast<GC3Denum>(param) == GC3D_BROWSER_DEFAULT_WEBGL || param == GL_NONE)
            m_unpackColorspaceConversion = static_cast<GC3Denum>(param);
        else {
            synthesizeGLError(GL_INVALID_VALUE, "pixelStorei", "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
            return;
        }
        break;
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            if (pname == GL_PACK_ALIGNMENT)
                m_packAlignment = param;
            else // GL_UNPACK_ALIGNMENT:
                m_unpackAlignment = param;
            m_context->pixelStorei(pname, param);
        } else {
            synthesizeGLError(GL_INVALID_VALUE, "pixelStorei", "invalid parameter for alignment");
            return;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "pixelStorei", "invalid parameter name");
        return;
    }
}

void WebGLRenderingContext::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    if (isContextLost())
        return;
    m_context->polygonOffset(factor, units);
}

void WebGLRenderingContext::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, ArrayBufferView* pixels)
{
    if (isContextLost())
        return;
    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvas()->originClean());
    // Validate input parameters.
    if (!pixels) {
        synthesizeGLError(GL_INVALID_VALUE, "readPixels", "no destination ArrayBufferView");
        return;
    }
    switch (format) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid format");
        return;
    }
    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
        return;
    }
    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
        synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "format not RGBA or type not UNSIGNED_BYTE");
        return;
    }
    // Validate array type against pixel type.
    if (pixels->getType() != ArrayBufferView::TypeUint8) {
        synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "ArrayBufferView not Uint8Array");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
        return;
    }
    // Calculate array size, taking into consideration of PACK_ALIGNMENT.
    unsigned int totalBytesRequired = 0;
    unsigned int padding = 0;
    GC3Denum error = m_context->computeImageSizeInBytes(format, type, width, height, m_packAlignment, &totalBytesRequired, &padding);
    if (error != GL_NO_ERROR) {
        synthesizeGLError(error, "readPixels", "invalid dimensions");
        return;
    }
    if (pixels->byteLength() < totalBytesRequired) {
        synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "ArrayBufferView not large enough for dimensions");
        return;
    }

    clearIfComposited();
    void* data = pixels->baseAddress();

    {
        ScopedDrawingBufferBinder binder(m_drawingBuffer.get(), m_framebufferBinding.get());
        m_context->readPixels(x, y, width, height, format, type, data);
    }

#if OS(MACOSX)
    // FIXME: remove this section when GL driver bug on Mac is fixed, i.e.,
    // when alpha is off, readPixels should set alpha to 255 instead of 0.
    if (!m_framebufferBinding && !m_context->getContextAttributes().alpha) {
        unsigned char* pixels = reinterpret_cast<unsigned char*>(data);
        for (GC3Dsizei iy = 0; iy < height; ++iy) {
            for (GC3Dsizei ix = 0; ix < width; ++ix) {
                pixels[3] = 255;
                pixels += 4;
            }
            pixels += padding;
        }
    }
#endif
}

void WebGLRenderingContext::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLost())
        return;
    if (target != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "renderbufferStorage", "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "renderbufferStorage", "no bound renderbuffer");
        return;
    }
    if (!validateSize("renderbufferStorage", width, height))
        return;
    switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
    case GL_STENCIL_INDEX8:
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->deleteEmulatedStencilBuffer(m_context.get());
        break;
    case GL_DEPTH_STENCIL_OES:
        if (isDepthStencilSupported()) {
            m_context->renderbufferStorage(target, Extensions3D::DEPTH24_STENCIL8, width, height);
        } else {
            WebGLRenderbuffer* emulatedStencilBuffer = ensureEmulatedStencilBuffer(target, m_renderbufferBinding.get());
            if (!emulatedStencilBuffer) {
                synthesizeGLError(GL_OUT_OF_MEMORY, "renderbufferStorage", "out of memory");
                return;
            }
            m_context->renderbufferStorage(target, GL_DEPTH_COMPONENT16, width, height);
            m_context->bindRenderbuffer(target, objectOrZero(emulatedStencilBuffer));
            m_context->renderbufferStorage(target, GL_STENCIL_INDEX8, width, height);
            m_context->bindRenderbuffer(target, objectOrZero(m_renderbufferBinding.get()));
            emulatedStencilBuffer->setSize(width, height);
            emulatedStencilBuffer->setInternalFormat(GL_STENCIL_INDEX8);
        }
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
        return;
    }
    applyStencilTest();
}

void WebGLRenderingContext::sampleCoverage(GC3Dfloat value, GC3Dboolean invert)
{
    if (isContextLost())
        return;
    m_context->sampleCoverage(value, invert);
}

void WebGLRenderingContext::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("scissor", width, height))
        return;
    m_context->scissor(x, y, width, height);
}

void WebGLRenderingContext::shaderSource(WebGLShader* shader, const String& string)
{
    if (isContextLost() || !validateWebGLObject("shaderSource", shader))
        return;
    String stringWithoutComments = StripComments(string).result();
    if (!validateString("shaderSource", stringWithoutComments))
        return;
    shader->setSource(string);
    m_context->shaderSource(objectOrZero(shader), stringWithoutComments);
}

void WebGLRenderingContext::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    if (isContextLost())
        return;
    if (!validateStencilOrDepthFunc("stencilFunc", func))
        return;
    m_stencilFuncRef = ref;
    m_stencilFuncRefBack = ref;
    m_stencilFuncMask = mask;
    m_stencilFuncMaskBack = mask;
    m_context->stencilFunc(func, ref, mask);
}

void WebGLRenderingContext::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    if (isContextLost())
        return;
    if (!validateStencilOrDepthFunc("stencilFuncSeparate", func))
        return;
    switch (face) {
    case GL_FRONT_AND_BACK:
        m_stencilFuncRef = ref;
        m_stencilFuncRefBack = ref;
        m_stencilFuncMask = mask;
        m_stencilFuncMaskBack = mask;
        break;
    case GL_FRONT:
        m_stencilFuncRef = ref;
        m_stencilFuncMask = mask;
        break;
    case GL_BACK:
        m_stencilFuncRefBack = ref;
        m_stencilFuncMaskBack = mask;
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "stencilFuncSeparate", "invalid face");
        return;
    }
    m_context->stencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContext::stencilMask(GC3Duint mask)
{
    if (isContextLost())
        return;
    m_stencilMask = mask;
    m_stencilMaskBack = mask;
    m_context->stencilMask(mask);
}

void WebGLRenderingContext::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    if (isContextLost())
        return;
    switch (face) {
    case GL_FRONT_AND_BACK:
        m_stencilMask = mask;
        m_stencilMaskBack = mask;
        break;
    case GL_FRONT:
        m_stencilMask = mask;
        break;
    case GL_BACK:
        m_stencilMaskBack = mask;
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "stencilMaskSeparate", "invalid face");
        return;
    }
    m_context->stencilMaskSeparate(face, mask);
}

void WebGLRenderingContext::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    if (isContextLost())
        return;
    m_context->stencilOp(fail, zfail, zpass);
}

void WebGLRenderingContext::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    if (isContextLost())
        return;
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
}

void WebGLRenderingContext::texImage2DBase(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels, ExceptionState& exceptionState)
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    // FIXME: Handle errors.
    WebGLTexture* tex = validateTextureBinding("texImage2D", target, true);
    ASSERT(validateTexFuncParameters("texImage2D", NotTexSubImage2D, target, level, internalformat, width, height, border, format, type));
    ASSERT(tex);
    ASSERT(!level || !WebGLTexture::isNPOT(width, height));
    ASSERT(!pixels || validateSettableTexFormat("texImage2D", internalformat));
    m_context->texImage2D(target, level, internalformat, width, height,
                          border, format, type, pixels);
    tex->setLevelInfo(target, level, internalformat, width, height, type);
}

void WebGLRenderingContext::texImage2DImpl(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Denum format, GC3Denum type, Image* image, GraphicsContext3D::ImageHtmlDomSource domSource, bool flipY, bool premultiplyAlpha, ExceptionState& exceptionState)
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    Vector<uint8_t> data;
    GraphicsContext3D::ImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GL_NONE);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GL_INVALID_VALUE, "texImage2D", "bad image data");
        return;
    }
    GraphicsContext3D::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContext3D::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();

    bool needConversion = true;
    if (type == GL_UNSIGNED_BYTE && sourceDataFormat == GraphicsContext3D::DataFormatRGBA8 && format == GL_RGBA && alphaOp == GraphicsContext3D::AlphaDoNothing && !flipY)
        needConversion = false;
    else {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), imageExtractor.imageSourceUnpackAlignment(), data)) {
            synthesizeGLError(GL_INVALID_VALUE, "texImage2D", "packImage error");
            return;
        }
    }

    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, image->width(), image->height(), 0, format, type, needConversion ? data.data() : imagePixelData, exceptionState);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

bool WebGLRenderingContext::validateTexFunc(const char* functionName, TexFuncValidationFunctionType functionType, TexFuncValidationSourceType sourceType, GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint xoffset, GC3Dint yoffset)
{
    if (!validateTexFuncParameters(functionName, functionType, target, level, internalformat, width, height, border, format, type))
        return false;

    WebGLTexture* texture = validateTextureBinding(functionName, target, true);
    if (!texture)
        return false;

    if (functionType == NotTexSubImage2D) {
        if (level && WebGLTexture::isNPOT(width, height)) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "level > 0 not power of 2");
            return false;
        }
        // For SourceArrayBufferView, function validateTexFuncData() would handle whether to validate the SettableTexFormat
        // by checking if the ArrayBufferView is null or not.
        if (sourceType != SourceArrayBufferView) {
            if (!validateSettableTexFormat(functionName, format))
                return false;
        }
    } else {
        if (!validateSettableTexFormat(functionName, format))
            return false;
        if (!validateSize(functionName, xoffset, yoffset))
            return false;
        // Before checking if it is in the range, check if overflow happens first.
        if (xoffset + width < 0 || yoffset + height < 0) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "bad dimensions");
            return false;
        }
        if (xoffset + width > texture->getWidth(target, level) || yoffset + height > texture->getHeight(target, level)) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "dimensions out of range");
            return false;
        }
        if (texture->getInternalFormat(target, level) != format || texture->getType(target, level) != type) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "type and format do not match texture");
            return false;
        }
    }

    return true;
}

PassRefPtr<Image> WebGLRenderingContext::drawImageIntoBuffer(Image* image, int width, int height)
{
    IntSize size(width, height);
    ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
    if (!buf) {
        synthesizeGLError(GL_OUT_OF_MEMORY, "texImage2D", "out of memory");
        return 0;
    }

    IntRect srcRect(IntPoint(), image->size());
    IntRect destRect(0, 0, size.width(), size.height());
    buf->context()->drawImage(image, destRect, srcRect);
    return buf->copyImage(ImageBuffer::fastCopyImageMode());
}

void WebGLRenderingContext::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
    GC3Dsizei width, GC3Dsizei height, GC3Dint border,
    GC3Denum format, GC3Denum type, ArrayBufferView* pixels, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateTexFuncData("texImage2D", level, width, height, format, type, pixels, NullAllowed)
        || !validateTexFunc("texImage2D", NotTexSubImage2D, SourceArrayBufferView, target, level, internalformat, width, height, border, format, type, 0, 0))
        return;
    void* data = pixels ? pixels->baseAddress() : 0;
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
                                           m_unpackAlignment,
                                           m_unpackFlipY, m_unpackPremultiplyAlpha,
                                           data,
                                           tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, width, height, border, format, type, data, exceptionState);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
    GC3Denum format, GC3Denum type, ImageData* pixels, ExceptionState& exceptionState)
{
    if (isContextLost() || !pixels || !validateTexFunc("texImage2D", NotTexSubImage2D, SourceImageData, target, level, internalformat, pixels->width(), pixels->height(), 0, format, type, 0, 0))
        return;
    Vector<uint8_t> data;
    bool needConversion = true;
    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    if (!m_unpackFlipY && !m_unpackPremultiplyAlpha && format == GL_RGBA && type == GL_UNSIGNED_BYTE)
        needConversion = false;
    else {
        if (!m_context->extractImageData(pixels->data()->data(), pixels->size(), format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GL_INVALID_VALUE, "texImage2D", "bad image data");
            return;
        }
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, pixels->width(), pixels->height(), 0, format, type, needConversion ? data.data() : pixels->data()->data(), exceptionState);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
    GC3Denum format, GC3Denum type, HTMLImageElement* image, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLImageElement("texImage2D", image, exceptionState))
        return;

    RefPtr<Image> imageForRender = image->cachedImage()->imageForRenderer(image->renderer());
    if (imageForRender->isSVGImage())
        imageForRender = drawImageIntoBuffer(imageForRender.get(), image->width(), image->height());

    if (!imageForRender || !validateTexFunc("texImage2D", NotTexSubImage2D, SourceHTMLImageElement, target, level, internalformat, imageForRender->width(), imageForRender->height(), 0, format, type, 0, 0))
        return;

    texImage2DImpl(target, level, internalformat, format, type, imageForRender.get(), GraphicsContext3D::HtmlDomImage, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

void WebGLRenderingContext::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
    GC3Denum format, GC3Denum type, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLCanvasElement("texImage2D", canvas, exceptionState) || !validateTexFunc("texImage2D", NotTexSubImage2D, SourceHTMLCanvasElement, target, level, internalformat, canvas->width(), canvas->height(), 0, format, type, 0, 0))
        return;

    WebGLTexture* texture = validateTextureBinding("texImage2D", target, true);
    // If possible, copy from the canvas element directly to the texture
    // via the GPU, without a read-back to system memory.
    if (GL_TEXTURE_2D == target && texture) {
        if (!canvas->is3D()) {
            ImageBuffer* buffer = canvas->buffer();
            if (buffer && buffer->copyToPlatformTexture(*m_context.get(), texture->object(), internalformat, type,
                level, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
                texture->setLevelInfo(target, level, internalformat, canvas->width(), canvas->height(), type);
                return;
            }
        } else {
            WebGLRenderingContext* gl = toWebGLRenderingContext(canvas->renderingContext());
            if (gl && gl->m_drawingBuffer->copyToPlatformTexture(*m_context.get(), texture->object(), internalformat, type,
                level, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
                texture->setLevelInfo(target, level, internalformat, canvas->width(), canvas->height(), type);
                return;
            }
        }
    }

    RefPtr<ImageData> imageData = canvas->getImageData();
    if (imageData)
        texImage2D(target, level, internalformat, format, type, imageData.get(), exceptionState);
    else
        texImage2DImpl(target, level, internalformat, format, type, canvas->copiedImage(), GraphicsContext3D::HtmlDomCanvas, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

PassRefPtr<Image> WebGLRenderingContext::videoFrameToImage(HTMLVideoElement* video, BackingStoreCopy backingStoreCopy)
{
    IntSize size(video->videoWidth(), video->videoHeight());
    ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
    if (!buf) {
        synthesizeGLError(GL_OUT_OF_MEMORY, "texImage2D", "out of memory");
        return 0;
    }
    IntRect destRect(0, 0, size.width(), size.height());
    // FIXME: Turn this into a GPU-GPU texture copy instead of CPU readback.
    video->paintCurrentFrameInContext(buf->context(), destRect);
    return buf->copyImage(backingStoreCopy);
}

void WebGLRenderingContext::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
    GC3Denum format, GC3Denum type, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLVideoElement("texImage2D", video, exceptionState)
        || !validateTexFunc("texImage2D", NotTexSubImage2D, SourceHTMLVideoElement, target, level, internalformat, video->videoWidth(), video->videoHeight(), 0, format, type, 0, 0))
        return;

    // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
    // Otherwise, it will fall back to the normal SW path.
    WebGLTexture* texture = validateTextureBinding("texImage2D", target, true);
    if (GL_TEXTURE_2D == target && texture) {
        if (video->copyVideoTextureToPlatformTexture(m_context.get(), texture->object(), level, type, internalformat, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
            texture->setLevelInfo(target, level, internalformat, video->videoWidth(), video->videoHeight(), type);
            return;
        }
    }

    // Normal pure SW path.
    RefPtr<Image> image = videoFrameToImage(video, ImageBuffer::fastCopyImageMode());
    if (!image)
        return;
    texImage2DImpl(target, level, internalformat, format, type, image.get(), GraphicsContext3D::HtmlDomVideo, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

void WebGLRenderingContext::texParameter(GC3Denum target, GC3Denum pname, GC3Dfloat paramf, GC3Dint parami, bool isFloat)
{
    if (isContextLost())
        return;
    WebGLTexture* tex = validateTextureBinding("texParameter", target, false);
    if (!tex)
        return;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_MAG_FILTER:
        break;
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        if ((isFloat && paramf != GL_CLAMP_TO_EDGE && paramf != GL_MIRRORED_REPEAT && paramf != GL_REPEAT)
            || (!isFloat && parami != GL_CLAMP_TO_EDGE && parami != GL_MIRRORED_REPEAT && parami != GL_REPEAT)) {
            synthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter");
            return;
        }
        break;
    case Extensions3D::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (!m_extTextureFilterAnisotropic) {
            synthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter, EXT_texture_filter_anisotropic not enabled");
            return;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter name");
        return;
    }
    if (isFloat) {
        tex->setParameterf(pname, paramf);
        m_context->texParameterf(target, pname, paramf);
    } else {
        tex->setParameteri(pname, parami);
        m_context->texParameteri(target, pname, parami);
    }
}

void WebGLRenderingContext::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param)
{
    texParameter(target, pname, param, 0, true);
}

void WebGLRenderingContext::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param)
{
    texParameter(target, pname, 0, param, false);
}

void WebGLRenderingContext::texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels, ExceptionState& exceptionState)
{
    // FIXME: Handle errors.
    ASSERT(!isContextLost());
    ASSERT(validateTexFuncParameters("texSubImage2D", TexSubImage2D, target, level, format, width, height, 0, format, type));
    ASSERT(validateSize("texSubImage2D", xoffset, yoffset));
    ASSERT(validateSettableTexFormat("texSubImage2D", format));
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    if (!tex) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT((xoffset + width) >= 0);
    ASSERT((yoffset + height) >= 0);
    ASSERT(tex->getWidth(target, level) >= (xoffset + width));
    ASSERT(tex->getHeight(target, level) >= (yoffset + height));
    ASSERT(tex->getInternalFormat(target, level) == format);
    ASSERT(tex->getType(target, level) == type);
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void WebGLRenderingContext::texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image* image, GraphicsContext3D::ImageHtmlDomSource domSource, bool flipY, bool premultiplyAlpha, ExceptionState& exceptionState)
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    Vector<uint8_t> data;
    GraphicsContext3D::ImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GL_NONE);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GL_INVALID_VALUE, "texSubImage2D", "bad image");
        return;
    }
    GraphicsContext3D::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContext3D::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();

    bool needConversion = true;
    if (type == GL_UNSIGNED_BYTE && sourceDataFormat == GraphicsContext3D::DataFormatRGBA8 && format == GL_RGBA && alphaOp == GraphicsContext3D::AlphaDoNothing && !flipY)
        needConversion = false;
    else {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), imageExtractor.imageSourceUnpackAlignment(), data)) {
            synthesizeGLError(GL_INVALID_VALUE, "texImage2D", "bad image data");
            return;
        }
    }

    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texSubImage2DBase(target, level, xoffset, yoffset, image->width(), image->height(), format, type,  needConversion ? data.data() : imagePixelData, exceptionState);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Dsizei width, GC3Dsizei height,
    GC3Denum format, GC3Denum type, ArrayBufferView* pixels, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateTexFuncData("texSubImage2D", level, width, height, format, type, pixels, NullNotAllowed)
        || !validateTexFunc("texSubImage2D", TexSubImage2D, SourceArrayBufferView, target, level, format, width, height, 0, format, type, xoffset, yoffset))
        return;
    void* data = pixels->baseAddress();
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
                                           m_unpackAlignment,
                                           m_unpackFlipY, m_unpackPremultiplyAlpha,
                                           data,
                                           tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, type, data, exceptionState);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Denum format, GC3Denum type, ImageData* pixels, ExceptionState& exceptionState)
{
    if (isContextLost() || !pixels || !validateTexFunc("texSubImage2D", TexSubImage2D, SourceImageData, target, level, format,  pixels->width(), pixels->height(), 0, format, type, xoffset, yoffset))
        return;

    Vector<uint8_t> data;
    bool needConversion = true;
    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE && !m_unpackFlipY && !m_unpackPremultiplyAlpha)
        needConversion = false;
    else {
        if (!m_context->extractImageData(pixels->data()->data(), pixels->size(), format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GL_INVALID_VALUE, "texSubImage2D", "bad image data");
            return;
        }
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    texSubImage2DBase(target, level, xoffset, yoffset, pixels->width(), pixels->height(), format, type, needConversion ? data.data() : pixels->data()->data(), exceptionState);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Denum format, GC3Denum type, HTMLImageElement* image, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLImageElement("texSubImage2D", image, exceptionState))
        return;

    RefPtr<Image> imageForRender = image->cachedImage()->imageForRenderer(image->renderer());
    if (imageForRender->isSVGImage())
        imageForRender = drawImageIntoBuffer(imageForRender.get(), image->width(), image->height());

    if (!imageForRender || !validateTexFunc("texSubImage2D", TexSubImage2D, SourceHTMLImageElement, target, level, format, imageForRender->width(), imageForRender->height(), 0, format, type, xoffset, yoffset))
        return;

    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, imageForRender.get(), GraphicsContext3D::HtmlDomImage, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Denum format, GC3Denum type, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLCanvasElement("texSubImage2D", canvas, exceptionState)
        || !validateTexFunc("texSubImage2D", TexSubImage2D, SourceHTMLCanvasElement, target, level, format, canvas->width(), canvas->height(), 0, format, type, xoffset, yoffset))
        return;

    RefPtr<ImageData> imageData = canvas->getImageData();
    if (imageData)
        texSubImage2D(target, level, xoffset, yoffset, format, type, imageData.get(), exceptionState);
    else
        texSubImage2DImpl(target, level, xoffset, yoffset, format, type, canvas->copiedImage(), GraphicsContext3D::HtmlDomCanvas, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
    GC3Denum format, GC3Denum type, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    if (isContextLost() || !validateHTMLVideoElement("texSubImage2D", video, exceptionState)
        || !validateTexFunc("texSubImage2D", TexSubImage2D, SourceHTMLVideoElement, target, level, format, video->videoWidth(), video->videoHeight(), 0, format, type, xoffset, yoffset))
        return;

    RefPtr<Image> image = videoFrameToImage(video, ImageBuffer::fastCopyImageMode());
    if (!image)
        return;
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, image.get(), GraphicsContext3D::HtmlDomVideo, m_unpackFlipY, m_unpackPremultiplyAlpha, exceptionState);
}

void WebGLRenderingContext::uniform1f(const WebGLUniformLocation* location, GC3Dfloat x)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform1f", "location not for current program");
        return;
    }

    m_context->uniform1f(location->location(), x);
}

void WebGLRenderingContext::uniform1fv(const WebGLUniformLocation* location, Float32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform1fv", location, v, 1))
        return;

    m_context->uniform1fv(location->location(), v->length(), v->data());
}

void WebGLRenderingContext::uniform1fv(const WebGLUniformLocation* location, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform1fv", location, v, size, 1))
        return;

    m_context->uniform1fv(location->location(), size, v);
}

void WebGLRenderingContext::uniform1i(const WebGLUniformLocation* location, GC3Dint x)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform1i", "location not for current program");
        return;
    }

    m_context->uniform1i(location->location(), x);
}

void WebGLRenderingContext::uniform1iv(const WebGLUniformLocation* location, Int32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform1iv", location, v, 1))
        return;

    m_context->uniform1iv(location->location(), v->length(), v->data());
}

void WebGLRenderingContext::uniform1iv(const WebGLUniformLocation* location, GC3Dint* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform1iv", location, v, size, 1))
        return;

    m_context->uniform1iv(location->location(), size, v);
}

void WebGLRenderingContext::uniform2f(const WebGLUniformLocation* location, GC3Dfloat x, GC3Dfloat y)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform2f", "location not for current program");
        return;
    }

    m_context->uniform2f(location->location(), x, y);
}

void WebGLRenderingContext::uniform2fv(const WebGLUniformLocation* location, Float32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform2fv", location, v, 2))
        return;

    m_context->uniform2fv(location->location(), v->length() / 2, v->data());
}

void WebGLRenderingContext::uniform2fv(const WebGLUniformLocation* location, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform2fv", location, v, size, 2))
        return;

    m_context->uniform2fv(location->location(), size / 2, v);
}

void WebGLRenderingContext::uniform2i(const WebGLUniformLocation* location, GC3Dint x, GC3Dint y)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform2i", "location not for current program");
        return;
    }

    m_context->uniform2i(location->location(), x, y);
}

void WebGLRenderingContext::uniform2iv(const WebGLUniformLocation* location, Int32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform2iv", location, v, 2))
        return;

    m_context->uniform2iv(location->location(), v->length() / 2, v->data());
}

void WebGLRenderingContext::uniform2iv(const WebGLUniformLocation* location, GC3Dint* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform2iv", location, v, size, 2))
        return;

    m_context->uniform2iv(location->location(), size / 2, v);
}

void WebGLRenderingContext::uniform3f(const WebGLUniformLocation* location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform3f", "location not for current program");
        return;
    }

    m_context->uniform3f(location->location(), x, y, z);
}

void WebGLRenderingContext::uniform3fv(const WebGLUniformLocation* location, Float32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform3fv", location, v, 3))
        return;

    m_context->uniform3fv(location->location(), v->length() / 3, v->data());
}

void WebGLRenderingContext::uniform3fv(const WebGLUniformLocation* location, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform3fv", location, v, size, 3))
        return;

    m_context->uniform3fv(location->location(), size / 3, v);
}

void WebGLRenderingContext::uniform3i(const WebGLUniformLocation* location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform3i", "location not for current program");
        return;
    }

    m_context->uniform3i(location->location(), x, y, z);
}

void WebGLRenderingContext::uniform3iv(const WebGLUniformLocation* location, Int32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform3iv", location, v, 3))
        return;

    m_context->uniform3iv(location->location(), v->length() / 3, v->data());
}

void WebGLRenderingContext::uniform3iv(const WebGLUniformLocation* location, GC3Dint* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform3iv", location, v, size, 3))
        return;

    m_context->uniform3iv(location->location(), size / 3, v);
}

void WebGLRenderingContext::uniform4f(const WebGLUniformLocation* location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform4f", "location not for current program");
        return;
    }

    m_context->uniform4f(location->location(), x, y, z, w);
}

void WebGLRenderingContext::uniform4fv(const WebGLUniformLocation* location, Float32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform4fv", location, v, 4))
        return;

    m_context->uniform4fv(location->location(), v->length() / 4, v->data());
}

void WebGLRenderingContext::uniform4fv(const WebGLUniformLocation* location, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform4fv", location, v, size, 4))
        return;

    m_context->uniform4fv(location->location(), size / 4, v);
}

void WebGLRenderingContext::uniform4i(const WebGLUniformLocation* location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform4i", "location not for current program");
        return;
    }

    m_context->uniform4i(location->location(), x, y, z, w);
}

void WebGLRenderingContext::uniform4iv(const WebGLUniformLocation* location, Int32Array* v)
{
    if (isContextLost() || !validateUniformParameters("uniform4iv", location, v, 4))
        return;

    m_context->uniform4iv(location->location(), v->length() / 4, v->data());
}

void WebGLRenderingContext::uniform4iv(const WebGLUniformLocation* location, GC3Dint* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformParameters("uniform4iv", location, v, size, 4))
        return;

    m_context->uniform4iv(location->location(), size / 4, v);
}

void WebGLRenderingContext::uniformMatrix2fv(const WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* v)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, v, 4))
        return;
    m_context->uniformMatrix2fv(location->location(), v->length() / 4, transpose, v->data());
}

void WebGLRenderingContext::uniformMatrix2fv(const WebGLUniformLocation* location, GC3Dboolean transpose, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, v, size, 4))
        return;
    m_context->uniformMatrix2fv(location->location(), size / 4, transpose, v);
}

void WebGLRenderingContext::uniformMatrix3fv(const WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* v)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, v, 9))
        return;
    m_context->uniformMatrix3fv(location->location(), v->length() / 9, transpose, v->data());
}

void WebGLRenderingContext::uniformMatrix3fv(const WebGLUniformLocation* location, GC3Dboolean transpose, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, v, size, 9))
        return;
    m_context->uniformMatrix3fv(location->location(), size / 9, transpose, v);
}

void WebGLRenderingContext::uniformMatrix4fv(const WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* v)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, v, 16))
        return;
    m_context->uniformMatrix4fv(location->location(), v->length() / 16, transpose, v->data());
}

void WebGLRenderingContext::uniformMatrix4fv(const WebGLUniformLocation* location, GC3Dboolean transpose, GC3Dfloat* v, GC3Dsizei size)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, v, size, 16))
        return;
    m_context->uniformMatrix4fv(location->location(), size / 16, transpose, v);
}

void WebGLRenderingContext::useProgram(WebGLProgram* program)
{
    bool deleted;
    if (!checkObjectToBeBound("useProgram", program, deleted))
        return;
    if (deleted)
        program = 0;
    if (program && !program->linkStatus()) {
        synthesizeGLError(GL_INVALID_OPERATION, "useProgram", "program not valid");
        return;
    }
    if (m_currentProgram != program) {
        if (m_currentProgram)
            m_currentProgram->onDetached(graphicsContext3D());
        m_currentProgram = program;
        m_context->useProgram(objectOrZero(program));
        if (program)
            program->onAttached();
    }
}

void WebGLRenderingContext::validateProgram(WebGLProgram* program)
{
    if (isContextLost() || !validateWebGLObject("validateProgram", program))
        return;
    m_context->validateProgram(objectOrZero(program));
}

void WebGLRenderingContext::vertexAttrib1f(GC3Duint index, GC3Dfloat v0)
{
    vertexAttribfImpl("vertexAttrib1f", index, 1, v0, 0.0f, 0.0f, 1.0f);
}

void WebGLRenderingContext::vertexAttrib1fv(GC3Duint index, Float32Array* v)
{
    vertexAttribfvImpl("vertexAttrib1fv", index, v, 1);
}

void WebGLRenderingContext::vertexAttrib1fv(GC3Duint index, GC3Dfloat* v, GC3Dsizei size)
{
    vertexAttribfvImpl("vertexAttrib1fv", index, v, size, 1);
}

void WebGLRenderingContext::vertexAttrib2f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1)
{
    vertexAttribfImpl("vertexAttrib2f", index, 2, v0, v1, 0.0f, 1.0f);
}

void WebGLRenderingContext::vertexAttrib2fv(GC3Duint index, Float32Array* v)
{
    vertexAttribfvImpl("vertexAttrib2fv", index, v, 2);
}

void WebGLRenderingContext::vertexAttrib2fv(GC3Duint index, GC3Dfloat* v, GC3Dsizei size)
{
    vertexAttribfvImpl("vertexAttrib2fv", index, v, size, 2);
}

void WebGLRenderingContext::vertexAttrib3f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    vertexAttribfImpl("vertexAttrib3f", index, 3, v0, v1, v2, 1.0f);
}

void WebGLRenderingContext::vertexAttrib3fv(GC3Duint index, Float32Array* v)
{
    vertexAttribfvImpl("vertexAttrib3fv", index, v, 3);
}

void WebGLRenderingContext::vertexAttrib3fv(GC3Duint index, GC3Dfloat* v, GC3Dsizei size)
{
    vertexAttribfvImpl("vertexAttrib3fv", index, v, size, 3);
}

void WebGLRenderingContext::vertexAttrib4f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    vertexAttribfImpl("vertexAttrib4f", index, 4, v0, v1, v2, v3);
}

void WebGLRenderingContext::vertexAttrib4fv(GC3Duint index, Float32Array* v)
{
    vertexAttribfvImpl("vertexAttrib4fv", index, v, 4);
}

void WebGLRenderingContext::vertexAttrib4fv(GC3Duint index, GC3Dfloat* v, GC3Dsizei size)
{
    vertexAttribfvImpl("vertexAttrib4fv", index, v, size, 4);
}

void WebGLRenderingContext::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, long long offset)
{
    if (isContextLost())
        return;
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_FLOAT:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "vertexAttribPointer", "invalid type");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribPointer", "index out of range");
        return;
    }
    if (size < 1 || size > 4 || stride < 0 || stride > 255 || offset < 0) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribPointer", "bad size, stride or offset");
        return;
    }
    if (!m_boundArrayBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribPointer", "no bound ARRAY_BUFFER");
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride
    unsigned int typeSize = sizeInBytes(type);
    if (!typeSize) {
        synthesizeGLError(GL_INVALID_ENUM, "vertexAttribPointer", "invalid type");
        return;
    }
    if ((stride % typeSize) || (static_cast<GC3Dintptr>(offset) % typeSize)) {
        synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribPointer", "stride or offset not valid for type");
        return;
    }
    GC3Dsizei bytesPerElement = size * typeSize;

    m_boundVertexArrayObject->setVertexAttribState(index, bytesPerElement, size, type, normalized, stride, static_cast<GC3Dintptr>(offset), m_boundArrayBuffer);
    m_context->vertexAttribPointer(index, size, type, normalized, stride, static_cast<GC3Dintptr>(offset));
}

void WebGLRenderingContext::vertexAttribDivisorANGLE(GC3Duint index, GC3Duint divisor)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisorANGLE", "index out of range");
        return;
    }

    m_boundVertexArrayObject->setVertexAttribDivisor(index, divisor);
    m_context->extensions()->vertexAttribDivisorANGLE(index, divisor);
}

void WebGLRenderingContext::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("viewport", width, height))
        return;
    m_context->viewport(x, y, width, height);
}

void WebGLRenderingContext::forceLostContext(WebGLRenderingContext::LostContextMode mode)
{
    if (isContextLost()) {
        synthesizeGLError(GL_INVALID_OPERATION, "loseContext", "context already lost");
        return;
    }

    m_contextGroup->loseContextGroup(mode);
}

void WebGLRenderingContext::loseContextImpl(WebGLRenderingContext::LostContextMode mode)
{
    if (isContextLost())
        return;

    m_contextLost = true;
    m_contextLostMode = mode;

    if (mode == RealLostContext) {
        // Inform the embedder that a lost context was received. In response, the embedder might
        // decide to take action such as asking the user for permission to use WebGL again.
        if (Frame* frame = canvas()->document().frame())
            frame->loader().client()->didLoseWebGLContext(m_context->extensions()->getGraphicsResetStatusARB());
    }

    // Make absolutely sure we do not refer to an already-deleted texture or framebuffer.
    m_drawingBuffer->setTexture2DBinding(0);
    m_drawingBuffer->setFramebufferBinding(0);

    detachAndRemoveAllObjects();

    // Lose all the extensions.
    for (size_t i = 0; i < m_extensions.size(); ++i) {
        ExtensionTracker* tracker = m_extensions[i];
        tracker->loseExtension();
    }

    removeAllCompressedTextureFormats();

    if (mode != RealLostContext)
        destroyGraphicsContext3D();

    ConsoleDisplayPreference display = (mode == RealLostContext) ? DisplayInConsole: DontDisplayInConsole;
    synthesizeGLError(GC3D_CONTEXT_LOST_WEBGL, "loseContext", "context lost", display);

    // Don't allow restoration unless the context lost event has both been
    // dispatched and its default behavior prevented.
    m_restoreAllowed = false;

    // Always defer the dispatch of the context lost event, to implement
    // the spec behavior of queueing a task.
    m_dispatchContextLostEventTimer.startOneShot(0);
}

void WebGLRenderingContext::forceRestoreContext()
{
    if (!isContextLost()) {
        synthesizeGLError(GL_INVALID_OPERATION, "restoreContext", "context not lost");
        return;
    }

    if (!m_restoreAllowed) {
        if (m_contextLostMode == SyntheticLostContext)
            synthesizeGLError(GL_INVALID_OPERATION, "restoreContext", "context restoration not allowed");
        return;
    }

    if (!m_restoreTimer.isActive())
        m_restoreTimer.startOneShot(0);
}

blink::WebLayer* WebGLRenderingContext::platformLayer() const
{
    return m_drawingBuffer->platformLayer();
}

void WebGLRenderingContext::removeSharedObject(WebGLSharedObject* object)
{
    m_contextGroup->removeObject(object);
}

void WebGLRenderingContext::addSharedObject(WebGLSharedObject* object)
{
    ASSERT(!isContextLost());
    m_contextGroup->addObject(object);
}

void WebGLRenderingContext::removeContextObject(WebGLContextObject* object)
{
    m_contextObjects.remove(object);
}

void WebGLRenderingContext::addContextObject(WebGLContextObject* object)
{
    ASSERT(!isContextLost());
    m_contextObjects.add(object);
}

void WebGLRenderingContext::detachAndRemoveAllObjects()
{
    while (m_contextObjects.size() > 0) {
        HashSet<WebGLContextObject*>::iterator it = m_contextObjects.begin();
        (*it)->detachContext();
    }
}

bool WebGLRenderingContext::hasPendingActivity() const
{
    return false;
}

void WebGLRenderingContext::stop()
{
    if (!isContextLost()) {
        forceLostContext(SyntheticLostContext);
        destroyGraphicsContext3D();
    }
}

WebGLGetInfo WebGLRenderingContext::getBooleanParameter(GC3Denum pname)
{
    GC3Dboolean value = 0;
    if (!isContextLost())
        m_context->getBooleanv(pname, &value);
    return WebGLGetInfo(static_cast<bool>(value));
}

WebGLGetInfo WebGLRenderingContext::getBooleanArrayParameter(GC3Denum pname)
{
    if (pname != GL_COLOR_WRITEMASK) {
        notImplemented();
        return WebGLGetInfo(0, 0);
    }
    GC3Dboolean value[4] = {0};
    if (!isContextLost())
        m_context->getBooleanv(pname, value);
    bool boolValue[4];
    for (int ii = 0; ii < 4; ++ii)
        boolValue[ii] = static_cast<bool>(value[ii]);
    return WebGLGetInfo(boolValue, 4);
}

WebGLGetInfo WebGLRenderingContext::getFloatParameter(GC3Denum pname)
{
    GC3Dfloat value = 0;
    if (!isContextLost())
        m_context->getFloatv(pname, &value);
    return WebGLGetInfo(value);
}

WebGLGetInfo WebGLRenderingContext::getIntParameter(GC3Denum pname)
{
    GC3Dint value = 0;
    if (!isContextLost())
        m_context->getIntegerv(pname, &value);
    return WebGLGetInfo(value);
}

WebGLGetInfo WebGLRenderingContext::getUnsignedIntParameter(GC3Denum pname)
{
    GC3Dint value = 0;
    if (!isContextLost())
        m_context->getIntegerv(pname, &value);
    return WebGLGetInfo(static_cast<unsigned int>(value));
}

WebGLGetInfo WebGLRenderingContext::getWebGLFloatArrayParameter(GC3Denum pname)
{
    GC3Dfloat value[4] = {0};
    if (!isContextLost())
        m_context->getFloatv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_DEPTH_RANGE:
        length = 2;
        break;
    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return WebGLGetInfo(Float32Array::create(value, length));
}

WebGLGetInfo WebGLRenderingContext::getWebGLIntArrayParameter(GC3Denum pname)
{
    GC3Dint value[4] = {0};
    if (!isContextLost())
        m_context->getIntegerv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
        length = 2;
        break;
    case GL_SCISSOR_BOX:
    case GL_VIEWPORT:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return WebGLGetInfo(Int32Array::create(value, length));
}

void WebGLRenderingContext::handleTextureCompleteness(const char* functionName, bool prepareToDraw)
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    bool resetActiveUnit = false;
    WebGLTexture::TextureExtensionFlag flag = static_cast<WebGLTexture::TextureExtensionFlag>((m_oesTextureFloatLinear ? WebGLTexture::TextureFloatLinearExtensionEnabled : 0)
        | (m_oesTextureHalfFloatLinear ? WebGLTexture::TextureHalfFloatLinearExtensionEnabled : 0));
    for (unsigned ii = 0; ii < m_onePlusMaxNonDefaultTextureUnit; ++ii) {
        if ((m_textureUnits[ii].m_texture2DBinding.get() && m_textureUnits[ii].m_texture2DBinding->needToUseBlackTexture(flag))
            || (m_textureUnits[ii].m_textureCubeMapBinding.get() && m_textureUnits[ii].m_textureCubeMapBinding->needToUseBlackTexture(flag))) {
            if (ii != m_activeTextureUnit) {
                m_context->activeTexture(ii);
                resetActiveUnit = true;
            } else if (resetActiveUnit) {
                m_context->activeTexture(ii);
                resetActiveUnit = false;
            }
            WebGLTexture* tex2D;
            WebGLTexture* texCubeMap;
            if (prepareToDraw) {
                String msg(String("texture bound to texture unit ") + String::number(ii)
                    + " is not renderable. It maybe non-power-of-2 and have incompatible texture filtering or is not 'texture complete'."
                    + " Or the texture is Float or Half Float type with linear filtering while OES_float_linear or OES_half_float_linear extension is not enabled.");
                emitGLWarning(functionName, msg.utf8().data());
                tex2D = m_blackTexture2D.get();
                texCubeMap = m_blackTextureCubeMap.get();
            } else {
                tex2D = m_textureUnits[ii].m_texture2DBinding.get();
                texCubeMap = m_textureUnits[ii].m_textureCubeMapBinding.get();
            }
            if (m_textureUnits[ii].m_texture2DBinding && m_textureUnits[ii].m_texture2DBinding->needToUseBlackTexture(flag))
                m_context->bindTexture(GL_TEXTURE_2D, objectOrZero(tex2D));
            if (m_textureUnits[ii].m_textureCubeMapBinding && m_textureUnits[ii].m_textureCubeMapBinding->needToUseBlackTexture(flag))
                m_context->bindTexture(GL_TEXTURE_CUBE_MAP, objectOrZero(texCubeMap));
        }
    }
    if (resetActiveUnit)
        m_context->activeTexture(m_activeTextureUnit);
}

void WebGLRenderingContext::createFallbackBlackTextures1x1()
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    unsigned char black[] = {0, 0, 0, 255};
    m_blackTexture2D = createTexture();
    m_context->bindTexture(GL_TEXTURE_2D, m_blackTexture2D->object());
    m_context->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->bindTexture(GL_TEXTURE_2D, 0);
    m_blackTextureCubeMap = createTexture();
    m_context->bindTexture(GL_TEXTURE_CUBE_MAP, m_blackTextureCubeMap->object());
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 1, 1,
        0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    m_context->bindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

bool WebGLRenderingContext::isTexInternalFormatColorBufferCombinationValid(GC3Denum texInternalFormat,
                                                                           GC3Denum colorBufferFormat)
{
    unsigned need = GraphicsContext3D::getChannelBitsByFormat(texInternalFormat);
    unsigned have = GraphicsContext3D::getChannelBitsByFormat(colorBufferFormat);
    return (need & have) == need;
}

GC3Denum WebGLRenderingContext::boundFramebufferColorFormat()
{
    if (m_framebufferBinding && m_framebufferBinding->object())
        return m_framebufferBinding->colorBufferFormat();
    if (m_attributes.alpha)
        return GL_RGBA;
    return GL_RGB;
}

int WebGLRenderingContext::boundFramebufferWidth()
{
    if (m_framebufferBinding && m_framebufferBinding->object())
        return m_framebufferBinding->colorBufferWidth();
    return m_drawingBuffer->size().width();
}

int WebGLRenderingContext::boundFramebufferHeight()
{
    if (m_framebufferBinding && m_framebufferBinding->object())
        return m_framebufferBinding->colorBufferHeight();
    return m_drawingBuffer->size().height();
}

WebGLTexture* WebGLRenderingContext::validateTextureBinding(const char* functionName, GC3Denum target, bool useSixEnumsForCubeMap)
{
    WebGLTexture* tex = 0;
    switch (target) {
    case GL_TEXTURE_2D:
        tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get();
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (!useSixEnumsForCubeMap) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture target");
            return 0;
        }
        tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding.get();
        break;
    case GL_TEXTURE_CUBE_MAP:
        if (useSixEnumsForCubeMap) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture target");
            return 0;
        }
        tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding.get();
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture target");
        return 0;
    }
    if (!tex)
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no texture");
    return tex;
}

bool WebGLRenderingContext::validateLocationLength(const char* functionName, const String& string)
{
    const unsigned maxWebGLLocationLength = 256;
    if (string.length() > maxWebGLLocationLength) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "location length > 256");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateSize(const char* functionName, GC3Dint x, GC3Dint y)
{
    if (x < 0 || y < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "size < 0");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateString(const char* functionName, const String& string)
{
    for (size_t i = 0; i < string.length(); ++i) {
        if (!validateCharacter(string[i])) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "string not ASCII");
            return false;
        }
    }
    return true;
}

bool WebGLRenderingContext::validateTexFuncFormatAndType(const char* functionName, GC3Denum format, GC3Denum type, GC3Dint level)
{
    switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
    case GL_RGB:
    case GL_RGBA:
        break;
    case GL_DEPTH_STENCIL_OES:
    case GL_DEPTH_COMPONENT:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "depth texture formats not enabled");
        return false;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture format");
        return false;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        break;
    case GL_FLOAT:
        if (m_oesTextureFloat)
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GL_HALF_FLOAT_OES:
        if (m_oesTextureHalfFloat)
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_SHORT:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture type");
        return false;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture type");
        return false;
    }

    // Verify that the combination of format and type is supported.
    switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
        if (type != GL_UNSIGNED_BYTE
            && type != GL_FLOAT
            && type != GL_HALF_FLOAT_OES) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "invalid type for format");
            return false;
        }
        break;
    case GL_RGB:
        if (type != GL_UNSIGNED_BYTE
            && type != GL_UNSIGNED_SHORT_5_6_5
            && type != GL_FLOAT
            && type != GL_HALF_FLOAT_OES) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "invalid type for RGB format");
            return false;
        }
        break;
    case GL_RGBA:
        if (type != GL_UNSIGNED_BYTE
            && type != GL_UNSIGNED_SHORT_4_4_4_4
            && type != GL_UNSIGNED_SHORT_5_5_5_1
            && type != GL_FLOAT
            && type != GL_HALF_FLOAT_OES) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "invalid type for RGBA format");
            return false;
        }
        break;
    case GL_DEPTH_COMPONENT:
        if (!m_webglDepthTexture) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format. DEPTH_COMPONENT not enabled");
            return false;
        }
        if (type != GL_UNSIGNED_SHORT
            && type != GL_UNSIGNED_INT) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "invalid type for DEPTH_COMPONENT format");
            return false;
        }
        if (level > 0) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "level must be 0 for DEPTH_COMPONENT format");
            return false;
        }
        break;
    case GL_DEPTH_STENCIL_OES:
        if (!m_webglDepthTexture) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format. DEPTH_STENCIL not enabled");
            return false;
        }
        if (type != GL_UNSIGNED_INT_24_8_OES) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "invalid type for DEPTH_STENCIL format");
            return false;
        }
        if (level > 0) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "level must be 0 for DEPTH_STENCIL format");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

bool WebGLRenderingContext::validateTexFuncLevel(const char* functionName, GC3Denum target, GC3Dint level)
{
    if (level < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "level < 0");
        return false;
    }
    switch (target) {
    case GL_TEXTURE_2D:
        if (level >= m_maxTextureLevel) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "level out of range");
            return false;
        }
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (level >= m_maxCubeMapTextureLevel) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "level out of range");
            return false;
        }
        break;
    }
    // This function only checks if level is legal, so we return true and don't
    // generate INVALID_ENUM if target is illegal.
    return true;
}

bool WebGLRenderingContext::validateTexFuncDimensions(const char* functionName, TexFuncValidationFunctionType functionType,
    GC3Denum target, GC3Dint level, GC3Dsizei width, GC3Dsizei height)
{
    if (width < 0 || height < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }

    switch (target) {
    case GL_TEXTURE_2D:
        if (width > (m_maxTextureSize >> level) || height > (m_maxTextureSize >> level)) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "width or height out of range");
            return false;
        }
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (functionType != TexSubImage2D && width != height) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "width != height for cube map");
            return false;
        }
        // No need to check height here. For texImage width == height.
        // For texSubImage that will be checked when checking yoffset + height is in range.
        if (width > (m_maxCubeMapTextureSize >> level)) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "width or height out of range for cube map");
            return false;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateTexFuncParameters(const char* functionName, TexFuncValidationFunctionType functionType, GC3Denum target,
    GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type)
{
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (!validateTexFuncFormatAndType(functionName, format, type, level) || !validateTexFuncLevel(functionName, target, level))
        return false;

    if (!validateTexFuncDimensions(functionName, functionType, target, level, width, height))
        return false;

    if (format != internalformat) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "format != internalformat");
        return false;
    }

    if (border) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "border != 0");
        return false;
    }

    return true;
}

bool WebGLRenderingContext::validateTexFuncData(const char* functionName, GC3Dint level,
                                                GC3Dsizei width, GC3Dsizei height,
                                                GC3Denum format, GC3Denum type,
                                                ArrayBufferView* pixels,
                                                NullDisposition disposition)
{
    // All calling functions check isContextLost, so a duplicate check is not needed here.
    if (!pixels) {
        if (disposition == NullAllowed)
            return true;
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no pixels");
        return false;
    }

    if (!validateTexFuncFormatAndType(functionName, format, type, level))
        return false;
    if (!validateSettableTexFormat(functionName, format))
        return false;

    switch (type) {
    case GL_UNSIGNED_BYTE:
        if (pixels->getType() != ArrayBufferView::TypeUint8) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
            return false;
        }
        break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        if (pixels->getType() != ArrayBufferView::TypeUint16) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
            return false;
        }
        break;
    case GL_FLOAT: // OES_texture_float
        if (pixels->getType() != ArrayBufferView::TypeFloat32) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "type FLOAT but ArrayBufferView not Float32Array");
            return false;
        }
        break;
    case GL_HALF_FLOAT_OES: // OES_texture_half_float
        // As per the specification, ArrayBufferView should be null when
        // OES_texture_half_float is enabled.
        if (pixels) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "type HALF_FLOAT_OES but ArrayBufferView is not NULL");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    unsigned int totalBytesRequired;
    GC3Denum error = m_context->computeImageSizeInBytes(format, type, width, height, m_unpackAlignment, &totalBytesRequired, 0);
    if (error != GL_NO_ERROR) {
        synthesizeGLError(error, functionName, "invalid texture dimensions");
        return false;
    }
    if (pixels->byteLength() < totalBytesRequired) {
        if (m_unpackAlignment != 1) {
          error = m_context->computeImageSizeInBytes(format, type, width, height, 1, &totalBytesRequired, 0);
          if (pixels->byteLength() == totalBytesRequired) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request with UNPACK_ALIGNMENT > 1");
            return false;
          }
        }
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateCompressedTexFormat(GC3Denum format)
{
    return m_compressedTextureFormats.contains(format);
}

bool WebGLRenderingContext::validateCompressedTexFuncData(const char* functionName,
                                                          GC3Dsizei width, GC3Dsizei height,
                                                          GC3Denum format, ArrayBufferView* pixels)
{
    if (!pixels) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no pixels");
        return false;
    }
    if (width < 0 || height < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }

    unsigned int bytesRequired = 0;

    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
        {
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            const int kBlockSize = 8;
            int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
            int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
            int numBlocks = numBlocksAcross * numBlocksDown;
            bytesRequired = numBlocks * kBlockSize;
        }
        break;
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT:
        {
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            const int kBlockSize = 16;
            int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
            int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
            int numBlocks = numBlocksAcross * numBlocksDown;
            bytesRequired = numBlocks * kBlockSize;
        }
        break;
    case Extensions3D::COMPRESSED_ATC_RGB_AMD:
        {
            bytesRequired = floor(static_cast<double>((width + 3) / 4)) * floor(static_cast<double>((height + 3) / 4)) * 8;
        }
        break;
    case Extensions3D::COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case Extensions3D::COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
        {
            bytesRequired = floor(static_cast<double>((width + 3) / 4)) * floor(static_cast<double>((height + 3) / 4)) * 16;
        }
    case Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        {
            bytesRequired = max(width, 8) * max(height, 8) / 2;
        }
        break;
    case Extensions3D::COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case Extensions3D::COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
        {
            bytesRequired = max(width, 8) * max(height, 8) / 4;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format");
        return false;
    }

    if (pixels->byteLength() != bytesRequired) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "length of ArrayBufferView is not correct for dimensions");
        return false;
    }

    return true;
}

bool WebGLRenderingContext::validateCompressedTexDimensions(const char* functionName, TexFuncValidationFunctionType functionType, GC3Denum target, GC3Dint level, GC3Dsizei width, GC3Dsizei height, GC3Denum format)
{
    if (!validateTexFuncDimensions(functionName, functionType, target, level, width, height))
        return false;

    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const int kBlockWidth = 4;
        const int kBlockHeight = 4;
        bool widthValid = (level && width == 1) || (level && width == 2) || !(width % kBlockWidth);
        bool heightValid = (level && height == 1) || (level && height == 2) || !(height % kBlockHeight);
        if (!widthValid || !heightValid) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "width or height invalid for level");
            return false;
        }
        return true;
    }
    default:
        return false;
    }
}

bool WebGLRenderingContext::validateCompressedTexSubDimensions(const char* functionName, GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                                               GC3Dsizei width, GC3Dsizei height, GC3Denum format, WebGLTexture* tex)
{
    if (xoffset < 0 || yoffset < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "xoffset or yoffset < 0");
        return false;
    }

    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const int kBlockWidth = 4;
        const int kBlockHeight = 4;
        if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "xoffset or yoffset not multiple of 4");
            return false;
        }
        if (width - xoffset > tex->getWidth(target, level)
            || height - yoffset > tex->getHeight(target, level)) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "dimensions out of range");
            return false;
        }
        return validateCompressedTexDimensions(functionName, TexSubImage2D, target, level, width, height, format);
    }
    default:
        return false;
    }
}

bool WebGLRenderingContext::validateDrawMode(const char* functionName, GC3Denum mode)
{
    switch (mode) {
    case GL_POINTS:
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_LINES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_TRIANGLES:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid draw mode");
        return false;
    }
}

bool WebGLRenderingContext::validateStencilSettings(const char* functionName)
{
    if (m_stencilMask != m_stencilMaskBack || m_stencilFuncRef != m_stencilFuncRefBack || m_stencilFuncMask != m_stencilFuncMaskBack) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "front and back stencils settings do not match");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateStencilOrDepthFunc(const char* functionName, GC3Denum func)
{
    switch (func) {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_GEQUAL:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid function");
        return false;
    }
}

void WebGLRenderingContext::printGLErrorToConsole(const String& message)
{
    if (!m_numGLErrorsToConsoleAllowed)
        return;

    --m_numGLErrorsToConsoleAllowed;
    printWarningToConsole(message);

    if (!m_numGLErrorsToConsoleAllowed)
        printWarningToConsole("WebGL: too many errors, no more errors will be reported to the console for this context.");

    return;
}

void WebGLRenderingContext::printWarningToConsole(const String& message)
{
    if (!canvas())
        return;
    canvas()->document().addConsoleMessage(RenderingMessageSource, WarningMessageLevel, message);
}

bool WebGLRenderingContext::validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    switch (attachment) {
    case GL_COLOR_ATTACHMENT0:
    case GL_DEPTH_ATTACHMENT:
    case GL_STENCIL_ATTACHMENT:
    case GC3D_DEPTH_STENCIL_ATTACHMENT_WEBGL:
        break;
    default:
        if (m_webglDrawBuffers
            && attachment > GL_COLOR_ATTACHMENT0
            && attachment < static_cast<GC3Denum>(GL_COLOR_ATTACHMENT0 + maxColorAttachments()))
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid attachment");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateBlendEquation(const char* functionName, GC3Denum mode)
{
    switch (mode) {
    case GL_FUNC_ADD:
    case GL_FUNC_SUBTRACT:
    case GL_FUNC_REVERSE_SUBTRACT:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid mode");
        return false;
    }
}

bool WebGLRenderingContext::validateBlendFuncFactors(const char* functionName, GC3Denum src, GC3Denum dst)
{
    if (((src == GL_CONSTANT_COLOR || src == GL_ONE_MINUS_CONSTANT_COLOR)
        && (dst == GL_CONSTANT_ALPHA || dst == GL_ONE_MINUS_CONSTANT_ALPHA))
        || ((dst == GL_CONSTANT_COLOR || dst == GL_ONE_MINUS_CONSTANT_COLOR)
        && (src == GL_CONSTANT_ALPHA || src == GL_ONE_MINUS_CONSTANT_ALPHA))) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "incompatible src and dst");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateCapability(const char* functionName, GC3Denum cap)
{
    switch (cap) {
    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

bool WebGLRenderingContext::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, Float32Array* v, GC3Dsizei requiredMinSize)
{
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return false;
    }
    return validateUniformMatrixParameters(functionName, location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, Int32Array* v, GC3Dsizei requiredMinSize)
{
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return false;
    }
    return validateUniformMatrixParameters(functionName, location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, void* v, GC3Dsizei size, GC3Dsizei requiredMinSize)
{
    return validateUniformMatrixParameters(functionName, location, false, v, size, requiredMinSize);
}

bool WebGLRenderingContext::validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation* location, GC3Dboolean transpose, Float32Array* v, GC3Dsizei requiredMinSize)
{
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return false;
    }
    return validateUniformMatrixParameters(functionName, location, transpose, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation* location, GC3Dboolean transpose, void* v, GC3Dsizei size, GC3Dsizei requiredMinSize)
{
    if (!location)
        return false;
    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "location is not from current program");
        return false;
    }
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return false;
    }
    if (transpose) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "transpose not FALSE");
        return false;
    }
    if (size < requiredMinSize || (size % requiredMinSize)) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid size");
        return false;
    }
    return true;
}

WebGLBuffer* WebGLRenderingContext::validateBufferDataParameters(const char* functionName, GC3Denum target, GC3Denum usage)
{
    WebGLBuffer* buffer = 0;
    switch (target) {
    case GL_ELEMENT_ARRAY_BUFFER:
        buffer = m_boundVertexArrayObject->boundElementArrayBuffer().get();
        break;
    case GL_ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return 0;
    }
    if (!buffer) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no buffer");
        return 0;
    }
    switch (usage) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
        return buffer;
    }
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid usage");
    return 0;
}

bool WebGLRenderingContext::validateHTMLImageElement(const char* functionName, HTMLImageElement* image, ExceptionState& exceptionState)
{
    if (!image || !image->cachedImage()) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no image");
        return false;
    }
    const KURL& url = image->cachedImage()->response().url();
    if (url.isNull() || url.isEmpty() || !url.isValid()) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid image");
        return false;
    }
    if (wouldTaintOrigin(image)) {
        exceptionState.throwSecurityError("The cross-origin image at " + url.elidedString() + " may not be loaded.");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    if (!canvas || !canvas->buffer()) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no canvas");
        return false;
    }
    if (wouldTaintOrigin(canvas)) {
        exceptionState.throwSecurityError("Tainted canvases may not be loaded.");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateHTMLVideoElement(const char* functionName, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    if (!video || !video->videoWidth() || !video->videoHeight()) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no video");
        return false;
    }
    if (wouldTaintOrigin(video)) {
        exceptionState.throwSecurityError("The video element contains cross-origin data, and may not be loaded.");
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateDrawArrays(const char* functionName, GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    if (isContextLost() || !validateDrawMode(functionName, mode))
        return false;

    if (!validateStencilSettings(functionName))
        return false;

    if (first < 0 || count < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "first or count < 0");
        return false;
    }

    if (!count) {
        markContextChanged();
        return false;
    }

    if (!validateRenderingState()) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "attribs not setup correctly");
        return false;
    }

    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
        return false;
    }

    return true;
}

bool WebGLRenderingContext::validateDrawElements(const char* functionName, GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset)
{
    if (isContextLost() || !validateDrawMode(functionName, mode))
        return false;

    if (!validateStencilSettings(functionName))
        return false;

    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
        break;
    case GL_UNSIGNED_INT:
        if (m_oesElementIndexUint)
            break;
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid type");
        return false;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid type");
        return false;
    }

    if (count < 0 || offset < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "count or offset < 0");
        return false;
    }

    if (!count) {
        markContextChanged();
        return false;
    }

    if (!m_boundVertexArrayObject->boundElementArrayBuffer()) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no ELEMENT_ARRAY_BUFFER bound");
        return false;
    }

    if (!validateRenderingState()) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "attribs not setup correctly");
        return false;
    }

    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), &reason)) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
        return false;
    }

    return true;
}

// Helper function to validate draw*Instanced calls
bool WebGLRenderingContext::validateDrawInstanced(const char* functionName, GC3Dsizei primcount)
{
    if (primcount < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "primcount < 0");
        return false;
    }

    // Ensure at least one enabled vertex attrib has a divisor of 0.
    for (unsigned i = 0; i < m_onePlusMaxEnabledAttribIndex; ++i) {
        const WebGLVertexArrayObjectOES::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(i);
        if (state.enabled && !state.divisor)
            return true;
    }

    synthesizeGLError(GL_INVALID_OPERATION, functionName, "at least one enabled attribute must have a divisor of 0");
    return false;
}

void WebGLRenderingContext::vertexAttribfImpl(const char* functionName, GC3Duint index, GC3Dsizei expectedSize, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "index out of range");
        return;
    }
    // In GL, we skip setting vertexAttrib0 values.
    switch (expectedSize) {
    case 1:
        m_context->vertexAttrib1f(index, v0);
        break;
    case 2:
        m_context->vertexAttrib2f(index, v0, v1);
        break;
    case 3:
        m_context->vertexAttrib3f(index, v0, v1, v2);
        break;
    case 4:
        m_context->vertexAttrib4f(index, v0, v1, v2, v3);
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.value[0] = v0;
    attribValue.value[1] = v1;
    attribValue.value[2] = v2;
    attribValue.value[3] = v3;
}

void WebGLRenderingContext::vertexAttribfvImpl(const char* functionName, GC3Duint index, Float32Array* v, GC3Dsizei expectedSize)
{
    if (isContextLost())
        return;
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return;
    }
    vertexAttribfvImpl(functionName, index, v->data(), v->length(), expectedSize);
}

void WebGLRenderingContext::vertexAttribfvImpl(const char* functionName, GC3Duint index, GC3Dfloat* v, GC3Dsizei size, GC3Dsizei expectedSize)
{
    if (isContextLost())
        return;
    if (!v) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
        return;
    }
    if (size < expectedSize) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid size");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "index out of range");
        return;
    }
    // In GL, we skip setting vertexAttrib0 values.
    switch (expectedSize) {
    case 1:
        m_context->vertexAttrib1fv(index, v);
        break;
    case 2:
        m_context->vertexAttrib2fv(index, v);
        break;
    case 3:
        m_context->vertexAttrib3fv(index, v);
        break;
    case 4:
        m_context->vertexAttrib4fv(index, v);
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.initValue();
    for (int ii = 0; ii < expectedSize; ++ii)
        attribValue.value[ii] = v[ii];
}

void WebGLRenderingContext::dispatchContextLostEvent(Timer<WebGLRenderingContext>*)
{
    RefPtr<WebGLContextEvent> event = WebGLContextEvent::create(EventTypeNames::webglcontextlost, false, true, "");
    canvas()->dispatchEvent(event);
    m_restoreAllowed = event->defaultPrevented();
    deactivateContext(this, m_contextLostMode != RealLostContext && m_restoreAllowed);
    if ((m_contextLostMode == RealLostContext || m_contextLostMode == AutoRecoverSyntheticLostContext) && m_restoreAllowed)
        m_restoreTimer.startOneShot(0);
}

void WebGLRenderingContext::maybeRestoreContext(Timer<WebGLRenderingContext>*)
{
    ASSERT(isContextLost());

    // The rendering context is not restored unless the default behavior of the
    // webglcontextlost event was prevented earlier.
    //
    // Because of the way m_restoreTimer is set up for real vs. synthetic lost
    // context events, we don't have to worry about this test short-circuiting
    // the retry loop for real context lost events.
    if (!m_restoreAllowed)
        return;

    Frame* frame = canvas()->document().frame();
    if (!frame)
        return;

    Settings* settings = frame->settings();

    if (!frame->loader().client()->allowWebGL(settings && settings->webGLEnabled()))
        return;

    // Reset the context attributes back to the requested attributes and re-apply restrictions
    m_attributes = adjustAttributes(m_requestedAttributes, settings);

    RefPtr<GraphicsContext3D> context(GraphicsContext3D::create(m_attributes));

    if (!context) {
        if (m_contextLostMode == RealLostContext) {
            m_restoreTimer.startOneShot(secondsBetweenRestoreAttempts);
        } else {
            // This likely shouldn't happen but is the best way to report it to the WebGL app.
            synthesizeGLError(GL_INVALID_OPERATION, "", "error restoring context");
        }
        return;
    }

    RefPtr<WebGLRenderingContextEvictionManager> contextEvictionManager = adoptRef(new WebGLRenderingContextEvictionManager());

    // Construct a new drawing buffer with the new GraphicsContext3D.
    m_drawingBuffer->releaseResources();
    DrawingBuffer::PreserveDrawingBuffer preserve = m_attributes.preserveDrawingBuffer ? DrawingBuffer::Preserve : DrawingBuffer::Discard;
    m_drawingBuffer = DrawingBuffer::create(context.get(), clampedCanvasSize(), preserve, contextEvictionManager.release());

    if (m_drawingBuffer->isZeroSized())
        return;

    m_drawingBuffer->bind();

    lost_context_errors_.clear();

    m_context = context;
    m_contextLost = false;

    setupFlags();
    initializeNewContext();
    canvas()->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextrestored, false, true, ""));
}

String WebGLRenderingContext::ensureNotNull(const String& text) const
{
    if (text.isNull())
        return WTF::emptyString();
    return text;
}

WebGLRenderingContext::LRUImageBufferCache::LRUImageBufferCache(int capacity)
    : m_buffers(adoptArrayPtr(new OwnPtr<ImageBuffer>[capacity]))
    , m_capacity(capacity)
{
}

ImageBuffer* WebGLRenderingContext::LRUImageBufferCache::imageBuffer(const IntSize& size)
{
    int i;
    for (i = 0; i < m_capacity; ++i) {
        ImageBuffer* buf = m_buffers[i].get();
        if (!buf)
            break;
        if (buf->size() != size)
            continue;
        bubbleToFront(i);
        return buf;
    }

    OwnPtr<ImageBuffer> temp(ImageBuffer::create(size));
    if (!temp)
        return 0;
    i = std::min(m_capacity - 1, i);
    m_buffers[i] = temp.release();

    ImageBuffer* buf = m_buffers[i].get();
    bubbleToFront(i);
    return buf;
}

void WebGLRenderingContext::LRUImageBufferCache::bubbleToFront(int idx)
{
    for (int i = idx; i > 0; --i)
        m_buffers[i].swap(m_buffers[i-1]);
}

namespace {

    String GetErrorString(GC3Denum error)
    {
        switch (error) {
        case GL_INVALID_ENUM:
            return "INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "INVALID_FRAMEBUFFER_OPERATION";
        case GC3D_CONTEXT_LOST_WEBGL:
            return "CONTEXT_LOST_WEBGL";
        default:
            return String::format("WebGL ERROR(0x%04X)", error);
        }
    }

} // namespace anonymous

void WebGLRenderingContext::synthesizeGLError(GC3Denum error, const char* functionName, const char* description, ConsoleDisplayPreference display)
{
    String errorType = GetErrorString(error);
    if (m_synthesizedErrorsToConsole && display == DisplayInConsole) {
        String message = String("WebGL: ") + errorType +  ": " + String(functionName) + ": " + String(description);
        printGLErrorToConsole(message);
    }
    if (!isContextLost())
        m_context->synthesizeGLError(error);
    else {
        if (lost_context_errors_.find(error) == WTF::kNotFound)
            lost_context_errors_.append(error);
    }
    InspectorInstrumentation::didFireWebGLError(canvas(), errorType);
}

void WebGLRenderingContext::emitGLWarning(const char* functionName, const char* description)
{
    if (m_synthesizedErrorsToConsole) {
        String message = String("WebGL: ") + String(functionName) + ": " + String(description);
        printGLErrorToConsole(message);
    }
    InspectorInstrumentation::didFireWebGLWarning(canvas());
}

void WebGLRenderingContext::applyStencilTest()
{
    bool haveStencilBuffer = false;

    if (m_framebufferBinding)
        haveStencilBuffer = m_framebufferBinding->hasStencilBuffer();
    else {
        RefPtr<WebGLContextAttributes> attributes = getContextAttributes();
        haveStencilBuffer = attributes->stencil();
    }
    enableOrDisable(GL_STENCIL_TEST,
                    m_stencilEnabled && haveStencilBuffer);
}

void WebGLRenderingContext::enableOrDisable(GC3Denum capability, bool enable)
{
    if (isContextLost())
        return;
    if (enable)
        m_context->enable(capability);
    else
        m_context->disable(capability);
}

IntSize WebGLRenderingContext::clampedCanvasSize()
{
    return IntSize(clamp(canvas()->width(), 1, m_maxViewportDims[0]),
                   clamp(canvas()->height(), 1, m_maxViewportDims[1]));
}

GC3Dint WebGLRenderingContext::maxDrawBuffers()
{
    if (isContextLost() || !m_webglDrawBuffers)
        return 0;
    if (!m_maxDrawBuffers)
        m_context->getIntegerv(Extensions3D::MAX_DRAW_BUFFERS_EXT, &m_maxDrawBuffers);
    if (!m_maxColorAttachments)
        m_context->getIntegerv(Extensions3D::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GC3Dint WebGLRenderingContext::maxColorAttachments()
{
    if (isContextLost() || !m_webglDrawBuffers)
        return 0;
    if (!m_maxColorAttachments)
        m_context->getIntegerv(Extensions3D::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    return m_maxColorAttachments;
}

void WebGLRenderingContext::setBackDrawBuffer(GC3Denum buf)
{
    m_backDrawBuffer = buf;
}

void WebGLRenderingContext::restoreCurrentFramebuffer()
{
    bindFramebuffer(GL_FRAMEBUFFER, m_framebufferBinding.get());
}

void WebGLRenderingContext::restoreCurrentTexture2D()
{
    bindTexture(GL_TEXTURE_2D, m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get());
}

void WebGLRenderingContext::multisamplingChanged(bool enabled)
{
    if (m_multisamplingAllowed != enabled) {
        m_multisamplingAllowed = enabled;
        forceLostContext(WebGLRenderingContext::AutoRecoverSyntheticLostContext);
    }
}

void WebGLRenderingContext::findNewMaxEnabledAttribIndex()
{
    // Trace backwards from the current max to find the new max enabled attrib index
    int startIndex = m_onePlusMaxEnabledAttribIndex - 1;
    for (int i = startIndex; i >= 0; --i) {
        if (m_boundVertexArrayObject->getVertexAttribState(i).enabled) {
            m_onePlusMaxEnabledAttribIndex = i + 1;
            return;
        }
    }
    m_onePlusMaxEnabledAttribIndex = 0;
}

void WebGLRenderingContext::findNewMaxNonDefaultTextureUnit()
{
    // Trace backwards from the current max to find the new max non-default texture unit
    int startIndex = m_onePlusMaxNonDefaultTextureUnit - 1;
    for (int i = startIndex; i >= 0; --i) {
        if (m_textureUnits[i].m_texture2DBinding
            || m_textureUnits[i].m_textureCubeMapBinding) {
            m_onePlusMaxNonDefaultTextureUnit = i + 1;
            return;
        }
    }
    m_onePlusMaxNonDefaultTextureUnit = 0;
}

} // namespace WebCore
