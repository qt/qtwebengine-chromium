/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#ifndef NamedFlow_h
#define NamedFlow_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class Document;
class NamedFlowCollection;
class Node;
class NodeList;
class RenderNamedFlowThread;
class ExecutionContext;

class NamedFlow : public RefCounted<NamedFlow>, public ScriptWrappable, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(NamedFlow);
public:
    static PassRefPtr<NamedFlow> create(PassRefPtr<NamedFlowCollection> manager, const AtomicString& flowThreadName);

    ~NamedFlow();

    const AtomicString& name() const;
    bool overset() const;
    int firstEmptyRegionIndex() const;
    PassRefPtr<NodeList> getRegionsByContent(Node*);
    PassRefPtr<NodeList> getRegions();
    PassRefPtr<NodeList> getContent();

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // This function is called from the JS binding code to determine if the NamedFlow object is reachable or not.
    // If the object has listeners, the object should only be discarded if the parent Document is not reachable.
    Node* ownerNode() const;

    void setRenderer(RenderNamedFlowThread* parentFlowThread);

    enum FlowState {
        FlowStateCreated,
        FlowStateNull
    };

    FlowState flowState() const { return m_parentFlowThread ? FlowStateCreated : FlowStateNull; }

    void dispatchRegionLayoutUpdateEvent();
    void dispatchRegionOversetChangeEvent();

private:
    NamedFlow(PassRefPtr<NamedFlowCollection>, const AtomicString&);

    // The name of the flow thread as specified in CSS.
    AtomicString m_flowThreadName;

    RefPtr<NamedFlowCollection> m_flowManager;
    RenderNamedFlowThread* m_parentFlowThread;
};

}

#endif
