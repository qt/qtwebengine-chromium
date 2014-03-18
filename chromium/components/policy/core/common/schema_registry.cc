// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_registry.h"

#include "base/logging.h"

namespace policy {

SchemaRegistry::Observer::~Observer() {}

SchemaRegistry::SchemaRegistry() : schema_map_(new SchemaMap) {
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i)
    domains_ready_[i] = false;
#if !defined(ENABLE_EXTENSIONS)
  domains_ready_[POLICY_DOMAIN_EXTENSIONS] = true;
#endif
}

SchemaRegistry::~SchemaRegistry() {}

void SchemaRegistry::RegisterComponent(const PolicyNamespace& ns,
                                       const Schema& schema) {
  ComponentMap map;
  map[ns.component_id] = schema;
  RegisterComponents(ns.domain, map);
}

void SchemaRegistry::RegisterComponents(PolicyDomain domain,
                                        const ComponentMap& components) {
  // Don't issue notifications if nothing is being registered.
  if (components.empty())
    return;
  // Assume that a schema was updated if the namespace was already registered
  // before.
  DomainMap map(schema_map_->GetDomains());
  for (ComponentMap::const_iterator it = components.begin();
       it != components.end(); ++it) {
    map[domain][it->first] = it->second;
  }
  schema_map_ = new SchemaMap(map);
  Notify(true);
}

void SchemaRegistry::UnregisterComponent(const PolicyNamespace& ns) {
  DomainMap map(schema_map_->GetDomains());
  if (map[ns.domain].erase(ns.component_id) != 0) {
    schema_map_ = new SchemaMap(map);
    Notify(false);
  } else {
    NOTREACHED();
  }
}

bool SchemaRegistry::IsReady() const {
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i) {
    if (!domains_ready_[i])
      return false;
  }
  return true;
}

void SchemaRegistry::SetReady(PolicyDomain domain) {
  if (domains_ready_[domain])
    return;
  domains_ready_[domain] = true;
  if (IsReady())
    FOR_EACH_OBSERVER(Observer, observers_, OnSchemaRegistryReady());
}

void SchemaRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SchemaRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SchemaRegistry::Notify(bool has_new_schemas) {
  FOR_EACH_OBSERVER(
      Observer, observers_, OnSchemaRegistryUpdated(has_new_schemas));
}

bool SchemaRegistry::HasObservers() const {
  return observers_.might_have_observers();
}

CombinedSchemaRegistry::CombinedSchemaRegistry()
    : own_schema_map_(new SchemaMap) {
  // The combined registry is always ready, since it can always start tracking
  // another registry that is not ready yet and going from "ready" to "not
  // ready" is not allowed.
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i)
    SetReady(static_cast<PolicyDomain>(i));
}

CombinedSchemaRegistry::~CombinedSchemaRegistry() {}

void CombinedSchemaRegistry::Track(SchemaRegistry* registry) {
  registries_.insert(registry);
  registry->AddObserver(this);
  // Recombine the maps only if the |registry| has any components other than
  // POLICY_DOMAIN_CHROME.
  if (registry->schema_map()->HasComponents())
    Combine(true);
}

void CombinedSchemaRegistry::Untrack(SchemaRegistry* registry) {
  registry->RemoveObserver(this);
  if (registries_.erase(registry) != 0) {
    if (registry->schema_map()->HasComponents())
      Combine(false);
  } else {
    NOTREACHED();
  }
}

void CombinedSchemaRegistry::RegisterComponents(
    PolicyDomain domain,
    const ComponentMap& components) {
  DomainMap map(own_schema_map_->GetDomains());
  for (ComponentMap::const_iterator it = components.begin();
       it != components.end(); ++it) {
    map[domain][it->first] = it->second;
  }
  own_schema_map_ = new SchemaMap(map);
  Combine(true);
}

void CombinedSchemaRegistry::UnregisterComponent(const PolicyNamespace& ns) {
  DomainMap map(own_schema_map_->GetDomains());
  if (map[ns.domain].erase(ns.component_id) != 0) {
    own_schema_map_ = new SchemaMap(map);
    Combine(false);
  } else {
    NOTREACHED();
  }
}

void CombinedSchemaRegistry::OnSchemaRegistryUpdated(bool has_new_schemas) {
  Combine(has_new_schemas);
}

void CombinedSchemaRegistry::OnSchemaRegistryReady() {
  // Ignore.
}

void CombinedSchemaRegistry::Combine(bool has_new_schemas) {
  // If two registries publish a Schema for the same component then it's
  // undefined which version gets in the combined registry.
  //
  // The common case is that both registries want policy for the same component,
  // and the Schemas should be the same; in that case this makes no difference.
  //
  // But if the Schemas are different then one of the components is out of date.
  // In that case the policy loaded will be valid only for one of them, until
  // the outdated components are updated. This is a known limitation of the
  // way policies are loaded currently, but isn't a problem worth fixing for
  // the time being.
  DomainMap map(own_schema_map_->GetDomains());
  for (std::set<SchemaRegistry*>::const_iterator reg_it = registries_.begin();
       reg_it != registries_.end(); ++reg_it) {
    const DomainMap& reg_domain_map = (*reg_it)->schema_map()->GetDomains();
    for (DomainMap::const_iterator domain_it = reg_domain_map.begin();
         domain_it != reg_domain_map.end(); ++domain_it) {
      const ComponentMap& reg_component_map = domain_it->second;
      for (ComponentMap::const_iterator comp_it = reg_component_map.begin();
           comp_it != reg_component_map.end(); ++comp_it) {
        map[domain_it->first][comp_it->first] = comp_it->second;
      }
    }
  }
  schema_map_ = new SchemaMap(map);
  Notify(has_new_schemas);
}

}  // namespace policy
