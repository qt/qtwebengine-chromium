/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "core/page/PageGroup.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

PageGroup::PageGroup()
{
}

PageGroup::~PageGroup()
{
    removeInjectedStyleSheets();
}

PageGroup* PageGroup::sharedGroup()
{
    DEFINE_STATIC_REF(PageGroup, staticSharedGroup, (create()));
    return staticSharedGroup;
}

void PageGroup::addPage(Page* page)
{
    ASSERT(page);
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
}

void PageGroup::removePage(Page* page)
{
    ASSERT(page);
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);
}

void PageGroup::injectStyleSheet(const String& source, const Vector<String>& whitelist, StyleInjectionTarget injectIn)
{
    m_injectedStyleSheets.append(adoptPtr(new InjectedStyleSheet(source, whitelist, injectIn)));
    invalidatedInjectedStyleSheetCacheInAllFrames();
}

void PageGroup::removeInjectedStyleSheets()
{
    m_injectedStyleSheets.clear();
    invalidatedInjectedStyleSheetCacheInAllFrames();
}

void PageGroup::invalidatedInjectedStyleSheetCacheInAllFrames()
{
    // Clear our cached sheets and have them just reparse.
    HashSet<Page*>::const_iterator end = m_pages.end();
    for (HashSet<Page*>::const_iterator it = m_pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext())
            frame->document()->styleEngine()->invalidateInjectedStyleSheetCache();
    }
}

} // namespace WebCore
