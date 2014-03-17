/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ClipPathOperation_h
#define ClipPathOperation_h

#include "core/rendering/style/BasicShapes.h"
#include "platform/graphics/Path.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ClipPathOperation : public RefCounted<ClipPathOperation> {
public:
    enum OperationType {
        REFERENCE,
        SHAPE
    };

    virtual ~ClipPathOperation() { }

    virtual bool operator==(const ClipPathOperation&) const = 0;
    bool operator!=(const ClipPathOperation& o) const { return !(*this == o); }

    OperationType type() const { return m_type; }
    bool isSameType(const ClipPathOperation& o) const { return o.type() == m_type; }

protected:
    ClipPathOperation(OperationType type)
        : m_type(type)
    {
    }

    OperationType m_type;
};

class ReferenceClipPathOperation : public ClipPathOperation {
public:
    static PassRefPtr<ReferenceClipPathOperation> create(const String& url, const String& fragment)
    {
        return adoptRef(new ReferenceClipPathOperation(url, fragment));
    }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }

private:
    virtual bool operator==(const ClipPathOperation& o) const OVERRIDE
    {
        return isSameType(o) && m_url == static_cast<const ReferenceClipPathOperation&>(o).m_url;
    }

    ReferenceClipPathOperation(const String& url, const String& fragment)
        : ClipPathOperation(REFERENCE)
        , m_url(url)
        , m_fragment(fragment)
    {
    }

    String m_url;
    String m_fragment;
};

DEFINE_TYPE_CASTS(ReferenceClipPathOperation, ClipPathOperation, op, op->type() == ClipPathOperation::REFERENCE, op.type() == ClipPathOperation::REFERENCE);

class ShapeClipPathOperation : public ClipPathOperation {
public:
    static PassRefPtr<ShapeClipPathOperation> create(PassRefPtr<BasicShape> shape)
    {
        return adoptRef(new ShapeClipPathOperation(shape));
    }

    const BasicShape* basicShape() const { return m_shape.get(); }
    WindRule windRule() const { return m_shape->windRule(); }
    const Path& path(const FloatRect& boundingRect)
    {
        ASSERT(m_shape);
        m_path.clear();
        m_path = adoptPtr(new Path);
        m_shape->path(*m_path, boundingRect);
        return *m_path;
    }

private:
    virtual bool operator==(const ClipPathOperation& o) const OVERRIDE
    {
        return isSameType(o) && m_shape == static_cast<const ShapeClipPathOperation&>(o).m_shape;
    }

    ShapeClipPathOperation(PassRefPtr<BasicShape> shape)
        : ClipPathOperation(SHAPE)
        , m_shape(shape)
    {
    }

    RefPtr<BasicShape> m_shape;
    OwnPtr<Path> m_path;
};

DEFINE_TYPE_CASTS(ShapeClipPathOperation, ClipPathOperation, op, op->type() == ClipPathOperation::SHAPE, op.type() == ClipPathOperation::SHAPE);

}

#endif // ClipPathOperation_h
