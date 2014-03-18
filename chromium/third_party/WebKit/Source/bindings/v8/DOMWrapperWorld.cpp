/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/DOMWrapperWorld.h"

#include "V8Window.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMActivityLogger.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/V8WindowShell.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/HashTraits.h"
#include "wtf/MainThread.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

unsigned DOMWrapperWorld::isolatedWorldCount = 0;
static bool initializingWindow = false;

void DOMWrapperWorld::setInitializingWindow(bool initializing)
{
    initializingWindow = initializing;
}

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::createMainWorld()
{
    return adoptRef(new DOMWrapperWorld(MainWorldId, mainWorldExtensionGroup));
}

DOMWrapperWorld::DOMWrapperWorld(int worldId, int extensionGroup)
    : m_worldId(worldId)
    , m_extensionGroup(extensionGroup)
{
    if (isIsolatedWorld())
        m_domDataStore = adoptPtr(new DOMDataStore(IsolatedWorld));
}

DOMWrapperWorld* DOMWrapperWorld::current()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    v8::Handle<v8::Context> context = isolate->GetCurrentContext();
    if (!V8DOMWrapper::isWrapperOfType(toInnerGlobalObject(context), &V8Window::wrapperTypeInfo))
        return 0;
    ASSERT(isMainThread());
    if (DOMWrapperWorld* world = isolatedWorld(context))
        return world;
    return mainThreadNormalWorld();
}

DOMWrapperWorld* mainThreadNormalWorld()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_REF(DOMWrapperWorld, cachedNormalWorld, (DOMWrapperWorld::createMainWorld()));
    return cachedNormalWorld;
}

// FIXME: Remove this function. There is currently an issue with the inspector related to the call to dispatchDidClearWindowObjectInWorld in ScriptController::windowShell.
DOMWrapperWorld* existingWindowShellWorkaroundWorld()
{
    DEFINE_STATIC_REF(DOMWrapperWorld, world, (adoptRef(new DOMWrapperWorld(MainWorldId - 1, DOMWrapperWorld::mainWorldExtensionGroup - 1))));
    return world;
}

bool DOMWrapperWorld::contextHasCorrectPrototype(v8::Handle<v8::Context> context)
{
    ASSERT(isMainThread());
    if (initializingWindow)
        return true;
    return V8DOMWrapper::isWrapperOfType(toInnerGlobalObject(context), &V8Window::wrapperTypeInfo);
}

void DOMWrapperWorld::setIsolatedWorldField(v8::Handle<v8::Context> context)
{
    V8PerContextDataHolder::from(context)->setIsolatedWorld(isMainWorld() ? 0 : this);
}

typedef HashMap<int, DOMWrapperWorld*> WorldMap;
static WorldMap& isolatedWorldMap()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(WorldMap, map, ());
    return map;
}

void DOMWrapperWorld::getAllWorlds(Vector<RefPtr<DOMWrapperWorld> >& worlds)
{
    worlds.append(mainThreadNormalWorld());
    WorldMap& isolatedWorlds = isolatedWorldMap();
    for (WorldMap::iterator it = isolatedWorlds.begin(); it != isolatedWorlds.end(); ++it)
        worlds.append(it->value);
}

DOMWrapperWorld::~DOMWrapperWorld()
{
    ASSERT(!isMainWorld());

    if (!isIsolatedWorld())
        return;

    WorldMap& map = isolatedWorldMap();
    WorldMap::iterator i = map.find(m_worldId);
    if (i == map.end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(i->value == this);

    map.remove(i);
    isolatedWorldCount--;
    ASSERT(map.size() == isolatedWorldCount);
}

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::ensureIsolatedWorld(int worldId, int extensionGroup)
{
    ASSERT(worldId > MainWorldId);

    WorldMap& map = isolatedWorldMap();
    WorldMap::AddResult result = map.add(worldId, 0);
    RefPtr<DOMWrapperWorld> world = result.iterator->value;
    if (world) {
        ASSERT(world->worldId() == worldId);
        ASSERT(world->extensionGroup() == extensionGroup);
        return world.release();
    }

    world = adoptRef(new DOMWrapperWorld(worldId, extensionGroup));
    result.iterator->value = world.get();
    isolatedWorldCount++;
    ASSERT(map.size() == isolatedWorldCount);

    return world.release();
}

v8::Handle<v8::Context> DOMWrapperWorld::context(ScriptController& controller)
{
    return controller.windowShell(this)->context();
}

typedef HashMap<int, RefPtr<SecurityOrigin> > IsolatedWorldSecurityOriginMap;
static IsolatedWorldSecurityOriginMap& isolatedWorldSecurityOrigins()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(IsolatedWorldSecurityOriginMap, map, ());
    return map;
}

SecurityOrigin* DOMWrapperWorld::isolatedWorldSecurityOrigin()
{
    ASSERT(this->isIsolatedWorld());
    IsolatedWorldSecurityOriginMap& origins = isolatedWorldSecurityOrigins();
    IsolatedWorldSecurityOriginMap::iterator it = origins.find(worldId());
    return it == origins.end() ? 0 : it->value.get();
}

void DOMWrapperWorld::setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin> securityOrigin)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    if (securityOrigin)
        isolatedWorldSecurityOrigins().set(worldID, securityOrigin);
    else
        isolatedWorldSecurityOrigins().remove(worldID);
}

void DOMWrapperWorld::clearIsolatedWorldSecurityOrigin(int worldID)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    isolatedWorldSecurityOrigins().remove(worldID);
}

typedef HashMap<int, bool> IsolatedWorldContentSecurityPolicyMap;
static IsolatedWorldContentSecurityPolicyMap& isolatedWorldContentSecurityPolicies()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(IsolatedWorldContentSecurityPolicyMap, map, ());
    return map;
}

bool DOMWrapperWorld::isolatedWorldHasContentSecurityPolicy()
{
    ASSERT(this->isIsolatedWorld());
    IsolatedWorldContentSecurityPolicyMap& policies = isolatedWorldContentSecurityPolicies();
    IsolatedWorldContentSecurityPolicyMap::iterator it = policies.find(worldId());
    return it == policies.end() ? false : it->value;
}

void DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(int worldID, const String& policy)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    if (!policy.isEmpty())
        isolatedWorldContentSecurityPolicies().set(worldID, true);
    else
        isolatedWorldContentSecurityPolicies().remove(worldID);
}

void DOMWrapperWorld::clearIsolatedWorldContentSecurityPolicy(int worldID)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    isolatedWorldContentSecurityPolicies().remove(worldID);
}

typedef HashMap<int, OwnPtr<V8DOMActivityLogger>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int> > DOMActivityLoggerMap;
static DOMActivityLoggerMap& domActivityLoggers()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(DOMActivityLoggerMap, map, ());
    return map;
}

void DOMWrapperWorld::setActivityLogger(int worldId, PassOwnPtr<V8DOMActivityLogger> logger)
{
    domActivityLoggers().set(worldId, logger);
}

V8DOMActivityLogger* DOMWrapperWorld::activityLogger(int worldId)
{
    DOMActivityLoggerMap& loggers = domActivityLoggers();
    DOMActivityLoggerMap::iterator it = loggers.find(worldId);
    return it == loggers.end() ? 0 : it->value.get();
}

} // namespace WebCore
