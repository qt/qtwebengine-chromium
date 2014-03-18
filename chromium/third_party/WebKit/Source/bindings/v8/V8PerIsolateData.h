/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8PerIsolateData_h
#define V8PerIsolateData_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "gin/public/gin_embedders.h"
#include <v8.h>
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class DOMDataStore;
class GCEventData;
class StringCache;
class V8HiddenPropertyName;
struct WrapperTypeInfo;

class ExternalStringVisitor;

typedef WTF::Vector<DOMDataStore*> DOMDataList;

class V8PerIsolateData {
public:
    static V8PerIsolateData* create(v8::Isolate*);
    static void ensureInitialized(v8::Isolate*);
    static V8PerIsolateData* current()
    {
        return from(v8::Isolate::GetCurrent());
    }
    static V8PerIsolateData* from(v8::Isolate* isolate)
    {
        ASSERT(isolate);
        ASSERT(isolate->GetData(gin::kEmbedderBlink));
        return static_cast<V8PerIsolateData*>(isolate->GetData(gin::kEmbedderBlink));
    }
    static void dispose(v8::Isolate*);

    typedef HashMap<const void*, UnsafePersistent<v8::FunctionTemplate> > TemplateMap;

    TemplateMap& rawDOMTemplateMap(WrapperWorldType worldType)
    {
        if (worldType == MainWorld)
            return m_rawDOMTemplatesForMainWorld;
        return m_rawDOMTemplatesForNonMainWorld;
    }

    TemplateMap& templateMap(WrapperWorldType worldType)
    {
        if (worldType == MainWorld)
            return m_templatesForMainWorld;
        return m_templatesForNonMainWorld;
    }

    v8::Handle<v8::FunctionTemplate> toStringTemplate();
    v8::Handle<v8::FunctionTemplate> lazyEventListenerToStringTemplate()
    {
        return v8::Local<v8::FunctionTemplate>::New(m_isolate, m_lazyEventListenerToStringTemplate);
    }

    StringCache* stringCache() { return m_stringCache.get(); }

    v8::Persistent<v8::Value>& ensureLiveRoot();

    DOMDataList& allStores() { return m_domDataList; }

    V8HiddenPropertyName* hiddenPropertyName() { return m_hiddenPropertyName.get(); }

    void registerDOMDataStore(DOMDataStore* domDataStore)
    {
        ASSERT(m_domDataList.find(domDataStore) == kNotFound);
        m_domDataList.append(domDataStore);
    }

    void unregisterDOMDataStore(DOMDataStore* domDataStore)
    {
        ASSERT(m_domDataList.find(domDataStore) != kNotFound);
        m_domDataList.remove(m_domDataList.find(domDataStore));
    }

    // DOMDataStore is owned outside V8PerIsolateData.
    DOMDataStore* workerDOMDataStore() { return m_workerDomDataStore; }
    void setWorkerDOMDataStore(DOMDataStore* store) { m_workerDomDataStore = store; }

    int recursionLevel() const { return m_recursionLevel; }
    int incrementRecursionLevel() { return ++m_recursionLevel; }
    int decrementRecursionLevel() { return --m_recursionLevel; }

#ifndef NDEBUG
    int internalScriptRecursionLevel() const { return m_internalScriptRecursionLevel; }
    int incrementInternalScriptRecursionLevel() { return ++m_internalScriptRecursionLevel; }
    int decrementInternalScriptRecursionLevel() { return --m_internalScriptRecursionLevel; }
#endif

    GCEventData* gcEventData() { return m_gcEventData.get(); }

    // Gives the system a hint that we should request garbage collection
    // upon the next close or navigation event, because some expensive
    // objects have been allocated that we want to take every opportunity
    // to collect.
    void setShouldCollectGarbageSoon() { m_shouldCollectGarbageSoon = true; }
    void clearShouldCollectGarbageSoon() { m_shouldCollectGarbageSoon = false; }
    bool shouldCollectGarbageSoon() const { return m_shouldCollectGarbageSoon; }

    v8::Handle<v8::FunctionTemplate> privateTemplate(WrapperWorldType, void* privatePointer, v8::FunctionCallback = 0, v8::Handle<v8::Value> data = v8::Handle<v8::Value>(), v8::Handle<v8::Signature> = v8::Handle<v8::Signature>(), int length = 0);
    v8::Handle<v8::FunctionTemplate> privateTemplateIfExists(WrapperWorldType, void* privatePointer);
    void setPrivateTemplate(WrapperWorldType, void* privatePointer, v8::Handle<v8::FunctionTemplate>);

    v8::Handle<v8::FunctionTemplate> rawDOMTemplate(const WrapperTypeInfo*, WrapperWorldType);

    bool hasInstance(const WrapperTypeInfo*, v8::Handle<v8::Value>, WrapperWorldType);

    v8::Local<v8::Context> ensureRegexContext();

    const char* previousSamplingState() const { return m_previousSamplingState; }
    void setPreviousSamplingState(const char* name) { m_previousSamplingState = name; }

private:
    explicit V8PerIsolateData(v8::Isolate*);
    ~V8PerIsolateData();
    static void constructorOfToString(const v8::FunctionCallbackInfo<v8::Value>&);

    v8::Isolate* m_isolate;
    TemplateMap m_rawDOMTemplatesForMainWorld;
    TemplateMap m_rawDOMTemplatesForNonMainWorld;
    TemplateMap m_templatesForMainWorld;
    TemplateMap m_templatesForNonMainWorld;
    ScopedPersistent<v8::FunctionTemplate> m_toStringTemplate;
    v8::Persistent<v8::FunctionTemplate> m_lazyEventListenerToStringTemplate;
    OwnPtr<StringCache> m_stringCache;

    Vector<DOMDataStore*> m_domDataList;
    DOMDataStore* m_workerDomDataStore;

    OwnPtr<V8HiddenPropertyName> m_hiddenPropertyName;
    ScopedPersistent<v8::Value> m_liveRoot;
    ScopedPersistent<v8::Context> m_regexContext;

    const char* m_previousSamplingState;

    bool m_constructorMode;
    friend class ConstructorMode;

    int m_recursionLevel;

#ifndef NDEBUG
    int m_internalScriptRecursionLevel;
#endif
    OwnPtr<GCEventData> m_gcEventData;
    bool m_shouldCollectGarbageSoon;
};

} // namespace WebCore

#endif // V8PerIsolateData_h
