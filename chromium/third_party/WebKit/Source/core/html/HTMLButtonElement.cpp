/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/html/HTMLButtonElement.h"

#include "HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLFormElement.h"
#include "core/rendering/RenderButton.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLButtonElement::HTMLButtonElement(Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(buttonTag, document, form)
    , m_type(SUBMIT)
    , m_isActivatedSubmit(false)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLButtonElement> HTMLButtonElement::create(Document& document, HTMLFormElement* form)
{
    return adoptRef(new HTMLButtonElement(document, form));
}

void HTMLButtonElement::setType(const AtomicString& type)
{
    setAttribute(typeAttr, type);
}

RenderObject* HTMLButtonElement::createRenderer(RenderStyle*)
{
    return new RenderButton(this);
}

const AtomicString& HTMLButtonElement::formControlType() const
{
    switch (m_type) {
        case SUBMIT: {
            DEFINE_STATIC_LOCAL(const AtomicString, submit, ("submit", AtomicString::ConstructFromLiteral));
            return submit;
        }
        case BUTTON: {
            DEFINE_STATIC_LOCAL(const AtomicString, button, ("button", AtomicString::ConstructFromLiteral));
            return button;
        }
        case RESET: {
            DEFINE_STATIC_LOCAL(const AtomicString, reset, ("reset", AtomicString::ConstructFromLiteral));
            return reset;
        }
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

bool HTMLButtonElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox and IE do, but not Opera.
        // See http://bugs.webkit.org/show_bug.cgi?id=12071
        return false;
    }

    return HTMLFormControlElement::isPresentationAttribute(name);
}

void HTMLButtonElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == typeAttr) {
        if (equalIgnoringCase(value, "reset"))
            m_type = RESET;
        else if (equalIgnoringCase(value, "button"))
            m_type = BUTTON;
        else
            m_type = SUBMIT;
        setNeedsWillValidateCheck();
    } else
        HTMLFormControlElement::parseAttribute(name, value);
}

void HTMLButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::DOMActivate && !isDisabledFormControl()) {
        if (form() && m_type == SUBMIT) {
            m_isActivatedSubmit = true;
            form()->prepareForSubmission(event);
            event->setDefaultHandled();
            m_isActivatedSubmit = false; // Do this in case submission was canceled.
        }
        if (form() && m_type == RESET) {
            form()->reset();
            event->setDefaultHandled();
        }
    }

    if (event->isKeyboardEvent()) {
        if (event->type() == EventTypeNames::keydown && toKeyboardEvent(event)->keyIdentifier() == "U+0020") {
            setActive(true);
            // No setDefaultHandled() - IE dispatches a keypress in this case.
            return;
        }
        if (event->type() == EventTypeNames::keypress) {
            switch (toKeyboardEvent(event)->charCode()) {
                case '\r':
                    dispatchSimulatedClick(event);
                    event->setDefaultHandled();
                    return;
                case ' ':
                    // Prevent scrolling down the page.
                    event->setDefaultHandled();
                    return;
            }
        }
        if (event->type() == EventTypeNames::keyup && toKeyboardEvent(event)->keyIdentifier() == "U+0020") {
            if (active())
                dispatchSimulatedClick(event);
            event->setDefaultHandled();
            return;
        }
    }

    HTMLFormControlElement::defaultEventHandler(event);
}

bool HTMLButtonElement::willRespondToMouseClickEvents()
{
    if (!isDisabledFormControl() && form() && (m_type == SUBMIT || m_type == RESET))
        return true;
    return HTMLFormControlElement::willRespondToMouseClickEvents();
}

bool HTMLButtonElement::canBeSuccessfulSubmitButton() const
{
    return m_type == SUBMIT;
}

bool HTMLButtonElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLButtonElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLButtonElement::appendFormData(FormDataList& formData, bool)
{
    if (m_type != SUBMIT || name().isEmpty() || !m_isActivatedSubmit)
        return false;
    formData.appendData(name(), value());
    return true;
}

void HTMLButtonElement::accessKeyAction(bool sendMouseEvents)
{
    focus();

    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

bool HTMLButtonElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == formactionAttr || HTMLFormControlElement::isURLAttribute(attribute);
}

const AtomicString& HTMLButtonElement::value() const
{
    return getAttribute(valueAttr);
}

bool HTMLButtonElement::recalcWillValidate() const
{
    return m_type == SUBMIT && HTMLFormControlElement::recalcWillValidate();
}

bool HTMLButtonElement::isInteractiveContent() const
{
    return true;
}

} // namespace
