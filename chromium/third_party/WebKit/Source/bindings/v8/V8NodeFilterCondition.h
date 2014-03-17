/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8NodeFilterCondition_h
#define V8NodeFilterCondition_h

#include "bindings/v8/ScopedPersistent.h"
#include "core/dom/NodeFilterCondition.h"
#include <v8.h>
#include "wtf/PassRefPtr.h"

namespace WebCore {

class Node;
class ScriptState;

// V8NodeFilterCondition maintains a Javascript implemented callback for
// filtering Node returned by NodeIterator/TreeWalker.
// A NodeFilterCondition is referenced by a NodeFilter, and A NodeFilter is
// referenced by a NodeIterator/TreeWalker. As V8NodeFilterCondition maintains
// a Javascript callback which may reference Document, we need to avoid circular
// reference spanning V8/Blink object space.
// To address this issue, V8NodeFilterCondition holds a weak reference to
// |m_filter|, the Javascript value, and the whole reference is exposed to V8 to
// let V8 GC handle collection of |m_filter|.
// (DOM)
// NodeIterator  ----RefPtr----> NodeFilter ----RefPtr----> NodeFilterCondition
//   |   ^                        |   ^                        |
//  weak |                       weak |                ScopedPersistent(weak)
//   | RefPtr                     | RefPtr                     |
//   v   |                        v   |                        v
// NodeIterator  --HiddenValue--> NodeFilter --HiddenValue--> JS Callback
// (V8)
class V8NodeFilterCondition : public NodeFilterCondition {
public:
    static PassRefPtr<V8NodeFilterCondition> create(v8::Handle<v8::Value> filter, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    {
        return adoptRef(new V8NodeFilterCondition(filter, owner, isolate));
    }

    virtual ~V8NodeFilterCondition();

    virtual short acceptNode(ScriptState*, Node*) const;

private:
    // As the value |filter| is maintained by V8GC, the |owner| which references
    // V8NodeFilterCondition, usually a wrapper of NodeFilter, is specified here
    // to hold a strong reference to |filter|.
    V8NodeFilterCondition(v8::Handle<v8::Value> filter, v8::Handle<v8::Object> owner, v8::Isolate*);

    static void setWeakCallback(const v8::WeakCallbackData<v8::Value, V8NodeFilterCondition>&);

    ScopedPersistent<v8::Value> m_filter;
};

} // namespace WebCore

#endif // V8NodeFilterCondition_h
