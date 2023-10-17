// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_page_data.h"

#include "base/no_destructor.h"

namespace content {

AdAuctionPageData::AdAuctionPageData(Page& page)
    : PageUserData<AdAuctionPageData>(page) {}

AdAuctionPageData::~AdAuctionPageData() = default;

PAGE_USER_DATA_KEY_IMPL(AdAuctionPageData);

void AdAuctionPageData::AddAuctionResultWitnessForOrigin(
    const url::Origin& origin,
    const std::string& response) {
  origin_auction_result_map_[origin].insert(response);
}

bool AdAuctionPageData::WitnessedAuctionResultForOrigin(
    const url::Origin& origin,
    const std::string& response) const {
  auto it = origin_auction_result_map_.find(origin);
  if (it == origin_auction_result_map_.end()) {
    return false;
  }

  return it->second.contains(response);
}

void AdAuctionPageData::AddAuctionSignalsWitnessForOrigin(
    const url::Origin& origin,
    const std::string& response) {
  origin_auction_signals_map_[origin].insert(response);
}

const std::set<std::string>& AdAuctionPageData::GetAuctionSignalsForOrigin(
    const url::Origin& origin) const {
  auto it = origin_auction_signals_map_.find(origin);
  if (it == origin_auction_signals_map_.end()) {
    static base::NoDestructor<std::set<std::string>> kEmptySet;
    return *kEmptySet;
  }

  return it->second;
}

}  // namespace content
