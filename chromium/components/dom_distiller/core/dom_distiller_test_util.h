// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_

#include "components/dom_distiller/core/dom_distiller_observer.h"
#include "components/dom_distiller/core/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {

class DomDistillerStore;

namespace test {
namespace util {

class ObserverUpdatesMatcher
    : public testing::MatcherInterface<
          const std::vector<DomDistillerObserver::ArticleUpdate>&> {
 public:
  explicit ObserverUpdatesMatcher(
      const std::vector<DomDistillerObserver::ArticleUpdate>&);

  // MatcherInterface overrides.
  virtual bool MatchAndExplain(
      const std::vector<DomDistillerObserver::ArticleUpdate>& actual_updates,
      testing::MatchResultListener* listener) const OVERRIDE;
  virtual void DescribeTo(std::ostream* os) const OVERRIDE;
  virtual void DescribeNegationTo(std::ostream* os) const OVERRIDE;

 private:
  void DescribeUpdates(std::ostream* os) const;
  const std::vector<DomDistillerObserver::ArticleUpdate>& expected_updates_;
};

testing::Matcher<const std::vector<DomDistillerObserver::ArticleUpdate>&>
    HasExpectedUpdates(const std::vector<DomDistillerObserver::ArticleUpdate>&);

// Creates a simple DomDistillerStore backed by |fake_db| and initialized
// with |store_model|.
DomDistillerStore* CreateStoreWithFakeDB(FakeDB* fake_db,
                                         FakeDB::EntryMap* store_model);

}  // namespace util
}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_
