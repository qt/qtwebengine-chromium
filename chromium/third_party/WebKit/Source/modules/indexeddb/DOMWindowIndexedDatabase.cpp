/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "modules/indexeddb/DOMWindowIndexedDatabase.h"

#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/page/Page.h"
#include "modules/indexeddb/IDBFactory.h"
#include "modules/indexeddb/PageGroupIndexedDatabase.h"

namespace WebCore {

DOMWindowIndexedDatabase::DOMWindowIndexedDatabase(DOMWindow* window)
    : DOMWindowProperty(window->frame())
    , m_window(window)
{
}

DOMWindowIndexedDatabase::~DOMWindowIndexedDatabase()
{
}

const char* DOMWindowIndexedDatabase::supplementName()
{
    return "DOMWindowIndexedDatabase";
}

DOMWindowIndexedDatabase* DOMWindowIndexedDatabase::from(DOMWindow* window)
{
    DOMWindowIndexedDatabase* supplement = static_cast<DOMWindowIndexedDatabase*>(Supplement<DOMWindow>::from(window, supplementName()));
    if (!supplement) {
        supplement = new DOMWindowIndexedDatabase(window);
        provideTo(window, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

void DOMWindowIndexedDatabase::willDestroyGlobalObjectInFrame()
{
    m_idbFactory = nullptr;
    DOMWindowProperty::willDestroyGlobalObjectInFrame();
}

void DOMWindowIndexedDatabase::willDetachGlobalObjectFromFrame()
{
    m_idbFactory = nullptr;
    DOMWindowProperty::willDetachGlobalObjectFromFrame();
}

IDBFactory* DOMWindowIndexedDatabase::indexedDB(DOMWindow* window)
{
    return from(window)->indexedDB();
}

IDBFactory* DOMWindowIndexedDatabase::indexedDB()
{
    Document* document = m_window->document();
    if (!document)
        return 0;

    Page* page = document->page();
    if (!page)
        return 0;

    if (!m_window->isCurrentlyDisplayedInFrame())
        return 0;

    if (!m_idbFactory)
        m_idbFactory = IDBFactory::create(PageGroupIndexedDatabase::from(page->group())->factoryBackend());
    return m_idbFactory.get();
}

} // namespace WebCore
