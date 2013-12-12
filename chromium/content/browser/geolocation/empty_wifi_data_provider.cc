// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/empty_wifi_data_provider.h"

namespace content {

EmptyWifiDataProvider::EmptyWifiDataProvider() {
}

EmptyWifiDataProvider::~EmptyWifiDataProvider() {
}

bool EmptyWifiDataProvider::GetData(WifiData* data) {
  DCHECK(data);
  // This is all the data we can get - nothing.
  return true;
}

// Only define for platforms that lack a real wifi data provider.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
// static
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new EmptyWifiDataProvider();
}
#endif

}  // namespace content
