/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 *
 */

#ifndef Attr_h
#define Attr_h

#include "core/dom/ContainerNode.h"
#include "core/dom/QualifiedName.h"

namespace WebCore {

class CSSStyleDeclaration;
class ExceptionState;
class MutableStylePropertySet;

// Attr can have Text children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the attribute upon
// destruction. however, this is not yet implemented.

class Attr FINAL : public ContainerNode {
public:
    static PassRefPtr<Attr> create(Element&, const QualifiedName&);
    static PassRefPtr<Attr> create(Document&, const QualifiedName&, const AtomicString& value);
    virtual ~Attr();

    String name() const { return qualifiedName().toString(); }
    bool specified() const { return true; }
    Element* ownerElement() const { return m_element; }

    const AtomicString& value() const;
    void setValue(const AtomicString&, ExceptionState&);
    void setValue(const AtomicString&);

    const QualifiedName& qualifiedName() const { return m_name; }

    bool isId() const;

    void attachToElement(Element*);
    void detachFromElementWithValue(const AtomicString&);

    virtual const AtomicString& localName() const OVERRIDE { return m_name.localName(); }
    virtual const AtomicString& namespaceURI() const OVERRIDE { return m_name.namespaceURI(); }
    virtual const AtomicString& prefix() const OVERRIDE { return m_name.prefix(); }

    virtual void setPrefix(const AtomicString&, ExceptionState&) OVERRIDE;

private:
    Attr(Element&, const QualifiedName&);
    Attr(Document&, const QualifiedName&, const AtomicString& value);

    void createTextChild();

    virtual String nodeName() const OVERRIDE { return name(); }
    virtual NodeType nodeType() const OVERRIDE { return ATTRIBUTE_NODE; }

    virtual String nodeValue() const OVERRIDE { return value(); }
    virtual void setNodeValue(const String&);
    virtual PassRefPtr<Node> cloneNode(bool deep = true);

    virtual bool isAttributeNode() const { return true; }
    virtual bool childTypeAllowed(NodeType) const;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    Attribute& elementAttribute();

    // Attr wraps either an element/name, or a name/value pair (when it's a standalone Node.)
    // Note that m_name is always set, but m_element/m_standaloneValue may be null.
    Element* m_element;
    QualifiedName m_name;
    AtomicString m_standaloneValue;
    unsigned m_ignoreChildrenChanged;
};

DEFINE_NODE_TYPE_CASTS(Attr, isAttributeNode());

} // namespace WebCore

#endif // Attr_h
