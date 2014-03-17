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
#include "core/html/canvas/CanvasRenderingContext.h"

#include "core/fetch/ImageResource.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/canvas/CanvasPattern.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

CanvasRenderingContext::CanvasRenderingContext(HTMLCanvasElement* canvas)
    : m_canvas(canvas)
{
    ScriptWrappable::init(this);
}

bool CanvasRenderingContext::wouldTaintOrigin(const CanvasPattern* pattern)
{
    if (canvas()->originClean() && pattern && !pattern->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLCanvasElement* sourceCanvas)
{
    if (canvas()->originClean() && sourceCanvas && !sourceCanvas->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLImageElement* image)
{
    if (!image || !canvas()->originClean())
        return false;

    ImageResource* cachedImage = image->cachedImage();
    if (!cachedImage->image()->currentFrameHasSingleSecurityOrigin())
        return true;

    return wouldTaintOrigin(cachedImage->response().url()) && !cachedImage->passesAccessControlCheck(canvas()->securityOrigin());
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLVideoElement* video)
{
    // FIXME: This check is likely wrong when a redirect is involved. We need
    // to test the finalURL. Please be careful when fixing this issue not to
    // make currentSrc be the final URL because then the
    // HTMLMediaElement.currentSrc DOM API would leak redirect destinations!
    if (!video || !canvas()->originClean())
        return false;

    if (!video->hasSingleSecurityOrigin())
        return true;

    if (!(video->player() && video->player()->didPassCORSAccessCheck()) && wouldTaintOrigin(video->currentSrc()))
        return true;

    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const KURL& url)
{
    if (!canvas()->originClean() || m_cleanURLs.contains(url.string()))
        return false;

    if (canvas()->securityOrigin()->taintsCanvas(url))
        return true;

    if (url.protocolIsData())
        return false;

    m_cleanURLs.add(url.string());
    return false;
}

void CanvasRenderingContext::checkOrigin(const KURL& url)
{
    if (wouldTaintOrigin(url))
        canvas()->setOriginTainted();
}

} // namespace WebCore
