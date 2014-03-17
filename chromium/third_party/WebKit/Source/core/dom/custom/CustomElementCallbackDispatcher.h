/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef CustomElementCallbackDispatcher_h
#define CustomElementCallbackDispatcher_h

#include "core/dom/custom/CustomElementBaseElementQueue.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementCallbackQueue;
class CustomElementCallbackScheduler;

class CustomElementCallbackDispatcher {
    WTF_MAKE_NONCOPYABLE(CustomElementCallbackDispatcher);
public:
    static CustomElementCallbackDispatcher& instance();

    // This is stack allocated in many DOM callbacks. Make it cheap.
    class CallbackDeliveryScope {
    public:
        CallbackDeliveryScope()
            : m_savedElementQueueStart(s_elementQueueStart)
        {
            s_elementQueueStart = s_elementQueueEnd;
        }

        ~CallbackDeliveryScope()
        {
            if (s_elementQueueStart != s_elementQueueEnd)
                processElementQueueAndPop();
            s_elementQueueStart = m_savedElementQueueStart;
        }

    private:
        size_t m_savedElementQueueStart;
    };

    // Returns true if more work may have to be performed at the
    // checkpoint by this or other workers (for example, this work
    // invoked author scripts)
    bool dispatch();

protected:
    friend class CustomElementCallbackScheduler;
    void enqueue(CustomElementCallbackQueue*);

private:
    CustomElementCallbackDispatcher()
    {
        // Add a null element as a sentinel. This makes it possible to
        // identify elements queued when there is no
        // CallbackDeliveryScope active. Also, if the processing stack
        // is popped when empty, this sentinel will cause a null deref
        // crash.
        CustomElementCallbackQueue* sentinel = 0;
        for (size_t i = 0; i < kNumSentinels; i++)
            m_flattenedProcessingStack.append(sentinel);
        ASSERT(s_elementQueueEnd == m_flattenedProcessingStack.size());
    }

    // The start of the element queue on the top of the processing
    // stack. An offset into instance().m_flattenedProcessingStack.
    static size_t s_elementQueueStart;

    // The end of the element queue on the top of the processing
    // stack. A cache of instance().m_flattenedProcessingStack.size().
    static size_t s_elementQueueEnd;

    static bool inCallbackDeliveryScope() { return s_elementQueueStart; }

    typedef int ElementQueue;
    static ElementQueue baseElementQueue() { return ElementQueue(0); }
    static ElementQueue currentElementQueue() { return ElementQueue(s_elementQueueStart); }

    static void processElementQueueAndPop();
    void processElementQueueAndPop(size_t start, size_t end);

    // The base element queue, used when no CallbackDeliveryScope is
    // active. Callbacks for elements created by the parser are
    // enqueued here.
    CustomElementBaseElementQueue m_baseElementQueue;

    // The processing stack, flattened. Element queues lower in the
    // stack appear toward the head of the vector. The first element
    // is a null sentinel value.
    static const size_t kNumSentinels = 1;
    Vector<CustomElementCallbackQueue*> m_flattenedProcessingStack;
};

}

#endif // CustomElementCallbackDispatcher_h
