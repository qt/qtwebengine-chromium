/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8Node.h"

#include "V8Attr.h"
#include "V8CDATASection.h"
#include "V8Comment.h"
#include "V8Document.h"
#include "V8DocumentFragment.h"
#include "V8DocumentType.h"
#include "V8Element.h"
#include "V8Entity.h"
#include "V8HTMLElement.h"
#include "V8Notation.h"
#include "V8ProcessingInstruction.h"
#include "V8SVGElement.h"
#include "V8ShadowRoot.h"
#include "V8Text.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListener.h"
#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/events/EventListener.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "wtf/RefPtr.h"

namespace WebCore {

// These functions are custom to prevent a wrapper lookup of the return value which is always
// part of the arguments.
void V8Node::insertBeforeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    Node* imp = V8Node::toNative(holder);

    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "insertBefore", "Node", info.Holder(), info.GetIsolate());
    Node* newChild = V8Node::hasInstance(info[0], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[0])) : 0;
    Node* refChild = V8Node::hasInstance(info[1], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[1])) : 0;
    imp->insertBefore(newChild, refChild, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValue(info, info[0]);
}

void V8Node::replaceChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    Node* imp = V8Node::toNative(holder);

    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "replaceChild", "Node", info.Holder(), info.GetIsolate());
    Node* newChild = V8Node::hasInstance(info[0], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[0])) : 0;
    Node* oldChild = V8Node::hasInstance(info[1], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[1])) : 0;
    imp->replaceChild(newChild, oldChild, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValue(info, info[1]);
}

void V8Node::removeChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    Node* imp = V8Node::toNative(holder);

    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "removeChild", "Node", info.Holder(), info.GetIsolate());
    Node* oldChild = V8Node::hasInstance(info[0], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[0])) : 0;
    imp->removeChild(oldChild, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValue(info, info[0]);
}

void V8Node::appendChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    Node* imp = V8Node::toNative(holder);

    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "appendChild", "Node", info.Holder(), info.GetIsolate());
    Node* newChild = V8Node::hasInstance(info[0], info.GetIsolate(), worldType(info.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(info[0])) : 0;
    imp->appendChild(newChild, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValue(info, info[0]);
}

v8::Handle<v8::Object> wrap(Node* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->nodeType()) {
    case Node::ELEMENT_NODE:
        // For performance reasons, this is inlined from V8Element::wrap and must remain in sync.
        if (impl->isHTMLElement())
            return wrap(toHTMLElement(impl), creationContext, isolate);
        if (impl->isSVGElement())
            return wrap(toSVGElement(impl), creationContext, isolate);
        return V8Element::createWrapper(toElement(impl), creationContext, isolate);
    case Node::ATTRIBUTE_NODE:
        return wrap(toAttr(impl), creationContext, isolate);
    case Node::TEXT_NODE:
        return wrap(toText(impl), creationContext, isolate);
    case Node::CDATA_SECTION_NODE:
        return wrap(toCDATASection(impl), creationContext, isolate);
    case Node::PROCESSING_INSTRUCTION_NODE:
        return wrap(toProcessingInstruction(impl), creationContext, isolate);
    case Node::COMMENT_NODE:
        return wrap(toComment(impl), creationContext, isolate);
    case Node::DOCUMENT_NODE:
        return wrap(toDocument(impl), creationContext, isolate);
    case Node::DOCUMENT_TYPE_NODE:
        return wrap(toDocumentType(impl), creationContext, isolate);
    case Node::DOCUMENT_FRAGMENT_NODE:
        if (impl->isShadowRoot())
            return wrap(toShadowRoot(impl), creationContext, isolate);
        return wrap(toDocumentFragment(impl), creationContext, isolate);
    case Node::ENTITY_NODE:
    case Node::NOTATION_NODE:
        // We never create objects of Entity and Notation.
        ASSERT_NOT_REACHED();
        break;
    default:
        break; // ENTITY_REFERENCE_NODE or XPATH_NAMESPACE_NODE
    }
    return V8Node::createWrapper(impl, creationContext, isolate);
}
} // namespace WebCore
