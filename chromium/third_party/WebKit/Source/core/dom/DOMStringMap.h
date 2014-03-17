/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMStringMap_h
#define DOMStringMap_h

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Element;

class DOMStringMap : public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(DOMStringMap); WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DOMStringMap();

    virtual void ref() = 0;
    virtual void deref() = 0;

    virtual void getNames(Vector<String>&) = 0;
    virtual String item(const String& name) = 0;
    virtual bool contains(const String& name) = 0;
    virtual void setItem(const String& name, const String& value, ExceptionState&) = 0;
    virtual void deleteItem(const String& name, ExceptionState&) = 0;
    bool anonymousNamedSetter(const String& name, const String& value, ExceptionState& exceptionState)
    {
        setItem(name, value, exceptionState);
        return true;
    }
    bool anonymousNamedDeleter(const AtomicString& name, ExceptionState&)
    {
        // FIXME: Remove ExceptionState parameter.

        TrackExceptionState exceptionState;
        deleteItem(name, exceptionState);
        bool result = !exceptionState.hadException();
        // DOMStringMap deleter should ignore exception.
        // Behavior of Firefox and Opera are same.
        // delete document.body.dataset["-foo"] // false instead of DOM Exception 12
        // LayoutTests/fast/dom/HTMLSelectElement/select-selectedIndex-multiple.html
        return result;
    }
    void namedPropertyEnumerator(Vector<String>& names, ExceptionState&)
    {
        getNames(names);
    }
    bool namedPropertyQuery(const AtomicString&, ExceptionState&);

    String anonymousIndexedGetter(uint32_t index)
    {
        return item(String::number(index));
    }
    bool anonymousIndexedSetter(uint32_t index, const String& value, ExceptionState& exceptionState)
    {
        return anonymousNamedSetter(String::number(index), value, exceptionState);
    }
    bool anonymousIndexedDeleter(uint32_t index, ExceptionState& exceptionState)
    {
        return anonymousNamedDeleter(AtomicString::number(index), exceptionState);
    }

    virtual Element* element() = 0;

protected:
    DOMStringMap()
    {
        ScriptWrappable::init(this);
    }
};

} // namespace WebCore

#endif // DOMStringMap_h
