// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_MATCHER_H_
#define EXTENSIONS_COMMON_EVENT_MATCHER_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace extensions {

class EventFilteringInfo;

extern const char kEventFilterServiceTypeKey[];

// Matches EventFilteringInfos against a set of criteria. This is intended to
// be used by EventFilter which performs efficient URL matching across
// potentially many EventMatchers itself. This is why this class only exposes
// MatchNonURLCriteria() - URL matching is handled by EventFilter.
class EventMatcher {
 public:
  EventMatcher(scoped_ptr<base::DictionaryValue> filter,
               int routing_id);
  ~EventMatcher();

  // Returns true if |event_info| satisfies this matcher's criteria, not taking
  // into consideration any URL criteria.
  bool MatchNonURLCriteria(const EventFilteringInfo& event_info) const;

  int GetURLFilterCount() const;
  bool GetURLFilter(int i, base::DictionaryValue** url_filter_out);

  std::string GetServiceTypeFilter() const;

  int HasURLFilters() const;

  int GetInstanceID() const;

  int GetRoutingID() const;

  base::DictionaryValue* value() const {
    return filter_.get();
  }

 private:
  // Contains a dictionary that corresponds to a single event filter, eg:
  //
  // {url: [{hostSuffix: 'google.com'}]}
  //
  // The valid filter keys are event-specific.
  scoped_ptr<base::DictionaryValue> filter_;

  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(EventMatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_MATCHER_H_
