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

#ifndef RenderMedia_h
#define RenderMedia_h

#include "core/rendering/RenderImage.h"

namespace WebCore {

class HTMLMediaElement;

class RenderMedia : public RenderImage {
public:
    explicit RenderMedia(HTMLMediaElement*);
    virtual ~RenderMedia();

    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    HTMLMediaElement* mediaElement() const;

protected:
    virtual void layout();

private:
    virtual RenderObjectChildList* virtualChildren() OVERRIDE FINAL { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const OVERRIDE FINAL { return children(); }

    // FIXME: RenderMedia::layout makes assumptions about what children are allowed
    // so we can't support generated content.
    virtual bool canHaveGeneratedChildren() const OVERRIDE FINAL { return false; }
    virtual bool canHaveChildren() const OVERRIDE FINAL { return true; }

    virtual const char* renderName() const { return "RenderMedia"; }
    virtual bool isMedia() const OVERRIDE FINAL { return true; }
    virtual bool isImage() const OVERRIDE FINAL { return false; }
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);

    RenderObjectChildList m_children;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderMedia, isMedia());

} // namespace WebCore

#endif // RenderMedia_h
