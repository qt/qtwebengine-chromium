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

#ifndef InputMethodContext_h
#define InputMethodContext_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/editing/CompositionUnderline.h"
#include "core/events/EventTarget.h"
#include "core/html/HTMLElement.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Composition;
class ExecutionContext;
class InputMethodController;
class Node;

class InputMethodContext : public ScriptWrappable, public EventTargetWithInlineData {
public:
    static PassOwnPtr<InputMethodContext> create(HTMLElement*);
    ~InputMethodContext();

    void ref() { m_element->ref(); }
    void deref() { m_element->deref(); }

    Composition* composition();
    String locale() const;
    HTMLElement* target() const;
    unsigned compositionStartOffset();
    unsigned compositionEndOffset();
    void confirmComposition();

    String compositionText() const;
    int selectionStart() const;
    int selectionEnd() const;
    const Vector<unsigned>& segments();

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(candidatewindowshow);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(candidatewindowupdate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(candidatewindowhide);

    void dispatchCandidateWindowShowEvent();
    void dispatchCandidateWindowUpdateEvent();
    void dispatchCandidateWindowHideEvent();

private:
    InputMethodContext(HTMLElement*);
    bool hasFocus() const;
    CompositionUnderline selectedSegment() const;
    InputMethodController& inputMethodController() const;

    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    HTMLElement* m_element;
    OwnPtr<Composition> m_composition;
    Vector<unsigned> m_segments;
};

} // namespace WebCore

#endif // InputMethodContext_h
