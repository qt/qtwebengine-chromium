/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2013 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/svg/SVGParserUtilities.h"

#include "core/dom/Document.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGTransformList.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/ASCIICType.h"
#include <limits>

namespace WebCore {

template <typename FloatType>
static inline bool isValidRange(const FloatType& x)
{
    static const FloatType max = std::numeric_limits<FloatType>::max();
    return x >= -max && x <= max;
}

// We use this generic parseNumber function to allow the Path parsing code to work
// at a higher precision internally, without any unnecessary runtime cost or code
// complexity.
template <typename CharType, typename FloatType>
static bool genericParseNumber(const CharType*& ptr, const CharType* end, FloatType& number, bool skip)
{
    FloatType integer, decimal, frac, exponent;
    int sign, expsign;
    const CharType* start = ptr;

    exponent = 0;
    integer = 0;
    frac = 1;
    decimal = 0;
    sign = 1;
    expsign = 1;

    // read the sign
    if (ptr < end && *ptr == '+')
        ptr++;
    else if (ptr < end && *ptr == '-') {
        ptr++;
        sign = -1;
    }

    if (ptr == end || ((*ptr < '0' || *ptr > '9') && *ptr != '.'))
        // The first character of a number must be one of [0-9+-.]
        return false;

    // read the integer part, build right-to-left
    const CharType* ptrStartIntPart = ptr;
    while (ptr < end && *ptr >= '0' && *ptr <= '9')
        ++ptr; // Advance to first non-digit.

    if (ptr != ptrStartIntPart) {
        const CharType* ptrScanIntPart = ptr - 1;
        FloatType multiplier = 1;
        while (ptrScanIntPart >= ptrStartIntPart) {
            integer += multiplier * static_cast<FloatType>(*(ptrScanIntPart--) - '0');
            multiplier *= 10;
        }
        // Bail out early if this overflows.
        if (!isValidRange(integer))
            return false;
    }

    if (ptr < end && *ptr == '.') { // read the decimals
        ptr++;

        // There must be a least one digit following the .
        if (ptr >= end || *ptr < '0' || *ptr > '9')
            return false;

        while (ptr < end && *ptr >= '0' && *ptr <= '9')
            decimal += (*(ptr++) - '0') * (frac *= static_cast<FloatType>(0.1));
    }

    // read the exponent part
    if (ptr != start && ptr + 1 < end && (*ptr == 'e' || *ptr == 'E')
        && (ptr[1] != 'x' && ptr[1] != 'm')) {
        ptr++;

        // read the sign of the exponent
        if (*ptr == '+')
            ptr++;
        else if (*ptr == '-') {
            ptr++;
            expsign = -1;
        }

        // There must be an exponent
        if (ptr >= end || *ptr < '0' || *ptr > '9')
            return false;

        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            exponent *= static_cast<FloatType>(10);
            exponent += *ptr - '0';
            ptr++;
        }
        // Make sure exponent is valid.
        if (!isValidRange(exponent) || exponent > std::numeric_limits<FloatType>::max_exponent)
            return false;
    }

    number = integer + decimal;
    number *= sign;

    if (exponent)
        number *= static_cast<FloatType>(pow(10.0, expsign * static_cast<int>(exponent)));

    // Don't return Infinity() or NaN().
    if (!isValidRange(number))
        return false;

    if (start == ptr)
        return false;

    if (skip)
        skipOptionalSVGSpacesOrDelimiter(ptr, end);

    return true;
}

template <typename CharType>
bool parseSVGNumber(CharType* begin, size_t length, double& number)
{
    const CharType* ptr = begin;
    const CharType* end = ptr + length;
    return genericParseNumber(ptr, end, number, false);
}

// Explicitly instantiate the two flavors of parseSVGNumber() to satisfy external callers
template bool parseSVGNumber(LChar* begin, size_t length, double&);
template bool parseSVGNumber(UChar* begin, size_t length, double&);

bool parseNumber(const LChar*& ptr, const LChar* end, float& number, bool skip)
{
    return genericParseNumber(ptr, end, number, skip);
}

bool parseNumber(const UChar*& ptr, const UChar* end, float& number, bool skip)
{
    return genericParseNumber(ptr, end, number, skip);
}

bool parseNumberFromString(const String& string, float& number, bool skip)
{
    if (string.isEmpty())
        return false;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return genericParseNumber(ptr, end, number, skip) && ptr == end;
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return genericParseNumber(ptr, end, number, skip) && ptr == end;
}

// only used to parse largeArcFlag and sweepFlag which must be a "0" or "1"
// and might not have any whitespace/comma after it
template <typename CharType>
bool genericParseArcFlag(const CharType*& ptr, const CharType* end, bool& flag)
{
    if (ptr >= end)
        return false;
    const CharType flagChar = *ptr++;
    if (flagChar == '0')
        flag = false;
    else if (flagChar == '1')
        flag = true;
    else
        return false;

    skipOptionalSVGSpacesOrDelimiter(ptr, end);

    return true;
}

bool parseArcFlag(const LChar*& ptr, const LChar* end, bool& flag)
{
    return genericParseArcFlag(ptr, end, flag);
}

bool parseArcFlag(const UChar*& ptr, const UChar* end, bool& flag)
{
    return genericParseArcFlag(ptr, end, flag);
}

template<typename CharType>
static bool genericParseNumberOptionalNumber(const CharType*& ptr, const CharType* end, float& x, float& y)
{
    if (!parseNumber(ptr, end, x))
        return false;

    if (ptr == end)
        y = x;
    else if (!parseNumber(ptr, end, y, false))
        return false;

    return ptr == end;
}

bool parseNumberOptionalNumber(const String& string, float& x, float& y)
{
    if (string.isEmpty())
        return false;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return genericParseNumberOptionalNumber(ptr, end, x, y);
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return genericParseNumberOptionalNumber(ptr, end, x, y);
}

template<typename CharType>
static bool genericParseRect(const CharType*& ptr, const CharType* end, FloatRect& rect)
{
    skipOptionalSVGSpaces(ptr, end);

    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    bool valid = parseNumber(ptr, end, x) && parseNumber(ptr, end, y) && parseNumber(ptr, end, width) && parseNumber(ptr, end, height, false);
    rect = FloatRect(x, y, width, height);
    return valid;
}

bool parseRect(const String& string, FloatRect& rect)
{
    if (string.isEmpty())
        return false;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return genericParseRect(ptr, end, rect);
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return genericParseRect(ptr, end, rect);
}

template<typename CharType>
static bool genericParsePointsList(SVGPointList& pointsList, const CharType*& ptr, const CharType* end)
{
    skipOptionalSVGSpaces(ptr, end);

    bool delimParsed = false;
    while (ptr < end) {
        delimParsed = false;
        float xPos = 0.0f;
        if (!parseNumber(ptr, end, xPos))
           return false;

        float yPos = 0.0f;
        if (!parseNumber(ptr, end, yPos, false))
            return false;

        skipOptionalSVGSpaces(ptr, end);

        if (ptr < end && *ptr == ',') {
            delimParsed = true;
            ptr++;
        }
        skipOptionalSVGSpaces(ptr, end);

        pointsList.append(FloatPoint(xPos, yPos));
    }
    return ptr == end && !delimParsed;
}

// FIXME: Why is the out parameter first?
bool pointsListFromSVGData(SVGPointList& pointsList, const String& points)
{
    if (points.isEmpty())
        return true;
    if (points.is8Bit()) {
        const LChar* ptr = points.characters8();
        const LChar* end = ptr + points.length();
        return genericParsePointsList(pointsList, ptr, end);
    }
    const UChar* ptr = points.characters16();
    const UChar* end = ptr + points.length();
    return genericParsePointsList(pointsList, ptr, end);
}

template<typename CharType>
static bool parseGlyphName(const CharType*& ptr, const CharType* end, HashSet<String>& values)
{
    skipOptionalSVGSpaces(ptr, end);

    while (ptr < end) {
        // Leading and trailing white space, and white space before and after separators, will be ignored.
        const CharType* inputStart = ptr;
        while (ptr < end && *ptr != ',')
            ++ptr;

        if (ptr == inputStart)
            break;

        // walk backwards from the ; to ignore any whitespace
        const CharType* inputEnd = ptr - 1;
        while (inputStart < inputEnd && isSVGSpace(*inputEnd))
            --inputEnd;

        values.add(String(inputStart, inputEnd - inputStart + 1));
        skipOptionalSVGSpacesOrDelimiter(ptr, end, ',');
    }

    return true;
}

bool parseGlyphName(const String& input, HashSet<String>& values)
{
    // FIXME: Parsing error detection is missing.
    values.clear();
    if (input.isEmpty())
        return true;
    if (input.is8Bit()) {
        const LChar* ptr = input.characters8();
        const LChar* end = ptr + input.length();
        return parseGlyphName(ptr, end, values);
    }
    const UChar* ptr = input.characters16();
    const UChar* end = ptr + input.length();
    return parseGlyphName(ptr, end, values);
}

template<typename CharType>
static bool parseUnicodeRange(const CharType* characters, unsigned length, UnicodeRange& range)
{
    if (length < 2 || characters[0] != 'U' || characters[1] != '+')
        return false;

    // Parse the starting hex number (or its prefix).
    unsigned startRange = 0;
    unsigned startLength = 0;

    const CharType* ptr = characters + 2;
    const CharType* end = characters + length;
    while (ptr < end) {
        if (!isASCIIHexDigit(*ptr))
            break;
        ++startLength;
        if (startLength > 6)
            return false;
        startRange = (startRange << 4) | toASCIIHexValue(*ptr);
        ++ptr;
    }

    // Handle the case of ranges separated by "-" sign.
    if (2 + startLength < length && *ptr == '-') {
        if (!startLength)
            return false;

        // Parse the ending hex number (or its prefix).
        unsigned endRange = 0;
        unsigned endLength = 0;
        ++ptr;
        while (ptr < end) {
            if (!isASCIIHexDigit(*ptr))
                break;
            ++endLength;
            if (endLength > 6)
                return false;
            endRange = (endRange << 4) | toASCIIHexValue(*ptr);
            ++ptr;
        }

        if (!endLength)
            return false;

        range.first = startRange;
        range.second = endRange;
        return true;
    }

    // Handle the case of a number with some optional trailing question marks.
    unsigned endRange = startRange;
    while (ptr < end) {
        if (*ptr != '?')
            break;
        ++startLength;
        if (startLength > 6)
            return false;
        startRange <<= 4;
        endRange = (endRange << 4) | 0xF;
        ++ptr;
    }

    if (!startLength)
        return false;

    range.first = startRange;
    range.second = endRange;
    return true;
}

template<typename CharType>
static bool genericParseKerningUnicodeString(const CharType*& ptr, const CharType* end, UnicodeRanges& rangeList, HashSet<String>& stringList)
{
    while (ptr < end) {
        const CharType* inputStart = ptr;
        while (ptr < end && *ptr != ',')
            ++ptr;

        if (ptr == inputStart)
            break;

        // Try to parse unicode range first
        UnicodeRange range;
        if (parseUnicodeRange(inputStart, ptr - inputStart, range))
            rangeList.append(range);
        else
            stringList.add(String(inputStart, ptr - inputStart));
        ++ptr;
    }

    return true;
}

bool parseKerningUnicodeString(const String& input, UnicodeRanges& rangeList, HashSet<String>& stringList)
{
    // FIXME: Parsing error detection is missing.
    if (input.isEmpty())
        return true;
    if (input.is8Bit()) {
        const LChar* ptr = input.characters8();
        const LChar* end = ptr + input.length();
        return genericParseKerningUnicodeString(ptr, end, rangeList, stringList);
    }
    const UChar* ptr = input.characters16();
    const UChar* end = ptr + input.length();
    return genericParseKerningUnicodeString(ptr, end, rangeList, stringList);
}

template<typename CharType>
static Vector<String> genericParseDelimitedString(const CharType*& ptr, const CharType* end, const char seperator)
{
    Vector<String> values;

    skipOptionalSVGSpaces(ptr, end);

    while (ptr < end) {
        // Leading and trailing white space, and white space before and after semicolon separators, will be ignored.
        const CharType* inputStart = ptr;
        while (ptr < end && *ptr != seperator) // careful not to ignore whitespace inside inputs
            ptr++;

        if (ptr == inputStart)
            break;

        // walk backwards from the ; to ignore any whitespace
        const CharType* inputEnd = ptr - 1;
        while (inputStart < inputEnd && isSVGSpace(*inputEnd))
            inputEnd--;

        values.append(String(inputStart, inputEnd - inputStart + 1));
        skipOptionalSVGSpacesOrDelimiter(ptr, end, seperator);
    }

    return values;
}

Vector<String> parseDelimitedString(const String& input, const char seperator)
{
    if (input.isEmpty())
        return Vector<String>();
    if (input.is8Bit()) {
        const LChar* ptr = input.characters8();
        const LChar* end = ptr + input.length();
        return genericParseDelimitedString(ptr, end, seperator);
    }
    const UChar* ptr = input.characters16();
    const UChar* end = ptr + input.length();
    return genericParseDelimitedString(ptr, end, seperator);
}

template <typename CharType>
bool parseFloatPoint(const CharType*& current, const CharType* end, FloatPoint& point)
{
    float x;
    float y;
    if (!parseNumber(current, end, x)
        || !parseNumber(current, end, y))
        return false;
    point = FloatPoint(x, y);
    return true;
}

template bool parseFloatPoint(const LChar*& current, const LChar* end, FloatPoint& point1);
template bool parseFloatPoint(const UChar*& current, const UChar* end, FloatPoint& point1);

template <typename CharType>
inline bool parseFloatPoint2(const CharType*& current, const CharType* end, FloatPoint& point1, FloatPoint& point2)
{
    float x1;
    float y1;
    float x2;
    float y2;
    if (!parseNumber(current, end, x1)
        || !parseNumber(current, end, y1)
        || !parseNumber(current, end, x2)
        || !parseNumber(current, end, y2))
        return false;
    point1 = FloatPoint(x1, y1);
    point2 = FloatPoint(x2, y2);
    return true;
}

template bool parseFloatPoint2(const LChar*& current, const LChar* end, FloatPoint& point1, FloatPoint& point2);
template bool parseFloatPoint2(const UChar*& current, const UChar* end, FloatPoint& point1, FloatPoint& point2);

template <typename CharType>
bool parseFloatPoint3(const CharType*& current, const CharType* end, FloatPoint& point1, FloatPoint& point2, FloatPoint& point3)
{
    float x1;
    float y1;
    float x2;
    float y2;
    float x3;
    float y3;
    if (!parseNumber(current, end, x1)
        || !parseNumber(current, end, y1)
        || !parseNumber(current, end, x2)
        || !parseNumber(current, end, y2)
        || !parseNumber(current, end, x3)
        || !parseNumber(current, end, y3))
        return false;
    point1 = FloatPoint(x1, y1);
    point2 = FloatPoint(x2, y2);
    point3 = FloatPoint(x3, y3);
    return true;
}

template bool parseFloatPoint3(const LChar*& current, const LChar* end, FloatPoint& point1, FloatPoint& point2, FloatPoint& point3);
template bool parseFloatPoint3(const UChar*& current, const UChar* end, FloatPoint& point1, FloatPoint& point2, FloatPoint& point3);

template<typename CharType>
static int parseTransformParamList(const CharType*& ptr, const CharType* end, float* values, int required, int optional)
{
    int optionalParams = 0, requiredParams = 0;

    if (!skipOptionalSVGSpaces(ptr, end) || *ptr != '(')
        return -1;

    ptr++;

    skipOptionalSVGSpaces(ptr, end);

    while (requiredParams < required) {
        if (ptr >= end || !parseNumber(ptr, end, values[requiredParams], false))
            return -1;
        requiredParams++;
        if (requiredParams < required)
            skipOptionalSVGSpacesOrDelimiter(ptr, end);
    }
    if (!skipOptionalSVGSpaces(ptr, end))
        return -1;

    bool delimParsed = skipOptionalSVGSpacesOrDelimiter(ptr, end);

    if (ptr >= end)
        return -1;

    if (*ptr == ')') { // skip optionals
        ptr++;
        if (delimParsed)
            return -1;
    } else {
        while (optionalParams < optional) {
            if (ptr >= end || !parseNumber(ptr, end, values[requiredParams + optionalParams], false))
                return -1;
            optionalParams++;
            if (optionalParams < optional)
                skipOptionalSVGSpacesOrDelimiter(ptr, end);
        }

        if (!skipOptionalSVGSpaces(ptr, end))
            return -1;

        delimParsed = skipOptionalSVGSpacesOrDelimiter(ptr, end);

        if (ptr >= end || *ptr != ')' || delimParsed)
            return -1;
        ptr++;
    }

    return requiredParams + optionalParams;
}

// These should be kept in sync with enum SVGTransformType
static const int requiredValuesForType[] =  {0, 6, 1, 1, 1, 1, 1};
static const int optionalValuesForType[] =  {0, 0, 1, 1, 2, 0, 0};

template<typename CharType>
static bool parseTransformValueInternal(unsigned type, const CharType*& ptr, const CharType* end, SVGTransform& transform)
{
    if (type == SVGTransform::SVG_TRANSFORM_UNKNOWN)
        return false;

    int valueCount = 0;
    float values[] = {0, 0, 0, 0, 0, 0};
    if ((valueCount = parseTransformParamList(ptr, end, values, requiredValuesForType[type], optionalValuesForType[type])) < 0)
        return false;

    switch (type) {
    case SVGTransform::SVG_TRANSFORM_SKEWX:
        transform.setSkewX(values[0]);
        break;
    case SVGTransform::SVG_TRANSFORM_SKEWY:
        transform.setSkewY(values[0]);
        break;
    case SVGTransform::SVG_TRANSFORM_SCALE:
        if (valueCount == 1) // Spec: if only one param given, assume uniform scaling
            transform.setScale(values[0], values[0]);
        else
            transform.setScale(values[0], values[1]);
        break;
    case SVGTransform::SVG_TRANSFORM_TRANSLATE:
        if (valueCount == 1) // Spec: if only one param given, assume 2nd param to be 0
            transform.setTranslate(values[0], 0);
        else
            transform.setTranslate(values[0], values[1]);
        break;
    case SVGTransform::SVG_TRANSFORM_ROTATE:
        if (valueCount == 1)
            transform.setRotate(values[0], 0, 0);
        else
            transform.setRotate(values[0], values[1], values[2]);
        break;
    case SVGTransform::SVG_TRANSFORM_MATRIX:
        transform.setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        break;
    }

    return true;
}

bool parseTransformValue(unsigned type, const LChar*& ptr, const LChar* end, SVGTransform& transform)
{
    return parseTransformValueInternal(type, ptr, end, transform);
}

bool parseTransformValue(unsigned type, const UChar*& ptr, const UChar* end, SVGTransform& transform)
{
    return parseTransformValueInternal(type, ptr, end, transform);
}

static const LChar skewXDesc[] =  {'s', 'k', 'e', 'w', 'X'};
static const LChar skewYDesc[] =  {'s', 'k', 'e', 'w', 'Y'};
static const LChar scaleDesc[] =  {'s', 'c', 'a', 'l', 'e'};
static const LChar translateDesc[] =  {'t', 'r', 'a', 'n', 's', 'l', 'a', 't', 'e'};
static const LChar rotateDesc[] =  {'r', 'o', 't', 'a', 't', 'e'};
static const LChar matrixDesc[] =  {'m', 'a', 't', 'r', 'i', 'x'};

template<typename CharType>
static inline bool parseAndSkipType(const CharType*& ptr, const CharType* end, unsigned short& type)
{
    if (ptr >= end)
        return false;

    if (*ptr == 's') {
        if (skipString(ptr, end, skewXDesc, WTF_ARRAY_LENGTH(skewXDesc)))
            type = SVGTransform::SVG_TRANSFORM_SKEWX;
        else if (skipString(ptr, end, skewYDesc, WTF_ARRAY_LENGTH(skewYDesc)))
            type = SVGTransform::SVG_TRANSFORM_SKEWY;
        else if (skipString(ptr, end, scaleDesc, WTF_ARRAY_LENGTH(scaleDesc)))
            type = SVGTransform::SVG_TRANSFORM_SCALE;
        else
            return false;
    } else if (skipString(ptr, end, translateDesc, WTF_ARRAY_LENGTH(translateDesc)))
        type = SVGTransform::SVG_TRANSFORM_TRANSLATE;
    else if (skipString(ptr, end, rotateDesc, WTF_ARRAY_LENGTH(rotateDesc)))
        type = SVGTransform::SVG_TRANSFORM_ROTATE;
    else if (skipString(ptr, end, matrixDesc, WTF_ARRAY_LENGTH(matrixDesc)))
        type = SVGTransform::SVG_TRANSFORM_MATRIX;
    else
        return false;

    return true;
}

SVGTransform::SVGTransformType parseTransformType(const String& string)
{
    if (string.isEmpty())
        return SVGTransform::SVG_TRANSFORM_UNKNOWN;
    unsigned short type = SVGTransform::SVG_TRANSFORM_UNKNOWN;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        parseAndSkipType(ptr, end, type);
    } else {
        const UChar* ptr = string.characters16();
        const UChar* end = ptr + string.length();
        parseAndSkipType(ptr, end, type);
    }
    return static_cast<SVGTransform::SVGTransformType>(type);
}

template<typename CharType>
bool parseTransformAttributeInternal(SVGTransformList& list, const CharType*& ptr, const CharType* end, TransformParsingMode mode)
{
    if (mode == ClearList)
        list.clear();

    bool delimParsed = false;
    while (ptr < end) {
        delimParsed = false;
        unsigned short type = SVGTransform::SVG_TRANSFORM_UNKNOWN;
        skipOptionalSVGSpaces(ptr, end);

        if (!parseAndSkipType(ptr, end, type))
            return false;

        SVGTransform transform;
        if (!parseTransformValue(type, ptr, end, transform))
            return false;

        list.append(transform);
        skipOptionalSVGSpaces(ptr, end);
        if (ptr < end && *ptr == ',') {
            delimParsed = true;
            ++ptr;
        }
        skipOptionalSVGSpaces(ptr, end);
    }

    return !delimParsed;
}

bool parseTransformAttribute(SVGTransformList& list, const LChar*& ptr, const LChar* end, TransformParsingMode mode)
{
    return parseTransformAttributeInternal(list, ptr, end, mode);
}

bool parseTransformAttribute(SVGTransformList& list, const UChar*& ptr, const UChar* end, TransformParsingMode mode)
{
    return parseTransformAttributeInternal(list, ptr, end, mode);
}

}
