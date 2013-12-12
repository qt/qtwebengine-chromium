// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "components/browser_context_keyed_service/browser_context_keyed_base_factory.h"
#include "content/public/browser/browser_context.h"

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/file_util.h"

// Dumps dependency information about our browser context keyed services
// into a dot file in the browser context directory.
const char kDumpBrowserContextDependencyGraphFlag[] =
    "dump-browser-context-graph";
#endif  // NDEBUG

void BrowserContextDependencyManager::AddComponent(
    BrowserContextKeyedBaseFactory* component) {
  dependency_graph_.AddNode(component);
}

void BrowserContextDependencyManager::RemoveComponent(
    BrowserContextKeyedBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void BrowserContextDependencyManager::AddEdge(
    BrowserContextKeyedBaseFactory* depended,
    BrowserContextKeyedBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void BrowserContextDependencyManager::CreateBrowserContextServices(
    content::BrowserContext* context) {
  DoCreateBrowserContextServices(context, false, false);
}

void BrowserContextDependencyManager::CreateBrowserContextServicesForTest(
    content::BrowserContext* context,
    bool force_register_prefs) {
  DoCreateBrowserContextServices(context, true, force_register_prefs);
}

void BrowserContextDependencyManager::DoCreateBrowserContextServices(
    content::BrowserContext* context,
    bool is_testing_context,
    bool force_register_prefs) {
  TRACE_EVENT0("browser",
    "BrowserContextDependencyManager::DoCreateBrowserContextServices")
#ifndef NDEBUG
  // Unmark |context| as dead. This exists because of unit tests, which will
  // often have similar stack structures. 0xWhatever might be created, go out
  // of scope, and then a new BrowserContext object might be created
  // at 0xWhatever.
  dead_context_pointers_.erase(context);
#endif

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpBrowserContextDependencies(context);
#endif

  for (size_t i = 0; i < construction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(construction_order[i]);

    if (!context->IsOffTheRecord() || force_register_prefs) {
      // We only register preferences on normal contexts because the incognito
      // context shares the pref service with the normal one. Always register
      // for standalone testing contexts (testing contexts that don't have an
      // "original" profile set) as otherwise the preferences won't be
      // registered.
      factory->RegisterUserPrefsOnBrowserContext(context);
    }

    if (is_testing_context && factory->ServiceIsNULLWhileTesting()) {
      factory->SetEmptyTestingFactory(context);
    } else if (factory->ServiceIsCreatedWithBrowserContext()) {
      // Create the service.
      factory->CreateServiceNow(context);
    }
  }
}

void BrowserContextDependencyManager::DestroyBrowserContextServices(
    content::BrowserContext* context) {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpBrowserContextDependencies(context);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(destruction_order[i]);
    factory->BrowserContextShutdown(context);
  }

#ifndef NDEBUG
  // The context is now dead to the rest of the program.
  dead_context_pointers_.insert(context);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(destruction_order[i]);
    factory->BrowserContextDestroyed(context);
  }
}

#ifndef NDEBUG
void BrowserContextDependencyManager::AssertBrowserContextWasntDestroyed(
    content::BrowserContext* context) {
  if (dead_context_pointers_.find(context) != dead_context_pointers_.end()) {
    NOTREACHED() << "Attempted to access a BrowserContext that was ShutDown(). "
                 << "This is most likely a heap smasher in progress. After "
                 << "BrowserContextKeyedService::Shutdown() completes, your "
                 << "service MUST NOT refer to depended BrowserContext "
                 << "services again.";
  }
}
#endif

// static
BrowserContextDependencyManager*
BrowserContextDependencyManager::GetInstance() {
  return Singleton<BrowserContextDependencyManager>::get();
}

BrowserContextDependencyManager::BrowserContextDependencyManager() {
}

BrowserContextDependencyManager::~BrowserContextDependencyManager() {
}

#ifndef NDEBUG
namespace {

std::string BrowserContextKeyedBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<BrowserContextKeyedBaseFactory*>(node)->name();
}

}  // namespace

void BrowserContextDependencyManager::DumpBrowserContextDependencies(
    content::BrowserContext* context) {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/context/context-dependencies.dot".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          kDumpBrowserContextDependencyGraphFlag)) {
    base::FilePath dot_file =
        context->GetPath().AppendASCII("browser-context-dependencies.dot");
    std::string contents = dependency_graph_.DumpAsGraphviz(
        "BrowserContext",
        base::Bind(&BrowserContextKeyedBaseFactoryGetNodeName));
    file_util::WriteFile(dot_file, contents.c_str(), contents.size());
  }
}
#endif  // NDEBUG
