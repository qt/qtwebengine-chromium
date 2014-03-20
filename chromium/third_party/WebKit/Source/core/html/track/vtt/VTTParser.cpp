/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
#include "core/html/track/vtt/VTTParser.h"

#include "core/dom/Document.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Text.h"
#include "core/html/track/vtt/VTTElement.h"
#include "platform/text/SegmentedString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

const double secondsPerHour = 3600;
const double secondsPerMinute = 60;
const double secondsPerMillisecond = 0.001;
const unsigned fileIdentifierLength = 6;

static unsigned scanDigits(const String& input, unsigned* position)
{
    unsigned startPosition = *position;
    while (*position < input.length() && isASCIIDigit(input[*position]))
        (*position)++;
    return *position - startPosition;
}

unsigned VTTParser::collectDigitsToInt(const String& input, unsigned* position, int& number)
{
    unsigned startPosition = *position;
    unsigned numDigits = scanDigits(input, position);
    if (!numDigits) {
        number = 0;
        return 0;
    }
    bool validNumber;
    if (input.is8Bit())
        number = charactersToInt(input.characters8() + startPosition, numDigits, &validNumber);
    else
        number = charactersToInt(input.characters16() + startPosition, numDigits, &validNumber);

    // Since we know that scanDigits only scanned valid (ASCII) digits (and
    // hence that's what got passed to charactersToInt()), the remaining
    // failure mode for charactersToInt() is overflow, so if |validNumber| is
    // not true, then set |number| to the maximum int value.
    if (!validNumber)
        number = std::numeric_limits<int>::max();
    return numDigits;
}

String VTTParser::collectWord(const String& input, unsigned* position)
{
    StringBuilder string;
    while (*position < input.length() && !isASpace(input[*position]))
        string.append(input[(*position)++]);
    return string.toString();
}

void VTTParser::skipWhiteSpace(const String& line, unsigned* position)
{
    while (*position < line.length() && isASpace(line[*position]))
        (*position)++;
}

bool VTTParser::parseFloatPercentageValue(const String& value, float& percentage)
{
    // '%' must be present and at the end of the setting value.
    if (value.isEmpty() || value[value.length() - 1] != '%')
        return false;

    unsigned position = 0;
    unsigned digitsBeforeDot = scanDigits(value, &position);
    unsigned digitsAfterDot = 0;
    if (value[position] == '.') {
        position++;

        digitsAfterDot = scanDigits(value, &position);
    }

    // At least one digit required.
    if (!digitsBeforeDot && !digitsAfterDot)
        return false;

    float number = value.toFloat();
    if (number < 0 || number > 100)
        return false;

    percentage = number;
    return true;
}

bool VTTParser::parseFloatPercentageValuePair(const String& value, char delimiter, FloatPoint& valuePair)
{
    // The delimiter can't be the first or second value because a pair of
    // percentages (x%,y%) implies that at least the first two characters
    // are the first percentage value.
    size_t delimiterOffset = value.find(delimiter, 2);
    if (delimiterOffset == kNotFound || delimiterOffset == value.length() - 1)
        return false;

    float firstCoord;
    if (!parseFloatPercentageValue(value.substring(0, delimiterOffset), firstCoord))
        return false;

    float secondCoord;
    if (!parseFloatPercentageValue(value.substring(delimiterOffset + 1, value.length() - 1), secondCoord))
        return false;

    valuePair = FloatPoint(firstCoord, secondCoord);
    return true;
}

VTTParser::VTTParser(VTTParserClient* client, Document& document)
    : m_document(&document)
    , m_state(Initial)
    , m_decoder(TextResourceDecoder::create("text/plain", UTF8Encoding()))
    , m_currentStartTime(0)
    , m_currentEndTime(0)
    , m_client(client)
{
}

void VTTParser::getNewCues(Vector<RefPtr<VTTCue> >& outputCues)
{
    outputCues = m_cuelist;
    m_cuelist.clear();
}

void VTTParser::getNewRegions(Vector<RefPtr<VTTRegion> >& outputRegions)
{
    outputRegions = m_regionList;
    m_regionList.clear();
}

void VTTParser::parseBytes(const char* data, unsigned length)
{
    String textData = m_decoder->decode(data, length);
    m_lineReader.append(textData);
    parse();
}

void VTTParser::flush()
{
    String textData = m_decoder->flush();
    m_lineReader.append(textData);
    m_lineReader.setEndOfStream();
    parse();
    flushPendingCue();
}

void VTTParser::parse()
{
    // WebVTT parser algorithm. (5.1 WebVTT file parsing.)
    // Steps 1 - 3 - Initial setup.

    String line;
    while (m_lineReader.getLine(line)) {
        switch (m_state) {
        case Initial:
            // Steps 4 - 9 - Check for a valid WebVTT signature.
            if (!hasRequiredFileIdentifier(line)) {
                if (m_client)
                    m_client->fileFailedToParse();
                return;
            }

            m_state = Header;
            break;

        case Header:
            // Steps 10 - 14 - Allow a header (comment area) under the WEBVTT line.
            collectMetadataHeader(line);

            if (line.isEmpty()) {
                if (m_client && m_regionList.size())
                    m_client->newRegionsParsed();

                m_state = Id;
                break;
            }

            // Step 15 - Break out of header loop if the line could be a timestamp line.
            if (line.contains("-->"))
                m_state = recoverCue(line);

            // Step 16 - Line is not the empty string and does not contain "-->".
            break;

        case Id:
            // Steps 17 - 20 - Allow any number of line terminators, then initialize new cue values.
            if (line.isEmpty())
                break;

            // Step 21 - Cue creation (start a new cue).
            resetCueValues();

            // Steps 22 - 25 - Check if this line contains an optional identifier or timing data.
            m_state = collectCueId(line);
            break;

        case TimingsAndSettings:
            // Steps 26 - 27 - Discard current cue if the line is empty.
            if (line.isEmpty()) {
                m_state = Id;
                break;
            }

            // Steps 28 - 29 - Collect cue timings and settings.
            m_state = collectTimingsAndSettings(line);
            break;

        case CueText:
            // Steps 31 - 41 - Collect the cue text, create a cue, and add it to the output.
            m_state = collectCueText(line);
            break;

        case BadCue:
            // Steps 42 - 48 - Discard lines until an empty line or a potential timing line is seen.
            m_state = ignoreBadCue(line);
            break;
        }
    }
}

void VTTParser::flushPendingCue()
{
    ASSERT(m_lineReader.isAtEndOfStream());
    // If we're in the CueText state when we run out of data, we emit the pending cue.
    if (m_state == CueText)
        createNewCue();
}

bool VTTParser::hasRequiredFileIdentifier(const String& line)
{
    // A WebVTT file identifier consists of an optional BOM character,
    // the string "WEBVTT" followed by an optional space or tab character,
    // and any number of characters that are not line terminators ...
    if (!line.startsWith("WEBVTT", fileIdentifierLength))
        return false;
    if (line.length() > fileIdentifierLength && !isASpace(line[fileIdentifierLength]))
        return false;

    return true;
}

void VTTParser::collectMetadataHeader(const String& line)
{
    // WebVTT header parsing (WebVTT parser algorithm step 12)
    DEFINE_STATIC_LOCAL(const AtomicString, regionHeaderName, ("Region", AtomicString::ConstructFromLiteral));

    // The only currently supported header is the "Region" header.
    if (!RuntimeEnabledFeatures::webVTTRegionsEnabled())
        return;

    // Step 12.4 If line contains the character ":" (A U+003A COLON), then set metadata's
    // name to the substring of line before the first ":" character and
    // metadata's value to the substring after this character.
    size_t colonPosition = line.find(':');
    if (colonPosition == kNotFound)
        return;

    String headerName = line.substring(0, colonPosition);

    // Steps 12.5 If metadata's name equals "Region":
    if (headerName == regionHeaderName) {
        String headerValue = line.substring(colonPosition + 1);
        // Steps 12.5.1 - 12.5.11 Region creation: Let region be a new text track region [...]
        createNewRegion(headerValue);
    }
}

VTTParser::ParseState VTTParser::collectCueId(const String& line)
{
    if (line.contains("-->"))
        return collectTimingsAndSettings(line);
    m_currentId = line;
    return TimingsAndSettings;
}

VTTParser::ParseState VTTParser::collectTimingsAndSettings(const String& line)
{
    // Collect WebVTT cue timings and settings. (5.3 WebVTT cue timings and settings parsing.)
    // Steps 1 - 3 - Let input be the string being parsed and position be a pointer into input.
    unsigned position = 0;
    skipWhiteSpace(line, &position);

    // Steps 4 - 5 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue start time be the collected time.
    if (!collectTimeStamp(line, &position, m_currentStartTime))
        return BadCue;
    if (position >= line.length())
        return BadCue;

    skipWhiteSpace(line, &position);

    // Steps 6 - 9 - If the next three characters are not "-->", abort and return failure.
    if (line.find("-->", position) == kNotFound)
        return BadCue;
    position += 3;
    if (position >= line.length())
        return BadCue;

    skipWhiteSpace(line, &position);

    // Steps 10 - 11 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue end time be the collected time.
    if (!collectTimeStamp(line, &position, m_currentEndTime))
        return BadCue;
    skipWhiteSpace(line, &position);

    // Step 12 - Parse the WebVTT settings for the cue (conducted in TextTrackCue).
    m_currentSettings = line.substring(position, line.length()-1);
    return CueText;
}

VTTParser::ParseState VTTParser::collectCueText(const String& line)
{
    // Step 34.
    if (line.isEmpty()) {
        createNewCue();
        return Id;
    }
    // Step 35.
    if (line.contains("-->")) {
        // Step 39-40.
        createNewCue();

        // Step 41 - New iteration of the cue loop.
        return recoverCue(line);
    }
    if (!m_currentContent.isEmpty())
        m_currentContent.append("\n");
    m_currentContent.append(line);

    return CueText;
}

VTTParser::ParseState VTTParser::recoverCue(const String& line)
{
    // Step 17 and 21.
    resetCueValues();

    // Step 22.
    return collectTimingsAndSettings(line);
}

VTTParser::ParseState VTTParser::ignoreBadCue(const String& line)
{
    if (line.isEmpty())
        return Id;
    if (line.contains("-->"))
        return recoverCue(line);
    return BadCue;
}

// A helper class for the construction of a "cue fragment" from the cue text.
class VTTTreeBuilder {
public:
    VTTTreeBuilder(Document& document)
        : m_document(document) { }

    PassRefPtr<DocumentFragment> buildFromString(const String& cueText);

private:
    void constructTreeFromToken(Document&);

    VTTToken m_token;
    RefPtr<ContainerNode> m_currentNode;
    Vector<AtomicString> m_languageStack;
    Document& m_document;
};

PassRefPtr<DocumentFragment> VTTTreeBuilder::buildFromString(const String& cueText)
{
    // Cue text processing based on
    // 5.4 WebVTT cue text parsing rules, and
    // 5.5 WebVTT cue text DOM construction rules

    RefPtr<DocumentFragment> fragment = DocumentFragment::create(m_document);

    if (cueText.isEmpty()) {
        fragment->parserAppendChild(Text::create(m_document, ""));
        return fragment;
    }

    m_currentNode = fragment;

    VTTTokenizer tokenizer(cueText);
    m_languageStack.clear();

    while (tokenizer.nextToken(m_token))
        constructTreeFromToken(m_document);

    return fragment.release();
}

PassRefPtr<DocumentFragment> VTTParser::createDocumentFragmentFromCueText(Document& document, const String& cueText)
{
    VTTTreeBuilder treeBuilder(document);
    return treeBuilder.buildFromString(cueText);
}

void VTTParser::createNewCue()
{
    RefPtr<VTTCue> cue = VTTCue::create(*m_document, m_currentStartTime, m_currentEndTime, m_currentContent.toString());
    cue->setId(m_currentId);
    cue->parseSettings(m_currentSettings);

    m_cuelist.append(cue);
    if (m_client)
        m_client->newCuesParsed();
}

void VTTParser::resetCueValues()
{
    m_currentId = emptyString();
    m_currentSettings = emptyString();
    m_currentStartTime = 0;
    m_currentEndTime = 0;
    m_currentContent.clear();
}

void VTTParser::createNewRegion(const String& headerValue)
{
    if (headerValue.isEmpty())
        return;

    // Steps 12.5.1 - 12.5.9 - Construct and initialize a WebVTT Region object.
    RefPtr<VTTRegion> region = VTTRegion::create();
    region->setRegionSettings(headerValue);

    // Step 12.5.10 If the text track list of regions regions contains a region
    // with the same region identifier value as region, remove that region.
    for (size_t i = 0; i < m_regionList.size(); ++i) {
        if (m_regionList[i]->id() == region->id()) {
            m_regionList.remove(i);
            break;
        }
    }

    // Step 12.5.11
    m_regionList.append(region);
}

bool VTTParser::collectTimeStamp(const String& line, unsigned* position, double& timeStamp)
{
    // Collect a WebVTT timestamp (5.3 WebVTT cue timings and settings parsing.)
    // Steps 1 - 4 - Initial checks, let most significant units be minutes.
    enum Mode { Minutes, Hours };
    Mode mode = Minutes;

    // Steps 5 - 7 - Collect a sequence of characters that are 0-9.
    // If not 2 characters or value is greater than 59, interpret as hours.
    int value1;
    unsigned value1Digits = collectDigitsToInt(line, position, value1);
    if (!value1Digits)
        return false;
    if (value1Digits != 2 || value1 > 59)
        mode = Hours;

    // Steps 8 - 11 - Collect the next sequence of 0-9 after ':' (must be 2 chars).
    if (*position >= line.length() || line[(*position)++] != ':')
        return false;
    int value2;
    if (collectDigitsToInt(line, position, value2) != 2)
        return false;

    // Step 12 - Detect whether this timestamp includes hours.
    int value3;
    if (mode == Hours || (*position < line.length() && line[*position] == ':')) {
        if (*position >= line.length() || line[(*position)++] != ':')
            return false;
        if (collectDigitsToInt(line, position, value3) != 2)
            return false;
    } else {
        value3 = value2;
        value2 = value1;
        value1 = 0;
    }

    // Steps 13 - 17 - Collect next sequence of 0-9 after '.' (must be 3 chars).
    if (*position >= line.length() || line[(*position)++] != '.')
        return false;
    int value4;
    if (collectDigitsToInt(line, position, value4) != 3)
        return false;
    if (value2 > 59 || value3 > 59)
        return false;

    // Steps 18 - 19 - Calculate result.
    timeStamp = (value1 * secondsPerHour) + (value2 * secondsPerMinute) + value3 + (value4 * secondsPerMillisecond);
    return true;
}

static VTTNodeType tokenToNodeType(VTTToken& token)
{
    switch (token.name().length()) {
    case 1:
        if (token.name()[0] == 'c')
            return VTTNodeTypeClass;
        if (token.name()[0] == 'v')
            return VTTNodeTypeVoice;
        if (token.name()[0] == 'b')
            return VTTNodeTypeBold;
        if (token.name()[0] == 'i')
            return VTTNodeTypeItalic;
        if (token.name()[0] == 'u')
            return VTTNodeTypeUnderline;
        break;
    case 2:
        if (token.name()[0] == 'r' && token.name()[1] == 't')
            return VTTNodeTypeRubyText;
        break;
    case 4:
        if (token.name()[0] == 'r' && token.name()[1] == 'u' && token.name()[2] == 'b' && token.name()[3] == 'y')
            return VTTNodeTypeRuby;
        if (token.name()[0] == 'l' && token.name()[1] == 'a' && token.name()[2] == 'n' && token.name()[3] == 'g')
            return VTTNodeTypeLanguage;
        break;
    }
    return VTTNodeTypeNone;
}

void VTTTreeBuilder::constructTreeFromToken(Document& document)
{
    // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules

    switch (m_token.type()) {
    case VTTTokenTypes::Character: {
        RefPtr<Text> child = Text::create(document, m_token.characters());
        m_currentNode->parserAppendChild(child);
        break;
    }
    case VTTTokenTypes::StartTag: {
        VTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType == VTTNodeTypeNone)
            break;

        VTTNodeType currentType = m_currentNode->isVTTElement() ? toVTTElement(m_currentNode.get())->webVTTNodeType() : VTTNodeTypeNone;
        // <rt> is only allowed if the current node is <ruby>.
        if (nodeType == VTTNodeTypeRubyText && currentType != VTTNodeTypeRuby)
            break;

        RefPtr<VTTElement> child = VTTElement::create(nodeType, &document);
        if (!m_token.classes().isEmpty())
            child->setAttribute(classAttr, m_token.classes());

        if (nodeType == VTTNodeTypeVoice) {
            child->setAttribute(VTTElement::voiceAttributeName(), m_token.annotation());
        } else if (nodeType == VTTNodeTypeLanguage) {
            m_languageStack.append(m_token.annotation());
            child->setAttribute(VTTElement::langAttributeName(), m_languageStack.last());
        }
        if (!m_languageStack.isEmpty())
            child->setLanguage(m_languageStack.last());
        m_currentNode->parserAppendChild(child);
        m_currentNode = child;
        break;
    }
    case VTTTokenTypes::EndTag: {
        VTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType == VTTNodeTypeNone)
            break;

        // The only non-VTTElement would be the DocumentFragment root. (Text
        // nodes and PIs will never appear as m_currentNode.)
        if (!m_currentNode->isVTTElement())
            break;

        VTTNodeType currentType = toVTTElement(m_currentNode.get())->webVTTNodeType();
        bool matchesCurrent = nodeType == currentType;
        if (!matchesCurrent) {
            // </ruby> auto-closes <rt>.
            if (currentType == VTTNodeTypeRubyText && nodeType == VTTNodeTypeRuby) {
                if (m_currentNode->parentNode())
                    m_currentNode = m_currentNode->parentNode();
            } else {
                break;
            }
        }
        if (nodeType == VTTNodeTypeLanguage)
            m_languageStack.removeLast();
        if (m_currentNode->parentNode())
            m_currentNode = m_currentNode->parentNode();
        break;
    }
    case VTTTokenTypes::TimestampTag: {
        unsigned position = 0;
        String charactersString = m_token.characters();
        double parsedTimeStamp;
        if (VTTParser::collectTimeStamp(charactersString, &position, parsedTimeStamp))
            m_currentNode->parserAppendChild(ProcessingInstruction::create(document, "timestamp", charactersString));
        break;
    }
    default:
        break;
    }
}

}

