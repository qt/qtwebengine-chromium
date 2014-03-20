/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/html/shadow/MediaControlElementTypes.h"

#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/css/StylePropertySet.h"
#include "core/events/MouseEvent.h"

namespace WebCore {

using namespace HTMLNames;

class Event;

HTMLMediaElement* toParentMediaElement(Node* node)
{
    if (!node)
        return 0;
    Node* mediaNode = node->shadowHost();
    if (!mediaNode)
        mediaNode = node;
    if (!mediaNode || !mediaNode->isElementNode() || !toElement(mediaNode)->isMediaElement())
        return 0;

    return toHTMLMediaElement(mediaNode);
}

MediaControlElementType mediaControlElementType(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(node->isMediaControlElement());
    HTMLElement* element = toHTMLElement(node);
    if (element->hasTagName(inputTag))
        return static_cast<MediaControlInputElement*>(element)->displayType();
    return static_cast<MediaControlDivElement*>(element)->displayType();
}

MediaControlElement::MediaControlElement(MediaControlElementType displayType, HTMLElement* element)
    : m_mediaController(0)
    , m_displayType(displayType)
    , m_element(element)
{
}

void MediaControlElement::hide()
{
    m_element->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

void MediaControlElement::show()
{
    m_element->removeInlineStyleProperty(CSSPropertyDisplay);
}

bool MediaControlElement::isShowing() const
{
    const StylePropertySet* propertySet = m_element->inlineStyle();
    // Following the code from show() and hide() above, we only have
    // to check for the presense of inline display.
    return (!propertySet || !propertySet->getPropertyCSSValue(CSSPropertyDisplay));
}

void MediaControlElement::setDisplayType(MediaControlElementType displayType)
{
    if (displayType == m_displayType)
        return;

    m_displayType = displayType;
    if (RenderObject* object = m_element->renderer())
        object->repaint();
}

// ----------------------------

MediaControlDivElement::MediaControlDivElement(Document& document, MediaControlElementType displayType)
    : HTMLDivElement(document)
    , MediaControlElement(displayType, this)
{
}

// ----------------------------

MediaControlInputElement::MediaControlInputElement(Document& document, MediaControlElementType displayType)
    : HTMLInputElement(document, 0, false)
    , MediaControlElement(displayType, this)
{
}

bool MediaControlInputElement::isMouseFocusable() const
{
    return false;
}

// ----------------------------

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(Document& document, MediaControlElementType displayType)
    : MediaControlDivElement(document, displayType)
    , m_currentValue(0)
{
}

void MediaControlTimeDisplayElement::setCurrentValue(double time)
{
    m_currentValue = time;
}

// ----------------------------

MediaControlMuteButtonElement::MediaControlMuteButtonElement(Document& document, MediaControlElementType displayType)
    : MediaControlInputElement(document, displayType)
{
}

void MediaControlMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        mediaController()->setMuted(!mediaController()->muted());
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlMuteButtonElement::changedMute()
{
    updateDisplayType();
}

void MediaControlMuteButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->muted() ? MediaUnMuteButton : MediaMuteButton);
}

// ----------------------------

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(Document& document)
    : MediaControlInputElement(document, MediaVolumeSlider)
    , m_clearMutedOnUserInteraction(false)
{
}

void MediaControlVolumeSliderElement::defaultEventHandler(Event* event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (event->isMouseEvent() && toMouseEvent(event)->button())
        return;

    if (!inDocument() || !document().isActive())
        return;

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == EventTypeNames::mouseover || event->type() == EventTypeNames::mouseout || event->type() == EventTypeNames::mousemove)
        return;

    double volume = value().toDouble();
    if (volume != mediaController()->volume())
        mediaController()->setVolume(volume, ASSERT_NO_EXCEPTION);
    if (m_clearMutedOnUserInteraction)
        mediaController()->setMuted(false);
}

bool MediaControlVolumeSliderElement::willRespondToMouseMoveEvents()
{
    if (!inDocument() || !document().isActive())
        return false;

    return MediaControlInputElement::willRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::willRespondToMouseClickEvents()
{
    if (!inDocument() || !document().isActive())
        return false;

    return MediaControlInputElement::willRespondToMouseClickEvents();
}

void MediaControlVolumeSliderElement::setVolume(double volume)
{
    if (value().toDouble() != volume)
        setValue(String::number(volume));
}

void MediaControlVolumeSliderElement::setClearMutedOnUserInteraction(bool clearMute)
{
    m_clearMutedOnUserInteraction = clearMute;
}

} // namespace WebCore
