/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "InstanceCounter.h"

#include "wtf/HashMap.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WTF {

#ifdef ENABLE_INSTANCE_COUNTER

// This function is used to stringify a typename T without using RTTI.
// The result of extractNameFunc<T>() is given as |funcName|. |extractNameFromFunctionName| then extracts a typename string from |funcName|.
String extractNameFromFunctionName(const char* funcName)
{
#if COMPILER(GCC)
    const size_t prefixLength = sizeof("const char* WTF::extractNameFunc() [with T = ") - 1;

    size_t funcNameLength = strlen(funcName);
    ASSERT(funcNameLength > prefixLength + 1);

    const char* funcNameWithoutPrefix = funcName + prefixLength;
    return String(funcNameWithoutPrefix, funcNameLength - prefixLength - 1 /* last ] */);
#else
    // FIXME: Support other compilers
    ASSERT(false);
#endif
}

class InstanceCounter {
public:
    void incrementInstanceCount(const String& instanceName, void* ptr);
    void decrementInstanceCount(const String& instanceName, void* ptr);
    String dump();

    static InstanceCounter* instance()
    {
        DEFINE_STATIC_LOCAL(InstanceCounter, self, ());
        return &self;
    }

private:
    InstanceCounter() { }

    Mutex m_mutex;
    HashMap<String, int> m_counterMap;
};

void incrementInstanceCount(const char* extractNameFuncName, void* ptr)
{
    String instanceName = extractNameFromFunctionName(extractNameFuncName);
    InstanceCounter::instance()->incrementInstanceCount(instanceName, ptr);
}

void decrementInstanceCount(const char* extractNameFuncName, void* ptr)
{
    String instanceName = extractNameFromFunctionName(extractNameFuncName);
    InstanceCounter::instance()->decrementInstanceCount(instanceName, ptr);
}

String dumpRefCountedInstanceCounts()
{
    return InstanceCounter::instance()->dump();
}

void InstanceCounter::incrementInstanceCount(const String& instanceName, void* ptr)
{
    MutexLocker locker(m_mutex);
    HashMap<String, int>::AddResult result = m_counterMap.add(instanceName, 1);
    if (!result.isNewEntry)
        ++(result.iterator->value);
}

void InstanceCounter::decrementInstanceCount(const String& instanceName, void* ptr)
{
    MutexLocker locker(m_mutex);
    HashMap<String, int>::iterator it = m_counterMap.find(instanceName);
    ASSERT(it != m_counterMap.end());

    --(it->value);
    if (!it->value)
        m_counterMap.remove(it);
}

String InstanceCounter::dump()
{
    MutexLocker locker(m_mutex);

    StringBuilder builder;

    builder.append("{");
    HashMap<String, int>::iterator it = m_counterMap.begin();
    HashMap<String, int>::iterator itEnd = m_counterMap.end();
    for (; it != itEnd; ++it) {
        if (it != m_counterMap.begin())
            builder.append(",");
        builder.append("\"");
        builder.append(it->key);
        builder.append("\": ");
        builder.append(String::number(it->value));
    }
    builder.append("}");

    return builder.toString();
}

#else

String dumpRefCountedInstanceCounts()
{
    return String("{}");
}

#endif // ENABLE_INSTANCE_COUNTER

} // namespace WTF
