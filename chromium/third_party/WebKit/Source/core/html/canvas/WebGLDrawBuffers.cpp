/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"

#include "core/html/canvas/WebGLDrawBuffers.h"

#include "platform/graphics/Extensions3D.h"

namespace WebCore {

WebGLDrawBuffers::WebGLDrawBuffers(WebGLRenderingContext* context)
    : WebGLExtension(context)
{
    ScriptWrappable::init(this);
    context->graphicsContext3D()->extensions()->ensureEnabled("GL_EXT_draw_buffers");
}

WebGLDrawBuffers::~WebGLDrawBuffers()
{
}

WebGLExtension::ExtensionName WebGLDrawBuffers::name() const
{
    return WebGLExtension::WebGLDrawBuffersName;
}

PassRefPtr<WebGLDrawBuffers> WebGLDrawBuffers::create(WebGLRenderingContext* context)
{
    return adoptRef(new WebGLDrawBuffers(context));
}

// static
bool WebGLDrawBuffers::supported(WebGLRenderingContext* context)
{
    Extensions3D* extensions = context->graphicsContext3D()->extensions();
    return (extensions->supports("GL_EXT_draw_buffers")
        && satisfiesWebGLRequirements(context));
}

const char* WebGLDrawBuffers::extensionName()
{
    return "WEBGL_draw_buffers";
}

void WebGLDrawBuffers::drawBuffersWEBGL(const Vector<GC3Denum>& buffers)
{
    if (isLost())
        return;
    GC3Dsizei n = buffers.size();
    const GC3Denum* bufs = buffers.data();
    if (!m_context->m_framebufferBinding) {
        if (n != 1) {
            m_context->synthesizeGLError(GL_INVALID_VALUE, "drawBuffersWEBGL", "more than one buffer");
            return;
        }
        if (bufs[0] != GL_BACK && bufs[0] != GL_NONE) {
            m_context->synthesizeGLError(GL_INVALID_OPERATION, "drawBuffersWEBGL", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GC3Denum value = (bufs[0] == GL_BACK) ? GL_COLOR_ATTACHMENT0 : GL_NONE;
        m_context->graphicsContext3D()->extensions()->drawBuffersEXT(1, &value);
        m_context->setBackDrawBuffer(bufs[0]);
    } else {
        if (n > m_context->maxDrawBuffers()) {
            m_context->synthesizeGLError(GL_INVALID_VALUE, "drawBuffersWEBGL", "more than max draw buffers");
            return;
        }
        for (GC3Dsizei i = 0; i < n; ++i) {
            if (bufs[i] != GL_NONE && bufs[i] != static_cast<GC3Denum>(Extensions3D::COLOR_ATTACHMENT0_EXT + i)) {
                m_context->synthesizeGLError(GL_INVALID_OPERATION, "drawBuffersWEBGL", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        m_context->m_framebufferBinding->drawBuffers(buffers);
    }
}

// static
bool WebGLDrawBuffers::satisfiesWebGLRequirements(WebGLRenderingContext* webglContext)
{
    GraphicsContext3D* context = webglContext->graphicsContext3D();

    // This is called after we make sure GL_EXT_draw_buffers is supported.
    GC3Dint maxDrawBuffers = 0;
    GC3Dint maxColorAttachments = 0;
    context->getIntegerv(Extensions3D::MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
    context->getIntegerv(Extensions3D::MAX_COLOR_ATTACHMENTS_EXT, &maxColorAttachments);
    if (maxDrawBuffers < 4 || maxColorAttachments < 4)
        return false;

    Platform3DObject fbo = context->createFramebuffer();
    context->bindFramebuffer(GL_FRAMEBUFFER, fbo);

    const unsigned char* buffer = 0; // Chromium doesn't allow init data for depth/stencil tetxures.
    bool supportsDepth = (context->extensions()->supports("GL_CHROMIUM_depth_texture")
        || context->extensions()->supports("GL_OES_depth_texture")
        || context->extensions()->supports("GL_ARB_depth_texture"));
    bool supportsDepthStencil = (context->extensions()->supports("GL_EXT_packed_depth_stencil")
        || context->extensions()->supports("GL_OES_packed_depth_stencil"));
    Platform3DObject depthStencil = 0;
    if (supportsDepthStencil) {
        depthStencil = context->createTexture();
        context->bindTexture(GL_TEXTURE_2D, depthStencil);
        context->texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_OES, 1, 1, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, buffer);
    }
    Platform3DObject depth = 0;
    if (supportsDepth) {
        depth = context->createTexture();
        context->bindTexture(GL_TEXTURE_2D, depth);
        context->texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, buffer);
    }

    Vector<Platform3DObject> colors;
    bool ok = true;
    GC3Dint maxAllowedBuffers = std::min(maxDrawBuffers, maxColorAttachments);
    for (GC3Dint i = 0; i < maxAllowedBuffers; ++i) {
        Platform3DObject color = context->createTexture();
        colors.append(color);
        context->bindTexture(GL_TEXTURE_2D, color);
        context->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color, 0);
        if (context->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ok = false;
            break;
        }
        if (supportsDepth) {
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
            if (context->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
        if (supportsDepthStencil) {
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencil, 0);
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencil, 0);
            if (context->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            context->framebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
    }

    webglContext->restoreCurrentFramebuffer();
    context->deleteFramebuffer(fbo);
    webglContext->restoreCurrentTexture2D();
    if (supportsDepth)
        context->deleteTexture(depth);
    if (supportsDepthStencil)
        context->deleteTexture(depthStencil);
    for (size_t i = 0; i < colors.size(); ++i)
        context->deleteTexture(colors[i]);
    return ok;
}

} // namespace WebCore
