/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WheelController_h
#define WheelController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/events/Event.h"
#include "core/frame/DOMWindowLifecycleObserver.h"


namespace WebCore {

class DOMWindow;

class WheelController : public DocumentSupplement, public DOMWindowLifecycleObserver {

public:
    virtual ~WheelController();

    static const char* supplementName();
    static WheelController* from(Document*);

    unsigned wheelEventHandlerCount() { return m_wheelEventHandlerCount; }

    void didAddWheelEventHandler(Document*);
    void didRemoveWheelEventHandler(Document*);

    // Inherited from DOMWindowLifecycleObserver
    virtual void didAddEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(DOMWindow*, const AtomicString&) OVERRIDE;

private:
    explicit WheelController(Document*);

    unsigned m_wheelEventHandlerCount;
};

} // namespace WebCore

#endif // WheelController_h
