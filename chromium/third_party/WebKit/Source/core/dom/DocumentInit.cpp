/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/dom/DocumentInit.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementRegistrationContext.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImportsController.h"
#include "core/frame/Frame.h"

namespace WebCore {

static Document* parentDocument(Frame* frame)
{
    if (!frame)
        return 0;
    Element* ownerElement = frame->ownerElement();
    if (!ownerElement)
        return 0;
    return &ownerElement->document();
}


static Document* ownerDocument(Frame* frame)
{
    if (!frame)
        return 0;

    Frame* ownerFrame = frame->tree().parent();
    if (!ownerFrame)
        ownerFrame = frame->loader().opener();
    if (!ownerFrame)
        return 0;
    return ownerFrame->document();
}

DocumentInit::DocumentInit(const KURL& url, Frame* frame, WeakPtr<Document> contextDocument, HTMLImport* import)
    : m_url(url)
    , m_frame(frame)
    , m_parent(parentDocument(frame))
    , m_owner(ownerDocument(frame))
    , m_contextDocument(contextDocument)
    , m_import(import)
{
}

DocumentInit::DocumentInit(const DocumentInit& other)
    : m_url(other.m_url)
    , m_frame(other.m_frame)
    , m_parent(other.m_parent)
    , m_owner(other.m_owner)
    , m_contextDocument(other.m_contextDocument)
    , m_import(other.m_import)
    , m_registrationContext(other.m_registrationContext)
{
}

DocumentInit::~DocumentInit()
{
}

bool DocumentInit::shouldSetURL() const
{
    Frame* frame = frameForSecurityContext();
    return (frame && frame->ownerElement()) || !m_url.isEmpty();
}

bool DocumentInit::shouldTreatURLAsSrcdocDocument() const
{
    return m_parent && m_frame->loader().shouldTreatURLAsSrcdocDocument(m_url);
}

bool DocumentInit::isSeamlessAllowedFor(Document* child) const
{
    if (!m_parent)
        return false;
    if (m_parent->isSandboxed(SandboxSeamlessIframes))
        return false;
    if (child->isSrcdocDocument())
        return true;
    if (m_parent->securityOrigin()->canAccess(child->securityOrigin()))
        return true;
    return m_parent->securityOrigin()->canRequest(child->url());
}

Frame* DocumentInit::frameForSecurityContext() const
{
    if (m_frame)
        return m_frame;
    if (m_import)
        return m_import->frame();
    return 0;
}

SandboxFlags DocumentInit::sandboxFlags() const
{
    ASSERT(frameForSecurityContext());
    return frameForSecurityContext()->loader().effectiveSandboxFlags();
}

Settings* DocumentInit::settings() const
{
    ASSERT(frameForSecurityContext());
    return frameForSecurityContext()->settings();
}

KURL DocumentInit::parentBaseURL() const
{
    return m_parent->baseURL();
}

DocumentInit& DocumentInit::withRegistrationContext(CustomElementRegistrationContext* registrationContext)
{
    ASSERT(!m_registrationContext);
    m_registrationContext = registrationContext;
    return *this;
}

PassRefPtr<CustomElementRegistrationContext> DocumentInit::registrationContext(Document* document) const
{
    if (!RuntimeEnabledFeatures::customElementsEnabled() && !RuntimeEnabledFeatures::embedderCustomElementsEnabled())
        return 0;

    if (!document->isHTMLDocument() && !document->isXHTMLDocument())
        return 0;

    if (m_registrationContext)
        return m_registrationContext.get();

    return CustomElementRegistrationContext::create();
}

WeakPtr<Document> DocumentInit::contextDocument() const
{
    return m_contextDocument;
}

DocumentInit DocumentInit::fromContext(WeakPtr<Document> contextDocument, const KURL& url)
{
    return DocumentInit(url, 0, contextDocument, 0);
}

} // namespace WebCore

