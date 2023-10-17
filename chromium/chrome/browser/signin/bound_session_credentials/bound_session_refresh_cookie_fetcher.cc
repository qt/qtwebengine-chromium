// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

std::ostream& operator<<(
    std::ostream& os,
    const BoundSessionRefreshCookieFetcher::Result& result) {
  switch (result) {
    case BoundSessionRefreshCookieFetcher::Result::kSuccess:
      return os << "Cookie rotation request finished with success.";
    case BoundSessionRefreshCookieFetcher::Result::kConnectionError:
      return os << "Cookie rotation request finished with Connection error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerTransientError:
      return os
             << "Cookie rotation request finished with Server transient error.";
    case BoundSessionRefreshCookieFetcher::Result::kServerPersistentError:
      return os << "Cookie rotation request finished with Server Persistent "
                   "error.";
  }
}
