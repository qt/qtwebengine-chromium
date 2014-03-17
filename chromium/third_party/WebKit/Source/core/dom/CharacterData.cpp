/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/dom/CharacterData.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/events/MutationEvent.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/inspector/InspectorInstrumentation.h"

using namespace std;

namespace WebCore {

void CharacterData::atomize()
{
    m_data = AtomicString(m_data);
}

void CharacterData::setData(const String& data)
{
    const String& nonNullData = !data.isNull() ? data : emptyString();
    if (m_data == nonNullData)
        return;

    RefPtr<CharacterData> protect = this;

    unsigned oldLength = length();

    setDataAndUpdate(nonNullData, 0, oldLength, nonNullData.length());
    document().didRemoveText(this, 0, oldLength);
}

String CharacterData::substringData(unsigned offset, unsigned count, ExceptionState& exceptionState)
{
    if (offset > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The offset " + String::number(offset) + " is greater than the node's length (" + String::number(length()) + ").");
        return String();
    }

    return m_data.substring(offset, count);
}

void CharacterData::parserAppendData(const String& string)
{
    unsigned oldLength = m_data.length();
    m_data.append(string);

    ASSERT(!renderer() || isTextNode());
    if (isTextNode())
        toText(this)->updateTextRenderer(oldLength, 0);

    document().incDOMTreeVersion();

    if (parentNode())
        parentNode()->childrenChanged();
}

void CharacterData::appendData(const String& data)
{
    String newStr = m_data + data;

    setDataAndUpdate(newStr, m_data.length(), 0, data.length());

    // FIXME: Should we call textInserted here?
}

void CharacterData::insertData(unsigned offset, const String& data, ExceptionState& exceptionState, RecalcStyleBehavior recalcStyleBehavior)
{
    if (offset > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The offset " + String::number(offset) + " is greater than the node's length (" + String::number(length()) + ").");
        return;
    }

    String newStr = m_data;
    newStr.insert(data, offset);

    setDataAndUpdate(newStr, offset, 0, data.length(), recalcStyleBehavior);

    document().didInsertText(this, offset, data.length());
}

void CharacterData::deleteData(unsigned offset, unsigned count, ExceptionState& exceptionState, RecalcStyleBehavior recalcStyleBehavior)
{
    if (offset > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The offset " + String::number(offset) + " is greater than the node's length (" + String::number(length()) + ").");
        return;
    }

    unsigned realCount;
    if (offset + count > length())
        realCount = length() - offset;
    else
        realCount = count;

    String newStr = m_data;
    newStr.remove(offset, realCount);

    setDataAndUpdate(newStr, offset, count, 0, recalcStyleBehavior);

    document().didRemoveText(this, offset, realCount);
}

void CharacterData::replaceData(unsigned offset, unsigned count, const String& data, ExceptionState& exceptionState)
{
    if (offset > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The offset " + String::number(offset) + " is greater than the node's length (" + String::number(length()) + ").");
        return;
    }

    unsigned realCount;
    if (offset + count > length())
        realCount = length() - offset;
    else
        realCount = count;

    String newStr = m_data;
    newStr.remove(offset, realCount);
    newStr.insert(data, offset);

    setDataAndUpdate(newStr, offset, count, data.length());

    // update the markers for spell checking and grammar checking
    document().didRemoveText(this, offset, realCount);
    document().didInsertText(this, offset, data.length());
}

String CharacterData::nodeValue() const
{
    return m_data;
}

bool CharacterData::containsOnlyWhitespace() const
{
    return m_data.containsOnlyWhitespace();
}

void CharacterData::setNodeValue(const String& nodeValue)
{
    setData(nodeValue);
}

void CharacterData::setDataAndUpdate(const String& newData, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, RecalcStyleBehavior recalcStyleBehavior)
{
    String oldData = m_data;
    m_data = newData;

    ASSERT(!renderer() || isTextNode());
    if (isTextNode())
        toText(this)->updateTextRenderer(offsetOfReplacedData, oldLength, recalcStyleBehavior);

    if (nodeType() == PROCESSING_INSTRUCTION_NODE)
        toProcessingInstruction(this)->checkStyleSheet();

    if (document().frame())
        document().frame()->selection().didUpdateCharacterData(this, offsetOfReplacedData, oldLength, newLength);

    document().incDOMTreeVersion();
    didModifyData(oldData);
}

void CharacterData::didModifyData(const String& oldData)
{
    if (OwnPtr<MutationObserverInterestGroup> mutationRecipients = MutationObserverInterestGroup::createForCharacterDataMutation(*this))
        mutationRecipients->enqueueMutationRecord(MutationRecord::createCharacterData(this, oldData));

    if (parentNode())
        parentNode()->childrenChanged();

    if (!isInShadowTree()) {
        if (document().hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER))
            dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMCharacterDataModified, true, 0, oldData, m_data));
        dispatchSubtreeModifiedEvent();
    }
    InspectorInstrumentation::characterDataModified(this);
}

int CharacterData::maxCharacterOffset() const
{
    return static_cast<int>(length());
}

bool CharacterData::offsetInCharacters() const
{
    return true;
}

} // namespace WebCore
