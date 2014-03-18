/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#ifndef SerializedScriptValue_h
#define SerializedScriptValue_h

#include "bindings/v8/ScriptValue.h"

#include "wtf/HashMap.h"
#include "wtf/ThreadSafeRefCounted.h"
#include <v8.h>

namespace WTF {

class ArrayBuffer;
class ArrayBufferContents;

}

namespace WebCore {

class BlobDataHandle;
class MessagePort;

typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;
typedef Vector<RefPtr<WTF::ArrayBuffer>, 1> ArrayBufferArray;
typedef HashMap<String, RefPtr<BlobDataHandle> > BlobDataHandleMap;

class SerializedScriptValue FINAL : public ThreadSafeRefCounted<SerializedScriptValue> {
public:
    // Increment this for each incompatible change to the wire format.
    // Version 2: Added StringUCharTag for UChar v8 strings.
    // Version 3: Switched to using uuids as blob data identifiers.
    // Version 4: Extended File serialization to be complete.
    static const uint32_t wireFormatVersion = 4;

    ~SerializedScriptValue();

    // If a serialization error occurs (e.g., cyclic input value) this
    // function returns an empty representation, schedules a V8 exception to
    // be thrown using v8::ThrowException(), and sets |didThrow|. In this case
    // the caller must not invoke any V8 operations until control returns to
    // V8. When serialization is successful, |didThrow| is false.
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value>, MessagePortArray*, ArrayBufferArray*, bool& didThrow, v8::Isolate*);
    static PassRefPtr<SerializedScriptValue> createFromWire(const String&);
    static PassRefPtr<SerializedScriptValue> createFromWireBytes(const Vector<uint8_t>&);
    static PassRefPtr<SerializedScriptValue> create(const String&);
    static PassRefPtr<SerializedScriptValue> create(const String&, v8::Isolate*);
    static PassRefPtr<SerializedScriptValue> create();

    static PassRefPtr<SerializedScriptValue> create(const ScriptValue&, bool& didThrow, ScriptState*);

    // Never throws exceptions.
    static PassRefPtr<SerializedScriptValue> createAndSwallowExceptions(v8::Handle<v8::Value>, v8::Isolate*);

    static PassRefPtr<SerializedScriptValue> nullValue();
    static PassRefPtr<SerializedScriptValue> nullValue(v8::Isolate*);

    String toWireString() const { return m_data; }
    void toWireBytes(Vector<char>&) const;

    // Deserializes the value (in the current context). Returns a null value in
    // case of failure.
    v8::Handle<v8::Value> deserialize(MessagePortArray* = 0);
    v8::Handle<v8::Value> deserialize(v8::Isolate*, MessagePortArray* = 0);

    // Only reflects the truth if the SSV was created by walking a v8 value, not reliable
    // if the SSV was created createdFromWire(data).
    bool containsBlobs() const { return !m_blobDataHandles.isEmpty(); }

    // Informs the V8 about external memory allocated and owned by this object. Large values should contribute
    // to GC counters to eventually trigger a GC, otherwise flood of postMessage() can cause OOM.
    // Ok to invoke multiple times (only adds memory once).
    // The memory registration is revoked automatically in destructor.
    void registerMemoryAllocatedWithCurrentScriptContext();

private:
    enum StringDataMode {
        StringValue,
        WireData
    };
    enum ExceptionPolicy {
        ThrowExceptions,
        DoNotThrowExceptions
    };
    typedef Vector<WTF::ArrayBufferContents, 1> ArrayBufferContentsArray;

    SerializedScriptValue();
    SerializedScriptValue(v8::Handle<v8::Value>, MessagePortArray*, ArrayBufferArray*, bool& didThrow, v8::Isolate*, ExceptionPolicy = ThrowExceptions);
    explicit SerializedScriptValue(const String& wireData);

    static PassOwnPtr<ArrayBufferContentsArray> transferArrayBuffers(ArrayBufferArray&, bool& didThrow, v8::Isolate*);

    String m_data;
    OwnPtr<ArrayBufferContentsArray> m_arrayBufferContentsArray;
    BlobDataHandleMap m_blobDataHandles;
    intptr_t m_externallyAllocatedMemory;
};

} // namespace WebCore

#endif // SerializedScriptValue_h
