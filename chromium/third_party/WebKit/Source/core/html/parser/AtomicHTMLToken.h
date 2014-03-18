/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AtomicHTMLToken_h
#define AtomicHTMLToken_h

#include "HTMLElementLookupTrie.h"
#include "core/dom/Attribute.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class AtomicHTMLToken {
    WTF_MAKE_NONCOPYABLE(AtomicHTMLToken);
public:

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

    HTMLToken::Type type() const { return m_type; }

    const AtomicString& name() const
    {
        ASSERT(usesName());
        return m_name;
    }

    void setName(const AtomicString& name)
    {
        ASSERT(usesName());
        m_name = name;
    }

    bool selfClosing() const
    {
        ASSERT(m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag);
        return m_selfClosing;
    }

    Attribute* getAttributeItem(const QualifiedName& attributeName)
    {
        ASSERT(usesAttributes());
        return findAttributeInVector(m_attributes, attributeName);
    }

    Vector<Attribute>& attributes()
    {
        ASSERT(usesAttributes());
        return m_attributes;
    }

    const Vector<Attribute>& attributes() const
    {
        ASSERT(usesAttributes());
        return m_attributes;
    }

    const String& characters() const
    {
        ASSERT(m_type == HTMLToken::Character);
        return m_data;
    }

    const String& comment() const
    {
        ASSERT(m_type == HTMLToken::Comment);
        return m_data;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    explicit AtomicHTMLToken(HTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLToken::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLToken::DOCTYPE:
            m_name = AtomicString(token.name());
            m_doctypeData = token.releaseDoctypeData();
            break;
        case HTMLToken::EndOfFile:
            break;
        case HTMLToken::StartTag:
        case HTMLToken::EndTag: {
            m_selfClosing = token.selfClosing();
            if (StringImpl* tagName = lookupHTMLTag(token.name().data(), token.name().size()))
                m_name = AtomicString(tagName);
            else
                m_name = AtomicString(token.name());
            initializeAttributes(token.attributes());
            break;
        }
        case HTMLToken::Character:
        case HTMLToken::Comment:
            if (token.isAll8BitData())
                m_data = String::make8BitFrom16BitSource(token.data());
            else
                m_data = String(token.data());
            break;
        }
    }

    explicit AtomicHTMLToken(const CompactHTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLToken::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLToken::DOCTYPE:
            m_name = token.data();
            m_doctypeData = adoptPtr(new DoctypeData());
            m_doctypeData->m_hasPublicIdentifier = true;
            append(m_doctypeData->m_publicIdentifier, token.publicIdentifier());
            m_doctypeData->m_hasSystemIdentifier = true;
            append(m_doctypeData->m_systemIdentifier, token.systemIdentifier());
            m_doctypeData->m_forceQuirks = token.doctypeForcesQuirks();
            break;
        case HTMLToken::EndOfFile:
            break;
        case HTMLToken::StartTag:
            m_attributes.reserveInitialCapacity(token.attributes().size());
            for (Vector<CompactHTMLToken::Attribute>::const_iterator it = token.attributes().begin(); it != token.attributes().end(); ++it) {
                QualifiedName name(nullAtom, it->name, nullAtom);
                // FIXME: This is N^2 for the number of attributes.
                if (!findAttributeInVector(m_attributes, name))
                    m_attributes.append(Attribute(name, it->value));
            }
            // Fall through!
        case HTMLToken::EndTag:
            m_selfClosing = token.selfClosing();
            m_name = token.data();
            break;
        case HTMLToken::Character:
        case HTMLToken::Comment:
            m_data = token.data();
            break;
        }
    }

    explicit AtomicHTMLToken(HTMLToken::Type type)
        : m_type(type)
        , m_selfClosing(false)
    {
    }

    AtomicHTMLToken(HTMLToken::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
        : m_type(type)
        , m_name(name)
        , m_selfClosing(false)
        , m_attributes(attributes)
    {
        ASSERT(usesName());
    }

private:
    HTMLToken::Type m_type;

    void initializeAttributes(const HTMLToken::AttributeList& attributes);
    QualifiedName nameForAttribute(const HTMLToken::Attribute&) const;

    bool usesName() const;

    bool usesAttributes() const;

    // "name" for DOCTYPE, StartTag, and EndTag
    AtomicString m_name;

    // "data" for Comment, "characters" for Character
    String m_data;

    // For DOCTYPE
    OwnPtr<DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;

    Vector<Attribute> m_attributes;
};

inline void AtomicHTMLToken::initializeAttributes(const HTMLToken::AttributeList& attributes)
{
    size_t size = attributes.size();
    if (!size)
        return;

    m_attributes.clear();
    m_attributes.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i) {
        const HTMLToken::Attribute& attribute = attributes[i];
        if (attribute.name.isEmpty())
            continue;

        // FIXME: We should be able to add the following ASSERT once we fix
        // https://bugs.webkit.org/show_bug.cgi?id=62971
        //   ASSERT(attribute.nameRange.start);
        ASSERT(attribute.nameRange.end);
        ASSERT(attribute.valueRange.start);
        ASSERT(attribute.valueRange.end);

        AtomicString value(attribute.value);
        const QualifiedName& name = nameForAttribute(attribute);
        // FIXME: This is N^2 for the number of attributes.
        if (!findAttributeInVector(m_attributes, name))
            m_attributes.append(Attribute(name, value));
    }
}

}

#endif
