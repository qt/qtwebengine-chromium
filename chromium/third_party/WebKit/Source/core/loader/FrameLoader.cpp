/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/FrameLoader.h"

#include "HTMLNames.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/dom/BeforeUnloadEvent.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/PageTransitionEvent.h"
#include "core/editing/Editor.h"
#include "core/history/BackForwardController.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/IconController.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/ResourceLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/cache/ResourceFetcher.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/ContentSecurityPolicyResponseHeaders.h"
#include "core/page/DOMWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/Logging.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/HTTPParsers.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

static const char defaultAcceptHeader[] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8";

bool isBackForwardLoadType(FrameLoadType type)
{
    return type == FrameLoadTypeBackForward;
}

// This is not in the FrameLoader class to emphasize that it does not depend on
// private FrameLoader data, and to avoid increasing the number of public functions
// with access to private data.  Since only this .cpp file needs it, making it
// non-member lets us exclude it from the header file, thus keeping core/loader/FrameLoader.h's
// API simpler.
//
static bool isDocumentSandboxed(Frame* frame, SandboxFlags mask)
{
    return frame->document() && frame->document()->isSandboxed(mask);
}

class FrameLoader::FrameProgressTracker {
public:
    static PassOwnPtr<FrameProgressTracker> create(Frame* frame) { return adoptPtr(new FrameProgressTracker(frame)); }
    ~FrameProgressTracker()
    {
        ASSERT(!m_inProgress || m_frame->page());
        if (m_inProgress)
            m_frame->page()->progress()->progressCompleted(m_frame);
    }

    void progressStarted()
    {
        ASSERT(m_frame->page());
        if (!m_inProgress)
            m_frame->page()->progress()->progressStarted(m_frame);
        m_inProgress = true;
    }

    void progressCompleted()
    {
        ASSERT(m_inProgress);
        ASSERT(m_frame->page());
        m_inProgress = false;
        m_frame->page()->progress()->progressCompleted(m_frame);
    }

private:
    FrameProgressTracker(Frame* frame)
        : m_frame(frame)
        , m_inProgress(false)
    {
    }

    Frame* m_frame;
    bool m_inProgress;
};

FrameLoader::FrameLoader(Frame* frame, FrameLoaderClient* client)
    : m_frame(frame)
    , m_client(client)
    , m_history(frame)
    , m_notifer(frame)
    , m_icon(adoptPtr(new IconController(frame)))
    , m_mixedContentChecker(frame)
    , m_state(FrameStateProvisional)
    , m_loadType(FrameLoadTypeStandard)
    , m_inStopAllLoaders(false)
    , m_pageDismissalEventBeingDispatched(NoDismissal)
    , m_isComplete(false)
    , m_containsPlugins(false)
    , m_needsClear(false)
    , m_checkTimer(this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_shouldCallCheckLoadComplete(false)
    , m_opener(0)
    , m_didAccessInitialDocument(false)
    , m_didAccessInitialDocumentTimer(this, &FrameLoader::didAccessInitialDocumentTimerFired)
    , m_suppressOpenerInNewFrame(false)
    , m_startingClientRedirect(false)
    , m_forcedSandboxFlags(SandboxNone)
    , m_hasAllowedNavigationViaBeforeUnloadConfirmationPanel(false)
{
}

FrameLoader::~FrameLoader()
{
    setOpener(0);

    HashSet<Frame*>::iterator end = m_openedFrames.end();
    for (HashSet<Frame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->loader()->m_opener = 0;

    m_client->frameLoaderDestroyed();
}

void FrameLoader::init()
{
    // This somewhat odd set of steps gives the frame an initial empty document.
    m_provisionalDocumentLoader = m_client->createDocumentLoader(ResourceRequest(KURL(ParsedURLString, emptyString())), SubstituteData());
    m_provisionalDocumentLoader->setFrame(m_frame);
    m_provisionalDocumentLoader->startLoadingMainResource();
    m_frame->document()->cancelParsing();
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);
    m_progressTracker = FrameProgressTracker::create(m_frame);
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_documentLoader)
        m_documentLoader->setDefersLoading(defers);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->setDefersLoading(defers);
    if (m_policyDocumentLoader)
        m_policyDocumentLoader->setDefersLoading(defers);
    history()->setDefersLoading(defers);

    if (!defers) {
        m_frame->navigationScheduler()->startTimer();
        startCheckCompleteTimer();
    }
}

void FrameLoader::submitForm(PassRefPtr<FormSubmission> submission)
{
    ASSERT(submission->method() == FormSubmission::PostMethod || submission->method() == FormSubmission::GetMethod);

    // FIXME: Find a good spot for these.
    ASSERT(submission->data());
    ASSERT(submission->state());
    ASSERT(!submission->state()->sourceDocument()->frame() || submission->state()->sourceDocument()->frame() == m_frame);

    if (!m_frame->page())
        return;

    if (submission->action().isEmpty())
        return;

    if (isDocumentSandboxed(m_frame, SandboxForms)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        m_frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked form submission to '" + submission->action().elidedString() + "' because the form's frame is sandboxed and the 'allow-forms' permission is not set.");
        return;
    }

    if (protocolIsJavaScript(submission->action())) {
        if (!m_frame->document()->contentSecurityPolicy()->allowFormAction(KURL(submission->action())))
            return;
        m_frame->script()->executeScriptIfJavaScriptURL(submission->action());
        return;
    }

    Frame* targetFrame = findFrameForNavigation(submission->target(), submission->state()->sourceDocument());
    if (!targetFrame) {
        if (!DOMWindow::allowPopUp(m_frame) && !ScriptController::processingUserGesture())
            return;

        targetFrame = m_frame;
    } else
        submission->clearTarget();

    if (!targetFrame->page())
        return;

    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.

    // We do not want to submit more than one form from the same page, nor do we want to submit a single
    // form more than once. This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.

    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.

    if (m_frame->tree()->isDescendantOf(targetFrame)) {
        if (m_submittedFormURL == submission->requestURL())
            return;
        m_submittedFormURL = submission->requestURL();
    }

    submission->setReferrer(outgoingReferrer());
    submission->setOrigin(outgoingOrigin());

    targetFrame->navigationScheduler()->scheduleFormSubmission(submission);
}

void FrameLoader::stopLoading(UnloadEventPolicy unloadEventPolicy)
{
    if (m_frame->document() && m_frame->document()->parser())
        m_frame->document()->parser()->stopParsing();

    if (unloadEventPolicy != UnloadEventPolicyNone) {
        if (m_frame->document()) {
            if (m_frame->document()->unloadEventStillNeeded()) {
                m_frame->document()->unloadEventStarted();
                Element* currentFocusedElement = m_frame->document()->focusedElement();
                if (currentFocusedElement && currentFocusedElement->hasTagName(inputTag))
                    toHTMLInputElement(currentFocusedElement)->endEditing();
                if (m_pageDismissalEventBeingDispatched == NoDismissal) {
                    if (unloadEventPolicy == UnloadEventPolicyUnloadAndPageHide) {
                        m_pageDismissalEventBeingDispatched = PageHideDismissal;
                        m_frame->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), m_frame->document());
                    }
                    RefPtr<Event> unloadEvent(Event::create(eventNames().unloadEvent, false, false));
                    // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed
                    // while dispatching the event, so protect it to prevent writing the end
                    // time into freed memory.
                    RefPtr<DocumentLoader> documentLoader = m_provisionalDocumentLoader;
                    m_pageDismissalEventBeingDispatched = UnloadDismissal;
                    if (documentLoader && !documentLoader->timing()->unloadEventStart() && !documentLoader->timing()->unloadEventEnd()) {
                        DocumentLoadTiming* timing = documentLoader->timing();
                        ASSERT(timing->navigationStart());
                        timing->markUnloadEventStart();
                        m_frame->domWindow()->dispatchEvent(unloadEvent, m_frame->document());
                        timing->markUnloadEventEnd();
                    } else {
                        m_frame->domWindow()->dispatchEvent(unloadEvent, m_frame->document());
                    }
                }
                m_pageDismissalEventBeingDispatched = NoDismissal;
                if (m_frame->document()) {
                    m_frame->document()->updateStyleIfNeeded();
                    m_frame->document()->unloadEventWasHandled();
                }
            }
        }

        // Dispatching the unload event could have made m_frame->document() null.
        if (m_frame->document()) {
            // Don't remove event listeners from a transitional empty document (see bug 28716 for more information).
            bool keepEventListeners = m_stateMachine.isDisplayingInitialEmptyDocument() && m_provisionalDocumentLoader
                && m_frame->document()->isSecureTransitionTo(m_provisionalDocumentLoader->url());

            if (!keepEventListeners)
                m_frame->document()->removeAllEventListeners();
        }
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing()

    if (m_frame->document() && m_frame->document()->parsing()) {
        finishedParsing();
        m_frame->document()->setParsing(false);
    }

    if (Document* doc = m_frame->document()) {
        // FIXME: HTML5 doesn't tell us to set the state to complete when aborting, but we do anyway to match legacy behavior.
        // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10537
        doc->setReadyState(Document::Complete);

        // FIXME: Should the DatabaseManager watch for something like ActiveDOMObject::stop() rather than being special-cased here?
        DatabaseManager::manager().stopDatabases(doc, 0);
    }

    // FIXME: This will cancel redirection timer, which really needs to be restarted when restoring the frame from b/f cache.
    m_frame->navigationScheduler()->cancel();
}

void FrameLoader::stop()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it will be deleted by checkCompleted().
    RefPtr<Frame> protector(m_frame);

    if (DocumentParser* parser = m_frame->document()->parser()) {
        parser->stopParsing();
        parser->finish();
    }
}

bool FrameLoader::closeURL()
{
    history()->saveDocumentState();

    // Should only send the pagehide event here if the current document exists.
    Document* currentDocument = m_frame->document();
    stopLoading(currentDocument ? UnloadEventPolicyUnloadAndPageHide : UnloadEventPolicyUnloadOnly);

    m_frame->editor()->clearUndoRedoOperations();
    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;

    // Calling document.open counts as committing the first real document load.
    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);

    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call.
    // Canceling redirection here works for all cases because document.open
    // implicitly precedes document.write.
    m_frame->navigationScheduler()->cancel();
}


void FrameLoader::cancelAndClear()
{
    m_frame->navigationScheduler()->cancel();

    if (!m_isComplete)
        closeURL();

    clear(false);
}

void FrameLoader::clear(bool clearWindowProperties, bool clearScriptObjects, bool clearFrameView)
{
    m_frame->editor()->clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;

    m_frame->document()->cancelParsing();
    m_frame->document()->stopActiveDOMObjects();
    if (m_frame->document()->attached()) {
        m_frame->document()->prepareForDestruction();
        m_frame->document()->removeFocusedElementOfSubtree(m_frame->document());
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        InspectorInstrumentation::frameWindowDiscarded(m_frame, m_frame->domWindow());
        m_frame->domWindow()->reset();
        m_frame->script()->clearWindowShell();
    }

    m_frame->selection()->prepareForDestruction();
    m_frame->eventHandler()->clear();
    if (clearFrameView && m_frame->view())
        m_frame->view()->clear();

    // Do not drop the DOMWindow (and Document) before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame->setDOMWindow(0);

    m_containsPlugins = false;

    if (clearScriptObjects)
        m_frame->script()->clearScriptObjects();

    m_frame->script()->enableEval();

    m_frame->navigationScheduler()->clear();

    m_checkTimer.stop();
    m_shouldCallCheckCompleted = false;
    m_shouldCallCheckLoadComplete = false;

    if (m_stateMachine.isDisplayingInitialEmptyDocument() && m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
}

void FrameLoader::receivedFirstData()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    dispatchDidCommitLoad();
    dispatchDidClearWindowObjectsInAllWorlds();

    if (m_documentLoader) {
        StringWithDirection ptitle = m_documentLoader->title();
        // If we have a title let the WebView know about it.
        if (!ptitle.isNull())
            m_client->dispatchDidReceiveTitle(ptitle);
    }
}

void FrameLoader::setOutgoingReferrer(const KURL& url)
{
    m_outgoingReferrer = url.strippedForUseAsReferrer();
}

void FrameLoader::didBeginDocument(bool dispatch)
{
    m_needsClear = true;
    m_isComplete = false;
    m_frame->document()->setReadyState(Document::Loading);

    if (history()->currentItem() && m_loadType == FrameLoadTypeBackForward)
        m_frame->document()->statePopped(history()->currentItem()->stateObject());

    if (dispatch)
        dispatchDidClearWindowObjectsInAllWorlds();

    m_frame->document()->initContentSecurityPolicy(m_documentLoader ? ContentSecurityPolicyResponseHeaders(m_documentLoader->response()) : ContentSecurityPolicyResponseHeaders());

    Settings* settings = m_frame->document()->settings();
    if (settings) {
        m_frame->document()->fetcher()->setImagesEnabled(settings->areImagesEnabled());
        m_frame->document()->fetcher()->setAutoLoadImages(settings->loadsImagesAutomatically());
    }

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            m_frame->document()->parseDNSPrefetchControlHeader(dnsPrefetchControl);

        String headerContentLanguage = m_documentLoader->response().httpHeaderField("Content-Language");
        if (!headerContentLanguage.isEmpty()) {
            size_t commaIndex = headerContentLanguage.find(',');
            headerContentLanguage.truncate(commaIndex); // notFound == -1 == don't truncate
            headerContentLanguage = headerContentLanguage.stripWhiteSpace(isHTMLSpace);
            if (!headerContentLanguage.isEmpty())
                m_frame->document()->setContentLanguage(headerContentLanguage);
        }
    }

    history()->restoreDocumentState();
}

void FrameLoader::finishedParsing()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<Frame> protector = m_frame->view() ? m_frame : 0;

    m_client->dispatchDidFinishDocumentLoad();

    checkCompleted();

    if (!m_frame->view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame->view()->restoreScrollbar();
    scrollToFragmentWithParentBoundary(m_frame->document()->url());
}

void FrameLoader::loadDone()
{
    checkCompleted();
}

bool FrameLoader::allChildrenAreComplete() const
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!child->loader()->m_isComplete)
            return false;
    }
    return true;
}

bool FrameLoader::allAncestorsAreComplete() const
{
    for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree()->parent()) {
        if (!ancestor->loader()->m_isComplete)
            return false;
    }
    return true;
}

void FrameLoader::checkCompleted()
{
    RefPtr<Frame> protect(m_frame);
    m_shouldCallCheckCompleted = false;

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame->document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (m_frame->document()->fetcher()->requestCount())
        return;

    // Still waiting for elements that don't go through a FrameLoader?
    if (m_frame->document()->isDelayingLoadEvent())
        return;

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // OK, completed.
    m_isComplete = true;
    m_requestedHistoryItem = 0;
    m_frame->document()->setReadyState(Document::Complete);
    if (m_frame->document()->loadEventStillNeeded())
        m_frame->document()->implicitClose();

    m_frame->navigationScheduler()->startTimer();

    completed();
    if (m_frame->page())
        checkLoadComplete();

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();
}

void FrameLoader::checkTimerFired(Timer<FrameLoader>*)
{
    RefPtr<Frame> protect(m_frame);

    if (Page* page = m_frame->page()) {
        if (page->defersLoading())
            return;
    }
    if (m_shouldCallCheckCompleted)
        checkCompleted();
    if (m_shouldCallCheckLoadComplete)
        checkLoadComplete();
}

void FrameLoader::startCheckCompleteTimer()
{
    if (!(m_shouldCallCheckCompleted || m_shouldCallCheckLoadComplete))
        return;
    if (m_checkTimer.isActive())
        return;
    m_checkTimer.startOneShot(0);
}

void FrameLoader::scheduleCheckCompleted()
{
    m_shouldCallCheckCompleted = true;
    startCheckCompleteTimer();
}

void FrameLoader::scheduleCheckLoadComplete()
{
    m_shouldCallCheckLoadComplete = true;
    startCheckCompleteTimer();
}

String FrameLoader::outgoingReferrer() const
{
    // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
    // for why we walk the parent chain for srcdoc documents.
    Frame* frame = m_frame;
    while (frame->document()->isSrcdocDocument()) {
        frame = frame->tree()->parent();
        // Srcdoc documents cannot be top-level documents, by definition,
        // because they need to be contained in iframes with the srcdoc.
        ASSERT(frame);
    }
    return frame->loader()->m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    return m_frame->document()->securityOrigin()->toString();
}

bool FrameLoader::checkIfFormActionAllowedByCSP(const KURL& url) const
{
    if (m_submittedFormURL.isEmpty())
        return true;

    return m_frame->document()->contentSecurityPolicy()->allowFormAction(url);
}

Frame* FrameLoader::opener()
{
    return m_opener;
}

void FrameLoader::setOpener(Frame* opener)
{
    if (m_opener && !opener)
        m_client->didDisownOpener();

    if (m_opener)
        m_opener->loader()->m_openedFrames.remove(m_frame);
    if (opener)
        opener->loader()->m_openedFrames.add(m_frame);
    m_opener = opener;

    if (m_frame->document())
        m_frame->document()->initSecurityContext();
}

// FIXME: This does not belong in FrameLoader!
void FrameLoader::handleFallbackContent()
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElement*>(owner)->renderFallbackContent();
}

bool FrameLoader::allowPlugins(ReasonForCallingAllowPlugins reason)
{
    Settings* settings = m_frame->settings();
    bool allowed = m_client->allowPlugins(settings && settings->arePluginsEnabled());
    if (!allowed && reason == AboutToInstantiatePlugin)
        m_client->didNotAllowPlugins();
    return allowed;
}

void FrameLoader::resetMultipleFormSubmissionProtection()
{
    m_submittedFormURL = KURL();
}

void FrameLoader::updateForSameDocumentNavigation(const KURL& newURL, SameDocumentNavigationSource sameDocumentNavigationSource, PassRefPtr<SerializedScriptValue> data, const String& title)
{
    // Update the data source's request with the new URL to fake the URL change
    KURL oldURL = m_frame->document()->url();
    m_frame->document()->setURL(newURL);
    setOutgoingReferrer(newURL);
    documentLoader()->replaceRequestURLForSameDocumentNavigation(newURL);

    if (sameDocumentNavigationSource == SameDocumentNavigationDefault)
        history()->updateForSameDocumentNavigation();
    else if (sameDocumentNavigationSource == SameDocumentNavigationPushState)
        history()->pushState(data, title, newURL.string());
    else if (sameDocumentNavigationSource == SameDocumentNavigationReplaceState)
        history()->replaceState(data, title, newURL.string());
    else
        ASSERT_NOT_REACHED();

    // Generate start and stop notifications only when loader is completed so that we
    // don't fire them for fragment redirection that happens in window.onload handler.
    // See https://bugs.webkit.org/show_bug.cgi?id=31838
    if (m_frame->document()->loadEventFinished())
        m_client->postProgressStartedNotification();

    m_documentLoader->clearRedirectChain();
    if (m_documentLoader->isClientRedirect())
        m_documentLoader->appendRedirect(oldURL);
    m_documentLoader->appendRedirect(newURL);

    m_client->dispatchDidNavigateWithinPage();

    if (m_frame->document()->loadEventFinished())
        m_client->postProgressFinishedNotification();
}

void FrameLoader::loadInSameDocument(const KURL& url, PassRefPtr<SerializedScriptValue> stateObject, bool isNewNavigation)
{
    // If we have a state object, we cannot also be a new navigation.
    ASSERT(!stateObject || (stateObject && !isNewNavigation));

    KURL oldURL = m_frame->document()->url();
    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    bool hashChange = equalIgnoringFragmentIdentifier(url, oldURL) && url.fragmentIdentifier() != oldURL.fragmentIdentifier();
    if (hashChange) {
        m_frame->eventHandler()->stopAutoscrollTimer();
        m_frame->document()->enqueueHashchangeEvent(oldURL, url);
    }

    m_documentLoader->setIsClientRedirect((m_startingClientRedirect && !isNewNavigation) || !UserGestureIndicator::processingUserGesture());
    m_documentLoader->setReplacesCurrentHistoryItem(!isNewNavigation);
    updateForSameDocumentNavigation(url, SameDocumentNavigationDefault, 0, String());

    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    started();

    // We need to scroll to the fragment whether or not a hash change occurred, since
    // the user might have scrolled since the previous navigation.
    scrollToFragmentWithParentBoundary(url);

    m_isComplete = false;
    checkCompleted();

    m_frame->document()->statePopped(stateObject ? stateObject : SerializedScriptValue::nullValue());
}

bool FrameLoader::isComplete() const
{
    return m_isComplete;
}

void FrameLoader::completed()
{
    RefPtr<Frame> protect(m_frame);

    for (Frame* descendant = m_frame->tree()->traverseNext(m_frame); descendant; descendant = descendant->tree()->traverseNext(m_frame))
        descendant->navigationScheduler()->startTimer();

    if (Frame* parent = m_frame->tree()->parent())
        parent->loader()->checkCompleted();

    if (m_frame->view())
        m_frame->view()->maintainScrollPositionAtAnchor(0);
}

void FrameLoader::started()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->parent())
        frame->loader()->m_isComplete = false;
}

void FrameLoader::prepareForHistoryNavigation()
{
    // If there is no currentItem, but we still want to engage in
    // history navigation we need to manufacture one, and update
    // the state machine of this frame to impersonate having
    // loaded it.
    RefPtr<HistoryItem> currentItem = history()->currentItem();
    if (!currentItem) {
        insertDummyHistoryItem();

        ASSERT(stateMachine()->isDisplayingInitialEmptyDocument());
        stateMachine()->advanceTo(FrameLoaderStateMachine::StartedFirstRealLoad);
        stateMachine()->advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
        stateMachine()->advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    }
}

void FrameLoader::setReferrerForFrameRequest(ResourceRequest& request, ShouldSendReferrer shouldSendReferrer)
{
    if (shouldSendReferrer == NeverSendReferrer) {
        request.clearHTTPReferrer();
        return;
    }

    String argsReferrer(request.httpReferrer());
    if (argsReferrer.isEmpty())
        argsReferrer = outgoingReferrer();
    String referrer = SecurityPolicy::generateReferrerHeader(m_frame->document()->referrerPolicy(), request.url(), argsReferrer);

    request.setHTTPReferrer(referrer);
    RefPtr<SecurityOrigin> referrerOrigin = SecurityOrigin::createFromString(referrer);
    addHTTPOriginIfNeeded(request, referrerOrigin->toString());
}

FrameLoadType FrameLoader::determineFrameLoadType(const FrameLoadRequest& request)
{
    if (m_frame->tree()->parent() && !m_stateMachine.startedFirstRealLoad())
        return FrameLoadTypeInitialInChildFrame;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        return FrameLoadTypeReload;
    if (request.lockBackForwardList())
        return FrameLoadTypeRedirectWithLockedBackForwardList;
    if (!request.requester() && shouldTreatURLAsSameAsCurrent(request.resourceRequest().url()))
        return FrameLoadTypeSame;
    if (shouldTreatURLAsSameAsCurrent(request.substituteData().failingURL()) && m_loadType == FrameLoadTypeReload)
        return FrameLoadTypeReload;
    return FrameLoadTypeStandard;
}

bool FrameLoader::prepareRequestForThisFrame(FrameLoadRequest& request)
{
    // If no SecurityOrigin was specified, skip security checks and assume the caller has fully initialized the FrameLoadRequest.
    if (!request.requester())
        return true;

    KURL url = request.resourceRequest().url();
    if (m_frame->script()->executeScriptIfJavaScriptURL(url))
        return false;

    if (!request.requester()->canDisplay(url)) {
        reportLocalLoadFailed(m_frame, url.elidedString());
        return false;
    }

    if (request.requester() && !request.formState() && request.frameName().isEmpty())
        request.setFrameName(m_frame->document()->baseTarget());

    // If the requesting SecurityOrigin is not this Frame's SecurityOrigin, the request was initiated by a different frame that should
    // have already set the referrer.
    if (request.requester() == m_frame->document()->securityOrigin())
        setReferrerForFrameRequest(request.resourceRequest(), request.shouldSendReferrer());
    return true;
}

static bool shouldOpenInNewWindow(Frame* targetFrame, const FrameLoadRequest& request, const NavigationAction& action)
{
    if (!targetFrame && !request.frameName().isEmpty())
        return true;
    if (!request.formState())
        return false;
    NavigationPolicy navigationPolicy = NavigationPolicyCurrentTab;
    if (!action.specifiesNavigationPolicy(&navigationPolicy))
        return false;
    return navigationPolicy != NavigationPolicyCurrentTab;
}

void FrameLoader::load(const FrameLoadRequest& passedRequest)
{
    ASSERT(!m_suppressOpenerInNewFrame);
    ASSERT(m_frame->document());

    // Protect frame from getting blown away inside dispatchBeforeLoadEvent in loadWithDocumentLoader.
    RefPtr<Frame> protect(m_frame);

    if (m_inStopAllLoaders)
        return;

    FrameLoadRequest request(passedRequest);
    if (!prepareRequestForThisFrame(request))
        return;

    // The search for a target frame is done earlier in the case of form submission.
    Frame* targetFrame = request.formState() ? 0 : findFrameForNavigation(request.frameName());
    if (targetFrame && targetFrame != m_frame) {
        request.setFrameName("_self");
        targetFrame->loader()->load(request);
        return;
    }

    FrameLoadType newLoadType = determineFrameLoadType(request);
    NavigationAction action(request.resourceRequest(), newLoadType, request.formState(), request.triggeringEvent());
    if (shouldOpenInNewWindow(targetFrame, request, action)) {
        TemporaryChange<bool> changeOpener(m_suppressOpenerInNewFrame, request.shouldSendReferrer() == NeverSendReferrer);
        checkNewWindowPolicyAndContinue(request.formState(), request.frameName(), action);
        return;
    }

    TemporaryChange<bool> changeClientRedirect(m_startingClientRedirect, request.clientRedirect());
    if (shouldPerformFragmentNavigation(request.formState(), request.resourceRequest().httpMethod(), newLoadType, request.resourceRequest().url())) {
        checkNavigationPolicyAndContinueFragmentScroll(action, newLoadType != FrameLoadTypeRedirectWithLockedBackForwardList);
        return;
    }
    bool sameURL = shouldTreatURLAsSameAsCurrent(request.resourceRequest().url());
    loadWithNavigationAction(request.resourceRequest(), action, newLoadType, request.formState(), request.substituteData());
    // Example of this case are sites that reload the same URL with a different cookie
    // driving the generated content, or a master frame with links that drive a target
    // frame, where the user has clicked on the same link repeatedly.
    if (sameURL && newLoadType != FrameLoadTypeReload && newLoadType != FrameLoadTypeReloadFromOrigin && request.resourceRequest().httpMethod() != "POST")
        m_loadType = FrameLoadTypeSame;
}

SubstituteData FrameLoader::defaultSubstituteDataForURL(const KURL& url)
{
    if (!shouldTreatURLAsSrcdocDocument(url))
        return SubstituteData();
    String srcdoc = m_frame->ownerElement()->fastGetAttribute(srcdocAttr);
    ASSERT(!srcdoc.isNull());
    CString encodedSrcdoc = srcdoc.utf8();
    return SubstituteData(SharedBuffer::create(encodedSrcdoc.data(), encodedSrcdoc.length()), "text/html", "UTF-8", KURL());
}

void FrameLoader::reportLocalLoadFailed(Frame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Not allowed to load local resource: " + url);
}

bool FrameLoader::willLoadMediaElementURL(KURL& url)
{
    ResourceRequest request(url);

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, ResourceResponse(url, String(), -1, String(), String()), 0, -1, -1, error);

    url = request.url();

    return error.isNull();
}

void FrameLoader::reload(ReloadPolicy reloadPolicy, const KURL& overrideURL, const String& overrideEncoding)
{
    DocumentLoader* documentLoader = activeDocumentLoader();
    if (!documentLoader)
        return;

    if (m_state == FrameStateProvisional)
        insertDummyHistoryItem();
    frame()->loader()->history()->saveDocumentAndScrollState();

    ResourceRequest request = documentLoader->request();
    // FIXME: We need to reset cache policy to prevent it from being incorrectly propagted to the reload.
    // Do we need to propagate anything other than the url?
    request.setCachePolicy(UseProtocolCachePolicy);
    if (!overrideURL.isEmpty())
        request.setURL(overrideURL);
    else if (!documentLoader->unreachableURL().isEmpty())
        request.setURL(documentLoader->unreachableURL());

    FrameLoadType type = reloadPolicy == EndToEndReload ? FrameLoadTypeReloadFromOrigin : FrameLoadTypeReload;
    NavigationAction action(request, type, request.httpMethod() == "POST");
    loadWithNavigationAction(request, action, type, 0, SubstituteData(), overrideEncoding);
}

void FrameLoader::stopAllLoaders(ClearProvisionalItemPolicy clearProvisionalItemPolicy)
{
    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;

    // Calling stopLoading() on the provisional document loader can blow away
    // the frame from underneath.
    RefPtr<Frame> protect(m_frame);

    m_inStopAllLoaders = true;

    // If no new load is in progress, we should clear the provisional item from history
    // before we call stopLoading.
    if (clearProvisionalItemPolicy == ShouldClearProvisionalItem)
        history()->setProvisionalItem(0);

    for (RefPtr<Frame> child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopAllLoaders(clearProvisionalItemPolicy);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->detachFromFrame();
    m_provisionalDocumentLoader = 0;

    m_checkTimer.stop();

    m_inStopAllLoaders = false;
}

void FrameLoader::stopForUserCancel(bool deferCheckLoadComplete)
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);
    stopAllLoaders();

    if (deferCheckLoadComplete)
        scheduleCheckLoadComplete();
    else if (m_frame->page())
        checkLoadComplete();
}

DocumentLoader* FrameLoader::activeDocumentLoader() const
{
    if (m_state == FrameStateProvisional)
        return m_provisionalDocumentLoader.get();
    return m_documentLoader.get();
}

void FrameLoader::didAccessInitialDocument()
{
    // We only need to notify the client once, and only for the main frame.
    if (isLoadingMainFrame() && !m_didAccessInitialDocument) {
        m_didAccessInitialDocument = true;
        // Notify asynchronously, since this is called within a JavaScript security check.
        m_didAccessInitialDocumentTimer.startOneShot(0);
    }
}

void FrameLoader::didAccessInitialDocumentTimerFired(Timer<FrameLoader>*)
{
    m_client->didAccessInitialDocument();
}

bool FrameLoader::isLoading() const
{
    DocumentLoader* docLoader = activeDocumentLoader();
    if (!docLoader)
        return false;
    return docLoader->isLoading();
}

void FrameLoader::commitProvisionalLoad()
{
    ASSERT(m_client->hasWebView());
    ASSERT(m_state == FrameStateProvisional);
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    RefPtr<Frame> protect(m_frame);

    closeOldDataSources();

    // Check if the destination page is allowed to access the previous page's timing information.
    if (m_frame->document()) {
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(pdl->request().url());
        pdl->timing()->setHasSameOriginAsPreviousDocument(securityOrigin->canRequest(m_frame->document()->url()));
    }

    clearAllowNavigationViaBeforeUnloadConfirmationPanel();

    // The call to closeURL() invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    if (m_documentLoader)
        closeURL();
    if (pdl != m_provisionalDocumentLoader)
        return;

    // detachChildren() can trigger this frame's unload event, and therefore
    // script can run and do just about anything. For example, an unload event that calls
    // document.write("") on its parent frame can lead to a recursive detachChildren()
    // invocation for this frame. Leave the loader that is being committed in a temporarily
    // detached state, such that it can't be found and cancelled.
    RefPtr<DocumentLoader> loaderBeingCommitted = m_provisionalDocumentLoader.release();
    detachChildren();
    if (m_documentLoader)
        m_documentLoader->detachFromFrame();
    m_documentLoader = loaderBeingCommitted;
    m_state = FrameStateCommittedPage;

    if (isLoadingMainFrame())
        m_frame->page()->chrome().client()->needTouchEvents(false);

    history()->updateForCommit();
    m_client->transitionToCommittedForNewPage();

    if (!m_stateMachine.creatingInitialEmptyDocument() && !m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);

    // A redirect was scheduled before the first real document was committed.
    // This can happen when one frame changes another frame's location.
    if (m_frame->navigationScheduler()->redirectScheduledDuringLoad())
        return;
    m_frame->navigationScheduler()->cancel();
    m_frame->editor()->clearLastEditCommand();

    // If we are still in the process of initializing an empty document then
    // its frame is not in a consistent state for rendering, so avoid setJSStatusBarText
    // since it may cause clients to attempt to render the frame.
    if (!m_stateMachine.creatingInitialEmptyDocument()) {
        DOMWindow* window = m_frame->domWindow();
        window->setStatus(String());
        window->setDefaultStatus(String());
    }
    started();
}

void FrameLoader::closeOldDataSources()
{
    // FIXME: Is it important for this traversal to be postorder instead of preorder?
    // If so, add helpers for postorder traversal, and use them. If not, then lets not
    // use a recursive algorithm here.
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->closeOldDataSources();

    if (m_documentLoader)
        m_client->dispatchWillClose();
}

bool FrameLoader::isHostedByObjectElement() const
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    return owner && owner->hasTagName(objectTag);
}

bool FrameLoader::isLoadingMainFrame() const
{
    Page* page = m_frame->page();
    return page && m_frame == page->mainFrame();
}

bool FrameLoader::subframeIsLoading() const
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling()) {
        FrameLoader* childLoader = child->loader();
        DocumentLoader* documentLoader = childLoader->documentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->provisionalDocumentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->policyDocumentLoader();
        if (documentLoader)
            return true;
    }
    return false;
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}

CachePolicy FrameLoader::subresourceCachePolicy() const
{
    if (m_isComplete)
        return CachePolicyVerify;

    if (m_loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame->tree()->parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader()->subresourceCachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (m_loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    const ResourceRequest& request(documentLoader()->request());

    if (request.cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;

    return CachePolicyVerify;
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());
    if (m_state != FrameStateCommittedPage)
        return;

    if (!m_documentLoader || (m_documentLoader->isLoadingInAPISense() && !m_documentLoader->isStopping()))
        return;

    m_state = FrameStateComplete;

    // FIXME: Is this subsequent work important if we already navigated away?
    // Maybe there are bugs because of that, or extra work we can skip because
    // the new page is ready.

    // If the user had a scroll point, scroll to it, overriding the anchor point if any.
    if (m_frame->page()) {
        if (isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin)
            history()->restoreScrollPositionAndViewState();
    }

    if (!m_stateMachine.committedFirstRealDocumentLoad())
        return;

    m_progressTracker->progressCompleted();
    if (Page* page = m_frame->page()) {
        if (m_frame == page->mainFrame())
            page->resetRelevantPaintedObjectCounter();
    }

    const ResourceError& error = m_documentLoader->mainDocumentError();
    if (!error.isNull())
        m_client->dispatchDidFailLoad(error);
    else
        m_client->dispatchDidFinishLoad();
    m_loadType = FrameLoadTypeStandard;
}

void FrameLoader::didLayout(LayoutMilestones milestones)
{
    m_client->dispatchDidLayout(milestones);
}

void FrameLoader::didFirstLayout()
{
    if (m_frame->page() && isBackForwardLoadType(m_loadType))
        history()->restoreScrollPositionAndViewState();
}

void FrameLoader::detachChildren()
{
    typedef Vector<RefPtr<Frame> > FrameVector;
    FrameVector childrenToDetach;
    childrenToDetach.reserveCapacity(m_frame->tree()->childCount());
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling())
        childrenToDetach.append(child);
    FrameVector::iterator end = childrenToDetach.end();
    for (FrameVector::iterator it = childrenToDetach.begin(); it != end; it++)
        (*it)->loader()->detachFromParent();
}

void FrameLoader::closeAndRemoveChild(Frame* child)
{
    child->tree()->detachFromParent();

    child->setView(0);
    if (child->ownerElement() && child->page())
        child->page()->decrementSubframeCount();
    child->willDetachPage();
    child->detachFromPage();

    m_frame->tree()->removeChild(child);
}

// Called every time a resource is completely loaded or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(m_client->hasWebView());

    m_shouldCallCheckLoadComplete = false;

    // FIXME: Always traversing the entire frame tree is a bit inefficient, but
    // is currently needed in order to null out the previous history item for all frames.
    if (Page* page = m_frame->page()) {
        Vector<RefPtr<Frame>, 10> frames;
        for (RefPtr<Frame> frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frames.append(frame);
        // To process children before their parents, iterate the vector backwards.
        for (size_t i = frames.size(); i; --i)
            frames[i - 1]->loader()->checkLoadCompleteForThisFrame();
    }
}

int FrameLoader::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return m_frame->document()->fetcher()->requestCount();

    int count = 0;
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame))
        count += frame->document()->fetcher()->requestCount();
    return count;
}

String FrameLoader::userAgent(const KURL& url) const
{
    String userAgent = m_client->userAgent(url);
    InspectorInstrumentation::applyUserAgentOverride(m_frame, &userAgent);
    return userAgent;
}

void FrameLoader::frameDetached()
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);
    stopAllLoaders();
    m_frame->document()->stopActiveDOMObjects();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);

    closeURL();
    history()->saveScrollPositionAndViewStateToItem(history()->currentItem());
    detachChildren();
    // stopAllLoaders() needs to be called after detachChildren(), because detachedChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    stopAllLoaders();

    InspectorInstrumentation::frameDetachedFromParent(m_frame);

    if (m_documentLoader)
        m_documentLoader->detachFromFrame();
    m_documentLoader = 0;
    m_client->detachedFromParent();

    m_progressTracker.clear();

    if (Frame* parent = m_frame->tree()->parent()) {
        parent->loader()->closeAndRemoveChild(m_frame);
        parent->loader()->scheduleCheckCompleted();
    } else {
        m_frame->setView(0);
        m_frame->willDetachPage();
        m_frame->detachFromPage();
    }
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request)
{
    bool isMainResource = (request.targetType() == ResourceRequest::TargetIsMainFrame) || (request.targetType() == ResourceRequest::TargetIsSubframe);

    if (isMainResource && isLoadingMainFrame())
        request.setFirstPartyForCookies(request.url());
    else
        request.setFirstPartyForCookies(m_frame->document()->firstPartyForCookies());

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    applyUserAgent(request);

    if (request.cachePolicy() == ReloadIgnoringCacheData) {
        if (m_loadType == FrameLoadTypeReload)
            request.setHTTPHeaderField("Cache-Control", "max-age=0");
        else if (m_loadType == FrameLoadTypeReloadFromOrigin) {
            request.setHTTPHeaderField("Cache-Control", "no-cache");
            request.setHTTPHeaderField("Pragma", "no-cache");
        }
    }

    if (isMainResource)
        request.setHTTPAccept(defaultAcceptHeader);

    // Make sure we send the Origin header.
    addHTTPOriginIfNeeded(request, String());
}

void FrameLoader::addHTTPOriginIfNeeded(ResourceRequest& request, const String& origin)
{
    if (!request.httpOrigin().isEmpty())
        return;  // Request already has an Origin header.

    // Don't send an Origin header for GET or HEAD to avoid privacy issues.
    // For example, if an intranet page has a hyperlink to an external web
    // site, we don't want to include the Origin of the request because it
    // will leak the internal host name. Similar privacy concerns have lead
    // to the widespread suppression of the Referer header at the network
    // layer.
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return;

    // For non-GET and non-HEAD methods, always send an Origin header so the
    // server knows we support this feature.

    if (origin.isEmpty()) {
        // If we don't know what origin header to attach, we attach the value
        // for an empty origin.
        request.setHTTPOrigin(SecurityOrigin::createUnique()->toString());
        return;
    }

    request.setHTTPOrigin(origin);
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    ASSERT(m_frame->document());
    String referrer = SecurityPolicy::generateReferrerHeader(m_frame->document()->referrerPolicy(), request.url(), outgoingReferrer());

    ResourceRequest initialRequest = request;
    initialRequest.setTimeoutInterval(10);

    if (!referrer.isEmpty())
        initialRequest.setHTTPReferrer(referrer);
    addHTTPOriginIfNeeded(initialRequest, outgoingOrigin());

    addExtraFieldsToRequest(initialRequest);

    unsigned long identifier = 0;
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        documentLoader()->applicationCacheHost()->willStartLoadingSynchronously(newRequest);
        ResourceLoader::loadResourceSynchronously(newRequest, storedCredentials, error, response, data);
    }
    int encodedDataLength = response.resourceLoadInfo() ? static_cast<int>(response.resourceLoadInfo()->encodedDataLength) : -1;
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, response, data.data(), data.size(), encodedDataLength, error);
    return identifier;
}

const ResourceRequest& FrameLoader::originalRequest() const
{
    return activeDocumentLoader()->originalRequestCopy();
}

void FrameLoader::receivedMainResourceError(const ResourceError& error)
{
    // Retain because the stop may release the last reference to it.
    RefPtr<Frame> protect(m_frame);

    RefPtr<DocumentLoader> loader = activeDocumentLoader();
    // FIXME: Don't want to do this if an entirely new load is going, so should check
    // that both data sources on the frame are either this or nil.
    stop();

    // FIXME: We really ought to be able to just check for isCancellation() here, but there are some
    // ResourceErrors that setIsCancellation() but aren't created by ResourceError::cancelledError().
    ResourceError c(ResourceError::cancelledError(KURL()));
    if (error.errorCode() != c.errorCode() || error.domain() != c.domain())
        handleFallbackContent();

    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        if (m_submittedFormURL == m_provisionalDocumentLoader->originalRequestCopy().url())
            m_submittedFormURL = KURL();

        m_client->dispatchDidFailProvisionalLoad(error);
        if (loader != m_provisionalDocumentLoader)
            return;
        m_provisionalDocumentLoader->detachFromFrame();
        m_provisionalDocumentLoader = 0;
        m_progressTracker->progressCompleted();
        m_state = FrameStateComplete;

        // Reset the back forward list to the last committed history item at the top level.
        RefPtr<HistoryItem> item = m_frame->page()->mainFrame()->loader()->history()->currentItem();
        if (isBackForwardLoadType(loadType()) && !history()->provisionalItem() && item)
            m_frame->page()->backForward()->setCurrentItem(item.get());
    }

    checkCompleted();
    if (m_frame->page())
        checkLoadComplete();
}

void FrameLoader::checkNavigationPolicyAndContinueFragmentScroll(const NavigationAction& action, bool isNewNavigation)
{
    m_documentLoader->setTriggeringAction(action);

    const ResourceRequest& request = action.resourceRequest();
    if (!m_documentLoader->shouldContinueForNavigationPolicy(request, DocumentLoader::PolicyCheckStandard))
        return;

    // If we have a provisional request for a different document, a fragment scroll should cancel it.
    if (m_provisionalDocumentLoader && !equalIgnoringFragmentIdentifier(m_provisionalDocumentLoader->request().url(), request.url())) {
        m_provisionalDocumentLoader->stopLoading();
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->detachFromFrame();
        m_provisionalDocumentLoader = 0;
    }
    if (isNewNavigation && !shouldTreatURLAsSameAsCurrent(request.url()))
        history()->updateBackForwardListForFragmentScroll();
    loadInSameDocument(request.url(), 0, isNewNavigation);
}

bool FrameLoader::shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType loadType, const KURL& url)
{
    ASSERT(loadType != FrameLoadTypeBackForward);
    ASSERT(loadType != FrameLoadTypeReloadFromOrigin);
    // We don't do this if we are submitting a form with method other than "GET", explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    return (!isFormSubmission || equalIgnoringCase(httpMethod, "GET"))
        && loadType != FrameLoadTypeReload
        && loadType != FrameLoadTypeSame
        && url.hasFragmentIdentifier()
        && equalIgnoringFragmentIdentifier(m_frame->document()->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame->document()->isFrameSet();
}

void FrameLoader::scrollToFragmentWithParentBoundary(const KURL& url)
{
    FrameView* view = m_frame->view();
    if (!view)
        return;

    // Leaking scroll position to a cross-origin ancestor would permit the so-called "framesniffing" attack.
    RefPtr<Frame> boundaryFrame(url.hasFragmentIdentifier() ? m_frame->document()->findUnsafeParentScrollPropagationBoundary() : 0);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(false);

    view->scrollToFragment(url);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(true);
}

bool FrameLoader::shouldClose()
{
    Page* page = m_frame->page();
    if (!page || !page->chrome().canRunBeforeUnloadConfirmPanel())
        return true;

    // Store all references to each subframe in advance since beforeunload's event handler may modify frame
    Vector<RefPtr<Frame> > targetFrames;
    targetFrames.append(m_frame);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->traverseNext(m_frame))
        targetFrames.append(child);

    bool shouldClose = false;
    {
        NavigationDisablerForBeforeUnload navigationDisabler;
        size_t i;

        for (i = 0; i < targetFrames.size(); i++) {
            if (!targetFrames[i]->tree()->isDescendantOf(m_frame))
                continue;
            if (!targetFrames[i]->loader()->fireBeforeUnloadEvent(page->chrome(), this))
                break;
        }

        if (i == targetFrames.size())
            shouldClose = true;
    }

    if (!shouldClose)
        m_submittedFormURL = KURL();

    return shouldClose;
}

bool FrameLoader::fireBeforeUnloadEvent(Chrome& chrome, FrameLoader* navigatingFrameLoader)
{
    DOMWindow* domWindow = m_frame->domWindow();
    if (!domWindow)
        return true;

    RefPtr<Document> document = m_frame->document();
    if (!document->body())
        return true;

    RefPtr<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();
    m_pageDismissalEventBeingDispatched = BeforeUnloadDismissal;
    domWindow->dispatchEvent(beforeUnloadEvent.get(), domWindow->document());
    m_pageDismissalEventBeingDispatched = NoDismissal;

    if (!beforeUnloadEvent->defaultPrevented())
        document->defaultEventHandler(beforeUnloadEvent.get());
    if (beforeUnloadEvent->result().isNull())
        return true;

    if (navigatingFrameLoader->hasAllowedNavigationViaBeforeUnloadConfirmationPanel()) {
        m_frame->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Blocked attempt to show multiple 'beforeunload' confirmation panels for a single navigation.");
        return true;
    }

    String text = document->displayStringModifiedByEncoding(beforeUnloadEvent->result());
    if (chrome.runBeforeUnloadConfirmPanel(text, m_frame)) {
        navigatingFrameLoader->didAllowNavigationViaBeforeUnloadConfirmationPanel();
        return true;
    }
    return false;
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, const NavigationAction& action, FrameLoadType type, PassRefPtr<FormState> formState, const SubstituteData& substituteData, const String& overrideEncoding)
{
    ASSERT(m_client->hasWebView());
    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    // We skip dispatching the beforeload event on the frame owner if we've already committed a real
    // document load because the event would leak subsequent activity by the frame which the parent
    // frame isn't supposed to learn. For example, if the child frame navigated to  a new URL, the
    // parent frame shouldn't learn the URL.
    if (!m_stateMachine.committedFirstRealDocumentLoad() && m_frame->ownerElement() && !m_frame->ownerElement()->dispatchBeforeLoadEvent(request.url().string()))
        return;

    if (!m_stateMachine.startedFirstRealLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::StartedFirstRealLoad);

    m_policyDocumentLoader = m_client->createDocumentLoader(request, substituteData.isValid() ? substituteData : defaultSubstituteDataForURL(request.url()));
    m_policyDocumentLoader->setFrame(m_frame);
    m_policyDocumentLoader->setTriggeringAction(action);
    m_policyDocumentLoader->setReplacesCurrentHistoryItem(type == FrameLoadTypeRedirectWithLockedBackForwardList);
    m_policyDocumentLoader->setIsClientRedirect(m_startingClientRedirect);

    if (Frame* parent = m_frame->tree()->parent())
        m_policyDocumentLoader->setOverrideEncoding(parent->loader()->documentLoader()->overrideEncoding());
    else if (!overrideEncoding.isEmpty())
        m_policyDocumentLoader->setOverrideEncoding(overrideEncoding);
    else if (m_documentLoader)
        m_policyDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);
    if (!m_policyDocumentLoader->shouldContinueForNavigationPolicy(request, DocumentLoader::PolicyCheckStandard) || !shouldClose()) {
        m_policyDocumentLoader->detachFromFrame();
        m_policyDocumentLoader = 0;
        return;
    }

    // A new navigation is in progress, so don't clear the history's provisional item.
    stopAllLoaders(ShouldNotClearProvisionalItem);

    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load.
    if (!m_frame->page())
        return;

    if (isLoadingMainFrame())
        m_frame->page()->inspectorController()->resume();
    m_frame->navigationScheduler()->cancel();

    m_provisionalDocumentLoader = m_policyDocumentLoader.release();
    m_loadType = type;
    m_state = FrameStateProvisional;

    if (formState)
        m_client->dispatchWillSubmitForm(formState);

    m_progressTracker->progressStarted();
    if (m_provisionalDocumentLoader->isClientRedirect())
        m_provisionalDocumentLoader->appendRedirect(m_frame->document()->url());
    m_provisionalDocumentLoader->appendRedirect(m_provisionalDocumentLoader->request().url());
    m_client->dispatchDidStartProvisionalLoad();
    ASSERT(m_provisionalDocumentLoader);
    m_provisionalDocumentLoader->startLoadingMainResource();
}

void FrameLoader::checkNewWindowPolicyAndContinue(PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action)
{
    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    if (m_frame->document() && m_frame->document()->isSandboxed(SandboxPopups))
        return;

    if (!DOMWindow::allowPopUp(m_frame))
        return;

    NavigationPolicy navigationPolicy = NavigationPolicyNewForegroundTab;
    action.specifiesNavigationPolicy(&navigationPolicy);

    if (navigationPolicy == NavigationPolicyDownload) {
        m_client->loadURLExternally(action.resourceRequest(), navigationPolicy);
        return;
    }

    RefPtr<Frame> frame = m_frame;
    RefPtr<Frame> mainFrame = m_frame;

    if (!m_frame->settings() || m_frame->settings()->supportsMultipleWindows()) {
        struct WindowFeatures features;
        Page* newPage = m_frame->page()->chrome().client()->createWindow(m_frame, FrameLoadRequest(m_frame->document()->securityOrigin()),
            features, action, navigationPolicy);

        // createWindow can return null (e.g., popup blocker denies the window).
        if (!newPage)
            return;
        mainFrame = newPage->mainFrame();
    }

    if (frameName != "_blank")
        mainFrame->tree()->setName(frameName);

    mainFrame->page()->setOpenedByDOM();
    mainFrame->page()->chrome().show(navigationPolicy);
    if (!m_suppressOpenerInNewFrame) {
        mainFrame->loader()->setOpener(frame.get());
        mainFrame->document()->setReferrerPolicy(frame->document()->referrerPolicy());
    }

    // FIXME: We can't just send our NavigationAction to the new FrameLoader's loadWithNavigationAction(), we need to
    // create a new one with a default NavigationType and no triggering event. We should figure out why.
    mainFrame->loader()->loadWithNavigationAction(action.resourceRequest(), NavigationAction(action.resourceRequest()), FrameLoadTypeStandard, formState, SubstituteData());
}

void FrameLoader::requestFromDelegate(ResourceRequest& request, unsigned long& identifier, ResourceError& error)
{
    ASSERT(!request.isNull());

    identifier = 0;
    if (Page* page = m_frame->page())
        identifier = createUniqueIdentifier();

    ResourceRequest newRequest(request);
    notifier()->dispatchWillSendRequest(m_documentLoader.get(), identifier, newRequest, ResourceResponse());

    if (newRequest.isNull())
        error = ResourceError::cancelledError(request.url());
    else
        error = ResourceError();

    request = newRequest;
}

void FrameLoader::loadedResourceFromMemoryCache(Resource* resource)
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (!resource->shouldSendResourceLoadCallbacks())
        return;

    // Main resource delegate messages are synthesized in MainResourceLoader, so we must not send them here.
    if (resource->type() == Resource::MainResource)
        return;

    ResourceRequest request(resource->url());
    m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize());

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    InspectorInstrumentation::markResourceAsCached(page, identifier);
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, resource->response(), 0, resource->encodedSize(), 0, error);
}

void FrameLoader::applyUserAgent(ResourceRequest& request)
{
    String userAgent = this->userAgent(request.url());
    ASSERT(!userAgent.isNull());
    request.setHTTPUserAgent(userAgent);
}

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const KURL& url, unsigned long requestIdentifier)
{
    UseCounter::count(m_frame->document(), UseCounter::XFrameOptions);

    Frame* topFrame = m_frame->tree()->top();
    if (m_frame == topFrame)
        return false;

    XFrameOptionsDisposition disposition = parseXFrameOptionsHeader(content);

    switch (disposition) {
    case XFrameOptionsSameOrigin: {
        UseCounter::count(m_frame->document(), UseCounter::XFrameOptionsSameOrigin);
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
        if (!origin->isSameSchemeHostPort(topFrame->document()->securityOrigin()))
            return true;
        for (Frame* frame = m_frame->tree()->parent(); frame; frame = frame->tree()->parent()) {
            if (!origin->isSameSchemeHostPort(frame->document()->securityOrigin())) {
                UseCounter::count(m_frame->document(), UseCounter::XFrameOptionsSameOriginWithBadAncestorChain);
                break;
            }
        }
        return false;
    }
    case XFrameOptionsDeny:
        return true;
    case XFrameOptionsAllowAll:
        return false;
    case XFrameOptionsConflict:
        m_frame->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Multiple 'X-Frame-Options' headers with conflicting values ('" + content + "') encountered when loading '" + url.elidedString() + "'. Falling back to 'DENY'.", requestIdentifier);
        return true;
    case XFrameOptionsInvalid:
        m_frame->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Invalid 'X-Frame-Options' header encountered when loading '" + url.elidedString() + "': '" + content + "' is not a recognized directive. The header will be ignored.", requestIdentifier);
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& url) const
{
    if (!history()->currentItem())
        return false;
    return url == history()->currentItem()->url() || url == history()->currentItem()->originalURL();
}

bool FrameLoader::shouldTreatURLAsSrcdocDocument(const KURL& url) const
{
    if (!equalIgnoringCase(url.string(), "about:srcdoc"))
        return false;
    HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement();
    if (!ownerElement)
        return false;
    if (!ownerElement->hasTagName(iframeTag))
        return false;
    return ownerElement->fastHasAttribute(srcdocAttr);
}

Frame* FrameLoader::findFrameForNavigation(const AtomicString& name, Document* activeDocument)
{
    Frame* frame = m_frame->tree()->find(name);

    // From http://www.whatwg.org/specs/web-apps/current-work/#seamlessLinks:
    //
    // If the source browsing context is the same as the browsing context
    // being navigated, and this browsing context has its seamless browsing
    // context flag set, and the browsing context being navigated was not
    // chosen using an explicit self-navigation override, then find the
    // nearest ancestor browsing context that does not have its seamless
    // browsing context flag set, and continue these steps as if that
    // browsing context was the one that was going to be navigated instead.
    if (frame == m_frame && name != "_self" && m_frame->document()->shouldDisplaySeamlesslyWithParent()) {
        for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree()->parent()) {
            if (!ancestor->document()->shouldDisplaySeamlesslyWithParent()) {
                frame = ancestor;
                break;
            }
        }
        ASSERT(frame != m_frame);
    }

    if (activeDocument) {
        if (!activeDocument->canNavigate(frame))
            return 0;
    } else {
        // FIXME: Eventually all callers should supply the actual activeDocument
        // so we can call canNavigate with the right document.
        if (!m_frame->document()->canNavigate(frame))
            return 0;
    }

    return frame;
}

void FrameLoader::loadSameDocumentItem(HistoryItem* item)
{
    ASSERT(item->documentSequenceNumber() == history()->currentItem()->documentSequenceNumber());

    // Save user view state to the current history item here since we don't do a normal load.
    // FIXME: Does form state need to be saved here too?
    history()->saveScrollPositionAndViewStateToItem(history()->currentItem());
    if (FrameView* view = m_frame->view())
        view->setWasScrolledByUser(false);

    history()->setCurrentItem(item);

    // loadInSameDocument() actually changes the URL and notifies load delegates of a "fake" load
    loadInSameDocument(item->url(), item->stateObject(), false);

    // Restore user view state from the current history item here since we don't do a normal load.
    history()->restoreScrollPositionAndViewState();
}

// FIXME: This function should really be split into a couple pieces, some of
// which should be methods of HistoryController and some of which should be
// methods of FrameLoader.
void FrameLoader::loadDifferentDocumentItem(HistoryItem* item)
{
    // Remember this item so we can traverse any child items as child frames load
    history()->setProvisionalItem(item);

    RefPtr<FormData> formData = item->formData();
    ResourceRequest request(item->url());
    request.setHTTPReferrer(item->referrer());
    if (formData) {
        request.setHTTPMethod("POST");
        request.setHTTPBody(formData);
        request.setHTTPContentType(item->formContentType());
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromString(item->referrer());
        addHTTPOriginIfNeeded(request, securityOrigin->toString());
    }

    loadWithNavigationAction(request, NavigationAction(request, FrameLoadTypeBackForward, false), FrameLoadTypeBackForward, 0, SubstituteData());
}

void FrameLoader::loadHistoryItem(HistoryItem* item)
{
    m_requestedHistoryItem = item;
    HistoryItem* currentItem = history()->currentItem();
    bool sameDocumentNavigation = currentItem && item->shouldDoSameDocumentNavigationTo(currentItem);

    if (sameDocumentNavigation)
        loadSameDocumentItem(item);
    else
        loadDifferentDocumentItem(item);
}

void FrameLoader::insertDummyHistoryItem()
{
    RefPtr<HistoryItem> currentItem = HistoryItem::create();
    history()->setCurrentItem(currentItem.get());
    frame()->page()->backForward()->setCurrentItem(currentItem.get());
}

void FrameLoader::setTitle(const StringWithDirection& title)
{
    documentLoader()->setTitle(title);
}

String FrameLoader::referrer() const
{
    return m_documentLoader ? m_documentLoader->request().httpReferrer() : "";
}

void FrameLoader::dispatchDocumentElementAvailable()
{
    m_client->documentElementAvailable();
}

void FrameLoader::dispatchDidClearWindowObjectsInAllWorlds()
{
    if (!m_frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;

    Vector<RefPtr<DOMWrapperWorld> > worlds;
    DOMWrapperWorld::getAllWorlds(worlds);
    for (size_t i = 0; i < worlds.size(); ++i)
        dispatchDidClearWindowObjectInWorld(worlds[i].get());
}

void FrameLoader::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (!m_frame->script()->canExecuteScripts(NotAboutToExecuteScript) || !m_frame->script()->existingWindowShell(world))
        return;

    m_client->dispatchDidClearWindowObjectInWorld(world);

    if (Page* page = m_frame->page())
        page->inspectorController()->didClearWindowObjectInWorld(m_frame, world);

    InspectorInstrumentation::didClearWindowObjectInWorld(m_frame, world);
}

SandboxFlags FrameLoader::effectiveSandboxFlags() const
{
    SandboxFlags flags = m_forcedSandboxFlags;
    if (Frame* parentFrame = m_frame->tree()->parent())
        flags |= parentFrame->document()->sandboxFlags();
    if (HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement())
        flags |= ownerElement->sandboxFlags();
    return flags;
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    if (loader == m_documentLoader) {
        // Must update the entries in the back-forward list too.
        history()->setCurrentItemTitle(loader->title());
        m_client->dispatchDidReceiveTitle(loader->title());
    }
}

void FrameLoader::dispatchDidCommitLoad()
{
    m_client->dispatchDidCommitLoad();

    InspectorInstrumentation::didCommitLoad(m_frame, m_documentLoader.get());

    m_frame->page()->didCommitLoad(m_frame);

    if (m_frame->page()->mainFrame() == m_frame)
        m_frame->page()->useCounter()->didCommitLoad();

}

} // namespace WebCore
