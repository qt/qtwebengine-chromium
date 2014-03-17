/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "core/dom/DecodedDataDocumentParser.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentEncodingData.h"
#include "core/fetch/TextResourceDecoder.h"

namespace WebCore {

DecodedDataDocumentParser::DecodedDataDocumentParser(Document* document)
    : DocumentParser(document)
    , m_hasAppendedData(false)
{
}

DecodedDataDocumentParser::~DecodedDataDocumentParser()
{
}

void DecodedDataDocumentParser::setDecoder(PassOwnPtr<TextResourceDecoder> decoder)
{
    m_decoder = decoder;
}

TextResourceDecoder* DecodedDataDocumentParser::decoder()
{
    return m_decoder.get();
}

void DecodedDataDocumentParser::setHasAppendedData()
{
    m_hasAppendedData = true;
}

void DecodedDataDocumentParser::appendBytes(const char* data, size_t length)
{
    if (!length)
        return;

    // This should be checking isStopped(), but XMLDocumentParser prematurely
    // stops parsing when handling an XSLT processing instruction and still
    // needs to receive decoded bytes.
    if (isDetached())
        return;

    String decoded = m_decoder->decode(data, length);
    updateDocument(decoded);
}

void DecodedDataDocumentParser::flush()
{
    // This should be checking isStopped(), but XMLDocumentParser prematurely
    // stops parsing when handling an XSLT processing instruction and still
    // needs to receive decoded bytes.
    if (isDetached())
        return;

    // null decoder indicates there is no data received.
    // We have nothing to do in that case.
    if (!m_decoder)
        return;

    String remainingData = m_decoder->flush();
    updateDocument(remainingData);
}

void DecodedDataDocumentParser::updateDocument(String& decodedData)
{
    DocumentEncodingData encodingData;
    encodingData.encoding = m_decoder->encoding();
    encodingData.wasDetectedHeuristically = m_decoder->encodingWasDetectedHeuristically();
    encodingData.sawDecodingError = m_decoder->sawError();
    document()->setEncodingData(encodingData);

    if (decodedData.isEmpty())
        return;

    append(decodedData.releaseImpl());
    // FIXME: Should be removed as part of https://code.google.com/p/chromium/issues/detail?id=319643
    if (!m_hasAppendedData) {
        m_hasAppendedData = true;
        if (m_decoder->encoding().usesVisualOrdering())
            document()->setVisuallyOrdered();
    }
}

};
