/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef DocumentType_h
#define DocumentType_h

#include "core/dom/Node.h"

namespace WebCore {

class NamedNodeMap;

class DocumentType FINAL : public Node {
public:
    static PassRefPtr<DocumentType> create(Document* document, const String& name, const String& publicId, const String& systemId)
    {
        return adoptRef(new DocumentType(document, name, publicId, systemId));
    }

    // FIXME: These wre removed from DOM4, we should add a UseCounter and see if
    // we can remove them since they never worked anyway.
    NamedNodeMap* entities() const { return 0; }
    NamedNodeMap* notations() const { return 0; }

    const String& name() const { return m_name; }
    const String& publicId() const { return m_publicId; }
    const String& systemId() const { return m_systemId; }
    const String& internalSubset() const { return m_subset; }

private:
    DocumentType(Document*, const String& name, const String& publicId, const String& systemId);

    virtual KURL baseURI() const OVERRIDE;
    virtual String nodeName() const OVERRIDE;
    virtual NodeType nodeType() const OVERRIDE;
    virtual PassRefPtr<Node> cloneNode(bool deep) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    String m_name;
    String m_publicId;
    String m_systemId;
    String m_subset;
};

DEFINE_NODE_TYPE_CASTS(DocumentType, nodeType() == Node::DOCUMENT_TYPE_NODE);

} // namespace WebCore

#endif
