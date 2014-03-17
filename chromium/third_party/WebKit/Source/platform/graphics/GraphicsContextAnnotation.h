/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef GraphicsContextAnnotation_h
#define GraphicsContextAnnotation_h

#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

enum AnnotationMode {
    AnnotateRendererName    = 1 << 0,
    AnnotatePaintPhase      = 1 << 1,
    AnnotateElementId       = 1 << 2,
    AnnotateElementClass    = 1 << 3,
    AnnotateElementTag      = 1 << 4,

    AnnotateAll             = 0x1f
};

typedef unsigned AnnotationModeFlags;
typedef Vector<std::pair<const char*, String> > AnnotationList;

class GraphicsContextAnnotation {
public:
    GraphicsContextAnnotation(const char*, const char*, const String&, const String&, const String&);

    String rendererName() const { return m_rendererName; }
    String paintPhase() const { return m_paintPhase; }
    String elementId() const { return m_elementId; }
    String elementClass() const { return m_elementClass; }
    String elementTag() const { return m_elementTag; }

    void asAnnotationList(AnnotationList&) const;

private:
    const char* m_rendererName;
    const char* m_paintPhase;
    String m_elementId;
    String m_elementClass;
    String m_elementTag;
};

} // namespace WebCore

#endif // GraphicsContextAnnotation_h
