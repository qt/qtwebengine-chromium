/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "platform/JSONValues.h"

#include "wtf/DecimalNumber.h"
#include "wtf/dtoa.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

namespace {

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

inline bool escapeChar(UChar c, StringBuilder* dst)
{
    switch (c) {
    case '\b': dst->append("\\b", 2); break;
    case '\f': dst->append("\\f", 2); break;
    case '\n': dst->append("\\n", 2); break;
    case '\r': dst->append("\\r", 2); break;
    case '\t': dst->append("\\t", 2); break;
    case '\\': dst->append("\\\\", 2); break;
    case '"': dst->append("\\\"", 2); break;
    default:
        return false;
    }
    return true;
}

inline void doubleQuoteString(const String& str, StringBuilder* dst)
{
    dst->append('"');
    for (unsigned i = 0; i < str.length(); ++i) {
        UChar c = str[i];
        if (!escapeChar(c, dst)) {
            if (c < 32 || c > 126 || c == '<' || c == '>') {
                // 1. Escaping <, > to prevent script execution.
                // 2. Technically, we could also pass through c > 126 as UTF8, but this
                //    is also optional. It would also be a pain to implement here.
                unsigned symbol = static_cast<unsigned>(c);
                String symbolCode = String::format("\\u%04X", symbol);
                dst->append(symbolCode);
            } else {
                dst->append(c);
            }
        }
    }
    dst->append('"');
}

} // anonymous namespace

bool JSONValue::asBoolean(bool*) const
{
    return false;
}

bool JSONValue::asNumber(double*) const
{
    return false;
}

bool JSONValue::asNumber(long*) const
{
    return false;
}

bool JSONValue::asNumber(int*) const
{
    return false;
}

bool JSONValue::asNumber(unsigned long*) const
{
    return false;
}

bool JSONValue::asNumber(unsigned*) const
{
    return false;
}

bool JSONValue::asString(String*) const
{
    return false;
}

bool JSONValue::asValue(RefPtr<JSONValue>* output)
{
    *output = this;
    return true;
}

bool JSONValue::asObject(RefPtr<JSONObject>*)
{
    return false;
}

bool JSONValue::asArray(RefPtr<JSONArray>*)
{
    return false;
}

PassRefPtr<JSONObject> JSONValue::asObject()
{
    return 0;
}

PassRefPtr<JSONArray> JSONValue::asArray()
{
    return 0;
}

String JSONValue::toJSONString() const
{
    StringBuilder result;
    result.reserveCapacity(512);
    writeJSON(&result);
    return result.toString();
}

void JSONValue::writeJSON(StringBuilder* output) const
{
    ASSERT(m_type == TypeNull);
    output->append(nullString, 4);
}

bool JSONBasicValue::asBoolean(bool* output) const
{
    if (type() != TypeBoolean)
        return false;
    *output = m_boolValue;
    return true;
}

bool JSONBasicValue::asNumber(double* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = m_doubleValue;
    return true;
}

bool JSONBasicValue::asNumber(long* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<long>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(int* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<int>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(unsigned long* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<unsigned long>(m_doubleValue);
    return true;
}

bool JSONBasicValue::asNumber(unsigned* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<unsigned>(m_doubleValue);
    return true;
}

void JSONBasicValue::writeJSON(StringBuilder* output) const
{
    ASSERT(type() == TypeBoolean || type() == TypeNumber);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            output->append(trueString, 4);
        else
            output->append(falseString, 5);
    } else if (type() == TypeNumber) {
        NumberToLStringBuffer buffer;
        if (!std::isfinite(m_doubleValue)) {
            output->append(nullString, 4);
            return;
        }
        DecimalNumber decimal = m_doubleValue;
        unsigned length = 0;
        if (decimal.bufferLengthForStringDecimal() > WTF::NumberToStringBufferLength) {
            // Not enough room for decimal. Use exponential format.
            if (decimal.bufferLengthForStringExponential() > WTF::NumberToStringBufferLength) {
                // Fallback for an abnormal case if it's too little even for exponential.
                output->append("NaN", 3);
                return;
            }
            length = decimal.toStringExponential(buffer, WTF::NumberToStringBufferLength);
        } else {
            length = decimal.toStringDecimal(buffer, WTF::NumberToStringBufferLength);
        }
        output->append(buffer, length);
    }
}

bool JSONString::asString(String* output) const
{
    *output = m_stringValue;
    return true;
}

void JSONString::writeJSON(StringBuilder* output) const
{
    ASSERT(type() == TypeString);
    doubleQuoteString(m_stringValue, output);
}

JSONObjectBase::~JSONObjectBase()
{
}

bool JSONObjectBase::asObject(RefPtr<JSONObject>* output)
{
    COMPILE_ASSERT(sizeof(JSONObject) == sizeof(JSONObjectBase), cannot_cast);
    *output = static_cast<JSONObject*>(this);
    return true;
}

PassRefPtr<JSONObject> JSONObjectBase::asObject()
{
    return openAccessors();
}

JSONObject* JSONObjectBase::openAccessors()
{
    COMPILE_ASSERT(sizeof(JSONObject) == sizeof(JSONObjectBase), cannot_cast);
    return static_cast<JSONObject*>(this);
}

bool JSONObjectBase::getBoolean(const String& name, bool* output) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
}

bool JSONObjectBase::getString(const String& name, String* output) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return false;
    return value->asString(output);
}

PassRefPtr<JSONObject> JSONObjectBase::getObject(const String& name) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return 0;
    return value->asObject();
}

PassRefPtr<JSONArray> JSONObjectBase::getArray(const String& name) const
{
    RefPtr<JSONValue> value = get(name);
    if (!value)
        return 0;
    return value->asArray();
}

PassRefPtr<JSONValue> JSONObjectBase::get(const String& name) const
{
    Dictionary::const_iterator it = m_data.find(name);
    if (it == m_data.end())
        return 0;
    return it->value;
}

void JSONObjectBase::remove(const String& name)
{
    m_data.remove(name);
    for (size_t i = 0; i < m_order.size(); ++i) {
        if (m_order[i] == name) {
            m_order.remove(i);
            break;
        }
    }
}

void JSONObjectBase::writeJSON(StringBuilder* output) const
{
    output->append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        ASSERT_WITH_SECURITY_IMPLICATION(it != m_data.end());
        if (i)
            output->append(',');
        doubleQuoteString(it->key, output);
        output->append(':');
        it->value->writeJSON(output);
    }
    output->append('}');
}

JSONObjectBase::JSONObjectBase()
    : JSONValue(TypeObject)
    , m_data()
    , m_order()
{
}

JSONArrayBase::~JSONArrayBase()
{
}

bool JSONArrayBase::asArray(RefPtr<JSONArray>* output)
{
    COMPILE_ASSERT(sizeof(JSONArrayBase) == sizeof(JSONArray), cannot_cast);
    *output = static_cast<JSONArray*>(this);
    return true;
}

PassRefPtr<JSONArray> JSONArrayBase::asArray()
{
    COMPILE_ASSERT(sizeof(JSONArrayBase) == sizeof(JSONArray), cannot_cast);
    return static_cast<JSONArray*>(this);
}

void JSONArrayBase::writeJSON(StringBuilder* output) const
{
    output->append('[');
    for (Vector<RefPtr<JSONValue> >::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        (*it)->writeJSON(output);
    }
    output->append(']');
}

JSONArrayBase::JSONArrayBase()
    : JSONValue(TypeArray)
    , m_data()
{
}

PassRefPtr<JSONValue> JSONArrayBase::get(size_t index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_data.size());
    return m_data[index];
}

} // namespace WebCore
