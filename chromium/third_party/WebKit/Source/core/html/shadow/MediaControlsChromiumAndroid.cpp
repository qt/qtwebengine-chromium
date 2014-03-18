/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/MediaControlsChromiumAndroid.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"

namespace WebCore {

MediaControlsChromiumAndroid::MediaControlsChromiumAndroid(Document& document)
    : MediaControlsChromium(document)
    , m_overlayPlayButton(0)
    , m_overlayEnclosure(0)
{
}

PassRefPtr<MediaControls> MediaControls::create(Document& document)
{
    return MediaControlsChromiumAndroid::createControls(document);
}

PassRefPtr<MediaControlsChromiumAndroid> MediaControlsChromiumAndroid::createControls(Document& document)
{
    if (!document.page())
        return 0;

    RefPtr<MediaControlsChromiumAndroid> controls = adoptRef(new MediaControlsChromiumAndroid(document));

    TrackExceptionState exceptionState;

    RefPtr<MediaControlOverlayEnclosureElement> overlayEnclosure = MediaControlOverlayEnclosureElement::create(document);
    RefPtr<MediaControlOverlayPlayButtonElement> overlayPlayButton = MediaControlOverlayPlayButtonElement::create(document);
    controls->m_overlayPlayButton = overlayPlayButton.get();
    overlayEnclosure->appendChild(overlayPlayButton.release(), exceptionState);
    if (exceptionState.hadException())
        return 0;

    controls->m_overlayEnclosure = overlayEnclosure.get();
    controls->appendChild(overlayEnclosure.release(), exceptionState);
    if (exceptionState.hadException())
        return 0;

    if (controls->initializeControls(document))
        return controls.release();

    return 0;
}

void MediaControlsChromiumAndroid::setMediaController(MediaControllerInterface* controller)
{
    if (m_overlayPlayButton)
        m_overlayPlayButton->setMediaController(controller);
    if (m_overlayEnclosure)
        m_overlayEnclosure->setMediaController(controller);
    MediaControlsChromium::setMediaController(controller);
}

void MediaControlsChromiumAndroid::playbackStarted()
{
    m_overlayPlayButton->updateDisplayType();
    MediaControlsChromium::playbackStarted();
}

void MediaControlsChromiumAndroid::playbackStopped()
{
    m_overlayPlayButton->updateDisplayType();
    MediaControlsChromium::playbackStopped();
}

void MediaControlsChromiumAndroid::insertTextTrackContainer(PassRefPtr<MediaControlTextTrackContainerElement> textTrackContainer)
{
    // Insert it before the overlay play button so it always displays behind it.
    m_overlayEnclosure->insertBefore(textTrackContainer, m_overlayPlayButton);
}
}
