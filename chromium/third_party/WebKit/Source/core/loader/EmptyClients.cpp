/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "core/loader/EmptyClients.h"

#include "core/html/HTMLFormElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/frame/Frame.h"
#include "platform/ColorChooser.h"
#include "platform/DateTimeChooser.h"
#include "platform/FileChooser.h"
#include "public/platform/WebServiceWorkerProvider.h"
#include "public/platform/WebServiceWorkerProviderClient.h"

namespace WebCore {

void fillWithEmptyClients(Page::PageClients& pageClients)
{
    static ChromeClient* dummyChromeClient = adoptPtr(new EmptyChromeClient).leakPtr();
    pageClients.chromeClient = dummyChromeClient;

    static ContextMenuClient* dummyContextMenuClient = adoptPtr(new EmptyContextMenuClient).leakPtr();
    pageClients.contextMenuClient = dummyContextMenuClient;

    static DragClient* dummyDragClient = adoptPtr(new EmptyDragClient).leakPtr();
    pageClients.dragClient = dummyDragClient;

    static EditorClient* dummyEditorClient = adoptPtr(new EmptyEditorClient).leakPtr();
    pageClients.editorClient = dummyEditorClient;

    static InspectorClient* dummyInspectorClient = adoptPtr(new EmptyInspectorClient).leakPtr();
    pageClients.inspectorClient = dummyInspectorClient;

    static BackForwardClient* dummyBackForwardClient = adoptPtr(new EmptyBackForwardClient).leakPtr();
    pageClients.backForwardClient = dummyBackForwardClient;

    static SpellCheckerClient* dummySpellCheckerClient = adoptPtr(new EmptySpellCheckerClient).leakPtr();
    pageClients.spellCheckerClient = dummySpellCheckerClient;
}

class EmptyPopupMenu : public PopupMenu {
public:
    virtual void show(const FloatQuad&, const IntSize&, int) OVERRIDE { }
    virtual void hide() OVERRIDE { }
    virtual void updateFromElement() OVERRIDE { }
    virtual void disconnectClient() OVERRIDE { }
};

PassRefPtr<PopupMenu> EmptyChromeClient::createPopupMenu(Frame&, PopupMenuClient*) const
{
    return adoptRef(new EmptyPopupMenu());
}

PassOwnPtr<ColorChooser> EmptyChromeClient::createColorChooser(ColorChooserClient*, const Color&)
{
    return nullptr;
}

PassRefPtr<DateTimeChooser> EmptyChromeClient::openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&)
{
    return PassRefPtr<DateTimeChooser>();
}

void EmptyChromeClient::openTextDataListChooser(HTMLInputElement&)
{
}

void EmptyChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser>)
{
}

String EmptyChromeClient::acceptLanguages()
{
    return String();
}

NavigationPolicy EmptyFrameLoaderClient::decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy)
{
    return NavigationPolicyIgnore;
}

void EmptyFrameLoaderClient::dispatchWillSendSubmitEvent(PassRefPtr<FormState>)
{
}

void EmptyFrameLoaderClient::dispatchWillSubmitForm(PassRefPtr<FormState>)
{
}

PassRefPtr<DocumentLoader> EmptyFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

PassRefPtr<Frame> EmptyFrameLoaderClient::createFrame(const KURL&, const String&, const String&, HTMLFrameOwnerElement*)
{
    return 0;
}

PassRefPtr<Widget> EmptyFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    return 0;
}

PassRefPtr<Widget> EmptyFrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&)
{
    return 0;
}

void EmptyTextCheckerClient::requestCheckingOfString(PassRefPtr<TextCheckingRequest>)
{
}

void EmptyFrameLoaderClient::didRequestAutocomplete(PassRefPtr<FormState>)
{
}

PassOwnPtr<blink::WebServiceWorkerProvider> EmptyFrameLoaderClient::createServiceWorkerProvider(PassOwnPtr<blink::WebServiceWorkerProviderClient>)
{
    return nullptr;
}

}
