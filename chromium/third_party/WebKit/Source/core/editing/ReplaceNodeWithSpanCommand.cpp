/*
 * Copyright (c) 2009 Google Inc. All rights reserved.
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
#include "core/editing/ReplaceNodeWithSpanCommand.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/editing/htmlediting.h"
#include "core/html/HTMLElement.h"
#include "wtf/Assertions.h"

namespace WebCore {

using namespace HTMLNames;

ReplaceNodeWithSpanCommand::ReplaceNodeWithSpanCommand(PassRefPtr<HTMLElement> element)
    : SimpleEditCommand(element->document())
    , m_elementToReplace(element)
{
    ASSERT(m_elementToReplace);
}

static void swapInNodePreservingAttributesAndChildren(HTMLElement* newNode, HTMLElement& nodeToReplace)
{
    ASSERT(nodeToReplace.inDocument());
    RefPtr<ContainerNode> parentNode = nodeToReplace.parentNode();
    parentNode->insertBefore(newNode, &nodeToReplace);

    NodeVector children;
    getChildNodes(nodeToReplace, children);
    for (size_t i = 0; i < children.size(); ++i)
        newNode->appendChild(children[i]);

    // FIXME: Fix this to send the proper MutationRecords when MutationObservers are present.
    newNode->cloneDataFromElement(nodeToReplace);

    parentNode->removeChild(&nodeToReplace, ASSERT_NO_EXCEPTION);
}

void ReplaceNodeWithSpanCommand::doApply()
{
    if (!m_elementToReplace->inDocument())
        return;
    if (!m_spanElement)
        m_spanElement = createHTMLElement(m_elementToReplace->document(), spanTag);
    swapInNodePreservingAttributesAndChildren(m_spanElement.get(), *m_elementToReplace);
}

void ReplaceNodeWithSpanCommand::doUnapply()
{
    if (!m_spanElement->inDocument())
        return;
    swapInNodePreservingAttributesAndChildren(m_elementToReplace.get(), *m_spanElement);
}

} // namespace WebCore
