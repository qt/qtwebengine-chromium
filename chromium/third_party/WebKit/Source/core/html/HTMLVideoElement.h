/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLVideoElement_h
#define HTMLVideoElement_h

#include "core/html/HTMLMediaElement.h"

namespace WebCore {

class ExceptionState;
class HTMLImageLoader;

class HTMLVideoElement FINAL : public HTMLMediaElement {
public:
    static PassRefPtr<HTMLVideoElement> create(Document&, bool createdByParser = false);

    unsigned videoWidth() const;
    unsigned videoHeight() const;

    // Fullscreen
    void webkitEnterFullscreen(ExceptionState&);
    void webkitExitFullscreen();
    bool webkitSupportsFullscreen();
    bool webkitDisplayingFullscreen();

    // Statistics
    unsigned webkitDecodedFrameCount() const;
    unsigned webkitDroppedFrameCount() const;

    // Used by canvas to gain raw pixel access
    void paintCurrentFrameInContext(GraphicsContext*, const IntRect&);

    // Used by WebGL to do GPU-GPU textures copy if possible.
    // See more details at MediaPlayer::copyVideoTextureToPlatformTexture() defined in Source/WebCore/platform/graphics/MediaPlayer.h.
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY);

    bool shouldDisplayPosterImage() const { return displayMode() == Poster || displayMode() == PosterWaitingForVideo; }

    KURL posterImageURL() const;

private:
    HTMLVideoElement(Document&, bool);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual bool isVideo() const OVERRIDE { return true; }
    virtual bool hasVideo() const OVERRIDE { return player() && player()->hasVideo(); }
    virtual bool supportsFullscreen() const OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual const AtomicString imageSourceURL() const OVERRIDE;

    bool hasAvailableVideoFrame() const;
    virtual void updateDisplayState() OVERRIDE;
    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;
    virtual void setDisplayMode(DisplayMode) OVERRIDE;

    OwnPtr<HTMLImageLoader> m_imageLoader;

    AtomicString m_defaultPosterURL;
};

inline bool isHTMLVideoElement(const Node* node)
{
    return node->hasTagName(HTMLNames::videoTag);
}

inline bool isHTMLVideoElement(const Element* element)
{
    return element->hasTagName(HTMLNames::videoTag);
}

DEFINE_NODE_TYPE_CASTS(HTMLVideoElement, hasTagName(HTMLNames::videoTag));

} //namespace

#endif
