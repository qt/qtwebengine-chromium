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

#include "config.h"
#include "core/html/HTMLImportsController.h"

#include "core/dom/Document.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/HTMLImportChild.h"
#include "core/html/HTMLImportChildClient.h"

namespace WebCore {

void HTMLImportsController::provideTo(Document* master)
{
    DEFINE_STATIC_LOCAL(const char*, name, ("HTMLImportsController"));
    OwnPtr<HTMLImportsController> controller = adoptPtr(new HTMLImportsController(master));
    master->setImport(controller.get());
    DocumentSupplement::provideTo(master, name, controller.release());
}

HTMLImportsController::HTMLImportsController(Document* master)
    : m_master(master)
    , m_unblockTimer(this, &HTMLImportsController::unblockTimerFired)
{
}

HTMLImportsController::~HTMLImportsController()
{
    ASSERT(!m_master);
}

void HTMLImportsController::clear()
{
    for (size_t i = 0; i < m_imports.size(); ++i)
        m_imports[i]->importDestroyed();
    if (m_master)
        m_master->setImport(0);
    m_master = 0;
}

HTMLImportChild* HTMLImportsController::createChild(const KURL& url, HTMLImport* parent, HTMLImportChildClient* client)
{
    OwnPtr<HTMLImportChild> loader = adoptPtr(new HTMLImportChild(url, client));
    parent->appendChild(loader.get());
    m_imports.append(loader.release());
    return m_imports.last().get();
}

HTMLImportChild* HTMLImportsController::load(HTMLImport* parent, HTMLImportChildClient* client, FetchRequest request)
{
    ASSERT(!request.url().isEmpty() && request.url().isValid());

    if (HTMLImportChild* found = findLinkFor(request.url())) {
        HTMLImportChild* child = createChild(request.url(), parent, client);
        child->wasAlreadyLoadedAs(found);
        return child;
    }

    request.setCrossOriginAccessControl(securityOrigin(), DoNotAllowStoredCredentials);
    ResourcePtr<RawResource> resource = parent->document()->fetcher()->fetchImport(request);
    if (!resource)
        return 0;

    HTMLImportChild* child = createChild(request.url(), parent, client);
    // We set resource after the import tree is built since
    // Resource::addClient() immediately calls back to feed the bytes when the resource is cached.
    child->startLoading(resource);

    return child;
}

void HTMLImportsController::showSecurityErrorMessage(const String& message)
{
    m_master->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);
}

HTMLImportChild* HTMLImportsController::findLinkFor(const KURL& url, HTMLImport* excluding) const
{
    for (size_t i = 0; i < m_imports.size(); ++i) {
        HTMLImportChild* candidate = m_imports[i].get();
        if (candidate != excluding && equalIgnoringFragmentIdentifier(candidate->url(), url) && !candidate->isDocumentBlocked())
            return candidate;
    }

    return 0;
}

SecurityOrigin* HTMLImportsController::securityOrigin() const
{
    return m_master->securityOrigin();
}

ResourceFetcher* HTMLImportsController::fetcher() const
{
    return m_master->fetcher();
}

HTMLImportRoot* HTMLImportsController::root()
{
    return this;
}

Document* HTMLImportsController::document() const
{
    return m_master;
}

void HTMLImportsController::wasDetachedFromDocument()
{
    clear();
}

void HTMLImportsController::didFinishParsing()
{
}

bool HTMLImportsController::isProcessing() const
{
    return m_master->parsing();
}

bool HTMLImportsController::isDone() const
{
    return !m_master->parsing();
}

void HTMLImportsController::blockerGone()
{
    scheduleUnblock();
}

void HTMLImportsController::scheduleUnblock()
{
    if (m_unblockTimer.isActive())
        return;
    m_unblockTimer.startOneShot(0);
}

void HTMLImportsController::unblockTimerFired(Timer<HTMLImportsController>*)
{
    do {
        m_unblockTimer.stop();
        HTMLImport::unblock(this);
    } while (m_unblockTimer.isActive());
}

} // namespace WebCore
