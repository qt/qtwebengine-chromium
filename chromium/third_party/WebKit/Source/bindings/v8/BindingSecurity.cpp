/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/BindingSecurity.h"

#include "bindings/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

static bool isDocumentAccessibleFromDOMWindow(Document* targetDocument, DOMWindow* activeWindow)
{
    if (!targetDocument)
        return false;

    if (!activeWindow)
        return false;

    if (activeWindow->document()->securityOrigin()->canAccess(targetDocument->securityOrigin()))
        return true;

    return false;
}

static bool canAccessDocument(Document* targetDocument, ExceptionState& es)
{
    DOMWindow* activeWindow = activeDOMWindow();
    if (isDocumentAccessibleFromDOMWindow(targetDocument, activeWindow))
        return true;

    es.throwSecurityError(targetDocument->domWindow()->sanitizedCrossDomainAccessErrorMessage(activeWindow), targetDocument->domWindow()->crossDomainAccessErrorMessage(activeWindow));
    return false;
}

static bool canAccessDocument(Document* targetDocument, SecurityReportingOption reportingOption = ReportSecurityError)
{
    DOMWindow* activeWindow = activeDOMWindow();
    if (isDocumentAccessibleFromDOMWindow(targetDocument, activeWindow))
        return true;

    if (reportingOption == ReportSecurityError) {
        if (Frame* frame = targetDocument->frame())
            frame->domWindow()->printErrorMessage(targetDocument->domWindow()->crossDomainAccessErrorMessage(activeWindow));
    }

    return false;
}

bool BindingSecurity::shouldAllowAccessToFrame(Frame* target, SecurityReportingOption reportingOption)
{
    return target && canAccessDocument(target->document(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToFrame(Frame* target, ExceptionState& es)
{
    return target && canAccessDocument(target->document(), es);
}

bool BindingSecurity::shouldAllowAccessToNode(Node* target)
{
    return target && canAccessDocument(&target->document());
}

}
