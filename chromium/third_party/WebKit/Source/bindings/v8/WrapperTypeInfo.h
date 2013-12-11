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

#ifndef WrapperTypeInfo_h
#define WrapperTypeInfo_h

#include "wtf/Assertions.h"
#include <v8.h>

namespace WebCore {

    class ActiveDOMObject;
    class DOMDataStore;
    class EventTarget;
    class Node;

    static const int v8DOMWrapperTypeIndex = 0;
    static const int v8DOMWrapperObjectIndex = 1;
    static const int v8DefaultWrapperInternalFieldCount = 2;
    static const int v8PrototypeTypeIndex = 0;
    static const int v8PrototypeInternalFieldcount = 1;

    static const uint16_t v8DOMNodeClassId = 1;
    static const uint16_t v8DOMObjectClassId = 2;

    enum WrapperWorldType {
        MainWorld,
        IsolatedWorld,
        WorkerWorld
    };

    typedef v8::Handle<v8::FunctionTemplate> (*GetTemplateFunction)(v8::Isolate*, WrapperWorldType);
    typedef void (*DerefObjectFunction)(void*);
    typedef ActiveDOMObject* (*ToActiveDOMObjectFunction)(v8::Handle<v8::Object>);
    typedef EventTarget* (*ToEventTargetFunction)(v8::Handle<v8::Object>);
    typedef void* (*OpaqueRootForGC)(void*, v8::Isolate*);
    typedef void (*InstallPerContextPrototypePropertiesFunction)(v8::Handle<v8::Object>, v8::Isolate*);

    enum WrapperTypePrototype {
        WrapperTypeObjectPrototype,
        WrapperTypeErrorPrototype
    };

    // This struct provides a way to store a bunch of information that is helpful when unwrapping
    // v8 objects. Each v8 bindings class has exactly one static WrapperTypeInfo member, so
    // comparing pointers is a safe way to determine if types match.
    struct WrapperTypeInfo {

        static WrapperTypeInfo* unwrap(v8::Handle<v8::Value> typeInfoWrapper)
        {
            return reinterpret_cast<WrapperTypeInfo*>(v8::External::Cast(*typeInfoWrapper)->Value());
        }


        bool equals(const WrapperTypeInfo* that) const
        {
            return this == that;
        }

        bool isSubclass(const WrapperTypeInfo* that) const
        {
            for (const WrapperTypeInfo* current = this; current; current = current->parentClass) {
                if (current == that)
                    return true;
            }

            return false;
        }

        v8::Handle<v8::FunctionTemplate> getTemplate(v8::Isolate* isolate, WrapperWorldType worldType) { return getTemplateFunction(isolate, worldType); }

        void derefObject(void* object)
        {
            if (derefObjectFunction)
                derefObjectFunction(object);
        }

        void installPerContextPrototypeProperties(v8::Handle<v8::Object> proto, v8::Isolate* isolate)
        {
            if (installPerContextPrototypePropertiesFunction)
                installPerContextPrototypePropertiesFunction(proto, isolate);
        }

        ActiveDOMObject* toActiveDOMObject(v8::Handle<v8::Object> object)
        {
            if (!toActiveDOMObjectFunction)
                return 0;
            return toActiveDOMObjectFunction(object);
        }

        EventTarget* toEventTarget(v8::Handle<v8::Object> object)
        {
            if (!toEventTargetFunction)
                return 0;
            return toEventTargetFunction(object);
        }

        void* opaqueRootForGC(void* object, v8::Isolate* isolate)
        {
            if (!opaqueRootForGCFunction)
                return object;
            return opaqueRootForGCFunction(object, isolate);
        }

        const GetTemplateFunction getTemplateFunction;
        const DerefObjectFunction derefObjectFunction;
        const ToActiveDOMObjectFunction toActiveDOMObjectFunction;
        const ToEventTargetFunction toEventTargetFunction;
        const OpaqueRootForGC opaqueRootForGCFunction;
        const InstallPerContextPrototypePropertiesFunction installPerContextPrototypePropertiesFunction;
        const WrapperTypeInfo* parentClass;
        const WrapperTypePrototype wrapperTypePrototype;
    };

    template<typename T, int offset>
    inline T* getInternalField(const v8::Persistent<v8::Object>& persistent)
    {
        // This would be unsafe, but InternalFieldCount and GetAlignedPointerFromInternalField are guaranteed not to allocate
        const v8::Handle<v8::Object>& object = reinterpret_cast<const v8::Handle<v8::Object>&>(persistent);
        ASSERT(offset < object->InternalFieldCount());
        return static_cast<T*>(object->GetAlignedPointerFromInternalField(offset));
    }

    template<typename T, int offset>
    inline T* getInternalField(v8::Handle<v8::Object> object)
    {
        ASSERT(offset < object->InternalFieldCount());
        return static_cast<T*>(object->GetAlignedPointerFromInternalField(offset));
    }

    inline void* toNative(const v8::Persistent<v8::Object>& object)
    {
        return getInternalField<void, v8DOMWrapperObjectIndex>(object);
    }

    inline void* toNative(v8::Handle<v8::Object> object)
    {
        return getInternalField<void, v8DOMWrapperObjectIndex>(object);
    }

    inline WrapperTypeInfo* toWrapperTypeInfo(const v8::Persistent<v8::Object>& object)
    {
        return getInternalField<WrapperTypeInfo, v8DOMWrapperTypeIndex>(object);
    }

    inline WrapperTypeInfo* toWrapperTypeInfo(v8::Handle<v8::Object> object)
    {
        return getInternalField<WrapperTypeInfo, v8DOMWrapperTypeIndex>(object);
    }

    struct WrapperConfiguration {

        enum Lifetime {
            Dependent, Independent
        };

        void configureWrapper(v8::Persistent<v8::Object>* wrapper) const
        {
            wrapper->SetWrapperClassId(classId);
            if (lifetime == Independent)
                wrapper->MarkIndependent();
        }

        const uint16_t classId;
        const Lifetime lifetime;
    };

    inline WrapperConfiguration buildWrapperConfiguration(void*, WrapperConfiguration::Lifetime lifetime)
    {
        WrapperConfiguration configuration = {v8DOMObjectClassId, lifetime};
        return configuration;
    }

    inline WrapperConfiguration buildWrapperConfiguration(Node*, WrapperConfiguration::Lifetime lifetime)
    {
        WrapperConfiguration configuration = {v8DOMNodeClassId, lifetime};
        return configuration;
    }

    template<class ElementType>
    class WrapperTypeTraits {
        // specialized classes have thier own functions, which are generated by binding generator.
    };
}

#endif // WrapperTypeInfo_h
