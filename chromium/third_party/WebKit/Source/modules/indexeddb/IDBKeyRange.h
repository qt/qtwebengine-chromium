/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef IDBKeyRange_h
#define IDBKeyRange_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBKey.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;

class IDBKeyRange : public ScriptWrappable, public RefCounted<IDBKeyRange> {
public:
    enum LowerBoundType {
        LowerBoundOpen,
        LowerBoundClosed
    };
    enum UpperBoundType {
        UpperBoundOpen,
        UpperBoundClosed
    };

    static PassRefPtr<IDBKeyRange> create(PassRefPtr<IDBKey> lower, PassRefPtr<IDBKey> upper, LowerBoundType lowerType, UpperBoundType upperType)
    {
        return adoptRef(new IDBKeyRange(lower, upper, lowerType, upperType));
    }
    // Null if the script value is null or undefined, the range if it is one, otherwise tries to convert to a key and throws if it fails.
    static PassRefPtr<IDBKeyRange> fromScriptValue(ExecutionContext*, const ScriptValue&, ExceptionState&);

    ~IDBKeyRange() { }

    // Implement the IDBKeyRange IDL
    PassRefPtr<IDBKey> lower() const { return m_lower; }
    PassRefPtr<IDBKey> upper() const { return m_upper; }

    ScriptValue lowerValue(ExecutionContext*) const;
    ScriptValue upperValue(ExecutionContext*) const;
    bool lowerOpen() const { return m_lowerType == LowerBoundOpen; }
    bool upperOpen() const { return m_upperType == UpperBoundOpen; }

    static PassRefPtr<IDBKeyRange> only(ExecutionContext*, const ScriptValue& key, ExceptionState&);
    static PassRefPtr<IDBKeyRange> lowerBound(ExecutionContext*, const ScriptValue& bound, bool open, ExceptionState&);
    static PassRefPtr<IDBKeyRange> upperBound(ExecutionContext*, const ScriptValue& bound, bool open, ExceptionState&);
    static PassRefPtr<IDBKeyRange> bound(ExecutionContext*, const ScriptValue& lower, const ScriptValue& upper, bool lowerOpen, bool upperOpen, ExceptionState&);

    static PassRefPtr<IDBKeyRange> only(PassRefPtr<IDBKey> value, ExceptionState&);

private:
    IDBKeyRange(PassRefPtr<IDBKey> lower, PassRefPtr<IDBKey> upper, LowerBoundType lowerType, UpperBoundType upperType);

    RefPtr<IDBKey> m_lower;
    RefPtr<IDBKey> m_upper;
    LowerBoundType m_lowerType;
    UpperBoundType m_upperType;
};

} // namespace WebCore

#endif // IDBKeyRange_h
