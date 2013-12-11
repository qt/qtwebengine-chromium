/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DateTimeFieldElement_h
#define DateTimeFieldElement_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSpanElement.h"

namespace WebCore {

class DateComponents;
class DateTimeFieldsState;
class Font;

// DateTimeFieldElement is base class of date time field element.
class DateTimeFieldElement : public HTMLSpanElement {
    WTF_MAKE_NONCOPYABLE(DateTimeFieldElement);

public:
    enum EventBehavior {
        DispatchNoEvent,
        DispatchEvent,
    };

    // FieldOwner implementer must call removeEventHandler when
    // it doesn't handle event, e.g. at destruction.
    class FieldOwner {
    public:
        virtual ~FieldOwner();
        virtual void didBlurFromField() = 0;
        virtual void didFocusOnField() = 0;
        virtual void fieldValueChanged() = 0;
        virtual bool focusOnNextField(const DateTimeFieldElement&) = 0;
        virtual bool focusOnPreviousField(const DateTimeFieldElement&) = 0;
        virtual bool isFieldOwnerDisabled() const = 0;
        virtual bool isFieldOwnerReadOnly() const = 0;
        virtual AtomicString localeIdentifier() const = 0;
    };

    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool hasValue() const = 0;
    bool isDisabled() const;
    virtual float maximumWidth(const Font&);
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) = 0;
    void removeEventHandler() { m_fieldOwner = 0; }
    void setDisabled();
    virtual void setEmptyValue(EventBehavior = DispatchNoEvent) = 0;
    virtual void setValueAsDate(const DateComponents&) = 0;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) = 0;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) = 0;
    virtual void stepDown() = 0;
    virtual void stepUp() = 0;
    virtual String value() const = 0;
    virtual String visibleValue() const = 0;

protected:
    DateTimeFieldElement(Document&, FieldOwner&);
    virtual void didBlur();
    virtual void didFocus();
    void focusOnNextField();
    virtual void handleKeyboardEvent(KeyboardEvent*) = 0;
    void initialize(const AtomicString& pseudo, const String& axHelpText, int axMinimum, int axMaximum);
    Locale& localeForOwner() const;
    AtomicString localeIdentifier() const;
    void updateVisibleValue(EventBehavior);
    virtual int valueAsInteger() const = 0;
    virtual int valueForARIAValueNow() const;

private:
    void defaultKeyboardEventHandler(KeyboardEvent*);
    virtual bool isDateTimeFieldElement() const OVERRIDE;
    bool isFieldOwnerDisabled() const;
    bool isFieldOwnerReadOnly() const;
    virtual bool supportsFocus() const OVERRIDE FINAL;

    FieldOwner* m_fieldOwner;
};

} // namespace WebCore

#endif
#endif
