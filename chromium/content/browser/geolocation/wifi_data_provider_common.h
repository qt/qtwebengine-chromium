// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_

#include <assert.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/geolocation/wifi_data_provider.h"
#include "content/browser/geolocation/wifi_polling_policy.h"
#include "content/common/content_export.h"

namespace content {

// Converts a MAC address stored as an array of uint8 to a string.
string16 MacAddressAsString16(const uint8 mac_as_int[6]);

// Base class to promote code sharing between platform specific wifi data
// providers. It's optional for specific platforms to derive this, but if they
// do polling behavior is taken care of by this base class, and all the platform
// need do is provide the underlying WLAN access API and polling policy.
// Also designed this way for ease of testing the cross-platform behavior.
class CONTENT_EXPORT WifiDataProviderCommon : public WifiDataProviderImplBase {
 public:
  // Interface to abstract the low level data OS library call, and to allow
  // mocking (hence public).
  class WlanApiInterface {
   public:
    virtual ~WlanApiInterface() {}
    // Gets wifi data for all visible access points.
    virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data) = 0;
  };

  WifiDataProviderCommon();

  // WifiDataProviderImplBase implementation
  virtual void StartDataProvider() OVERRIDE;
  virtual void StopDataProvider() OVERRIDE;
  virtual bool GetData(WifiData* data) OVERRIDE;

 protected:
  virtual ~WifiDataProviderCommon();

  // Returns ownership.
  virtual WlanApiInterface* NewWlanApi() = 0;

  // Returns ownership.
  virtual WifiPollingPolicy* NewPollingPolicy() = 0;

 private:
  // Runs a scan. Calls the callbacks if new data is found.
  void DoWifiScanTask();

  // Will schedule a scan; i.e. enqueue DoWifiScanTask deferred task.
  void ScheduleNextScan(int interval);

  WifiData wifi_data_;

  // Whether we've successfully completed a scan for WiFi data.
  bool is_first_scan_complete_;

  // Underlying OS wifi API.
  scoped_ptr<WlanApiInterface> wlan_api_;

  // Controls the polling update interval.
  scoped_ptr<WifiPollingPolicy> polling_policy_;

  // Holder for delayed tasks; takes care of cleanup.
  base::WeakPtrFactory<WifiDataProviderCommon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderCommon);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_COMMON_H_
