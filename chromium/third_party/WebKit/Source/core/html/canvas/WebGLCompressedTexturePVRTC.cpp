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

#include "core/html/canvas/WebGLCompressedTexturePVRTC.h"

#include "core/html/canvas/WebGLRenderingContext.h"
#include "platform/graphics/Extensions3D.h"

namespace WebCore {

WebGLCompressedTexturePVRTC::WebGLCompressedTexturePVRTC(WebGLRenderingContext* context)
    : WebGLExtension(context)
{
    ScriptWrappable::init(this);
    context->addCompressedTextureFormat(Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
    context->addCompressedTextureFormat(Extensions3D::COMPRESSED_RGB_PVRTC_2BPPV1_IMG);
    context->addCompressedTextureFormat(Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
    context->addCompressedTextureFormat(Extensions3D::COMPRESSED_RGBA_PVRTC_2BPPV1_IMG);
}

WebGLCompressedTexturePVRTC::~WebGLCompressedTexturePVRTC()
{
}

WebGLExtension::ExtensionName WebGLCompressedTexturePVRTC::name() const
{
    return WebGLCompressedTexturePVRTCName;
}

PassRefPtr<WebGLCompressedTexturePVRTC> WebGLCompressedTexturePVRTC::create(WebGLRenderingContext* context)
{
    return adoptRef(new WebGLCompressedTexturePVRTC(context));
}

bool WebGLCompressedTexturePVRTC::supported(WebGLRenderingContext* context)
{
    Extensions3D* extensions = context->graphicsContext3D()->extensions();
    return extensions->supports("GL_IMG_texture_compression_pvrtc");
}

const char* WebGLCompressedTexturePVRTC::extensionName()
{
    return "WEBGL_compressed_texture_pvrtc";
}

} // namespace WebCore
