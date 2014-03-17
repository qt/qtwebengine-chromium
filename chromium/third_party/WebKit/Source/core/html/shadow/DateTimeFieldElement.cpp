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

#include "config.h"
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/shadow/DateTimeFieldElement.h"

#include "HTMLNames.h"
#include "core/dom/Text.h"
#include "core/events/KeyboardEvent.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

static String emptyValueAXText()
{
    return Locale::defaultLocale().queryString(blink::WebLocalizedString::AXDateTimeFieldEmptyValueText);
}

DateTimeFieldElement::FieldOwner::~FieldOwner()
{
}

DateTimeFieldElement::DateTimeFieldElement(Document& document, FieldOwner& fieldOwner)
    : HTMLSpanElement(document)
    , m_fieldOwner(&fieldOwner)
{
}

void DateTimeFieldElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::blur)
        didBlur();

    if (event->type() == EventTypeNames::focus)
        didFocus();

    if (event->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = toKeyboardEvent(event);
        if (!isDisabled() && !isFieldOwnerDisabled() && !isFieldOwnerReadOnly()) {
            handleKeyboardEvent(keyboardEvent);
            if (keyboardEvent->defaultHandled())
                return;
        }
        defaultKeyboardEventHandler(keyboardEvent);
        if (keyboardEvent->defaultHandled())
            return;
    }

    HTMLElement::defaultEventHandler(event);
}

void DateTimeFieldElement::defaultKeyboardEventHandler(KeyboardEvent* keyboardEvent)
{
    if (keyboardEvent->type() != EventTypeNames::keydown)
        return;

    if (isDisabled() || isFieldOwnerDisabled())
        return;

    const String& keyIdentifier = keyboardEvent->keyIdentifier();

    if (keyIdentifier == "Left") {
        if (!m_fieldOwner)
            return;
        // FIXME: We'd like to use FocusController::advanceFocus(FocusDirectionLeft, ...)
        // but it doesn't work for shadow nodes. webkit.org/b/104650
        if (!localeForOwner().isRTL() && m_fieldOwner->focusOnPreviousField(*this))
            keyboardEvent->setDefaultHandled();
        return;
    }

    if (keyIdentifier == "Right") {
        if (!m_fieldOwner)
            return;
        // FIXME: We'd like to use FocusController::advanceFocus(FocusDirectionRight, ...)
        // but it doesn't work for shadow nodes. webkit.org/b/104650
        if (!localeForOwner().isRTL() && m_fieldOwner->focusOnNextField(*this))
            keyboardEvent->setDefaultHandled();
        return;
    }

    if (isFieldOwnerReadOnly())
        return;

    if (keyIdentifier == "Down") {
        if (keyboardEvent->getModifierState("Alt"))
            return;
        keyboardEvent->setDefaultHandled();
        stepDown();
        return;
    }

    if (keyIdentifier == "Up") {
        keyboardEvent->setDefaultHandled();
        stepUp();
        return;
    }

    if (keyIdentifier == "U+0008" || keyIdentifier == "U+007F") {
        keyboardEvent->setDefaultHandled();
        setEmptyValue(DispatchEvent);
        return;
    }
}

void DateTimeFieldElement::didBlur()
{
    if (m_fieldOwner)
        m_fieldOwner->didBlurFromField();
}

void DateTimeFieldElement::didFocus()
{
    if (m_fieldOwner)
        m_fieldOwner->didFocusOnField();
}

void DateTimeFieldElement::focusOnNextField()
{
    if (!m_fieldOwner)
        return;
    m_fieldOwner->focusOnNextField(*this);
}

void DateTimeFieldElement::initialize(const AtomicString& pseudo, const String& axHelpText, int axMinimum, int axMaximum)
{
    // On accessibility, DateTimeFieldElement acts like spin button.
    setAttribute(roleAttr, AtomicString("spinbutton", AtomicString::ConstructFromLiteral));
    setAttribute(aria_valuetextAttr, emptyValueAXText());
    setAttribute(aria_valueminAttr, String::number(axMinimum));
    setAttribute(aria_valuemaxAttr, String::number(axMaximum));

    setAttribute(aria_helpAttr, axHelpText);
    setPseudo(pseudo);
    appendChild(Text::create(document(), visibleValue()));
}

bool DateTimeFieldElement::isDateTimeFieldElement() const
{
    return true;
}

bool DateTimeFieldElement::isFieldOwnerDisabled() const
{
    return m_fieldOwner && m_fieldOwner->isFieldOwnerDisabled();
}

bool DateTimeFieldElement::isFieldOwnerReadOnly() const
{
    return m_fieldOwner && m_fieldOwner->isFieldOwnerReadOnly();
}

bool DateTimeFieldElement::isDisabled() const
{
    return fastHasAttribute(disabledAttr);
}

Locale& DateTimeFieldElement::localeForOwner() const
{
    return document().getCachedLocale(localeIdentifier());
}

AtomicString DateTimeFieldElement::localeIdentifier() const
{
    return m_fieldOwner ? m_fieldOwner->localeIdentifier() : nullAtom;
}

float DateTimeFieldElement::maximumWidth(const Font&)
{
    const float paddingLeftAndRight = 2; // This should match to html.css.
    return paddingLeftAndRight;
}

void DateTimeFieldElement::setDisabled()
{
    // Set HTML attribute disabled to change apperance.
    setBooleanAttribute(disabledAttr, true);
    setNeedsStyleRecalc();
}

bool DateTimeFieldElement::supportsFocus() const
{
    return !isDisabled() && !isFieldOwnerDisabled();
}

void DateTimeFieldElement::updateVisibleValue(EventBehavior eventBehavior)
{
    Text* const textNode = toText(firstChild());
    const String newVisibleValue = visibleValue();
    ASSERT(newVisibleValue.length() > 0);

    if (textNode->wholeText() == newVisibleValue)
        return;

    textNode->replaceWholeText(newVisibleValue);
    if (hasValue()) {
        setAttribute(aria_valuetextAttr, newVisibleValue);
        setAttribute(aria_valuenowAttr, String::number(valueForARIAValueNow()));
    } else {
        setAttribute(aria_valuetextAttr, emptyValueAXText());
        removeAttribute(aria_valuenowAttr);
    }

    if (eventBehavior == DispatchEvent && m_fieldOwner)
        m_fieldOwner->fieldValueChanged();
}

int DateTimeFieldElement::valueForARIAValueNow() const
{
    return valueAsInteger();
}

} // namespace WebCore

#endif
